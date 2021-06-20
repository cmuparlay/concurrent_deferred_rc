
#ifndef BARRIER_HPP_
#define BARRIER_HPP_

#include <atomic>

class Barrier {
private:
  std::atomic<int> count;

public:
  Barrier(int initial) : count(initial) {};
  void wait() {
    count--;
    while(count != 0) {};
  }
};

#endif /* BARRIER_HPP_ */