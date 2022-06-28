
#ifndef ARC_FWD_DECL_H
#define ARC_FWD_DECL_H

#include "smr/acquire_retire.h"
// #include "smr/acquire_retire_ebr.h"
// #include "smr/acquire_retire_ibr.h"

namespace cdrc {

namespace internal {

// Blank policy that adds nothing
class default_pointer_policy {
 public:
  template<typename T>
  using pointer_type = std::add_pointer_t<T>;

  template<typename T>
  class arc_ptr_policy { };

  template<typename T>
  class rc_ptr_policy { };

  template<typename T>
  class snapshot_ptr_policy { };
};

template<typename T>
using default_memory_manager = internal::acquire_retire<T>;

}  // namespace internal

template<typename T, typename memory_manager = internal::default_memory_manager<T>, typename pointer_policy = internal::default_pointer_policy>
class atomic_rc_ptr;

template<typename T, typename memory_manager = internal::default_memory_manager<T>, typename pointer_policy = internal::default_pointer_policy>
class rc_ptr;

template<typename T, typename memory_manager = internal::default_memory_manager<T>, typename pointer_policy = internal::default_pointer_policy>
class snapshot_ptr;

template<typename T, typename memory_manager = internal::default_memory_manager<T>, typename pointer_policy = internal::default_pointer_policy>
class atomic_weak_ptr;

template<typename T, typename memory_manager = internal::default_memory_manager<T>, typename pointer_policy = internal::default_pointer_policy>
class weak_ptr;

template<typename T, typename memory_manager = internal::default_memory_manager<T>, typename pointer_policy = internal::default_pointer_policy>
class weak_snapshot_ptr;

}  // namespace cdrc

#endif //ARC_FWD_DECL_H
