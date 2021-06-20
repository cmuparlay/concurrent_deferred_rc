
#ifndef WEAK_ATOMIC_H
#define WEAK_ATOMIC_H

#include <thread>
#include <iostream>
#include <vector>
#include <unordered_set>
#include <experimental/optional>
#include <experimental/algorithm>
#include <atomic>
#include <stdio.h>
#include <string.h>

// #include "weak_atomic_large.hpp"
#include "weak_atomic_small.hpp"
#include "acquire_retire_lockfree.hpp"

/*
  ASSUMPTIONS:
    - T is 8 bytes or 16 bytes
    - T must be trivially move constructable
    - There exists an "empty" instantiation of T that
      is trivial to construct and destruct
*/

template <typename T, template<typename, typename> typename AcquireRetire = AcquireRetireLockfree>
using weak_atomic = weak_atomic_small<T, AcquireRetire>;
// using weak_atomic = typename std::conditional<(sizeof(T) <= 8), 
//                                       weak_atomic_small<T, AcquireRetire>,
//                                       weak_atomic_large<T, AcquireRetire> >::type;

#endif // WEAK_ATOMIC_H
