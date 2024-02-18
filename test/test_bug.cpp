#include "gtest/gtest.h"

#include <cdrc/atomic_rc_ptr.h>
#include <cdrc/atomic_weak_ptr.h>

#include <thread>

using namespace cdrc;
using namespace std;

std::atomic<bool> destroyed = false;

struct test {
  int x;
  ~test() { destroyed = true; }
};

void task1(cdrc::rc_ptr<test> x) {
  // ASSERT_EQ(x.use_count(), 1);
  x = nullptr;
}

void task2(cdrc::weak_ptr<test> y) {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  cdrc::rc_ptr<test> z = y.lock();
  // ASSERT_EQ(y.use_count(), 1);
  // ASSERT_EQ(y.weak_count(), 2);

  y=nullptr;
  z=nullptr;

  // ASSERT_TRUE(destroyed);
}

TEST(TestBug, Idk) {
  const size_t num_trials = 1;
  for (size_t trial = 0; trial < num_trials; ++trial) {
    cdrc::rc_ptr<test> x = cdrc::make_rc<test>(5);
    cdrc::weak_ptr<test> y = x;

    destroyed = false;

    jthread other_thread(task2, move(y));

    task1(move(x));
    ASSERT_EQ(x, nullptr);
  }

  ASSERT_TRUE(true);
}
