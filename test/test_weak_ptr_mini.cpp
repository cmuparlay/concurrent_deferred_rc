#include "gtest/gtest.h"

#include <cdrc/atomic_weak_ptr.h>
#include <cdrc/rc_ptr.h>
#include <cdrc/weak_ptr.h>

TEST(TestWeakPtrsMini, bug) {
  cdrc::rc_ptr<int> x = cdrc::make_rc<int>(0);
  cdrc::weak_ptr<int> y = x;

  cdrc::atomic_weak_ptr<int> ap;
  ap.store(std::move(x));
  // auto ss = ap.get_snapshot();
}
