#include "gtest/gtest.h"

#include <cstdio>

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "../benchmarks/datastructures/queue.h"

using namespace std;

static const size_t N = cdrc::utils::num_threads() - 1;   // Number of threads
static const int M = 10000;                      // Number of operations per thread


TEST(TestBenchmarkQueue, TestSeq) {
  puts("SEQUENTIAL TEST...");
  cdrc::weak_ptr_queue::atomic_queue<int> q;
  ASSERT_TRUE(!q.dequeue().has_value());   // Initially empty
  for (int i = 0; i < 1000; i++) {
    q.enqueue(i);
  }
  for (int i = 0; i < 1000; i++) {
    auto x = q.dequeue();
    ASSERT_TRUE(x.has_value());
    ASSERT_TRUE(x.value() == i);
  }
  ASSERT_TRUE(!q.dequeue().has_value());
  for (int i = 0; i < 1000; i++) {
    q.enqueue(i);
  }
  for (int i = 0; i < 1000; i++) {
    auto x = q.dequeue();
    ASSERT_TRUE(x.has_value());
    ASSERT_TRUE(x.value() == i);
  }
  ASSERT_TRUE(!q.dequeue().has_value());
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
TEST(TestBenchmarkQueue, TestPar) {
  puts("PARALLEL PRODUCER AND CONSUMER TEST...");

  cdrc::weak_ptr_queue::atomic_queue<int> q;
  std::atomic<size_t> done;
  std::vector<long long int> consumer_sums(N/2);
  std::vector<long long int> producer_sums(N/2);

  std::vector<thread> consumers;
  consumers.reserve(N/2);
  for (size_t i = 0; i < N/2; i++) {
    consumers.emplace_back([i, &q, &done, &consumer_sums] {
      long long int local_sum = 0;
      while (done.load() < N/2 || q.peek()) {
        auto val = q.dequeue();
        if(val) local_sum += val.value();
      }
      consumer_sums[i] = local_sum;
    });
  }

  std::vector<thread> producers;
  producers.reserve(N/2);
  for (size_t i = 0; i < N/2; i++) {
    producers.emplace_back([i, &q, &done, &producer_sums] {
      long long int local_sum = 0;
      for(int j = 0; j < M; j++) {
        q.enqueue(j);
        local_sum += j;
      }
      producer_sums[i] = local_sum;
      done.fetch_add(1);
    });
  }

  while (done.load() < N/2) {}

  long long actualsum = 0;
  long long checksum = 0;
  for (size_t i = 0; i < N/2; i++) {
    producers[i].join();
    consumers[i].join();
    actualsum += producer_sums[i];
    checksum += consumer_sums[i];
  }

  ASSERT_EQ(checksum, actualsum);
  puts("\tOK");
}

// Each thread both dequeues and enqueues
TEST(TestBenchmarkQueue, TestPar2) {
  puts("PARALLEL DEQUEUE+ENQUEUE TEST");

  // The queue contains one element per thread
  cdrc::weak_ptr_queue::atomic_queue<int> q;
  for (size_t t = 0; t < N; t++) {
    q.enqueue(t);
  }

  std::vector<long long int> sums(N);
  std::vector<std::thread> threads;
  threads.reserve(N);

  for (size_t t = 0; t < N; t++) {
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
