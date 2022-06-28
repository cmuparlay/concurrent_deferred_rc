
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>
#include <stdlib.h>
#include <thread>

#include <boost/program_options.hpp>

#include "common.hpp"
#include "barrier.hpp"

using namespace std;
namespace po = boost::program_options;

namespace bench_params{
  int iterations = 1;
  double runtime = 1;
  int threads = 4;
  int size = 10;
  int store_percent = 10;
  int cas_percent = 0;
  string alg = "gnu";
}

template<template<typename> typename AtomicSPType, template<typename> typename SPType>
struct RefCountBenchmark : Benchmark {

  RefCountBenchmark(): Benchmark(), 
                       N(bench_params::size),
                       asp_vec(new utils::Padded<AtomicSPType<PaddedInt>>[N]) {
    if(N > 100000) {  // initialize in parallel
      size_t n_threads = bench_params::threads;
      assert(n_threads <= utils::num_threads());
      std::vector<std::thread> threads;

      for(size_t p = 0; p < n_threads; p++) {
        threads.emplace_back([this, p, n_threads]() {
          utils::rand::init(p+1);
          size_t chunk_size = N/n_threads + 1;
          for(size_t i = p*chunk_size; i < N && i < (p+1)*chunk_size; i++)
            asp_vec[i].store(make_shared_int<SPType>(3));
        });        
      }
      for (auto& t : threads) t.join();     
    }
    else { // intialize sequentially 
      for(size_t i = 0; i < N; i++)
        asp_vec[i].store(make_shared_int<SPType>(3));
    }
  }

  ~RefCountBenchmark() {
    delete[] asp_vec;
  }

  void bench() override {
    for(int i = 0; i < bench_params::iterations; i++) {
      size_t n_threads = bench_params::threads;
      
      std::vector<long long int> cnt(n_threads);
      std::vector<std::thread> threads;

      std::atomic<bool> done = false;
      Barrier barrier(n_threads+1);
      //cout << N << endl;

      for (size_t p = 0; p < n_threads; p++) {
        threads.emplace_back([&barrier, &done, this, &cnt, p]() {
          utils::rand::init(p+1);

          barrier.wait();
          
          long long int ops = 0;
          volatile long long int sum = 0;
          
          for (; !done; ops++) {
            int op = utils::rand::get_rand()%100;
            int asp_index = utils::rand::get_rand()%N;
            if(op < bench_params::store_percent){ // store
              asp_vec[asp_index].store(make_shared_int<SPType>(ops & (1023)));
            } else if(op < bench_params::store_percent + bench_params::cas_percent) {  // CAS
              cerr << "not implemented" << endl;
              exit(1);
            } else {  // load
              SPType<PaddedInt> sp = asp_vec[asp_index].load();
              int x = sp->getInt();
              sum = sum + x;
            }
          }
          cnt[p] = ops;
        });
      }
      
      // Run benchmark for one second
      barrier.wait();
      start_timer();

      std::vector<size_t> allocations;
      double elapsed_time = read_timer();
      while (elapsed_time < bench_params::runtime) {
        // If the current SPType supports tracking the number of allocations, keep
        // track of it here so we can estimate the amount of deferred reclamation
        if constexpr (AllocationTrackable<AtomicSPType<PaddedInt>>) {
          allocations.push_back(AtomicSPType<PaddedInt>::currently_allocated());
        }
        usleep(1000);
        elapsed_time = read_timer();
      }
      done.store(true);
      
      for (auto& t : threads) t.join();

      // Read results
      long long int total = std::accumulate(std::begin(cnt), std::end(cnt), 0LL);
      std::cout << "\tTotal Throughput = " << total/1000000.0/elapsed_time << " Mop/s in " << elapsed_time << " second(s)" << std::endl;

      if constexpr (AllocationTrackable<AtomicSPType<PaddedInt>>) {
        assert(allocations.size() > 0);
        auto avg_alloc = std::accumulate(std::begin(allocations), std::end(allocations), 0.0) / allocations.size();
        auto max_alloc = *std::max_element(std::begin(allocations), std::end(allocations));
        std::cout << "\tAverage number of allocated objects: " << avg_alloc << " (" << allocations.size() << " samples)" << std::endl;
        std::cout << "\tMaximum number of allocated objects: " << max_alloc << std::endl;
      }
    }
  }

  static void print_name() {
    std::cout << "----------------------------------------------------------------" << std::endl;
    std::cout << "\tMicro-benchmark: P = " << bench_params::threads << ", N = " << bench_params::size << ", stores = " << 
                        bench_params::store_percent << ", CASes = " << bench_params::cas_percent << std::endl;
    std::cout << "--------------------------------------------------------------" << std::endl;
    //std::cout << AtomicSPType<int>::get_name() << std::endl;
  }

  size_t N;
  utils::Padded<AtomicSPType<PaddedInt>> *asp_vec;
};

int main(int argc, char* argv[]) {
  po::options_description description("Usage:");

  description.add_options()
  ("help,h", "Display this help message")
  ("threads,t", po::value<int>()->default_value(4), "Number of Threads")
  ("size,s", po::value<int>()->default_value(10), "Number of atomic_shared_ptrs")
  ("update,u", po::value<int>()->default_value(10), "Percentage of Stores")
  ("runtime,r", po::value<double>()->default_value(0.5), "Runtime of Benchmark (seconds)")
  ("iterations,i", po::value<int>()->default_value(5), "Number of times to run benchmark")
  ("alg,a", po::value<string>()->default_value("gnu"), "Choose one of: gnu, jss, folly, herlihy, weak_atomic, arc, orc");


  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
  po::notify(vm);

  if (vm.count("help")){
    cout << description;
    exit(0);
  }

  bench_params::iterations = vm["iterations"].as<int>();
  bench_params::alg = vm["alg"].as<string>();
  bench_params::runtime = vm["runtime"].as<double>();
  bench_params::threads = vm["threads"].as<int>();
  bench_params::size = vm["size"].as<int>();
  bench_params::store_percent = vm["update"].as<int>();

  run_benchmark<RefCountBenchmark>(bench_params::alg);
}


