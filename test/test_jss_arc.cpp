#include "gtest/gtest.h"

#include <thread>
#include <iostream>
#include <fstream>
#include <memory>
#include <atomic>

// Just::threads
#include <experimental/atomic>

using namespace std;

struct node {
  int key, val;
  atomic<int> count;
  node *left, *right;

  node() : key(0), val(0), count(0), left(nullptr), right(nullptr) {};
  node(int _key, int _val) : key(_key), val(_val), count(0), left(nullptr), right(nullptr) {};
};

const int M = 50000;

// two readers one updater
void stress_test1() {
  experimental::atomic_shared_ptr<node> atomic_ptr(experimental::shared_ptr<node>(new node(0, 0)));
  atomic<long long int> sum(0);
  
  thread other1 ([&] () {
    thread other2 ([&] () {
        for(int i = 0; i < M; i++) {
          experimental::shared_ptr<node> n(atomic_ptr.load());
          sum += n.get()->key;
        }
    });

    for(int i = 0; i < M; i++) {
      experimental::shared_ptr<node> n(atomic_ptr.load());
      sum += n.get()->key;
    }
    other2.join();
  });

  for(int i = 0; i < M; i++) {
    experimental::shared_ptr<node> new_node(new node(i, i));
    atomic_ptr.store(new_node); 
  }
  other1.join();

  cout << sum << endl;
}

// one writer, one reader, and one CASer
void stress_test2() {
  experimental::atomic_shared_ptr<node> atomic_ptr(experimental::shared_ptr<node>(new node(0, 0)));
  atomic<long long int> sum(0);

  thread other1 ([&] () {
    thread other2 ([&] () {
        for(int i = 0; i < M; i++) {
          sum += atomic_ptr.load().get()->key;
        }
    });

    for(int i = 0; i < M; i++) {
      atomic_ptr.store(experimental::shared_ptr<node>(new node(i, i))); 
    }
    other2.join();
  });

  for(int i = 0; i < M; i++) {
    atomic_ptr.store(experimental::shared_ptr<node>(new node(i, i))); 
  }
  other1.join();

  cout << sum << endl;
}


int main () {
  cout << "Testing JSS Atomic Shared Pointer" << endl;

  experimental::shared_ptr<node> p1(new node(1, 2));
  experimental::shared_ptr<node> p2(new node(3, 4));
  experimental::shared_ptr<node> p3(std::move(p1));
  experimental::shared_ptr<node> p4(p2);
  
  p2 = p3;

  cout << p1.get() << endl;
  cout << p2.get()->key << endl;
  cout << p3.get()->key << endl;
  cout << p4.get()->key << endl;

  experimental::atomic_shared_ptr<node> atomic_ptr(experimental::shared_ptr<node>(new node(5, 6)));
  thread other ([&] () {
      experimental::shared_ptr<node> a1(atomic_ptr.load());
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      experimental::shared_ptr<node> a2(atomic_ptr.load());
      cout << "a1 key = " << a1.get()->key << endl;
      if(a2.get() != nullptr)
        cout << "a2 key = " << a2.get()->key << endl;
  });
  experimental::shared_ptr<node> b1(atomic_ptr.load());
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  atomic_ptr.compare_exchange_strong(b1, p1);
  atomic_ptr.compare_exchange_strong(p1, p4);
  other.join();
  cout << atomic_ptr.load().get()->key << endl;

  stress_test1();
  stress_test2();
}

TEST(TEstJssArc, Port) {
  ASSERT_EQ(false);
}
  
