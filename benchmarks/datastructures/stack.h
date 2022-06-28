
#ifndef CDRC_BENCHMARKS_DATASTRUCTURES_STACK_H
#define CDRC_BENCHMARKS_DATASTRUCTURES_STACK_H

#include <utility>

#include <cdrc/atomic_rc_ptr.h>
#include <cdrc/rc_ptr.h>

#include "../benchmarks/common.hpp"

template<typename T>
concept SharedPointer = requires(T a) {
  T(a);                                                         // A shared pointer is copyable
  T(std::declval<T&&>());                                       // A shared pointer is movable
  *a;                                                           // A shared pointer can be dereferenced
  a.get();                                                      // We can access the raw pointer
  a.operator->();                                               // We can access members of the underlying object
  requires std::is_pointer_v<decltype(a.get())>;                // The raw pointer is in fact a pointer
  requires std::is_pointer_v<decltype(a.operator->())>;
};

// An atomic shared pointer must at minimum support load, store, and compare exchange weak
template<typename T>
concept AtomicSharedPointer = requires(T a) {
  a.load();                                                         // We can load from an atomic shared pointer
  a.store(std::declval<decltype(a.load())>());                      // We can store to an atomic shared pointer
  a.compare_exchange_weak(std::declval<decltype(a.load())&>(),      // We can CAS on an atomic shared pointer
                          std::declval<decltype(a.load())>());
   SharedPointer<decltype(a.load())>;                               // The loaded object is a shared pointer
};

// Detect atomic storage types that support a get_snapshot
// method, so we can use it preferentially instead of load
// A snapshot should be dereferenceable to the same type
// as the shared pointer
template<typename T>
concept Snapshotable = requires(T a) {
    requires AtomicSharedPointer<T>;
    a.get_snapshot();
  std::is_same_v<decltype(*(a.get_snapshot())), decltype(*(a.load()))>;
};

// Detect whether the given atomic storage supports swapping
// with a given value. This helps to implement faster push
template<typename T>
concept Swappable = requires(T a) {
    requires AtomicSharedPointer<T>;
  a.swap(std::declval<T>().load());
};

// A concurrent stack that can use any given atomic shared pointer type
//
// The atomic shared pointer type must support the following operations:
//  - load(): Load the currently stored shared pointer
//  - compare_exchange_weak(expected, desired): CAS desired into the stored location if
//      it currently contains expected, otherwise, load the current value into expected.
//      May spuriously fail.
//
// Optionally, the atomic shared pointer type may also support the following, which
// may be used to improve the efficiency of some of the operations:
//  - get_snapshot(): Return a snapshot to the object managed by the currently stored
//      shared pointer. A snapshot object should guarantee that the underlying object
//      is safe from destruction and reclamation as long as it is alive
//  - swap(other): Swap the contents of the atomic shared pointer with the given
//      shared pointer. The operation must be atomic from the perspective of the
//      atomic shared pointer, but need not be with respect to the non-atomic
//      other.
//
template<typename T, template<typename> typename AtomicSPType, template<typename> typename SPType>
class alignas(64) atomic_stack {

  struct Node;
  using atomic_sp_t = AtomicSPType<Node>;
  using load_t = std::remove_cvref_t<decltype(std::declval<atomic_sp_t>().load())>;
  using sp_t = SPType<Node>;

  // Should use the explicit partial specialization for OrcGC since it
  // has different signatures and does not work with this implementation
  static_assert(!std::is_same_v<AtomicSPType<Node>, OrcAtomicRcPtr<Node>>);

  // Load should return the shared_ptr-like type
  static_assert(std::is_same_v<load_t, sp_t>);

  struct Node {
    T t;
    sp_t next;
    Node() = default;
    Node(T t_) : t(std::move(t_)) { }
  };

  atomic_sp_t head;

  atomic_stack(atomic_stack&) = delete;
  void operator=(atomic_stack) = delete;

  // Return a snapshot if the atomic_ptr_type supports it,
  // otherwise perform a regular atomic load
  auto snapshot_or_load() {
    if constexpr (Snapshotable<atomic_sp_t>) { return head.get_snapshot(); }
    else return head.load();
  }

 public:
  atomic_stack() = default;

