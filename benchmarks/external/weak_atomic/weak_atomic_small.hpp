
#ifndef WEAK_ATOMIC_SMALL_H
#define WEAK_ATOMIC_SMALL_H

#include <thread>
#include <iostream>
#include <vector>
#include <unordered_set>
#include <experimental/optional>
#include <experimental/algorithm>
#include <atomic>
#include <stdio.h>
#include <string.h>

#include "acquire_retire_lockfree.hpp"

namespace weak_atomic_utils {
  //POD turns T into a plain old data type
  template <class T>
  using POD =
    typename std::conditional<(sizeof(T) == 8), uint64_t,
        uint128_t
    >::type;

  template<typename T>
  struct DefaultWeakAtomicTraits {
    static T empty_t() { return T{}; }
  };
}


/*
  ASSUMPTIONS:
    - T is 8 bytes or 16 bytes
    - T must be trivially move constructable
    - There exists an "empty" instantiation of T that
      is trivial to construct and destruct
    - T must support move assignment
*/
template <typename T, template<typename, typename> typename AcquireRetire = AcquireRetireLockfree, typename Traits = weak_atomic_utils::DefaultWeakAtomicTraits<T>>
struct weak_atomic_small {

  using P = weak_atomic_utils::POD<T>;
  
  static T empty_t;
  static P empty_p;

  public:
    weak_atomic_small(const weak_atomic_small&) = delete;
    weak_atomic_small& operator=(weak_atomic_small const&) = delete;
    weak_atomic_small() {
      T t;
      x.store(to_bits(t), std::memory_order_relaxed);
      set_to_empty(&t);
      // new (&x) T();
    };

    template<typename... Args>
    weak_atomic_small(Args&&... args) {
      T t(std::forward<Args>(args)...);
      x.store(to_bits(t), std::memory_order_relaxed);
      set_to_empty(&t);
      // new (&x) T(std::forward<Args>(args)...);
    };

    ~weak_atomic_small() {
      P p = x.load(std::memory_order_relaxed);
      destroy(&p);
    }

    T load() {
      auto id = utils::threadID.getTID();
      P z_bits = ar.slots[id].acquire(&x);
      T z(copy(z_bits));
      ar.slots[id].release();
      return z;
    }

    void store(T desired) {
      auto id = utils::threadID.getTID();
      P new_bits = to_bits(desired);
      P old_bits = x.exchange(new_bits, std::memory_order_seq_cst);
      set_to_empty(&desired);
      if (old_bits != empty_p) ar.slots[id].retire(old_bits);
      ar.slots[id].eject_and_destruct();
    }

    // we might need to assume that there are no races on old_val and new_val
    // while this is going on
    // I believe this would work for std::move(new_val) as well
    // We have to create a new copy of new_val just in case the CAS succeeds
    bool compare_exchange_strong(const T &old_val, T& new_val) {
      auto id = utils::threadID.getTID();
      P old_bits = to_bits(old_val);
      P new_bits = to_bits(new_val);
      if(x.compare_exchange_strong(old_bits, new_bits, std::memory_order_seq_cst)) {
        set_to_empty(&new_val);                 // prevent new_copy from being destructed
        // destruct old_val
        if(old_bits != empty_p)                 // never retire nullptr because it is used
          ar.slots[id].retire(old_bits);     // to represent 'empty' in the annoucement array        
        ar.slots[id].eject_and_destruct();
        return true;
      } else {
        return false;
      }
    }

    bool compare_exchange_weak(T& old_val, T new_val) {
      if (compare_exchange_strong(old_val, new_val)) return true;
      else {
        old_val = load();
        return false;
      }
    }
    
    // calls the destructor of an object of type T at
    // the memory location p
    inline static void destroy(P* p) {
      reinterpret_cast<T*>(p)->~T();
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
    // Set the value pointed to by ptr to the bits
    // that represent the empty object.
    inline static void set_to_empty(T* ptr) {
      memcpy(ptr, &empty_t, sizeof(T)); 
    }
#pragma GCC diagnostic pop

    static bool eject_and_destruct(int tid) {
      std::optional<P> t = ar.slots[tid].eject();
      if(t) {
        destroy(&t.value());
        return true;
      } 
      return false;
    }

    // // Calls eject_all() for all processes and destructs everything that gets returned
    // // Only call this during a quiescent state 
    // // Important for cleaning up lingering items in the retired list
    // static void clear_all() {
    //   for(int tid = 0; tid < MAX_THREADS; tid++) {
    //     for(P t : ar.slots[tid].eject_all())  
    //       destroy(&t);
    //     while(eject_and_destruct(tid)) { }    
    //   }
    // }
    
  private:
    alignas(16) std::atomic<P> x;
    static AcquireRetire<P, T> ar;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

    /*
      This function calls the copy constructor of T using a bit sequence.

      This function probably places restrictions on the copy constructor of T.
    */
    static T copy(P t) {
      return T(*reinterpret_cast<T*>(&t));
    }

    static P to_bits(const T &t) {
      return *reinterpret_cast<const P*>(&t);
    }

#pragma GCC diagnostic pop

};

template<typename T, template<typename, typename> typename AcquireRetire, typename Traits>
T weak_atomic_small<T, AcquireRetire, Traits>::empty_t = Traits::empty_t();

template<typename T, template<typename, typename> typename AcquireRetire, typename Traits>
typename weak_atomic_small<T, AcquireRetire, Traits>::P weak_atomic_small<T, AcquireRetire, Traits>::empty_p = weak_atomic_small<T, AcquireRetire, Traits>::to_bits(Traits::empty_t());


template<typename T, template<typename, typename> typename AcquireRetire, typename Traits>
AcquireRetire<typename weak_atomic_small<T, AcquireRetire, Traits>::P, T> weak_atomic_small<T, AcquireRetire, Traits>::ar{static_cast<int>(utils::num_threads()), weak_atomic_small<T, AcquireRetire, Traits>::to_bits(Traits::empty_t())};

#endif /* WEAK_ATOMIC_SMALL_H */
