
#ifndef HERLIHY_ARC_PTR_OPT_H
#define HERLIHY_ARC_PTR_OPT_H

#include "herlihy_acquire_retire.h"
#include "herlihy_rc_ptr.h"

// optmized Herlihy's to use fetch_and_add when decrementing.
// Also optimized it to use exchange instead of CAS loop for store
template<typename T>
class herlihy_arc_ptr_opt {

  using rc_ptr_t = herlihy_rc_ptr<T, true>;

public:
  herlihy_arc_ptr_opt() : atomic_ptr(nullptr) { }
  explicit herlihy_arc_ptr_opt(rc_ptr_t desired) : atomic_ptr(desired.release()) { }
  ~herlihy_arc_ptr_opt() {
    auto ptr = atomic_ptr.load();
    destroy(ptr);
  }

  void store(rc_ptr_t desired) noexcept {
    auto new_ptr = desired.release();
    auto old_ptr = atomic_ptr.exchange(new_ptr, std::memory_order_seq_cst);
    if (old_ptr != nullptr) destroy(old_ptr);
  }

  rc_ptr_t load() const noexcept {
    bool incremented = false;
    internal::herlihy_counted_object<T>* ptr = nullptr;
    while(!incremented) {
      
      auto acquired_ptr = ar.acquire(&atomic_ptr);
      ptr = acquired_ptr.value;
      if(ptr == nullptr) break;
      uint64_t r = ptr->ref_cnt;
      while(r > 0) {
        if(ptr->ref_cnt.compare_exchange_strong(r, r+1)) {
          incremented = true;
          break;
        }
      }
    }
    return rc_ptr_t(ptr, rc_ptr_t::AddRef::no);
  }

  bool compare_exchange_strong(rc_ptr_t& expected, rc_ptr_t desired) noexcept {
    auto desired_ptr = desired.get_counted();
    auto expected_ptr = expected.get_counted();

    if (atomic_ptr.compare_exchange_strong(expected_ptr, desired_ptr, std::memory_order_seq_cst)) {
      if (expected_ptr != nullptr) {
        destroy(expected_ptr);
      }
      desired.release();
      return true;
    }
    else {
      expected = load();
      return false;
    }
  }

  bool compare_exchange_weak(rc_ptr_t& expected, rc_ptr_t desired) noexcept {
    return compare_exchange_strong(expected, std::move(desired));
  }

  static size_t currently_allocated() {
    size_t total = 0;
    for (size_t t = 0; t < utils::num_threads(); t++) {
      total += internal_data.num_allocated[t].load(std::memory_order_acquire);
    }
    return total;
  }

private:
  friend class herlihy_rc_ptr<T, true>;
  std::atomic<internal::herlihy_counted_object<T>*> atomic_ptr;
  
  struct counted_deleter {
    void operator()(internal::herlihy_counted_object<T>* ptr) const {
      assert(ptr != nullptr);
      delete ptr;
      decrement_allocations();
    }
  };

  static void destroy(internal::herlihy_counted_object<T>* ptr) {
    if (ptr != nullptr && ptr->release_refs(1) == 1) {
      ptr->destroy_object();
      ar.retire(ptr);
    }
  }

  // Static data members. We initialize them together in a class to force the order of construction!
  // Explanation: This is important since the destructor of acquire_retire may write to num_allocated
  // (decrementing by one every time something is destructed). This means that acquire retire must be
  // destroyed first, or it will try to write to a destructed vector. Constructing it second ensures
  // that it will be destructed first.
  struct internals {
    std::vector<utils::Padded<std::atomic<std::ptrdiff_t>>> num_allocated;
    herlihy::acquire_retire<internal::herlihy_counted_object<T>*, counted_deleter> ar;
    internals() : num_allocated(utils::num_threads()), ar(utils::num_threads()) { }
  };

  static void decrement_allocations() {
    auto id = utils::threadID.getTID();
    auto cur = internal_data.num_allocated[id].load(std::memory_order_relaxed);
    internal_data.num_allocated[id].store(cur - 1, std::memory_order_release);
  }

  static void increment_allocations() {
    auto id = utils::threadID.getTID();
    auto cur = internal_data.num_allocated[id].load(std::memory_order_relaxed);
    internal_data.num_allocated[id].store(cur + 1, std::memory_order_release);
  }

  static inline internals internal_data;
  static inline auto& ar = internal_data.ar;
};

#endif