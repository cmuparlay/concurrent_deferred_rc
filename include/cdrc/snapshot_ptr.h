
#ifndef CDRC_SNAPSHOT_PTR_H_
#define CDRC_SNAPSHOT_PTR_H_

#include <cstddef>

#include <type_traits>

#include "internal/counted_object.h"
#include "internal/fwd_decl.h"

namespace cdrc {

template<typename T, typename memory_manager, typename pointer_policy>
class snapshot_ptr : public pointer_policy::template snapshot_ptr_policy<T> {

  using counted_object_t = internal::counted_object<T>;
  using counted_ptr_t = typename pointer_policy::template pointer_type<counted_object_t>;

  using atomic_ptr_t = atomic_rc_ptr<T, memory_manager, pointer_policy>;
  using rc_ptr_t = rc_ptr<T, memory_manager, pointer_policy>;
  using atomic_weak_ptr_t = atomic_weak_ptr<T, memory_manager, pointer_policy>;

  friend atomic_ptr_t;
  friend rc_ptr_t;
  friend atomic_weak_ptr_t;

  using acquired_pointer_t = typename memory_manager::template acquired_pointer<counted_ptr_t>;

  friend typename pointer_policy::template arc_ptr_policy<T>;
  friend typename pointer_policy::template snapshot_ptr_policy<T>;

 public:
  snapshot_ptr() : acquired_ptr() {}

  snapshot_ptr(std::nullptr_t) : acquired_ptr() {}

  snapshot_ptr(snapshot_ptr &&other) noexcept: acquired_ptr(std::move(other.acquired_ptr)) {}

  snapshot_ptr(const snapshot_ptr&) = delete;
  snapshot_ptr &operator=(const snapshot_ptr&) = delete;

  snapshot_ptr &operator=(snapshot_ptr &&other) {
    clear();
    swap(other);
    return *this;
  }

  typename std::add_lvalue_reference_t<T> operator*() { return *(acquired_ptr.get()->get()); }

  const typename std::add_lvalue_reference_t<T> operator*() const { return *(acquired_ptr.get()->get()); }

  T *get() { 
    counted_ptr_t ptr = acquired_ptr.get();
    return (ptr == nullptr) ? nullptr : ptr->get(); 
  }

  const T *get() const { 
    counted_ptr_t ptr = acquired_ptr.get();
    return (ptr == nullptr) ? nullptr : ptr->get(); 
  }

  T *operator->() { 
    counted_ptr_t ptr = acquired_ptr.get();
    return (ptr == nullptr) ? nullptr : ptr->get(); 
  }

  const T *operator->() const { 
    counted_ptr_t ptr = acquired_ptr.getValue();
    return (ptr == nullptr) ? nullptr : ptr->get(); 
  }

  explicit operator bool() const { return acquired_ptr.get() != nullptr; }

  bool operator==(const snapshot_ptr<T> &other) const { return get() == other.get(); }

  bool operator!=(const snapshot_ptr<T> &other) const { return get() != other.get(); }

  void swap(snapshot_ptr &other) {
    acquired_ptr.swap(other.acquired_ptr);
  }

  ~snapshot_ptr() { clear(); }

  void clear() {
    counted_ptr_t ptr = acquired_ptr.get();
    if (!acquired_ptr.is_protected() && ptr != nullptr) {
      mm.decrement_ref_cnt(ptr);
    }
    acquired_ptr.clear();
  }

 protected:

  snapshot_ptr(acquired_pointer_t&& acquired_ptr) :
      acquired_ptr(std::move(acquired_ptr)) {}

  counted_ptr_t get_counted() const {
    return acquired_ptr.get();
  }

  counted_ptr_t& get_counted() {
    return acquired_ptr.get();
  }

  counted_ptr_t release() {
    auto old_ptr = acquired_ptr.getValue();
    if (acquired_ptr.is_protected()) {
      mm.increment_ref_cnt(old_ptr);
      acquired_ptr.clear_protection(mm);
    }
    acquired_ptr.clear();
    return old_ptr;
  }

  [[nodiscard]] bool is_protected() const {
    return acquired_ptr.is_protected();
  }

  static inline memory_manager& mm = memory_manager::instance();

  acquired_pointer_t acquired_ptr;
};

}  // namespace cdrc

#endif  // CDRC_SNAPSHOT_PTR_H_
