
#ifndef PARLAY_ATOMIC_ATOMIC_RC_PTR_H
#define PARLAY_ATOMIC_ATOMIC_RC_PTR_H

#include <atomic>

#include "internal/fwd_decl.h"
#include "internal/acquire_retire.h"

#include "rc_ptr.h"
#include "snapshot_ptr.h"

namespace cdrc {

template<typename T, template<typename> typename pointer_type, typename pointer_policy>
class atomic_rc_ptr : public pointer_policy::template arc_ptr_policy<T> {

  // The pointer_type template argument introduces a customization point
  // that allows atomic_rc_ptr to be used with various kinds of marked
  // pointers. pointer_type<T> should be implicitly convertible to T*
  // and support all of the usual pointer-like operations.
  using counted_ptr_t = pointer_type<internal::counted_object<T>>;
  using raw_counted_ptr_t = internal::counted_object<T> *;

  using rc_ptr_t = rc_ptr<T, pointer_type, pointer_policy>;
  using snapshot_ptr_t = snapshot_ptr<T, pointer_type, pointer_policy>;

  friend typename pointer_policy::template arc_ptr_policy<T>;

 public:
  atomic_rc_ptr() : atomic_ptr(nullptr) {}

  atomic_rc_ptr(std::nullptr_t) : atomic_ptr(nullptr) {}

  atomic_rc_ptr(rc_ptr_t desired) : atomic_ptr(desired.release()) {}

  ~atomic_rc_ptr() {
    auto ptr = atomic_ptr.load();
    // if (ptr != nullptr) decrement_counter(ptr);
    if (ptr != nullptr) ar.retire(ptr);
  }

  atomic_rc_ptr(const atomic_rc_ptr &) = delete;

  atomic_rc_ptr &operator=(const atomic_rc_ptr &) = delete;

  atomic_rc_ptr(atomic_rc_ptr &&) = delete;

  atomic_rc_ptr &operator=(atomic_rc_ptr &&) = delete;

  bool is_lock_free() const noexcept { return true; }

  static constexpr bool is_always_lock_free = true;

  void store(std::nullptr_t) noexcept {
    auto old_ptr = atomic_ptr.exchange(nullptr, std::memory_order_seq_cst);
    if (old_ptr != nullptr) ar.retire(old_ptr);
  }

  void store(rc_ptr_t desired) noexcept {
    auto new_ptr = desired.release();
    auto old_ptr = atomic_ptr.exchange(new_ptr, std::memory_order_seq_cst);
    if (old_ptr != nullptr) ar.retire(old_ptr);
  }

  void store_non_racy(rc_ptr_t desired) noexcept {
    auto new_ptr = desired.release();
    auto old_ptr = atomic_ptr.load();
    atomic_ptr.store(new_ptr, std::memory_order_release);
    if (old_ptr != nullptr) ar.retire(old_ptr);
  }

  void store(const snapshot_ptr_t &desired) noexcept {
    auto new_ptr = desired.get_counted();
    if (new_ptr != nullptr) increment_counter(new_ptr);
    auto old_ptr = atomic_ptr.exchange(new_ptr, std::memory_order_seq_cst);
    if (old_ptr != nullptr) ar.retire(old_ptr);
  }

  rc_ptr_t load() const noexcept {
    auto acquired_ptr = ar.acquire(&atomic_ptr);
    rc_ptr_t result(acquired_ptr.value, rc_ptr_t::AddRef::yes);
    return result;
  }

  snapshot_ptr_t get_snapshot() const noexcept {
    auto[value, slot] = ar.protect_snapshot(&atomic_ptr);
    return snapshot_ptr_t(value, slot);
  }

  bool compare_exchange_weak(rc_ptr_t &expected, const rc_ptr_t &desired) noexcept {
    if (!compare_and_swap(expected, desired)) {
      expected = load();
      return false;
    } else
      return true;
  }

  bool compare_exchange_weak(snapshot_ptr_t &expected, const rc_ptr_t &desired) noexcept {
    if (!compare_and_swap(expected, desired)) {
      expected = get_snapshot();
      return false;
    } else
      return true;
  }

  // Atomically compares the underlying rc_ptr with expected, and if they refer to
  // the same managed object, replaces the current rc_ptr with a copy of desired
  // (incrementing its reference count) and returns true. Otherwise returns false.
  bool compare_and_swap(const auto &expected, const auto &desired) noexcept {

    // We need to make a reservation if the desired snapshot pointer no longer has
    // an announcement slot. Otherwise, desired is protected, assuming that another
    // thread can not clear the announcement slot (this might change one day!)
    auto reservation = !desired.is_protected() ?
                       ar.reserve(desired.get_counted()) : ar.template reserve_nothing<counted_ptr_t>();

    if (compare_and_swap_impl(expected.get_counted(), desired.get_counted())) {
      auto desired_ptr = desired.get_counted();
      if (desired_ptr != nullptr) increment_counter(desired_ptr);
      return true;
    } else {
      return false;
    }
  }

