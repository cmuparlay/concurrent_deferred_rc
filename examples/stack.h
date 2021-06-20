// A simple example of a concurrent stack implemented using our library

#ifndef CONCURRENT_DEFERRED_RC_STACK_H
#define CONCURRENT_DEFERRED_RC_STACK_H

#include <optional>

#include <cdrc/atomic_rc_ptr.h>
#include <cdrc/rc_ptr.h>

namespace cdrc {

template<typename T>
class alignas(64) atomic_stack {

  struct Node;
  using atomic_sp_t = atomic_rc_ptr<Node>;
  using sp_t = rc_ptr<Node>;

  struct Node {
    T t;
    sp_t next;
    Node() = default;
    Node(T t_, sp_t _next) : t(std::move(t_)), next(std::move(_next)) { }
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

    auto ss = head.get_snapshot();
    auto node = ss.get();
    while (node && node->t != t)
      node = node->next.get();
    return node != nullptr;
  }

  void push_front(T t) {
    auto new_node = make_rc<Node>(t, head.load());
    while (!head.compare_exchange_weak(new_node->next, new_node)) {}
  }

  std::optional<T> front() {
    auto ss = head.get_snapshot();
    if (ss) return {ss->t};
    else return {};
  }

  std::optional<T> pop_front() {
    auto ss = head.get_snapshot();
    while (ss && !head.compare_exchange_weak(ss, ss->next)) {}
    if (ss) return {ss->t};
    else return {};
  }
};

}

#endif //CONCURRENT_DEFERRED_RC_STACK_H
