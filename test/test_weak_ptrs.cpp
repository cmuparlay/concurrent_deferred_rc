#include <cassert>

#include <cdrc/atomic_weak_ptr.h>
#include <cdrc/rc_ptr.h>
#include <cdrc/weak_ptr.h>

int main() {
  cdrc::rc_ptr<int> x = cdrc::make_rc<int>(0);
  cdrc::weak_ptr<int> y = x;

  assert(x != nullptr);
  assert(!y.expired());
  auto locked = y.lock();
  assert(locked != nullptr);
  assert(locked.get() == x.get());

  x = cdrc::make_rc<int>(1);
  locked = nullptr;
  assert(y.expired());
  locked = y.lock();
  assert(locked == nullptr);

  cdrc::atomic_weak_ptr<int> ap;
  ap.store(x);
  auto z = ap.load();
  assert(!z.expired());
  auto locked2 = z.lock();
  assert(locked2 != nullptr);
  assert(locked2.get() == x.get());

  auto ss = ap.get_snapshot();
  assert(ss != nullptr);
  assert(ss.get() == x.get());
}
