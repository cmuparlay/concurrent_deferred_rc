
#ifndef PARLAY_ATOMIC_ACQUIRE_RETIRE_H
#define PARLAY_ATOMIC_ACQUIRE_RETIRE_H

#include <cstddef>

#include <algorithm>
#include <array>
#include <atomic>
#include <unordered_set>
#include <vector>

#include "utils.hpp"

namespace cdrc {

namespace internal {

// A vector that is stored at 64-byte-aligned memory (this
// means that the header of the vector, not the heap buffer,
// is aligned to 64 bytes)
template<typename _Tp>
struct alignas(64) AlignedVector : public std::vector<_Tp> {
};

// A cache-line-aligned int to prevent false sharing
struct alignas(64) AlignedInt {
  AlignedInt() : x(0) {}

  /* implicit */ AlignedInt(unsigned int x_) : x(x_) {}

  operator unsigned int() const { return x; }

 private:
  unsigned int x;
};

// A cache-line-aligned bool to prevent false sharing
struct alignas(64) AlignedBool {
  AlignedBool() : b(false) {}

  /* implicit */ AlignedBool(bool b_) : b(b_) {}

  operator bool() const { return b; }

 private:
  bool b;
};

// An interface for safe memory reclamation that protects reference-counted
// resources by deferring their reference count decrements until no thread
// is still reading them.
//
// Unlike hazard pointers, acquire-retire allows multiple concurrent retires
// of the same handle, which is what makes it suitable for managing reference
// counted pointers, since multiple copies of the same reference counted pointer
// may need to be destructed (i.e., have their counter decremented) concurrently.
//
// T =          The type of the handle being protected. Must be trivially copyable
//              and pointer-like.
// Deleter =    A stateless functor whose call operator takes a handle to a retired
//              pointer and decrements its reference count
// Incrementer = A stateless functor whose call operator takes a handle to a snapshotted
//               object and increments its reference count
// snapshot_slots = The number of additional announcement slots available for
//                  snapshot pointers. More allows more snapshots to be alive
//                  at a time, but makes reclamation slower
// delay =    The maximum number of deferred destructions that will be held by
//            any one worker thread is at most delay * #threads.
//
template<typename T, typename Deleter, typename Incrementer, size_t snapshot_slots = 7, size_t delay = 1>
struct acquire_retire {

  static_assert(std::is_trivially_copyable_v<T>, "T must be a trivially copyable type for acquire-retire");

 private:
  // Align to cache line boundary to avoid false sharing
  struct alignas(64) LocalSlot {
    std::atomic<T> announcement;
    std::array<std::atomic<T>, snapshot_slots> snapshot_announcements{};
    alignas(64) size_t last_free{0};

    LocalSlot() : announcement(nullptr) {
      for (auto &a : snapshot_announcements) {
        std::atomic_init(&a, nullptr);
      }
    }
  };

 public:
  // An RAII wrapper around an acquired handle. Automatically
  // releases the handle when the wrapper goes out of scope.
  template<typename U> //requires std::is_convertible_v<U, T>
  struct acquired {
   public:
    U value;

    acquired &operator=(acquired &&other) {
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
    friend struct acquire_retire;

    friend class atomic_rc_ptr;

    std::atomic<T> *slot;

    acquired() : value(nullptr), slot(nullptr) {}

    acquired(U value_, std::atomic<T> &slot_) : value(value_), slot(std::addressof(slot_)) {}
  };

  acquire_retire(size_t num_threads) :
      announcement_slots(num_threads),
      in_progress(num_threads),
      deferred_destructs(num_threads),
      amortized_work(num_threads) {}

  template<typename U>
  //requires std::is_convertible_v<U, T>
  [[nodiscard]] acquired<U> acquire(const std::atomic<U> *p) {
    auto id = utils::threadID.getTID();
    U result;
    do {
      result = p->load(std::memory_order_seq_cst);
      announcement_slots[id].announcement.store(static_cast<T>(result), std::memory_order_seq_cst);
    } while (p->load(std::memory_order_seq_cst) != result);
    return {result, announcement_slots[id].announcement};
  }

