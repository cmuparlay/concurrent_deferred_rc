
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

#include "datastructures/stack.h"

using namespace std;
namespace po = boost::program_options;

namespace bench_params {
  int iterations = 1;
  double runtime = 1;
  int threads = 4;
  int size = 10;
  int update_percent = 10;
  size_t stack_size = 20;
  string alg = "gnu";
  bool peek = false;
}

template<template<typename> typename AtomicSPType, template<typename> typename SPType>
struct StackBenchmark : Benchmark {

  using stack_type = atomic_stack<int, AtomicSPType, SPType>;

  StackBenchmark(): Benchmark(),
                    N(bench_params::size),
                    stacks(N),
                    expected_stack_size(bench_params::stack_size + bench_params::threads / N) {

    // Add P/N extra items so that the expected size of every stack under
    // contention is bench_params::stack_size (to account for pops in progress).

    if(N > 100000) {  // initialize in parallel
      size_t n_threads = bench_params::threads;
      assert(n_threads <= utils::num_threads());
      std::vector<std::thread> threads;
      for(size_t p = 0; p < n_threads; p++) {
        threads.emplace_back([this, p, n_threads]() {
          utils::rand::init(p+1);
          size_t chunk_size = N/n_threads + 1;
          for(size_t i = p*chunk_size; i < N && i < (p+1)*chunk_size; i++) {
            for (size_t j = 0; j < expected_stack_size; j++) {
              stacks[i].push_front(utils::rand::get_rand()%expected_stack_size);
            }
          }
        });
      }
      for (auto& t : threads) t.join();
    }
    else { // intialize sequentially
      for(size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < expected_stack_size; j++) {
          stacks[i].push_front(utils::rand::get_rand()%expected_stack_size);
        }
      }
    }
  }

  void bench() override {
    for(int i = 0; i < bench_params::iterations; i++) {
      size_t n_threads = bench_params::threads;
      assert(n_threads <= utils::num_threads());

      std::vector<long long int> cnt(n_threads);
      std::vector<std::thread> threads;

      std::atomic<bool> done = false;
      Barrier barrier(n_threads+1);

      for (size_t p = 0; p < n_threads; p++) {
        threads.emplace_back([&barrier, &done, this, &cnt, p]() {
          utils::rand::init(p+1);

          barrier.wait();

          long long int ops = 0;
          long long int sum = 0;

          for (; !done; ops++) {
            int op = utils::rand::get_rand()%100;

            if(op < bench_params::update_percent){
              int stack_index1 = utils::rand::get_rand()%N;
              int stack_index2 = utils::rand::get_rand()%N;

              auto val = stacks[stack_index1].pop_front();
              if (val.has_value()) {
                stacks[stack_index2].push_front(val.value());
              }
            }
            // Do a search
            else {
              int stack_index = utils::rand::get_rand()%N;
              if (bench_params::peek) {
                auto val = stacks[stack_index].front();
                sum += val.has_value();
              }
              else {
                auto val = utils::rand::get_rand() % expected_stack_size;
                auto found = stacks[stack_index].find(val);
                sum += found;
              }
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
        if constexpr (stack_type::tracks_allocations) {
          allocations.push_back(stack_type::currently_allocated());
        }
        usleep(1000);
        elapsed_time = read_timer();
      }
      done.store(true);

      for (auto& t : threads) t.join();


      // Read results
      long long int total = std::accumulate(std::begin(cnt), std::end(cnt), 0LL);
      std::cout << "\tTotal Throughput = " << total/1000000.0/elapsed_time << " Mop/s in " << elapsed_time << " second(s)" << std::endl;

      if constexpr (stack_type::tracks_allocations) {
        assert(allocations.size() > 0);
        auto avg_alloc = std::accumulate(std::begin(allocations), std::end(allocations), 0.0) / allocations.size();
        auto max_alloc = *std::max_element(std::begin(allocations), std::end(allocations));
        std::cout << "\tAverage number of allocated objects: " << avg_alloc << " (" << allocations.size() << " samples)" << std::endl;
        std::cout << "\tMaximum number of allocated objects: " << max_alloc << std::endl;
      }
      size_t total_nodes = 0;
      for(size_t i = 0; i < N; i++) {
        total_nodes += stacks[i].size();
      }
      std::cout << "\tTotal stack size = " << total_nodes << std::endl;
    }
  }

  static void print_name() {
    std::cout << "----------------------------------------------------------------" << std::endl;
    std::cout << "\tMicro-benchmark: P = " << bench_params::threads << ", N = "
              << bench_params::size << ", updates = " << bench_params::update_percent << std::endl;
    std::cout << "--------------------------------------------------------------" << std::endl;
  }

  size_t N;
  std::vector<stack_type> stacks;
  size_t expected_stack_size;
};

//static_assert(sizeof(parlay::details::control_block_inplace<int>) == 32);
//static_assert(sizeof(std::_Sp_counted_ptr_inplace<int, std::allocator<int>, std::_S_atomic>) == 24);
//static_assert(sizeof(internal::herlihy_counted_object<int>) == 16);

//static_assert(sizeof(parlay::details::control_block_with_ptr<int>) == 24);
//static_assert(sizeof(std::_Sp_counted_ptr<int, std::_S_atomic>) == 24);

//static_assert(sizeof(atomic_stack<int, parlay::atomic_shared_ptr, parlay::shared_ptr>::Node) == 16);
//static_assert(sizeof(atomic_stack<int, herlihy_arc_ptr_opt, HerlihyRcPtrOpt>::Node) == 16);

//static_assert(sizeof(parlay::details::control_block_with_ptr<atomic_stack<int, parlay::atomic_shared_ptr, parlay::shared_ptr>::Node>) == 24);
static_assert(sizeof(parlay::details::control_block_inplace<atomic_stack<int, parlay::atomic_shared_ptr, parlay::shared_ptr>::Node>) == 40);
//static_assert(sizeof(parlay::details::fast_control_block<atomic_stack<int, parlay::atomic_shared_ptr, parlay::shared_ptr>::Node>) == 24);
//static_assert(sizeof(std::_Sp_counted_ptr<atomic_stack<int, folly::atomic_shared_ptr, std::shared_ptr>::Node, std::_S_atomic>) == 40);
//static_assert(sizeof(internal::herlihy_counted_object<atomic_stack<int, herlihy_arc_ptr_opt, HerlihyRcPtrOpt>::Node>) == 24);

int main(int argc, char* argv[]) {
  po::options_description description("Usage:");

  description.add_options()
      ("help,h", "Display this help message")
      ("threads,t", po::value<int>()->default_value(4), "Number of Threads")
      ("size,s", po::value<int>()->default_value(10), "Number of stacks")
      ("update,u", po::value<int>()->default_value(10), "Percentage of pushes/pops")
      ("runtime,r", po::value<double>()->default_value(0.5), "Runtime of Benchmark (seconds)")
      ("iterations,i", po::value<int>()->default_value(5), "Number of times to run benchmark")
      ("alg,a", po::value<string>()->default_value("gnu"), "Choose one of: gnu, jss, folly, herlihy, weak_atomic, arc, orc")
      ("stack_size", po::value<int>()->default_value(20), "Number of initial elements in each stack")
      ("peek", po::value<bool>()->default_value(false), "Use peek instead of find as the read workload");


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
  bench_params::update_percent = vm["update"].as<int>();
  bench_params::peek = vm["peek"].as<bool>();
  bench_params::stack_size = vm["stack_size"].as<int>();

  run_benchmark<StackBenchmark>(bench_params::alg);
}


