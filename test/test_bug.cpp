#include "gtest/gtest.h"

#include <cdrc/marked_arc_ptr.h>

using namespace cdrc;
using namespace std;

TEST(TestBug, Idk) {
  thread first_thread([&]() {

  });

  thread second_thread([&]() {

  });

  first_thread.join();
  second_thread.join();

  ASSERT_TRUE(true);
}
