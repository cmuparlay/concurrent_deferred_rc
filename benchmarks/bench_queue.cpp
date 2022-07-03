
#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

#include <boost/program_options.hpp>

#include <cdrc/internal/smr/acquire_retire_ebr.h>

#include "barrier.hpp"
#include "datastructures/queue.h"
#include "external/doublelink/CRDoubleLinkQueue.hpp"

using namespace std;
namespace po = boost::program_options;

template<typename T>
using our_hp_queue = cdrc::weak_ptr_queue::atomic_queue<T>;

template<typename T>
using ebr = cdrc::internal::acquire_retire_ebr<T>;

template<typename T>
using our_ebr_queue = cdrc::weak_ptr_queue::atomic_queue<T, ebr>;

#ifdef ARC_JUST_THREADS_AVAILABLE
template<typename T>
using jss_queue = cdrc::jss_queue::atomic_queue<T>;
#endif

struct NoGuard {};

template<template<typename> typename Queue, typename Guard = NoGuard>
void benchmark_queue(size_t num_threads, size_t num_queues, double runtime, size_t iterations, size_t queue_size) {
  for (size_t it = 1; it <= iterations; it++) {
    std::vector<Queue<int>> queues(num_queues);
    for (size_t i = 0; i < num_queues; i++) {
      for (size_t j = 0; j < queue_size; j++) {
        queues[i].enqueue(j);
      }
    }

    std::vector<long long int> cnt(num_threads);
    std::vector<std::thread> threads;

    std::atomic<bool> done = false;
    Barrier barrier(num_threads+1);

    for (size_t t = 0; t < num_threads; t++) {
      threads.emplace_back([&queues, &barrier, &done, &cnt, t, num_queues]() {
        cdrc::utils::rand::init(t+1);
        barrier.wait();
        long long int ops = 0;

        for (; !done; ops++) {
          size_t q_idx1 = cdrc::utils::rand::get_rand() % num_queues;
          size_t q_idx2 = cdrc::utils::rand::get_rand() % num_queues;

          [[maybe_unused]] Guard g;
          auto val = queues[q_idx1].dequeue();
          if (val.has_value()) {
            queues[q_idx2].enqueue(val.value());
          }
        }

        cnt[t] = ops;
      });
    }

    barrier.wait();

    auto start = std::chrono::high_resolution_clock::now();
    auto read_timer = [start]() {
      auto now = std::chrono::high_resolution_clock::now();
      double elapsed_seconds = std::chrono::duration<double>(now - start).count();
      return elapsed_seconds;
    };

    double elapsed_time = read_timer();
    while (elapsed_time < runtime) {
      usleep(1000);
      elapsed_time = read_timer();
    }
    done.store(true);

    for (auto& t : threads) t.join();

    // Read results
    long long int total = std::accumulate(std::begin(cnt), std::end(cnt), 0LL);
    std::cout << "\tTotal Throughput = " << total/1000000.0/elapsed_time << " Mop/s in " << elapsed_time << " second(s)" << std::endl;
  }
}

int main(int argc, char* argv[]) {
  po::options_description description("Usage:");

  description.add_options()
    ("help,h", "Display this help message")
    ("threads,t", po::value<int>()->default_value(4), "Number of Threads")
    ("size,s", po::value<int>()->default_value(10), "Number of queues")
    ("runtime,r", po::value<double>()->default_value(0.5), "Runtime of Benchmark (seconds)")
    ("iterations,i", po::value<int>()->default_value(5), "Number of times to run benchmark")
    ("alg,a", po::value<string>()->default_value("wp"), "Choose one of: dl, wp, sp")
    ("queue_size", po::value<int>()->default_value(20), "Number of initial elements in each queue");

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
  po::notify(vm);

  if (vm.count("help")){
    cout << description;
    exit(0);
  }



  if (vm["alg"].as<string>() == "wp") benchmark_queue<our_hp_queue,NoGuard>(
    vm["threads"].as<int>(),
    vm["size"].as<int>(),
    vm["runtime"].as<double>(),
    vm["iterations"].as<int>(),
    vm["queue_size"].as<int>());
  else if (vm["alg"].as<string>() == "dl") benchmark_queue<CRDoubleLinkQueue,NoGuard>(
    vm["threads"].as<int>(),
    vm["size"].as<int>(),
    vm["runtime"].as<double>(),
    vm["iterations"].as<int>(),
    vm["queue_size"].as<int>());
  else if (vm["alg"].as<string>() == "wp-epoch") benchmark_queue<our_ebr_queue,cdrc::epoch_guard>(
    vm["threads"].as<int>(),
    vm["size"].as<int>(),
    vm["runtime"].as<double>(),
    vm["iterations"].as<int>(),
    vm["queue_size"].as<int>());
#ifdef ARC_JUST_THREADS_AVAILABLE
  else if (vm["alg"].as<string>() == "jss") benchmark_queue<jss_queue,NoGuard>(
    vm["threads"].as<int>(),
    vm["size"].as<int>(),
    vm["runtime"].as<double>(),
    vm["iterations"].as<int>(),
    vm["queue_size"].as<int>());
#endif

}
