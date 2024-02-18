#include "gtest/gtest.h"

#include <cstddef>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include <rideables/SortedUnorderedMapRCSS.hpp>
#include <rideables/NatarajanTreeRCSS.hpp>
#include <rideables/LinkListRCSS.hpp>

#include <cdrc/internal/smr/acquire_retire.h>
#include <cdrc/internal/smr/acquire_retire_ibr.h>
#include <cdrc/internal/smr/acquire_retire_ebr.h>
#include <cdrc/internal/smr/acquire_retire_hyaline.h>

#include "../benchmarks/barrier.hpp"

using namespace std;

const size_t NUM_THREADS = cdrc::utils::num_threads() - 1;

template<class SetFactory>
void test_simple() {
  auto* setFactory = new SetFactory();
  auto* set = setFactory->build(nullptr);
  ASSERT_TRUE(!set->get(2, 0));
  ASSERT_EQ(set->size(), 0);
  ASSERT_TRUE(!set->remove(0, 0));
  ASSERT_EQ(set->size(), 0);
  ASSERT_TRUE(set->insert(2, 2, 0));
  // std::cout << set->size() << std::endl;
  ASSERT_EQ(set->size(), 1);
  ASSERT_TRUE(set->insert(3, 3, 0));
  // std::cout << set->size() << std::endl;
  ASSERT_EQ(set->size(), 2);
  ASSERT_TRUE(set->insert(1, 1, 0));
  ASSERT_EQ(set->size(), 3);
  ASSERT_EQ(set->keySum(), 6);
  ASSERT_TRUE(set->get(2, 0));
  ASSERT_TRUE(set->get(3, 0));
  ASSERT_TRUE(set->get(1, 0));
  ASSERT_TRUE(!set->get(4, 0));
  ASSERT_TRUE(set->remove(2, 0));
  ASSERT_EQ(set->size(), 2);
  ASSERT_EQ(!set->get(2, 0));
  ASSERT_TRUE(set->get(3, 0));
  ASSERT_TRUE(set->get(1, 0));
  ASSERT_TRUE(!set->get(4, 0));
  ASSERT_EQ(set->size(), 2);
  delete set;
  delete setFactory;
}

template<class SetFactory>
void stress_test(int num_iter) {
  auto* setFactory = new SetFactory();
  auto* set = setFactory->build(nullptr);
  
  vector<int> keys;
  for(int i = 0; i < num_iter; i++)
    keys.push_back(i);
  std::random_shuffle ( keys.begin(), keys.end() );

  Barrier barrier(NUM_THREADS);

  vector<thread> threads;
  for(size_t p = 0; p < NUM_THREADS; p++) {
    threads.emplace_back([p, &set, &keys, &barrier, &num_iter] () {
      for(int i = p; i < num_iter; i+=NUM_THREADS)
        ASSERT_EQ(set->insert(keys[i], i, p));
      barrier.wait();
      for(int i = p; i < num_iter; i+=NUM_THREADS)
        ASSERT_EQ(set->remove(i, p));      
    });        
  }
  for (auto& t : threads) t.join(); 

  delete set;
  delete setFactory;
}

template<class SetFactory>
void test_concurrent_delete() {
  auto* setFactory = new SetFactory();
  auto* set = setFactory->build(nullptr);
  int num_iter = 1000;
  
  long long expected_sum = 0;
  atomic<long long> actual_sum = 0;
  for(int i = 0; i < num_iter; i++) {
    ASSERT_EQ(set->insert(i, i, 0));
    expected_sum += i;
  }

  Barrier barrier(NUM_THREADS);
  atomic<int> remove_next = 0;

  vector<thread> threads;
  for(size_t p = 0; p < NUM_THREADS; p++) {
    threads.emplace_back([p, &set, &barrier, &remove_next, &actual_sum, &num_iter] () {
      barrier.wait();
      long long local_sum = 0;
      while(true) {
        int key = remove_next.fetch_add(1);
        if(key >= num_iter) break;
        if(set->remove(key, p)) local_sum += key;
      }    
      actual_sum += local_sum;
    });        
  }
  for (auto& t : threads) t.join(); 

  ASSERT_EQ(actual_sum, expected_sum);
  delete set;
  delete setFactory;
}

template<class SetFactory>
void run_all_tests(int num_iter) {
  test_simple<SetFactory>();
  stress_test<SetFactory>(num_iter);
  test_concurrent_delete<SetFactory>();  
}

template<typename T>
using hp = cdrc::internal::acquire_retire<T>;

template<typename T>
using ebr = cdrc::internal::acquire_retire_ebr<T>;

template<typename T>
using ibr = cdrc::internal::acquire_retire_ibr<T>;

template<typename T>
using hyaline = cdrc::internal::acquire_retire_hyaline<T>;

int main() {
  run_all_tests<LinkListRCSSFactory<int, int, hp>>(4000);
  run_all_tests<LinkListRCSSFactory<int, int, ebr, cdrc::epoch_guard>>(4000);
  run_all_tests<LinkListRCSSFactory<int, int, ibr, cdrc::epoch_guard>>(4000);
  run_all_tests<LinkListRCSSFactory<int, int, hyaline, cdrc::hyaline_guard>>(4000);

  run_all_tests<NatarajanTreeRCSSFactory<int, int, hp>>(100000);
  run_all_tests<NatarajanTreeRCSSFactory<int, int, ebr, cdrc::epoch_guard>>(100000);
  run_all_tests<NatarajanTreeRCSSFactory<int, int, ibr, cdrc::epoch_guard>>(100000);
  run_all_tests<NatarajanTreeRCSSFactory<int, int, hyaline, cdrc::hyaline_guard>>(100000);

  run_all_tests<SortedUnorderedMapRCSSTestFactory<int, int, hp>>(100000);
  run_all_tests<SortedUnorderedMapRCSSTestFactory<int, int, ebr, cdrc::epoch_guard>>(100000);
  run_all_tests<SortedUnorderedMapRCSSTestFactory<int, int, ibr, cdrc::epoch_guard>>(100000);
  run_all_tests<SortedUnorderedMapRCSSTestFactory<int, int, hyaline, cdrc::hyaline_guard>>(100000);
}

TEST(TestRcRidables, Port) {
  ASSERT_TRUE(false);
}
