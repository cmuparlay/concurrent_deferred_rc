
#ifndef BENCHMARKS_COMMON_HPP_
#define BENCHMARKS_COMMON_HPP_

#include <atomic>
#include <chrono>
#include <iostream>

#include <cdrc/rc_ptr.h>
#include <cdrc/atomic_rc_ptr.h>

#ifndef _MSC_VER
#include "external/weak_atomic/weak_atomic.hpp"
#endif

#include "external/herlihy/herlihy_arc_ptr.h"
#include "external/herlihy/herlihy_arc_ptr_opt.h"
#include "external/herlihy/herlihy_rc_ptr.h"

#include "external/orcgc/OrcPTP.hpp"

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
struct StlAtomicSharedPtr {
  StlAtomicSharedPtr() = default;
  StlAtomicSharedPtr(const std::shared_ptr<T>& other) : sp(other) { }
  std::shared_ptr<T> load() { return std::atomic_load(&sp); }
  void store(std::shared_ptr<T> r) { std::atomic_store(&sp, std::move(r)); }
  bool compare_exchange_strong(std::shared_ptr<T>& expected, const std::shared_ptr<T>& desired) {
    return atomic_compare_exchange_strong(&sp, &expected, desired);
  }
  bool compare_exchange_weak(std::shared_ptr<T>& expected, const std::shared_ptr<T>& desired) {
    return atomic_compare_exchange_weak(&sp, &expected, desired);
  }
  std::shared_ptr<T> sp;
};

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

#ifndef _MSC_VER
template<typename T>
using WeakAtomicRcPtrLockfree = weak_atomic<cdrc::rc_ptr<T>, AcquireRetireLockfree>;
#endif

template<typename T>
using SnapshottingArcPtr = cdrc::atomic_rc_ptr<T>;

template<typename T>
using OurRcPtr = cdrc::rc_ptr<T>;

template<typename T>
using HerlihyRcPtr = herlihy_rc_ptr<T, false>;

template<typename T>
using HerlihyRcPtrOpt = herlihy_rc_ptr<T, true>;

template<typename T>
using OrcAtomicRcPtr = orcgc_ptp::orc_atomic<T*>;

template<typename T>
using OrcRcPtr = orcgc_ptp::orc_ptr<T*>;

struct alignas(32) PaddedInt : orcgc_ptp::orc_base {
  int x;
  PaddedInt(int x) : x(x) {}
  int getInt() { return x; }
};

template<template<typename> typename SPType>
SPType<PaddedInt> make_shared_int(int val) {
  if constexpr (std::is_same<SPType<PaddedInt>, std::shared_ptr<PaddedInt>>::value)
    return std::make_shared<PaddedInt>(val);                                        // STL shared_ptr
#ifdef ARC_JUST_THREADS_AVAILABLE
  else if constexpr (std::is_same<SPType<PaddedInt>, std::experimental::shared_ptr<PaddedInt>>::value)
    return std::experimental::make_shared<PaddedInt>(val);                          // JustThreads shared_ptr
#endif
  else if constexpr (std::is_same<SPType<PaddedInt>, HerlihyRcPtr<PaddedInt>>::value)
    return herlihy_rc_ptr<PaddedInt, false>::make_shared(val);                             // Herlihy's algorithm
  else if constexpr (std::is_same<SPType<PaddedInt>, HerlihyRcPtrOpt<PaddedInt>>::value)
    return herlihy_rc_ptr<PaddedInt, true>::make_shared(val);                             // Herlihy's algorithm
  else if constexpr (std::is_same<SPType<PaddedInt>, cdrc::rc_ptr<PaddedInt>>::value)
    return cdrc::rc_ptr<PaddedInt>::make_shared(val);                                     // Our algorithm
  else if constexpr (std::is_same<SPType<PaddedInt>, OrcRcPtr<PaddedInt>>::value)   // ORC-GC's "orc_ptr"
    return orcgc_ptp::make_orc<PaddedInt>(val);
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
  else if constexpr (std::is_same<SPType<T>, HerlihyRcPtr<T>>::value)
    return HerlihyRcPtr<T>::make_shared();
  else if constexpr (std::is_same<SPType<T>, HerlihyRcPtrOpt<T>>::value)
    return HerlihyRcPtrOpt<T>::make_shared();
  else if constexpr (std::is_same<SPType<T>, cdrc::rc_ptr<T>>::value)
    return cdrc::rc_ptr<T>::make_shared();
  else if (std::is_same<SPType<PaddedInt>, OrcRcPtr<PaddedInt>>::value)   // ORC-GC's "orc_ptr"
    return orcgc_ptp::make_orc<T>();
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
#ifndef _MSC_VER
  else if (alg == "weak_atomic")
    run_benchmark_helper<BenchmarkType, WeakAtomicRcPtrLockfree, OurRcPtr>("Weak Atomic rc_ptr (lockfree)");
#endif
  else if (alg == "herlihy")
    run_benchmark_helper<BenchmarkType, herlihy_arc_ptr, HerlihyRcPtr>("Herlihy");
  else if (alg == "herlihy-opt")
    run_benchmark_helper<BenchmarkType, herlihy_arc_ptr_opt, HerlihyRcPtrOpt>("Herlihy-Opt");
  else if (alg == "arc")
    run_benchmark_helper<BenchmarkType, SnapshottingArcPtr, OurRcPtr>("ARC");
  else if (alg == "orc")
    run_benchmark_helper<BenchmarkType, OrcAtomicRcPtr, OrcRcPtr>("ORC-GC");
  else {
    std::cout << "invalid alg name: " << alg << std::endl;
    exit(1);
  }

}

#endif  // BENCHMARKS_COMMON_HPP_

