
#include <iostream>

#include "../benchmarks/external/atomic_shared_ptr/benchmark/external/anthonywilliams/atomic_shared_ptr"


int main() {

  jss::atomic_shared_ptr<int> asp(jss::make_shared<int>(42));

  auto sp = asp.load();
  auto sp2 = asp.load();

  std::cout << sp.use_count() << std::endl;


}
