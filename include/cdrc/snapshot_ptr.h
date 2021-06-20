
#ifndef PARLAY_ATOMIC_SNAPSHOT_PTR_H_
#define PARLAY_ATOMIC_SNAPSHOT_PTR_H_

#include "internal/fwd_decl.h"

namespace cdrc {

template<typename T, template<typename> typename pointer_type, typename pointer_policy>
class snapshot_ptr : public pointer_policy::template snapshot_ptr_policy<T> {

  using counted_ptr_t = pointer_type<internal::counted_object<T>>;
  using raw_counted_ptr_t = internal::counted_object<T> *;

  friend typename pointer_policy::template snapshot_ptr_policy<T>;

 public:
  snapshot_ptr() : ptr(nullptr), slot(nullptr) {}

  snapshot_ptr(std::nullptr_t) : ptr(nullptr), slot(nullptr) {}

  snapshot_ptr(snapshot_ptr &&other) noexcept: ptr(other.ptr), slot(other.slot) {
    other.ptr = nullptr;
    other.slot = nullptr;
  }

  snapshot_ptr(const snapshot_ptr&) = delete;
  snapshot_ptr &operator=(const snapshot_ptr&) = delete;

  snapshot_ptr &operator=(snapshot_ptr &&other) {
    clear();
    swap(other);
    return *this;
  }

  typename std::add_lvalue_reference_t<T> operator*() { return *(ptr->get()); }

  const typename std::add_lvalue_reference_t<T> operator*() const { return *(ptr->get()); }

  T *get() { return (ptr == nullptr) ? nullptr : ptr->get(); }

  const T *get() const { return (ptr == nullptr) ? nullptr : ptr->get(); }

  T *operator->() { return (ptr == nullptr) ? nullptr : ptr->get(); }

  const T *operator->() const { return (ptr == nullptr) ? nullptr : ptr->get(); }

  explicit operator bool() const { return ptr != nullptr; }

  bool operator==(const snapshot_ptr<T> &other) const { return get() == other.get(); }

  bool operator!=(const snapshot_ptr<T> &other) const { return get() != other.get(); }

  void swap(snapshot_ptr &other) {
    std::swap(ptr, other.ptr);
    std::swap(slot, other.slot);
  }

  ~snapshot_ptr() { clear(); }

  void clear() {
    if (ptr != nullptr) {
      assert(slot != nullptr);
      if (is_protected()) {
        slot->store(nullptr, std::memory_order_release);
      } else {
        decrement_counter(ptr);
      }
      ptr = nullptr;
    }
    slot = nullptr;
  }

 protected:
  friend class atomic_rc_ptr<T, pointer_type, pointer_policy>;

  friend class rc_ptr<T, pointer_type, pointer_policy>;

  friend typename pointer_policy::template arc_ptr_policy<T>;

  snapshot_ptr(counted_ptr_t ptr_, std::atomic<raw_counted_ptr_t> *slot_) :
      ptr(ptr_), slot(slot_) {}

  counted_ptr_t get_counted() const {
    return ptr;
  }

  counted_ptr_t release() {
    auto old_ptr = ptr;
    if (slot != nullptr && is_protected()) {
      increment_counter(old_ptr);
      slot->store(nullptr, std::memory_order_release);
    }
    ptr = nullptr;
    slot = nullptr;
    return old_ptr;
  }

  // Returns true if the snapshot still holds an announcement slot. If false, the
  // slot has been cleared and the snapshot is protected by a reference count
  bool is_protected() const {
    return slot != nullptr && slot->load(std::memory_order_seq_cst) == static_cast<raw_counted_ptr_t>(ptr);
  }

  static void increment_counter(counted_ptr_t ptr) {
    assert(ptr != nullptr);
    ptr->add_refs(1);
  }

  static void decrement_counter(counted_ptr_t ptr) {
    assert(ptr != nullptr);
    if (ptr->release_refs(1) == 1) {
      delete ptr;
    }
  }

  counted_ptr_t ptr;
  std::atomic<raw_counted_ptr_t> *slot;
};

}  // namespace cdrc

#endif  // PARLAY_ATOMIC_SNAPSHOT_PTR_H_
