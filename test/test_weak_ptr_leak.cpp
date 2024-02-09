#include "gtest/gtest.h"

#include <cdrc/atomic_weak_ptr.h>
#include <cdrc/rc_ptr.h>
#include <cdrc/weak_ptr.h>

TEST(TestWeakPtrs, TestWeakPtrs) {
  cdrc::rc_ptr<int> x = cdrc::make_rc<int>(0);
  cdrc::weak_ptr<int> y = x;

  cdrc::rc_ptr<int> locked = y.lock();

  ASSERT_EQ(x.use_count(), 2);
  x = cdrc::make_rc<int>(1);
  locked = nullptr;
  ASSERT_EQ(y.use_count(), 0);
  locked = y.lock(); // extra increment weak happens here

  cdrc::atomic_weak_ptr<int> ap;
  ap.store(x);
  auto z = ap.load();
  auto locked2 = z.lock();

  // auto ss = ap.get_snapshot();
}
