
#ifndef ARC_FWD_DECL_H
#define ARC_FWD_DECL_H

namespace cdrc {

namespace internal {

// Blank policy that adds nothing
class default_pointer_policy {
 public:
  template<typename T>
  class arc_ptr_policy {
  };

  template<typename T>
  class rc_ptr_policy {
  };

  template<typename T>
  class snapshot_ptr_policy {
  };
};

template<typename T>
struct counted_object;

}  // namespace internal

template<typename T, template<typename> typename pointer_type = std::add_pointer_t,
    typename pointer_policy = internal::default_pointer_policy>
class atomic_rc_ptr;

template<typename T, template<typename> typename pointer_type = std::add_pointer_t,
    typename pointer_policy = internal::default_pointer_policy>
class rc_ptr;

template<typename T, template<typename> typename pointer_type = std::add_pointer_t,
    typename pointer_policy = internal::default_pointer_policy>
class snapshot_ptr;

}  // namespace cdrc

#endif //ARC_FWD_DECL_H
