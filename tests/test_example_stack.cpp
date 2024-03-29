#include <cassert>

#include <array>
#include <atomic>
#include <iostream>
#include <optional>
#include <thread>
#include <vector>

#include "../examples/stack.h"

using namespace cdrc;
using namespace std;

const int M = 1000;

void test_seq() {
  cdrc::atomic_stack<int> stack;
  assert(!stack.find(5));
  assert(!stack.front());
  assert(!stack.pop_front());
  stack.push_front(5);
  assert(stack.find(5));
  assert(stack.front().value() == 5);
  assert(stack.pop_front().value() == 5);
  assert(!stack.find(5));
  assert(!stack.front());
  assert(!stack.pop_front());
  assert(!stack.front());
  stack.push_front(5);
  stack.push_front(6);
  stack.push_front(7);
  assert(stack.find(6));
  assert(stack.find(7));
  assert(stack.front().value() == 7);
  assert(stack.pop_front().value() == 7);
  assert(stack.front().value() == 6);
  assert(stack.pop_front().value() == 6);
  assert(stack.front().value() == 5);
  assert(stack.pop_front().value() == 5);
  assert(!stack.front());
}

void test_par() {
  volatile long long sum = 0;
  volatile long long checksum1 = 0;
  volatile long long checksum2 = 0;
  volatile long long actualsum1 = 0;
  volatile long long actualsum2 = 0;

  atomic_stack<int> stack;
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

  assert(checksum1 + checksum2 == actualsum1 + actualsum2);

  cout << sum << endl;
}

void run_all_tests() {
  test_seq();
  test_par();  
}

int main () {
  run_all_tests();
}

  
