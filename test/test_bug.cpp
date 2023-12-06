#include "gtest/gtest.h"

#include <cdrc/atomic_rc_ptr.h>
#include <cdrc/atomic_weak_ptr.h>

#include <thread>

using namespace cdrc;
using namespace std;

void task1(cdrc::rc_ptr<int> &x) {
  sleep(0);
  x = nullptr;
}

void task2(cdrc::weak_ptr<int> y) {
  cdrc::rc_ptr<int> z = y.lock();
}

TEST(TestBug, Idk) {
  const size_t num_trials = 10000;
  for (size_t trial = 0; trial < num_trials; ++trial) {
    cdrc::rc_ptr<int> x = cdrc::make_rc<int>(5);
    cdrc::weak_ptr<int> y = x;

    jthread other_thread(task2, move(y));

    task1(x);
  }

  ASSERT_TRUE(true);
}
