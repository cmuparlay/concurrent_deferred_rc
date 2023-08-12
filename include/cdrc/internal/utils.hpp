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


// A wait-free atomic counter that supports increment and decrement,
// such that attempting to increment the counter from zero fails and
// does not perform the increment.
//
// Useful for incrementing reference counting, where the underlying
// managed memory is freed when the counter hits zero, so that other
// racing threads can not increment the counter back up from zero
//
// Note: The counter steals the top two bits of the integer for book-
// keeping purposes. Hence the maximum representable value in the
// counter is 2^(8*sizeof(T)-2) - 1
template<typename T>
struct StickyCounter {
  static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);

  StickyCounter() noexcept : x(1) {}
  explicit StickyCounter(T desired) noexcept : x(desired) {}

  [[nodiscard]] bool is_lock_free() const { return true; }
  static constexpr bool is_always_lock_free = true;
  [[nodiscard]] constexpr T max_value() const { return zero_pending_flag - 1; }

  StickyCounter& operator=(T desired) noexcept { x.store(desired); }
  void store(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept { x.store(desired, order); }

  explicit operator T() const noexcept { return load(); }

  T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    auto val = x.load(order);
    if (val == 0) {
      if (x.compare_exchange_strong(val, zero_flag | zero_pending_flag)) return 0;
      if (val & zero_flag) return 0;
      else return val;
    }
    return (val & zero_flag) ? 0 : val;
  }

  bool compare_exchange_strong(T& expected, T desired) noexcept {
    return x.compare_exchange_strong(expected, desired);
  }

  // Increment the counter by arg. Returns false on failure, i.e., if the counter
  // was previously zero. Otherwise returns true.
  T increment(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept {
    auto val = x.fetch_add(arg, order);
    return (val & zero_flag) == 0;
  }

  // Decrement the counter by arg. Returns true if this operation was responsible
  // for decrementing the counter to zero. Otherwise returns false.
  T decrement(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept {
    if (x.fetch_sub(arg, order) == arg) {
      T current = 0;
      if (x.compare_exchange_strong(current, zero_flag)) return true;
      else if (current & zero_pending_flag) { return x.compare_exchange_strong(current, zero_flag); }
    }
    return false;
  }

private:
  static constexpr inline T zero_flag = T(1) << (sizeof(T)*8) - 1;
  static constexpr inline T zero_pending_flag = T(1) << (sizeof(T)*8) - 2;
  mutable std::atomic<T> x;
};


/*
// An atomic integer variable that supports fetch_add and fetch_sub, such that
// fetch_add fails if the value was initially zero.
template<typename T>
struct StickyCounter {

  static_assert(std::is_integral_v<T>);
  static_assert(std::is_unsigned_v<T>);

  StickyCounter() : x(1) {}
  explicit StickyCounter(T desired) : x(desired) {}

  [[nodiscard]] constexpr bool is_lock_free() const { return true; }

  StickyCounter& operator=(T desired) noexcept { x.store(desired); }

  void store(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept { x.store(desired); }

  explicit operator T() const noexcept { return load(); }

  bool compare_exchange_strong(T& expected, T desired) {
    return x.compare_exchange_strong(expected, desired);
  }

  T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    auto val = x.load();
    return (val & flag) ? 0 : val;
  }

  bool increment(T arg) {
    return fetch_add(arg) != 0;
  }

  bool decrement(T arg) {
    return fetch_sub(arg) == arg;
  }

  T fetch_add(T arg, std::memory_order order = std::memory_order_seq_cst) {
    auto val = x.fetch_add(arg);
    return (val & flag) ? 0 : val;
  }

  T fetch_sub(T arg, std::memory_order order = std::memory_order_seq_cst) {
    assert(!(x.load() & flag));     // Don't subtract from a stuck counter
    assert(x.load() >= arg);        // Don't go negative
    auto val = x.fetch_sub(arg);
    if (val == arg) {
      T expected = 0;
      if (x.compare_exchange_strong(expected, flag)) return val;
      else if (expected & flag) return arg + 1;
      else return expected + arg;
    }
    return val;
  }

private:
  static constexpr inline T flag = T(1) << (sizeof(T)*8) - 1;
  std::atomic<T> x;
};
 */

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
