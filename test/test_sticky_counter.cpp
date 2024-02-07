#include "gtest/gtest.h"
#include <cdrc/internal/utils.h>

using namespace cdrc::utils;

TEST(TestStickyCounter, Idk) {
  StickyCounter<uint32_t> counter(0);
  ASSERT_FALSE(counter.increment(1));// return false because the counter was 0
}