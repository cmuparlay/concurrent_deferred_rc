#include "gtest/gtest.h"
#include <cdrc/internal/utils.h>

using namespace cdrc::utils;

TEST(TestStickyCounter, Idk) {
  StickyCounter<uint32_t> counter(0);
  ASSERT_FALSE(counter.increment(1));// return false because the counter incremented from 0

  StickyCounter<uint32_t> counter2(1);
  ASSERT_TRUE(counter2.decrement(1));// return true because the decremented to 0
}
