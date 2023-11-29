#include "gtest/gtest.h"

#include <cdrc/marked_arc_ptr.h>

TEST(TestMarkedPtrs, TestMarkedPtrs) {
  // Test marked pointers
  {
    cdrc::marked_arc_ptr<int> p;
    p.store(cdrc::marked_rc_ptr<int>::make_shared(5));
    p.set_mark(1);
    ASSERT_EQ(p.get_mark(), 1);
    ASSERT_EQ(*p.load(), 5);

    auto ptr = p.load();
    ASSERT_EQ(ptr.get_mark(), 1);
    ASSERT_EQ(*ptr, 5);
    ptr.set_mark(0);
    ASSERT_EQ(p.get_mark(), 1);
    ASSERT_EQ(ptr.get_mark(), 0);
    ASSERT_EQ(*ptr, 5);
    ASSERT_TRUE(!p.contains(ptr));
    ptr.set_mark(1);
    ASSERT_TRUE(p.contains(ptr));

    auto snapshot = p.get_snapshot();
    ASSERT_EQ(snapshot.get_mark(), 1);
    ASSERT_EQ(*snapshot, 5);
    snapshot.set_mark(0);
    ASSERT_EQ(snapshot.get_mark(), 0);
    ASSERT_EQ(*snapshot, 5);
    ASSERT_TRUE(!p.contains(snapshot));
    snapshot.set_mark(1);
    ASSERT_TRUE(p.contains(snapshot));

    ASSERT_EQ(ptr.get(), snapshot.get());
  }
}


TEST(TestMarkedPtrs, TestMarkedWeakPtrs) {
  // Test marked weak pointers
  {
    cdrc::marked_arc_ptr<int> p1;
    p1.store(cdrc::marked_rc_ptr<int>::make_shared(5));

    cdrc::marked_aw_ptr<int> p;
    p.store(cdrc::marked_weak_ptr<int>(p1));
    p.set_mark(1);
    ASSERT_EQ(p.get_mark(), 1);
    ASSERT_EQ(*(p.load().lock()), 5);

    auto ptr = p.load();
    ASSERT_EQ(ptr.get_mark(), 1);
    ASSERT_EQ(*(ptr.lock()), 5);
    ptr.set_mark(0);
    ASSERT_EQ(p.get_mark(), 1);
    ASSERT_EQ(ptr.get_mark(), 0);
    ASSERT_EQ(*(ptr.lock()), 5);
    ASSERT_TRUE(!p.contains(ptr));
    ptr.set_mark(1);
    ASSERT_TRUE(p.contains(ptr));

    auto snapshot = p.get_snapshot();
    ASSERT_EQ(snapshot.get_mark(), 1);
    ASSERT_EQ(*snapshot, 5);
    snapshot.set_mark(0);
    ASSERT_EQ(snapshot.get_mark(), 0);
    ASSERT_EQ(*snapshot, 5);
    ASSERT_TRUE(!p.contains(snapshot));
    snapshot.set_mark(1);
    ASSERT_TRUE(p.contains(snapshot));

    ASSERT_EQ(ptr.lock().get(), snapshot.get());
  }
}
