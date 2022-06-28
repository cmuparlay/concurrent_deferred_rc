
#include <cassert>

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

#include "../benchmarks/datastructures/queue.h"

using namespace std;

static const int N = utils::num_threads() - 1;   // Number of threads
static const int M = 10000;                      // Number of operations per thread


void test_seq() {
  puts("SEQUENTIAL TEST...");
  cdrc::weak_ptr_queue::atomic_queue<int> q;
  assert(!q.dequeue().has_value());   // Initially empty
  for (int i = 0; i < 1000; i++) {
    q.enqueue(i);
  }
  for (int i = 0; i < 1000; i++) {
    auto x = q.dequeue();
    assert(x.has_value());
    assert(x.value() == i);
  }
  assert(!q.dequeue().has_value());
  for (int i = 0; i < 1000; i++) {
    q.enqueue(i);
  }
  for (int i = 0; i < 1000; i++) {
    auto x = q.dequeue();
    assert(x.has_value());
    assert(x.value() == i);
  }
  assert(!q.dequeue().has_value());
  puts("\tOK");
}

// Don't pop anything. This should check that the queue actually empties
// itself when it is destructed at the end.
void test_destructor() {
  puts("DESTRUCTOR TEST...");
  {
    cdrc::weak_ptr_queue::atomic_queue<int> q;
    for (int i = 0; i < 1000; i++) {
      q.enqueue(i);
    }
  }
  // If the queue doesn't destroy itself, we'll detect a leak after the test completes
  puts("\tOK");
}


// Seperate threads are assigned to just enqueue (produce) or just dequeue (consume)
void test_par() {
  puts("PARALLEL PRODUCER AND CONSUMER TEST...");

  cdrc::weak_ptr_queue::atomic_queue<int> q;
  std::atomic<int> done;
  std::vector<long long int> consumer_sums(N/2);
  std::vector<long long int> producer_sums(N/2);

  std::vector<thread> consumers;
  consumers.reserve(N/2);
  for (int i = 0; i < N/2; i++) {
    consumers.emplace_back([i, &q, &done, &consumer_sums] {
      long long int local_sum = 0;
      while (done.load(std::memory_order_acquire) < N/2 || q.peek()) {
        auto val = q.dequeue();
        if(val) local_sum += val.value();
      }
      consumer_sums[i] = local_sum;
    });
  }

  std::vector<thread> producers;
  producers.reserve(N/2);
  for (int i = 0; i < N/2; i++) {
    producers.emplace_back([i, &q, &done, &producer_sums] {
      long long int local_sum = 0;
      for(int j = 0; j < M; j++) {
        q.enqueue(j);
        local_sum += j;
      }
      producer_sums[i] = local_sum;
      done.fetch_add(1, std::memory_order_acq_rel);
    });
  }

  while (done.load(std::memory_order_acquire) < N/2) {}

  long long actualsum = 0;
  long long checksum = 0;
  for (int i = 0; i < N/2; i++) {
    producers[i].join();
    consumers[i].join();
    actualsum += producer_sums[i];
    checksum += consumer_sums[i];
  }

  assert(checksum == actualsum);
  puts("\tOK");
}

// Each thread both dequeues and enqueues
void test_par2() {
  puts("PARALLEL DEQUEUE+ENQUEUE TEST");

  // The queue contains one element per thread
  cdrc::weak_ptr_queue::atomic_queue<int> q;
  for (int t = 0; t < N; t++) {
    q.enqueue(t);
  }

  std::vector<long long int> sums(N);
  std::vector<std::thread> threads;
  threads.reserve(N);

  std::atomic<bool> done = false;

  for (int t = 0; t < N; t++) {
    threads.emplace_back([&q, &sums, t]() {
      long long int local_sum = 0;
      for (size_t i = 0; i < M; i++) {
        auto val = q.dequeue();
        if (val.has_value()) {
          local_sum += val.value();
          q.enqueue(val.value());
        }
      }
      sums[t] = local_sum;
    });
  }

  for (auto& t : threads) t.join();
  puts("\tOK");
}

void run_all_tests() {
  test_seq();
  test_destructor();
  test_par();
  test_par2();
}

int main () {
  std::cout << "Running tests using up to " << utils::num_threads() << " threads." << std::endl;
  run_all_tests();
}


