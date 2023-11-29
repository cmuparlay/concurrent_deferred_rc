#include "gtest/gtest.h"

#include <cdrc/atomic_weak_ptr.h>
#include <cdrc/rc_ptr.h>
#include <cdrc/weak_ptr.h>

TEST(TestWeakPtrs, TestWeakPtrs) {
  cdrc::rc_ptr<int> x = cdrc::make_rc<int>(0);
  cdrc::weak_ptr<int> y = x;

  ASSERT_NE(x , nullptr);
  ASSERT_TRUE(!y.expired());
  auto locked = y.lock();
  ASSERT_NE(locked , nullptr);
  ASSERT_EQ(locked.get(), x.get());

  x = cdrc::make_rc<int>(1);
  locked = nullptr;
  ASSERT_TRUE(y.expired());
  locked = y.lock();
  ASSERT_EQ(locked, nullptr);

  cdrc::atomic_weak_ptr<int> ap;
  ap.store(x);
  auto z = ap.load();
  ASSERT_TRUE(!z.expired());
  auto locked2 = z.lock();
  ASSERT_NE(locked2 , nullptr);
  ASSERT_EQ(locked2.get(), x.get());

  auto ss = ap.get_snapshot();
  ASSERT_NE(ss , nullptr);
  ASSERT_EQ(ss.get(), x.get());
}
