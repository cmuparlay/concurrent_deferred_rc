#ifndef UTILS_H
#define UTILS_H

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <atomic>
#include <iostream>
#include <memory>
#include <map>
#include <sstream>
#include <thread>
#include <type_traits>
#include <vector>

const int PADDING = 64;

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;
const uint128_t MASK = (~uint128_t(0)>>64);

namespace utils {

  size_t num_threads() {
    static size_t n_threads = []() -> size_t {
      if (const auto env_p = std::getenv("NUM_THREADS")) {
        return std::stoi(env_p) + 1;
      } else {
        return std::thread::hardware_concurrency() + 1;
      }
    }();
    return n_threads;
  }

  template<typename T, typename Sfinae = void>
  struct Padded;

  // Use user-defined conversions to pad primitive types
  template<typename T>
  struct alignas(128) Padded<T, typename std::enable_if<std::is_fundamental<T>::value>::type> {
    Padded() = default;
    Padded(T _x) : x(_x) { }
    operator T() { return x; }
    T x;
  };

  // Use inheritance to pad class types
  template<typename T>
  struct alignas(128) Padded<T, typename std::enable_if<std::is_class<T>::value>::type> : public T {

  };

  struct ThreadID {
    static std::vector<std::atomic<bool>> in_use; // initialzie to false
    int tid;

    ThreadID() {
      for(uint i = 0; i < num_threads(); i++) {
        bool expected = false;
        if(!in_use[i] && in_use[i].compare_exchange_strong(expected, true)) {
          tid = i;
          return;
        }
      }
      std::cerr << "Error: more than " << num_threads() << " threads created" << std::endl;
      std::exit(1);
    }

    ~ThreadID() {
      in_use[tid] = false;
    }

    int getTID() { return tid; }
  };

  std::vector<std::atomic<bool>> ThreadID::in_use(num_threads());

  thread_local ThreadID threadID;

  // a slightly cheaper, but possibly not as good version
  // based on splitmix64
  inline uint64_t hash64_2(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
  } 

  template <typename T>
  struct CustomHash;

  template<>
  struct CustomHash<uint64_t> {
    size_t operator()(uint64_t a) const {
      return hash64_2(a);
    }    
  };

  template<class T>
  struct CustomHash<T*> {
    size_t operator()(T* a) const {
      return hash64_2((uint64_t) a);
    }    
  };

  template<>
  struct CustomHash<uint128_t> {
    size_t operator()(const uint128_t &a) const {
      return hash64_2(a>>64) + hash64_2(a & MASK);
    }    
  };

  namespace rand {
    thread_local static unsigned long x=123456789, y=362436069, z=521288629;

    void init(int seed) {
      x += seed;
    }

    unsigned long get_rand(void) {          //period 2^96-1
      unsigned long t;
      x ^= x << 16;
      x ^= x >> 5;
      x ^= x << 1;

      t = x;
      x = y;
      y = z;
      z = t ^ x ^ y;

      return z;
    }
  }
}
#endif /* UTILS_H */
