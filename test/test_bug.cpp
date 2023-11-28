#include <cdrc/marked_arc_ptr.h>
#define fakeassert(X) if (!(X)) { return 1; }

using namespace cdrc;
using namespace std;

int reproduce_bug() {
  fakeassert(false);
  thread first_thread([&]() {

  });

  thread second_thread([&]() {

  });

  first_thread.join();
  second_thread.join();
  return 0;
}

int run_all_tests() {
  fakeassert(reproduce_bug());
  return 0;
}

int main() { 
  return 0;
}