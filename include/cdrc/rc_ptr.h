
#ifndef PARLAY_RC_PTR_H_
#define PARLAY_RC_PTR_H_

#include <atomic>
#include <type_traits>

#include "internal/fwd_decl.h"
#include "internal/utils.hpp"

namespace cdrc {

namespace internal {

// An instance of an object of type T with an atomic reference count.
template<typename T>
struct counted_object {
  T object;
  std::atomic<uint64_t> ref_cnt;

  template<typename... Args>
  counted_object(Args &&... args) : object(std::forward<Args>(args)...), ref_cnt(1) {}

  ~counted_object() = default;

  counted_object(const counted_object &) = delete;

  counted_object(counted_object &&) = delete;

  T *get() { return std::addressof(object); }

  const T *get() const { return std::addressof(object); }

  uint64_t add_refs(uint64_t count) { return ref_cnt.fetch_add(count); }

  uint64_t release_refs(uint64_t count) { return ref_cnt.fetch_sub(count); }
};

}  // namespace internal

template<typename T, template<typename> typename pointer_type, typename pointer_policy>
class rc_ptr : public pointer_policy::template rc_ptr_policy<T> {

  using atomic_ptr_t = atomic_rc_ptr<T, pointer_type, pointer_policy>;
  using counted_ptr_t = pointer_type<internal::counted_object<T>>;
  using snapshot_ptr_t = snapshot_ptr<T, pointer_type, pointer_policy>;

  friend typename pointer_policy::template rc_ptr_policy<T>;

 public:
  rc_ptr() noexcept: ptr(nullptr) {}

  rc_ptr(std::nullptr_t) noexcept: ptr(nullptr) {}

  rc_ptr(const snapshot_ptr_t &other) noexcept: ptr(other.get_counted()) {
    if (ptr) increment_counter(ptr);
  }

  rc_ptr(const rc_ptr &other) noexcept: ptr(other.ptr) { if (ptr) increment_counter(ptr); }

  rc_ptr(rc_ptr &&other) noexcept: ptr(other.release()) {}

  ~rc_ptr() { clear(); }

  void clear() {
    if (ptr) decrement_counter(ptr);
    ptr = nullptr;
  }

  // copy assignment
  rc_ptr &operator=(const rc_ptr &other) {
    auto tmp = ptr;
    ptr = other.ptr;
    if (ptr) increment_counter(ptr);
    if (tmp) decrement_counter(tmp);
    return *this;
  }

  // move assignment
  rc_ptr &operator=(rc_ptr &&other) {
    auto tmp = ptr;
    ptr = other.release();
    if (tmp) decrement_counter(tmp);
    return *this;
  }

  typename std::add_lvalue_reference_t<T> operator*() { return *(ptr->get()); }

  const typename std::add_lvalue_reference_t<T> operator*() const { return *(ptr->get()); }

  T *get() { return (ptr == nullptr) ? nullptr : ptr->get(); }

  const T *get() const { return (ptr == nullptr) ? nullptr : ptr->get(); }

  T *operator->() { return (ptr == nullptr) ? nullptr : ptr->get(); }

  const T *operator->() const { return (ptr == nullptr) ? nullptr : ptr->get(); }

  explicit operator bool() const { return ptr != nullptr; }

  bool operator==(const rc_ptr &other) const { return get() == other.get(); }

  bool operator!=(const rc_ptr &other) const { return get() != other.get(); }

  size_t use_count() const noexcept { return (ptr == nullptr) ? 0 : ptr->ref_cnt.load(); }

  void swap(rc_ptr &other) {
    std::swap(ptr, other.ptr);
  }

  // Create a new rc_ptr containing an object of type T constructed from (args...).
  template<typename... Args>
  static rc_ptr make_shared(Args &&... args) {
    auto ptr = counted_ptr_t(new internal::counted_object<T>(std::forward<Args>(args)...));
    atomic_ptr_t::increment_allocations();
    return rc_ptr(ptr, AddRef::no);
  }

 protected:
  friend class atomic_rc_ptr<T, pointer_type, pointer_policy>;

  friend typename pointer_policy::template arc_ptr_policy<T>;

  enum class AddRef {
    yes, no
  };

  explicit rc_ptr(counted_ptr_t ptr_, AddRef add_ref) : ptr(ptr_) {
    if (ptr && add_ref == AddRef::yes) increment_counter(ptr);
  }

  bool is_protected() const {
    return false;
  }

  counted_ptr_t release() {
    auto p = ptr;
    ptr = nullptr;
    return p;
  }

  counted_ptr_t get_counted() const {
    return ptr;
  }

  static void increment_counter(counted_ptr_t ptr) {
    assert(ptr != nullptr);
    ptr->add_refs(1);
  }

  static void decrement_counter(counted_ptr_t ptr) {
    assert(ptr != nullptr);
    if (ptr->release_refs(1) == 1) {
      delete ptr;
      atomic_ptr_t::decrement_allocations();
    }
  }

  counted_ptr_t ptr;
};

// Create a new rc_ptr containing an object of type T constructed from (args...).
template<typename T, typename... Args>
static rc_ptr<T> make_shared(Args &&... args) {
  return rc_ptr<T>::make_shared(std::forward<Args>(args)...);
}

// Create a new rc_ptr containing an object of type T constructed from (args...).
template<typename T, typename... Args>
static rc_ptr<T> make_rc(Args &&... args) {
  return rc_ptr<T>::make_shared(std::forward<Args>(args)...);
}

}  // namespace cdrc

#endif  // PARLAY_RC_PTR_H_
