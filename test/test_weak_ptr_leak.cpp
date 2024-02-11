#include "gtest/gtest.h"

#include <cdrc/atomic_weak_ptr.h>
#include <cdrc/rc_ptr.h>
#include <cdrc/weak_ptr.h>

TEST(TestWeakPtrLeak, Leak) {
  cdrc::rc_ptr<int> x = cdrc::make_rc<int>(0);
  ASSERT_EQ(x.use_count(), 1);
  ASSERT_EQ(x.weak_count(), 0);

  cdrc::weak_ptr<int> y = x;
  ASSERT_EQ(x.use_count(), 1);
  ASSERT_EQ(x.weak_count(), 1);

  cdrc::rc_ptr<int> locked = y.lock();
  ASSERT_EQ(x.use_count(), 2);
  ASSERT_EQ(x.weak_count(), 1);

  x = cdrc::make_rc<int>(1);
  ASSERT_EQ(x.use_count(), 1);
  ASSERT_EQ(x.weak_count(), 0);
  ASSERT_EQ(locked.use_count(), 1);
  ASSERT_EQ(locked.weak_count(), 1);
  ASSERT_EQ(y.use_count(), 1);
  ASSERT_EQ(y.weak_count(), 2);

  locked = nullptr;
  ASSERT_EQ(y.use_count(), 0);
  ASSERT_EQ(y.weak_count(), 2);

  locked = y.lock(); // extra increment weak happens here
  ASSERT_EQ(locked.use_count(), 0);
  ASSERT_EQ(locked.weak_count(), 0);
  ASSERT_EQ(y.use_count(), 0);
  ASSERT_EQ(y.weak_count(), 2);

  cdrc::atomic_weak_ptr<int> ap;
  ap.store(x);
  ASSERT_EQ(x.use_count(), 1);
  ASSERT_EQ(x.weak_count(), 1);

  cdrc::weak_ptr<int> z = ap.load();
  ASSERT_EQ(z.use_count(), 1);
  ASSERT_EQ(z.weak_count(), 3);

  cdrc::rc_ptr<int> locked2 = z.lock();
  ASSERT_EQ(locked2.use_count(), 2);
  ASSERT_EQ(locked2.weak_count(), 2);

  // auto ss = ap.get_snapshot();
}
