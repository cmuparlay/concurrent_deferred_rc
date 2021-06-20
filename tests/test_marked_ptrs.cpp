
#include <cdrc/marked_arc_ptr.h>

int main() {
  // Test marked pointers
  cdrc::marked_arc_ptr<int> p;
  p.store(cdrc::marked_rc_ptr<int>::make_shared(5));
  p.set_mark(1);
  assert(p.get_mark() == 1);
  assert(*p.load() == 5);

  auto ptr = p.load();
  assert(ptr.get_mark() == 1);
  assert(*ptr == 5);
  ptr.set_mark(0);
  assert(p.get_mark() == 1);
  assert(ptr.get_mark() == 0);
  assert(*ptr == 5);
  assert(!p.contains(ptr));
  ptr.set_mark(1);
  assert(p.contains(ptr));

  auto snapshot = p.get_snapshot();
  assert(snapshot.get_mark() == 1);
  assert(*snapshot == 5);
  snapshot.set_mark(0);
  assert(snapshot.get_mark() == 0);
  assert(*snapshot == 5);
  assert(!p.contains(snapshot));
  snapshot.set_mark(1);
  assert(p.contains(snapshot));
  
  assert(ptr.get() == snapshot.get());
}