  // Like acquire, but assuming that the caller already has a
  // copy of the handle and knows that it is protected
  template<typename U>
  //requires std::is_convertible_v<U, T>
  [[nodiscard]] acquired<U> reserve(U p) {
    auto id = utils::threadID.getTID();
    announcement_slots[id].announcement.store(static_cast<T>(p),
                                              std::memory_order_seq_cst); // TODO: memory_order_release could be sufficient here
    return {p, announcement_slots[id].announcement};
  }

  // Dummy function for when we need to conditionally reserve
  // something, but might need to reserve nothing
  template<typename U>
  [[nodiscard]] acquired<U> reserve_nothing() const {
    return {};
  }

  template<typename U>
  //requires std::is_convertible_v<U, T>
  [[nodiscard]] std::pair<U, std::atomic<T> *> protect_snapshot(const std::atomic<U> *p) {
    auto *slot = get_free_slot();
    U result;
    do {
      result = p->load(std::memory_order_seq_cst);
      if (result == nullptr) {
        slot->store(nullptr, std::memory_order_release);
        return std::make_pair(result, nullptr); // TODO: make sure we handled marked nullptrs correctly
      }
      slot->store(static_cast<T>(result), std::memory_order_seq_cst);
    } while (p->load(std::memory_order_seq_cst) != result);
    return std::make_pair(result, slot);
  }

  [[nodiscard]] std::atomic<T> *get_free_slot() {
    assert(snapshot_slots != 0);
    auto id = utils::threadID.getTID();
    for (size_t i = 0; i < snapshot_slots; i++) {
      if (announcement_slots[id].snapshot_announcements[i].load(std::memory_order_relaxed) == nullptr) {
        return std::addressof(announcement_slots[id].snapshot_announcements[i]);
      }
    }
    auto &last_free = announcement_slots[id].last_free;
    auto kick_ptr = announcement_slots[id].snapshot_announcements[last_free].load(std::memory_order_relaxed);
    assert(kick_ptr != nullptr);
    Incrementer{}(kick_ptr);
    std::atomic<T> *return_slot = std::addressof(announcement_slots[id].snapshot_announcements[last_free]);
    last_free = (last_free + 1 == snapshot_slots) ? 0 : last_free + 1;
    return return_slot;
  }

  void release() {
    auto id = utils::threadID.getTID();
    auto &slot = announcement_slots[id].announcement;
    slot.store(nullptr, std::memory_order_release);
  }

  void retire(T p) {
    auto id = utils::threadID.getTID();
    deferred_destructs[id].push_back(p);
    work_toward_deferred_decrements(1);
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
      for (const auto &free_slot : announcement_slot.snapshot_announcements) {
        auto y = free_slot.load(std::memory_order_seq_cst);
        if (y != nullptr) f(y);
      }
    }
  }

  void work_toward_deferred_decrements(size_t work = 1) {
    auto id = utils::threadID.getTID();
    amortized_work[id] = amortized_work[id] + work;
    auto threshold = std::max<size_t>(30, delay * amortized_work.size());  // Always attempt at least 30 ejects
    while (!in_progress[id] && amortized_work[id] >= threshold) {
      amortized_work[id] = 0;
      if (deferred_destructs[id].size() == 0) break; // nothing to collect
      in_progress[id] = true;
      auto deferred = AlignedVector<T>(std::move(deferred_destructs[id]));

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

  std::vector<LocalSlot> announcement_slots;          // Announcement array slots
  std::vector<AlignedBool> in_progress;               // Local flags to prevent reentrancy while destructing
  std::vector<AlignedVector<T>> deferred_destructs;   // Thread-local lists of pending deferred destructs
  std::vector<AlignedInt> amortized_work;             // Amortized work to pay for ejecting deferred destructs
};

}  // namespace internal

}  // namespace cdrc

#endif  // PARLAY_ATOMIC_ACQUIRE_RETIRE_H
