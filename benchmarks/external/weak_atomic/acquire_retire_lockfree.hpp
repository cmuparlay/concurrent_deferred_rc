
#ifndef AR_LOCKFREE_H
#define AR_LOCKFREE_H

#include <algorithm>
#include <atomic>
#include <iostream>
#include <optional>
#include <unordered_set>
#include <vector>

#include <cdrc/internal/utils.hpp>

/* acquire-retire should only be used on plain old data types
   that fit into 8 btyes (while there is 16 btye CAS, there's no 16 byte read).
   Currently assumes that T = uintptr_t and that nullptr will never be retired.
   If nullptr is ever retired, it will possibly never be ejected.

   Fences are placed assuming x86
   Membarrier only works on linux based systems.
*/
template <bool Membarrier, typename T, typename D = T>
struct AcquireRetireLockfreeBase {
  struct alignas(128) slot_t {
    slot_t(T _empty) : empty(_empty), announce(empty), parent(nullptr) {}
    slot_t(AcquireRetireLockfreeBase* parent, T _empty) : empty(_empty), announce(empty), parent(parent) {}

    // move constructor
    slot_t(slot_t&& s) : empty(std::move(s.empty)), announce(s.announce.load()), 
                         retired(std::move(s.retired)), free(std::move(s.free)),
                         parent(s.parent) {}

    T empty;
    std::atomic<T> announce;
    std::vector<T> retired;
    std::vector<T> free;
    AcquireRetireLockfreeBase* parent;
    
    T acquire(std::atomic<T>* x) {
      T a;
      do {
        a = x->load(std::memory_order_seq_cst); // this read does not have to be atomic
        announce.store(a, std::memory_order_seq_cst); // this write does not have to be atomic
        // if constexpr (Membarrier) asm volatile ("" : : : "memory"); // std::atomic_signal_fence(std::memory_order_seq_cst);
        // else std::atomic_thread_fence(std::memory_order_seq_cst);
      } while (x->load(std::memory_order_seq_cst) != a);
      return a;
    }

    // We can use a cheaper acquire when x is a non-racy location
    // in general, we might need stronger fences in here. 
    T acquire(T* x) {
      T a = *x;
      announce.store(a, std::memory_order_release);
      return a;
    }

    void release() {
      announce.store(empty, std::memory_order_release);
    }

    void retire(T t) {
      retired.push_back(t);
    }

    std::optional<T> eject() {
      if(retired.size() == AcquireRetireLockfreeBase::RECLAIM_DELAY*utils::num_threads())
      {
        std::vector<T> tmp = eject_all();
        free.insert(free.end(), tmp.begin(), tmp.end());
      }
      
      if(free.empty()) return std::nullopt;
      else {
        T ret = free.back();
        free.pop_back();
        return ret;
      }
    }

    std::vector<T> eject_all() {
      // if constexpr (Membarrier) syscall(__NR_membarrier, MEMBARRIER_CMD_SHARED, 0);
      // else std::atomic_thread_fence(std::memory_order_seq_cst);
      // std::atomic_thread_fence(std::memory_order_seq_cst);
      std::unordered_multiset<T, utils::CustomHash<T>> held;
      std::vector<T> ejected;
      for (auto &x : parent->slots) {
        T reserved = x.announce.load(std::memory_order_seq_cst); // this read does not need to be atomic
        if (reserved != empty) 
          held.insert(reserved);
      }
      auto f = [&] (T x) {
        auto it = held.find(x);
        if (it == held.end()) {
          ejected.push_back(x);
          return true;
        } else {
          held.erase(it);
          return false;
        }
      };
      retired.erase(remove_if(retired.begin(), retired.end(), f),
          retired.end());
      return ejected;
    }
    
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

    void eject_and_destruct() {
      if(retired.size() == AcquireRetireLockfreeBase::RECLAIM_DELAY*utils::num_threads())
      {
        // if constexpr (Membarrier) syscall(__NR_membarrier, MEMBARRIER_CMD_SHARED, 0);
        // else std::atomic_thread_fence(std::memory_order_seq_cst);
        // std::atomic_thread_fence(std::memory_order_seq_cst);
        std::unordered_multiset<T, utils::CustomHash<T>> held;
        for (auto &x : parent->slots) {
          T reserved = x.announce.load(std::memory_order_seq_cst); // this read does not need to be atomic
          if (reserved != empty) 
            held.insert(reserved);
        }
        auto f = [&] (T x) {
          auto it = held.find(x);
          if (it == held.end()) {
            reinterpret_cast<D*>(&x)->~D();
            return true;
          } else {
            held.erase(it);
            return false;
          }
        };
        retired.erase(remove_if(retired.begin(), retired.end(), f), retired.end());
      }
    }

  };

  T empty;  
  std::atomic<int> num_slots;
  std::vector<slot_t> slots;
  static constexpr double RECLAIM_DELAY = 10;
  
  AcquireRetireLockfreeBase(const AcquireRetireLockfreeBase&) = delete;
  AcquireRetireLockfreeBase& operator=(AcquireRetireLockfreeBase const&) = delete;
  
  AcquireRetireLockfreeBase(int max_slots, T _empty) : empty(_empty), num_slots(0), slots{}
  {
    slots.reserve(max_slots*4);
    for(int i = 0; i < max_slots; i++)
      slots.push_back(slot_t(this, empty));
  }

  ~AcquireRetireLockfreeBase() {
    for(uint i = 0; i < slots.size(); i++) {
      // why is it safe to free here?
      // I guess that we are assuming all acquires are released when the acquire-retire object is destructed.
      // By passing a non-trivial D, the user is saying that a retired handle is safe to destruct as soon as all it's acquires have been released.
      for(T t : slots[i].retired)
        reinterpret_cast<D*>(&t)->~D();
      for(T t : slots[i].free)
        reinterpret_cast<D*>(&t)->~D();
    }
  }

#pragma GCC diagnostic pop

  // slot_t* register_ar() {
  //   int i = num_slots.fetch_add(1);
  //   if (i > slots.size()) abort();
  //   return &slots[i];
  // }
    
};

template <typename T, typename D = T>
using AcquireRetireLockfree = AcquireRetireLockfreeBase<false, T, D>;

template <typename T, typename D = T>
using AcquireRetireLockfreeMembar = AcquireRetireLockfreeBase<true, T, D>;

#endif /* AR_LOCKFREE_H */
