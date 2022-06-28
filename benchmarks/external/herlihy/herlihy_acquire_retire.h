
#ifndef HERLIHY_ACQUIRE_RETIRE_H
#define HERLIHY_ACQUIRE_RETIRE_H

#include <cstddef>

#include <algorithm>
#include <array>
#include <atomic>
#include <unordered_set>
#include <vector>

#include <cdrc/internal/utils.h>

namespace herlihy {

template<typename T, typename Deleter, size_t delay = 1>
struct acquire_retire {

  static_assert(std::is_trivially_copyable_v<T>, "T must be a trivially copyable type for acquire-retire");

 private:
  // Align to cache line boundary to avoid false sharing
  struct alignas(64) LocalSlot {
    std::atomic<T> announcement;
    LocalSlot() : announcement(nullptr) { }
  };

 public:
  // An RAII wrapper around an acquired handle. Automatically
  // releases the handle when the wrapper goes out of scope.
  template<typename U>
  struct acquired {
   public:
    U value;

    acquired& operator=(acquired &&other) noexcept {
      value = other.value;
      slot = other.slot;
      other.value = nullptr;
      other.slot = nullptr;
    }

    ~acquired() {
      if (value != nullptr) {
        assert(slot != nullptr);
        slot->store(nullptr, std::memory_order_release);
      }
    }

   private:
    friend acquire_retire;  // enable acquire_retire to use the private constructor
    std::atomic<T> *slot;

    acquired() : value(nullptr), slot(nullptr) {}
    acquired(U value_, std::atomic<T> &slot_) : value(value_), slot(std::addressof(slot_)) {}
  };

  explicit acquire_retire(size_t num_threads) :
      announcement_slots(num_threads),
      in_progress(num_threads),
      deferred_destructs(num_threads),
      amortized_work(num_threads) {}

  template<typename U>
  [[nodiscard]] acquired<U> acquire(const std::atomic<U> *p) {
    auto id = utils::threadID.getTID();
    U result;
    do {
      result = p->load(std::memory_order_seq_cst);
      announcement_slots[id].announcement.store(static_cast<T>(result), std::memory_order_seq_cst);
    } while (p->load(std::memory_order_seq_cst) != result);
    return {result, announcement_slots[id].announcement};
  }

  void release() {
    auto id = utils::threadID.getTID();
    auto &slot = announcement_slots[id].announcement;
    slot.store(nullptr, std::memory_order_release);
  }

  void retire(T p) {
    auto id = utils::threadID.getTID();
    deferred_destructs[id].push_back(p);
    work_toward_ejects(1);
  }

  // Perform any remaining deferred destruction. Need to be very careful
  // about additional objects being queued for deferred destruction by
  // an object that was just destructed.
  ~acquire_retire() {
    in_progress.assign(in_progress.size(), true);

    // Loop because the destruction of one object could trigger the deferred
    // destruction of another object (possibly even in another thread), and
    // so on recursively.
    while (std::any_of(deferred_destructs.begin(), deferred_destructs.end(),
                       [](const auto &v) { return !v.empty(); })) {

      // Move all of the contents from the deferred destruction lists
      // into a single local list. We don't want to just iterate the
      // deferred lists because a destruction may trigger another
      // deferred destruction to be added to one of the lists, which
      // would invalidate its iterators
      std::vector<T> destructs;
      for (auto &v : deferred_destructs) {
        destructs.insert(destructs.end(), v.begin(), v.end());
        v.clear();
      }

      // Perform all of the pending deferred destructions
      for (auto x : destructs) {
        Deleter{}(x);
      }
    }
  }

 private:
  // Apply the function f to every currently announced handle
  template<typename F>
  void scan_slots(F &&f) {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    for (const auto &announcement_slot : announcement_slots) {
      auto x = announcement_slot.announcement.load(std::memory_order_seq_cst);
      if (x != nullptr) f(x);
    }
  }

  void work_toward_ejects(size_t work = 1) {
    auto id = utils::threadID.getTID();
    amortized_work[id] = amortized_work[id] + work;
    auto threshold = std::max<size_t>(30, delay * amortized_work.size());  // Always attempt at least 30 ejects
    while (!in_progress[id] && amortized_work[id] >= threshold) {
      amortized_work[id] = 0;
      if (deferred_destructs[id].size() == 0) break; // nothing to collect
      in_progress[id] = true;
      auto deferred = cdrc::internal::AlignedVector<T>(std::move(deferred_destructs[id]));

      std::unordered_multiset<T> announced;
      scan_slots([&](auto reserved) { announced.insert(reserved); });

      // For a given deferred decrement, we first check if it is announced, and, if so,
      // we defer it again. If it is not announced, it can be safely applied. If an
      // object is deferred / announced multiple times, each announcement only protects
      // against one of the deferred decrements, so for each object, the amount of
      // decrements applied in total will be #deferred - #announced
      auto f = [&](auto x) {
        auto it = announced.find(x);
        if (it == announced.end()) {
          Deleter{}(x);
          return true;
        } else {
          announced.erase(it);
          return false;
        }
      };

      // Remove the deferred decrements that are successfully applied
      deferred.erase(remove_if(deferred.begin(), deferred.end(), f), deferred.end());
      deferred_destructs[id].insert(deferred_destructs[id].end(), deferred.begin(), deferred.end());
      in_progress[id] = false;
    }
  }

  std::vector<LocalSlot> announcement_slots;                          // Announcement array slots
  std::vector<cdrc::internal::AlignedBool> in_progress;               // Local flags to prevent reentrancy while destructing
  std::vector<cdrc::internal::AlignedVector<T>> deferred_destructs;   // Thread-local lists of pending deferred destructs
  std::vector<cdrc::internal::AlignedInt> amortized_work;             // Amortized work to pay for ejecting deferred destructs
};

}  // namespace herlihy


#endif  // HERLIHY_ACQUIRE_RETIRE_H
