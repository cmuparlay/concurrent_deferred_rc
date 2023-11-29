#include "gtest/gtest.h"

#include <atomic>
#include <algorithm>
#include <random>
#include <vector>
#include <thread>

#include <cdrc/internal/utils.h>

#include "../examples/linked_list.h"
#include "../benchmarks/barrier.hpp"

using namespace std;

const size_t NUM_THREADS = cdrc::utils::num_threads() - 1;

TEST(TestExampleLinkedList, TestSimple) {
  cdrc::atomic_linked_list set;
  ASSERT_TRUE(!set.find(2));
  ASSERT_TRUE(!set.remove(0));
  ASSERT_TRUE(!set.find(2));
  ASSERT_TRUE(set.insert(2));
  ASSERT_TRUE(set.find(2));
  ASSERT_TRUE(!set.find(3));
  ASSERT_TRUE(set.insert(3));
  // std::cout << set.size() << std::endl;
  ASSERT_TRUE(set.find(3));
  ASSERT_TRUE(!set.find(1));
  ASSERT_TRUE(set.insert(1));
  ASSERT_TRUE(set.find(1));
  ASSERT_TRUE(set.find(2));
  ASSERT_TRUE(set.find(3));
  ASSERT_TRUE(!set.find(4));
  ASSERT_TRUE(set.remove(2));
  ASSERT_TRUE(!set.find(2));
  ASSERT_TRUE(set.find(3));
  ASSERT_TRUE(set.find(1));
  ASSERT_TRUE(!set.find(4));
}

TEST(TestExampleLinkedList, StressTest) {
  const int num_iter = 4000;

  cdrc::atomic_linked_list set;
  
  vector<int> keys;
  for(int i = 0; i < num_iter; i++)
    keys.push_back(i);

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle ( keys.begin(), keys.end(), g );

  Barrier barrier(NUM_THREADS);

  vector<thread> threads;
  for(size_t p = 0; p < NUM_THREADS; p++) {
    threads.emplace_back([p, &set, &keys, &barrier, &num_iter] () {
      for(int i = p; i < num_iter; i+=NUM_THREADS)
        ASSERT_TRUE(set.insert(keys[i]));
      barrier.wait();
      for(int i = p; i < num_iter; i+=NUM_THREADS)
        ASSERT_TRUE(set.remove(i));      
    });        
  }
  for (auto& t : threads) t.join(); 
}

TEST(TestExampleLinkedList, TestConcurrentDelete) {
  cdrc::atomic_linked_list set;
  int num_iter = 1000;
  
  long long expected_sum = 0;
  atomic<long long> actual_sum = 0;
  for(int i = 0; i < num_iter; i++) {
    ASSERT_TRUE(set.insert(i));
    expected_sum += i;
  }

  Barrier barrier(NUM_THREADS);
  atomic<int> remove_next = 0;

  vector<thread> threads;
  for(size_t p = 0; p < NUM_THREADS; p++) {
    threads.emplace_back([&set, &barrier, &remove_next, &actual_sum, &num_iter] () {
      barrier.wait();
      long long local_sum = 0;
      while(true) {
        int key = remove_next.fetch_add(1);
        if(key >= num_iter) break;
        if(set.remove(key)) local_sum += key;
      }    
      actual_sum += local_sum;
    });        
  }
  for (auto& t : threads) t.join(); 

  ASSERT_EQ(actual_sum, expected_sum);
}