  // Atomically compares the underlying rc_ptr with expected, and if they are equal,
  // replaces the current rc_ptr with desired by move assignment, hence leaving its
  // reference count unchanged. Otherwise returns false and leaves desired unmodified.
  bool compare_and_swap(const auto &expected,
                        auto &&desired) noexcept requires std::is_rvalue_reference_v<decltype(desired)> {
    if (compare_and_swap_impl(expected.get_counted(), desired.get_counted())) {
      desired.release();
      return true;
    } else {
      return false;
    }
  }

  // Swaps the currently stored shared pointer with the given
  // shared pointer. This operation does not affect the reference
  // counts of either shared pointer.
  //
  // Note that it is not safe to concurrently access desired
  // while this operation is taking place, since desired is a
  // non-atomic shared pointer!
  void swap(rc_ptr_t &desired) noexcept {
    auto desired_ptr = desired.release();
    auto current_ptr = atomic_ptr.load();
    desired = rc_ptr_t(current_ptr, rc_ptr_t::AddRef::no);
    while (!atomic_ptr.compare_exchange_weak(desired.ptr, desired_ptr)) {
    }
  }

  rc_ptr_t exchange(rc_ptr_t desired) noexcept {
    auto new_ptr = desired.release();
    auto old_ptr = atomic_ptr.exchange(new_ptr, std::memory_order_seq_cst);
    return rc_ptr_t(old_ptr, rc_ptr_t::AddRef::no);
  }

  void operator=(rc_ptr_t desired) noexcept { store(std::move(desired)); }

  operator rc_ptr_t() const noexcept { return load(); }

  static size_t currently_allocated() {
    size_t total = 0;
    for (size_t t = 0; t < utils::num_threads(); t++) {
      total += internal_data.num_allocated[t].load(std::memory_order_acquire);
    }
    return total;
  }

 protected:
  friend class snapshot_ptr<T, pointer_type, pointer_policy>;

  friend class rc_ptr<T, pointer_type, pointer_policy>;

  std::atomic<counted_ptr_t> atomic_ptr;

  bool compare_and_swap_impl(counted_ptr_t expected_ptr, counted_ptr_t desired_ptr) noexcept {
    if (atomic_ptr.compare_exchange_strong(expected_ptr, desired_ptr, std::memory_order_seq_cst)) {
      if (expected_ptr != nullptr) {
        ar.retire(expected_ptr);
      }
      return true;
    } else {
      return false;
    }
  }

  static void increment_counter(counted_ptr_t ptr) {
    assert(ptr != nullptr);
    ptr->add_refs(1);
  }

  static void decrement_counter(counted_ptr_t ptr) {
    assert(ptr != nullptr);
    if (ptr->release_refs(1) == 1) {
      delete ptr;
      decrement_allocations();
    }
  }

  struct counted_deleter {
    void operator()(counted_ptr_t ptr) const {
      assert(ptr != nullptr);
      decrement_counter(ptr);
    }
  };

  struct counted_incrementer {
    void operator()(counted_ptr_t ptr) const {
      assert(ptr != nullptr);
      increment_counter(ptr);
    }
  };

  // Static data members. We initialize them together in a class to force the order of construction!
  // Explanation: This is important since the destructor of acquire_retire may write to num_allocated
  // (decrementing by one every time something is destructed). This means that acquire retire must be
  // destroyed first, or it will try to write to a destructed vector. Constructing it second ensures
  // that it will be destructed first.
  struct internals {
    std::vector<utils::Padded<std::atomic<std::ptrdiff_t>>> num_allocated;
    internal::acquire_retire<raw_counted_ptr_t, counted_deleter, counted_incrementer> ar;

    internals() : num_allocated(utils::num_threads()), ar(utils::num_threads()) {}
  };

  static void decrement_allocations() {
    int tid = utils::threadID.getTID();
    num_allocated[tid].store(num_allocated[tid].load() - 1);
  }

  static void increment_allocations() {
    int tid = utils::threadID.getTID();
    num_allocated[tid].store(num_allocated[tid].load() + 1);
  }

  static inline internals internal_data;
  static inline auto &num_allocated = internal_data.num_allocated;
  static inline auto &ar = internal_data.ar;
};

}  // namespace cdrc

#endif  // PARLAY_ATOMIC_RC_PTR_H
