
#ifndef BENCHMARKS_COMMON_HPP_
#define BENCHMARKS_COMMON_HPP_

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>

#include <cdrc/internal/utils.hpp>

#include "external/herlihy/herlihy_arc_ptr_opt.h"
#include "external/herlihy/herlihy_rc_ptr.h"

#include "/home/daniel/atomic_shared_ptr/include/parlay/atomic_shared_ptr_custom.hpp"

#ifdef ARC_JUST_THREADS_AVAILABLE
#include <experimental/atomic>
#endif

#ifdef ARC_FOLLY_AVAILABLE
#include <folly/concurrency/AtomicSharedPtr.h>
#endif

// ==================================================================
//                  Shared pointer implementations
// ==================================================================

// C++ standard library atomic support for shared ptrs
template<typename T>
using StlAtomicSharedPtr = std::atomic<std::shared_ptr<T>>;

#ifdef ARC_JUST_THREADS_AVAILABLE
// Just::threads atomic support for shared ptrs
template<typename T>
using JssAtomicSharedPtr = std::experimental::atomic_shared_ptr<T>;
#endif

#ifdef ARC_FOLLY_AVAILABLE
// Folly atomic support for shared ptrs with std::shared_ptr
template<typename T>
using FollyAtomicStdSharedPtr = folly::atomic_shared_ptr<T>;
#endif

// Optimized Herlihy algorithm with custom shared ptr
template<typename T>
using HerlihyRcPtrOpt = herlihy_rc_ptr<T, true>;

// My new atomic shared pointers
template<typename T>
using MyAtomicSharedPtr = parlay::atomic_shared_ptr<T>;

template<typename T>
using MySharedPtr = parlay::shared_ptr<T>;



using PaddedInt = utils::Padded<int>;

static_assert(std::constructible_from<PaddedInt, int&>);

template<template<typename> typename SPType>
SPType<PaddedInt> make_shared_int(int val) {
  if constexpr (std::is_same_v<SPType<PaddedInt>, std::shared_ptr<PaddedInt>>)
    return std::make_shared<PaddedInt>(val);                                        // STL shared_ptr
#ifdef ARC_JUST_THREADS_AVAILABLE
  else if constexpr (std::is_same_v<SPType<PaddedInt>, std::experimental::shared_ptr<PaddedInt>>)
    return std::experimental::make_shared<PaddedInt>(val);                          // JustThreads shared_ptr
#endif
  else if constexpr (std::is_same_v<SPType<PaddedInt>, HerlihyRcPtrOpt<PaddedInt>>)
    return herlihy_rc_ptr<PaddedInt, true>::make_shared(val);                       // Herlihy's algorithm
  else if constexpr (std::is_same_v<SPType<PaddedInt>, MySharedPtr<PaddedInt>>)
    return parlay::make_shared<PaddedInt>(val);
  else // homebrew shared pointer [depricated]
  {
    std::cerr << "invalid SPType" << std::endl;
    exit(1);
  }
}

// TODO: forward arguments
template<typename T, template<typename> typename SPType>
SPType<T> make_shared() {
  if constexpr (std::is_same<SPType<T>, std::shared_ptr<T>>::value)
    return std::make_shared<T>();
#ifdef ARC_JUST_THREADS_AVAILABLE
  else if constexpr (std::is_same<SPType<T>, std::experimental::shared_ptr<T>>::value)
    return std::experimental::make_shared<T>();
#endif
  else if constexpr (std::is_same<SPType<T>, HerlihyRcPtrOpt<T>>::value)
    return HerlihyRcPtrOpt<T>::make_shared();
  else {
    std::cerr << "invalid SPType" << std::endl;
    exit(1);
  }
}

template<typename T>
concept AllocationTrackable = requires(T&& t) {
  t.currently_allocated();
};

// ==================================================================
//                     Benchmarking framework
// ==================================================================

// Generic base class for benchmarks
// Benchmarks should implement the bench() function, which
// runs the benchmark with P threads.
struct Benchmark {
  
  Benchmark() { }

  void start_timer() {
    start = std::chrono::high_resolution_clock::now();
  }

  double read_timer() {
    auto now = std::chrono::high_resolution_clock::now();
    double elapsed_seconds = std::chrono::duration<double>(now - start).count();
    return elapsed_seconds;
  }

  virtual void bench() = 0;
  virtual ~Benchmark() = default;
  
  std::chrono::time_point<std::chrono::high_resolution_clock> start;
  //int P;    // Number of threads
};

// Run a specific benchmark with a specific shared ptr implementation on varying thread counts
template<template<template<typename> typename, template<typename> typename> typename BenchmarkType, template<typename> typename AtomicSPType, template<typename> typename SPType>
void run_benchmark_helper(std::string name) {
  BenchmarkType<AtomicSPType, SPType>::print_name();
  std::cout << name << std::endl;

  BenchmarkType<AtomicSPType, SPType> benchmark;
  benchmark.bench();

  std::cout << std::endl;
}

// Run the given benchmark with all shared ptr implementations
template<template<template<typename> typename, template<typename> typename> typename BenchmarkType>
void run_benchmark(std::string alg) {
  if (alg == "gnu")
    run_benchmark_helper<BenchmarkType, StlAtomicSharedPtr, std::shared_ptr>("GNU");
#ifdef ARC_JUST_THREADS_AVAILABLE
  else if (alg == "jss")
    run_benchmark_helper<BenchmarkType, JssAtomicSharedPtr, std::experimental::shared_ptr>("Just::threads atomic shared ptr");
#endif
#ifdef ARC_FOLLY_AVAILABLE
  else if (alg == "folly")
    run_benchmark_helper<BenchmarkType, FollyAtomicStdSharedPtr, std::shared_ptr>("Folly atomic std::shared_ptr");
#endif
  else if (alg == "herlihy")
    run_benchmark_helper<BenchmarkType, herlihy_arc_ptr_opt, HerlihyRcPtrOpt>("Herlihy-Opt");
  else if (alg == "mine")
    run_benchmark_helper<BenchmarkType, MyAtomicSharedPtr, MySharedPtr>("My atomic shared ptr");
  else {
    std::cout << "invalid alg name: " << alg << std::endl;
    exit(1);
  }

}

#endif  // BENCHMARKS_COMMON_HPP_