  // Avoid exploding the call stack when destructing
  ~atomic_stack() {
    // Note: We need to go through and unlink the nodes in the stack
    // because of deferred reclamation. The "standard" algorithm of
    // just popping each node off might not prevent a stack overflow
    // because the destruction might be deferred, and hence there
    // might still be a long chain that gets recursively destroyed,
    // which will blow the call stack if the stack is large.
    auto node = head.load();
    if (node) {
      head.store(nullptr);
      auto next = node->next;
      node->next = nullptr;
      while (next) {
        node = next;
        next = node->next;
        node->next = nullptr;
      }
    }
  }

  bool find(T t) {
    // Holding a snapshot protects the entire list from
    // destruction while we are reading it. The alternative
    // is to copy rc_ptrs all the way down, which allows
    // the list to be destroyed while the find is in progress
    // but will slow down find by requiring it to perform
    // lots of reference count increments and decrements.

    auto ss = snapshot_or_load();
    auto node = ss.get();
    while (node && node->t != t)
      node = node->next.get();
    return node != nullptr;
  }

  void push_front(T t) {
    auto new_node = make_shared<Node, SPType>();
    new_node->t = t;
    new_node->next = head.load();
    while (!head.compare_exchange_weak(new_node->next, new_node)) {}
  }

  std::optional<T> front() {
    auto ss = snapshot_or_load();
    if (ss) return {ss->t};
    else return {};
  }

  std::optional<T> pop_front() {
    auto ss = snapshot_or_load();
    while (ss && !head.compare_exchange_weak(ss, ss->next)) {}
    if (ss) return {ss->t};
    else return {};
  }

  size_t size() {
    size_t result = 0;
    auto ss = snapshot_or_load();
    auto node = ss.get();
    while (node) {
      result++;
      node = node->next.get();
    }
    return result;
  }

  static constexpr bool tracks_allocations = AllocationTrackable<atomic_sp_t>;

  static std::ptrdiff_t currently_allocated() requires AllocationTrackable<atomic_sp_t> {
    return atomic_sp_t::currently_allocated();
  }
};


// Stack specialization for OrcGC, since it unfortunately does not adhere to the C++
// standard signatures for atomic shared pointers, so everything has to be written
// slightly differently
template<typename T>
class alignas(64) atomic_stack<T, OrcAtomicRcPtr, OrcRcPtr> {

  struct Node;
  using atomic_sp_t = OrcAtomicRcPtr<Node>;
  using sp_t = OrcRcPtr<Node>;

  struct Node : orcgc_ptp::orc_base {
    T t;
    atomic_sp_t next;
    Node() = default;
    Node(T t_) : t(std::move(t_)) { }
  };

  atomic_sp_t head;

  atomic_stack(atomic_stack&) = delete;
  void operator=(atomic_stack) = delete;

 public:
  atomic_stack() = default;

  // Avoid exploding the call stack when destructing
  ~atomic_stack() {
    // Note: We need to go through and unlink the nodes in the stack
    // because of deferred reclamation. The "standard" algorithm of
    // just popping each node off might not prevent a stack overflow
    // because the destruction might be deferred, and hence there
    // might still be a long chain that gets recursively destroyed,
    // which will blow the call stack if the stack is large.
    sp_t node = head.load();
    if (node) {
      head.store(nullptr);
      sp_t next = node->next;
      node->next = nullptr;
      while (next) {
        node = next;
        next = node->next;
        node->next = nullptr;
      }
    }
  }

  bool find(T t) {
    sp_t node = head.load();
    while (node && node->t != t)
      node = node->next.load();
    return node != nullptr;
  }

  void push_front(T t) {
    sp_t new_node = make_shared<Node, OrcRcPtr>();
    sp_t next_node = head.load();
    new_node->t = t;
    new_node->next = next_node;

    while (!head.compare_exchange_weak(next_node, new_node)) {
      next_node = head.load();
      new_node->next = next_node;
    }

  }

  std::optional<T> front() {
    OrcRcPtr<Node> ss = head.load();
    if (ss) return {ss->t};
    else return {};
  }

  std::optional<T> pop_front() {
    sp_t ss = head.load();
    sp_t next;
    if (ss) next = ss->next.load();
    while (ss && !head.compare_exchange_weak(ss, next)) {
      ss = head.load();
      if (ss) next = ss->next.load();
    }
    if (ss) return {ss->t};
    else return {};
  }

  size_t size() {
    size_t result = 0;
    sp_t node = head.load();
    while (node) {
      result++;
      node = node->next;
    }
    return result;
  }

  static constexpr bool tracks_allocations = true;

  static std::ptrdiff_t currently_allocated() { return atomic_sp_t::currently_allocated(); }

};


#endif  // CDRC_BENCHMARKS_DATASTRUCTURES_STACK_H
