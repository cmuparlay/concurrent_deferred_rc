
#ifndef CDRC_BENCHMARKS_DATASTRUCTURES_QUEUE_H
#define CDRC_BENCHMARKS_DATASTRUCTURES_QUEUE_H

#include <optional>
#include <utility>

#include <cdrc/atomic_rc_ptr.h>
#include <cdrc/rc_ptr.h>
#include <cdrc/weak_ptr.h>
#include <cdrc/atomic_weak_ptr.h>

// Just::threads
#ifdef ARC_JUST_THREADS_AVAILABLE
#include <experimental/atomic>
#endif

namespace cdrc {
namespace weak_ptr_queue {

// A doubly-linked queue based on "DoubleLink"
// http://concurrencyfreaks.blogspot.com/2017/01/doublelink-low-overhead-lock-free-queue.html
template<typename T, template<typename> typename MemoryManager = internal::default_memory_manager>
class atomic_queue {

  struct Node;
  using atomic_sp_t = atomic_rc_ptr<Node, MemoryManager<Node>>;
  using sp_t = rc_ptr<Node, MemoryManager<Node>>;
  using weak_ptr_t = weak_ptr<Node, MemoryManager<Node>>;
  using atomic_weak_ptr_t = atomic_weak_ptr<Node, MemoryManager<Node>>;

  struct Node {
    T t;
    atomic_sp_t next;
    atomic_weak_ptr_t prev;

    Node() = default;

    Node(T t_, sp_t next_, weak_ptr_t prev_) : t(std::move(t_)), next(std::move(next_)), prev(std::move(prev_)) {}
  };

  alignas(128) atomic_sp_t head;
  alignas(128) atomic_sp_t tail;

public:

  atomic_queue() {
    auto sentinel_node = sp_t::make_shared();
    tail.store(sentinel_node);
    head.store(std::move(sentinel_node));
  };

  atomic_queue(const atomic_queue&) = delete;
  atomic_queue& operator=(const atomic_queue&) = delete;

  ~atomic_queue() = default;

  void enqueue(T t) {
    auto new_node = sp_t::make_shared(t, nullptr, nullptr);
    while (true) {
      auto ltail = tail.get_snapshot();
      new_node->prev.store(ltail, std::memory_order_relaxed);

      // Help the previous enqueue in case it fell asleep before setting its next ptr
      auto lprev = ltail->prev.get_snapshot();
      if (lprev && lprev->next == nullptr) lprev->next.store(ltail, std::memory_order_relaxed);

      if (tail.compare_and_swap(ltail, new_node)) {
        ltail->next.store(std::move(new_node), std::memory_order_release);
        return;
      }
    }
  }

  std::optional<T> peek() {
    auto ss = head.get_snapshot()->next.get_snapshot();
    if (ss) return {ss->t};
    else return {};
  }

  std::optional<T> dequeue() {
    while (true) {
      auto lhead = head.get_snapshot();
      auto lnext = lhead->next.get_snapshot();
      if (!lnext) return {};  // Queue is empty
      if (head.compare_and_swap(lhead, lnext)) {
        return {std::move(lnext->t)};
      }
    }
  }
};

}  // namespace weak_ptr_queue

#ifdef ARC_JUST_THREADS_AVAILABLE
namespace jss_queue {

template<typename T>
class alignas(128) atomic_queue {

  struct Node;
  using atomic_sp_t = std::experimental::atomic_shared_ptr<Node>;
  using sp_t = std::experimental::shared_ptr<Node>;
  using weak_ptr_t = std::experimental::weak_ptr<Node>;
  using atomic_weak_ptr_t = std::experimental::atomic_weak_ptr<Node>;

  struct Node {
    T t;
    atomic_sp_t next;
    weak_ptr_t prev;

    Node() = default;

    Node(T t_, sp_t next_, weak_ptr_t prev_) : t(std::move(t_)), next(std::move(next_)), prev(std::move(prev_)) {}
    Node(T t_) : t(std::move(t_)), next(), prev() {}
  };

  alignas(128) atomic_sp_t head;
  alignas(128) atomic_sp_t tail;

public:

  atomic_queue() {
    auto sentinel_node = std::experimental::make_shared<Node>();
    tail.store(sentinel_node);
    head.store(std::move(sentinel_node));
  };

  atomic_queue(atomic_queue &) = delete;

  void operator=(atomic_queue) = delete;

  ~atomic_queue() = default;

  void enqueue(T t) {
    auto new_node = std::experimental::make_shared<Node>(t);
    while (true) {
      auto ltail = tail.load();
      new_node->prev = ltail;

      // Help the previous enqueue in case it fell asleep before setting its next ptr
      auto lprev = ltail->prev.lock();
      if (lprev && lprev->next.load().get() == nullptr) lprev->next.store(lprev, std::memory_order_relaxed);

      if (tail.compare_exchange_strong(ltail, new_node)) {
        ltail->next.store(std::move(new_node), std::memory_order_release);
        return;
      }
    }
  }

  std::optional<T> peek() {
    auto ss = head.lock()->next.lock();
    if (ss) return {ss->t};
    else return {};
  }

  std::optional<T> dequeue() {
    while (true) {
      auto lhead = head.load();
      auto lnext = lhead->next.load();
      if (!lnext) return {};  // Queue is empty
      if (head.compare_exchange_strong(lhead, lnext)) {
        return {std::move(lnext->t)};
      }
    }
  }
};

}  // namespace jss_queue
#endif

}

#endif  // CDRC_BENCHMARKS_DATASTRUCTURES_QUEUE_H
