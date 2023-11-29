#include "gtest/gtest.h"

#include <atomic>
#include <iostream>
#include <thread>

#include <cdrc/atomic_rc_ptr.h>

#include "../benchmarks/datastructures/stack.h"

using namespace std;

const int M = 10000;

template<typename T>
using atomic_sp_type = cdrc::atomic_rc_ptr<T>;

template<typename T>
using sp_type = cdrc::rc_ptr<T>;

TEST(TestBenchmarkStack, TestSeq) {
  atomic_stack<int, atomic_sp_type, sp_type> stack;
  ASSERT_TRUE(!stack.find(5));
  ASSERT_TRUE(!stack.front());
  ASSERT_TRUE(!stack.pop_front());
  stack.push_front(5);
  ASSERT_TRUE(stack.find(5));
  ASSERT_TRUE(stack.front().value() == 5);
  ASSERT_TRUE(stack.pop_front().value() == 5);
  ASSERT_TRUE(!stack.find(5));
  ASSERT_TRUE(!stack.front());
  ASSERT_TRUE(!stack.pop_front());
  ASSERT_TRUE(!stack.front());
  stack.push_front(5);
  stack.push_front(6);
  stack.push_front(7);
  ASSERT_TRUE(stack.find(6));
  ASSERT_TRUE(stack.find(7));
  ASSERT_EQ(stack.front().value(), 7);
  ASSERT_EQ(stack.pop_front().value(), 7);
  ASSERT_EQ(stack.front().value(), 6);
  ASSERT_EQ(stack.pop_front().value(), 6);
  ASSERT_EQ(stack.front().value(), 5);
  ASSERT_EQ(stack.pop_front().value(), 5);
  ASSERT_TRUE(!stack.front());
}

TEST(TestBenchmarkStack, TestPar) {
  volatile long long sum = 0;
  volatile long long checksum1 = 0;
  volatile long long checksum2 = 0;
  volatile long long actualsum1 = 0;
  volatile long long actualsum2 = 0;

  atomic_stack<int, atomic_sp_type, sp_type> stack;
  atomic<bool> done1 = false;
  atomic<bool> done2 = false;

  thread pusher1 ([&] () {
    for(int i = 0; i < M; i++) {
      stack.push_front(i);
      actualsum1 = actualsum1 + i;
    }
    done1 = true;
  });

  thread pusher2 ([&] () {
    for(int i = 0; i < M; i++) {
      stack.push_front(i);
      actualsum2 = actualsum2 + i;
    }
    done2 = true;
  });

  thread popper1 ([&] () {
    while(!done1 || !done2 || stack.front()) {
      auto val = stack.pop_front();
      if(val) checksum1 = checksum1 + val.value();
    }
  });

  thread popper2 ([&] () {
    while(!done1 || !done2 || stack.front()) {
      auto val = stack.pop_front();
      if(val) checksum2 = checksum2 + val.value();
    }
  });

  while(!done1 || !done2) {
    auto v = stack.front();
    if(v)
      sum = sum + v.value();
    if(stack.find(4))
      sum = sum + 1;
  }

  pusher1.join();
  pusher2.join();

  popper1.join();
  popper2.join();

  ASSERT_EQ(stack.size(), 0);
  ASSERT_EQ(checksum1 + checksum2, actualsum1 + actualsum2);

  cout << sum << endl;
}
