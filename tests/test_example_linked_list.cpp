
#include <assert.h>
#include <vector>
#include <algorithm>
#include <thread>

#include "../examples/linked_list.h"
#include "../benchmarks/barrier.hpp"

using namespace cdrc;
using namespace std;

int NUM_THREADS = utils::num_threads()-1;

void test_simple() {
  atomic_linked_list set;
  assert(!set.find(2));
  assert(!set.remove(0));
  assert(!set.find(2));
  assert(set.insert(2));
  assert(set.find(2));
  assert(!set.find(3));
  assert(set.insert(3));
  // std::cout << set.size() << std::endl;
  assert(set.find(3));
  assert(!set.find(1));
  assert(set.insert(1));
  assert(set.find(1));
  assert(set.find(2));
  assert(set.find(3));
  assert(!set.find(4));
  assert(set.remove(2));
  assert(!set.find(2));
  assert(set.find(3));
  assert(set.find(1));
  assert(!set.find(4));
}

void stress_test(int num_iter) {
  atomic_linked_list set;
  
  vector<int> keys;
  for(int i = 0; i < num_iter; i++)
    keys.push_back(i);
  std::random_shuffle ( keys.begin(), keys.end() );

  Barrier barrier(NUM_THREADS);

  vector<thread> threads;
  for(int p = 0; p < NUM_THREADS; p++) {
    threads.emplace_back([p, &set, &keys, &barrier, &num_iter] () {
      for(int i = p; i < num_iter; i+=NUM_THREADS)
        assert(set.insert(keys[i]));
      barrier.wait();
      for(int i = p; i < num_iter; i+=NUM_THREADS)
        assert(set.remove(i));      
    });        
  }
  for (auto& t : threads) t.join(); 
}

void test_concurrent_delete() {
  atomic_linked_list set;
  int num_iter = 1000;
  
  long long expected_sum = 0;
  atomic<long long> actual_sum = 0;
  for(int i = 0; i < num_iter; i++) {
    assert(set.insert(i));
    expected_sum += i;
  }

  Barrier barrier(NUM_THREADS);
  atomic<int> remove_next = 0;

  vector<thread> threads;
  for(int p = 0; p < NUM_THREADS; p++) {
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

  assert(actual_sum == expected_sum);
}

void run_all_tests(int num_iter) {
  test_simple();
  stress_test(num_iter);
  test_concurrent_delete();  
}

int main() {
  run_all_tests(4000);
  return 0;
}
