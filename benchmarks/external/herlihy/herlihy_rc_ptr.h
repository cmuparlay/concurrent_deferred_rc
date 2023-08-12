
#ifndef HERLIHY_RC_PTR_H_
#define HERLIHY_RC_PTR_H_

#include <atomic>

#include <cdrc/internal/acquire_retire.h>
#include <cdrc/internal/utils.hpp>

namespace internal {

// An instance of an object of type T prepended with an atomic reference count.
// Use of this type for allocating shared objects ensures that the ref count can
// be found from a pointer to the object, and that the two are both corectly aligned.
template<typename T>
struct herlihy_counted_object {
  typename std::aligned_storage<sizeof(T), alignof(T)>::type object;
  utils::StickyCounter<uint64_t> ref_cnt;

  template<typename... Args>
  herlihy_counted_object(Args&&... args) : ref_cnt(1) {
    new (std::addressof(object)) T(std::forward<Args>(args)...);
  }
  ~herlihy_counted_object() = default;

  herlihy_counted_object(const herlihy_counted_object&) = delete;
  herlihy_counted_object(herlihy_counted_object&&) = delete;

  void destroy_object() { get()->~T(); }

  T* get() { return std::launder(reinterpret_cast<T*>(std::addressof(object))); }
  const T* get() const { return std::launder(reinterpret_cast<T*>(std::addressof(object))); }

  bool add_refs(uint64_t count) { return ref_cnt.increment(count); }
  bool release_refs(uint64_t count) { return ref_cnt.decrement(count); }
};

}  // namespace internal

template<typename T>
class herlihy_arc_ptr;

template<typename T>
class herlihy_arc_ptr_opt;

template<typename T, bool opt>
class herlihy_rc_ptr {

  using arc_ptr_t = std::conditional_t<opt, herlihy_arc_ptr_opt<T>, herlihy_arc_ptr<T>>;

 public:
  herlihy_rc_ptr() noexcept : ptr(nullptr) { }
  herlihy_rc_ptr(std::nullptr_t) noexcept : ptr(nullptr) { }
  herlihy_rc_ptr(const herlihy_rc_ptr& other) noexcept : ptr(other.ptr) { if (ptr) increment_counter(ptr); }
  herlihy_rc_ptr(herlihy_rc_ptr&& other) noexcept : ptr(other.release()) { }
  ~herlihy_rc_ptr() { if (ptr) decrement_counter(ptr); }

  // copy assignment
  herlihy_rc_ptr& operator=(const herlihy_rc_ptr& other) {
    auto tmp = ptr;
    ptr = other.ptr;
    if (ptr) increment_counter(ptr);
    if (tmp) decrement_counter(tmp);
    return *this;
  }

  // move assignment
  herlihy_rc_ptr& operator=(herlihy_rc_ptr&& other) {
    auto tmp = ptr;
    ptr = other.release();
    if (tmp) decrement_counter(tmp);
    return *this;
  }

  typename std::add_lvalue_reference_t<T> operator*() const { return *(ptr->get()); }

  T* get() { return (ptr == nullptr) ? nullptr : ptr->get(); }
  const T* get() const { return (ptr == nullptr) ? nullptr : ptr->get(); }

  T* operator->() { return (ptr == nullptr) ? nullptr : ptr->get(); }
  const T* operator->() const { return (ptr == nullptr) ? nullptr : ptr->get(); }

  explicit operator bool() const { return ptr != nullptr; }

  bool operator==(const herlihy_rc_ptr& other) const { return get() == other.get(); }
  bool operator!=(const herlihy_rc_ptr& other) const { return get() != other.get(); }

  size_t use_count() const noexcept { return (ptr == nullptr) ? 0 : ptr->ref_cnt.load(); }

  // Create a new rc_ptr containing an object of type T constructed from (args...).
  template<typename... Args>
  static herlihy_rc_ptr<T, opt> make_shared(Args&&... args) {
    internal::herlihy_counted_object<T>* ptr = new internal::herlihy_counted_object<T>(std::forward<Args>(args)...);
    arc_ptr_t::increment_allocations();
    return herlihy_rc_ptr<T, opt>(ptr, AddRef::no);
  }

 private:
  friend class herlihy_arc_ptr<T>;
  friend class herlihy_arc_ptr_opt<T>;

  enum class AddRef { yes, no };

  explicit herlihy_rc_ptr(internal::herlihy_counted_object<T>* ptr_, AddRef add_ref) : ptr(ptr_) {
    if (ptr && add_ref == AddRef::yes) increment_counter(ptr);
  }

  internal::herlihy_counted_object<T>* release() {
    auto p = ptr;
    ptr = nullptr;
    return p;
  }

  internal::herlihy_counted_object<T>* get_counted() const {
    return ptr;
  }

  static void increment_counter(internal::herlihy_counted_object<T>* ptr) {
    assert(ptr != nullptr);
    ptr->add_refs(1);
  }

  static void decrement_counter(internal::herlihy_counted_object<T>* ptr) {
    assert(ptr != nullptr);
    if (ptr->release_refs(1)) {
      ptr->destroy_object();
      arc_ptr_t::ar.retire(ptr);
    }
  }

  internal::herlihy_counted_object<T>* ptr;
};

#endif  // PARLAY_RC_PTR_H_
