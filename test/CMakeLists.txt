include(dtest)

# Benchmarks Data Structures

add_dtests(NAME test_benchmark_queue FILES test_benchmark_queue.cpp LIBS cdrc)
add_dtests(NAME test_benchmark_stack FILES test_benchmark_stack.cpp LIBS cdrc)

# Example Data Structures

add_dtests(NAME test_example_linked_list FILES test_example_linked_list.cpp LIBS cdrc)
add_dtests(NAME test_example_stack FILES test_example_stack.cpp LIBS cdrc)

# Idk

add_dtests(NAME test_weak_ptrs FILES test_weak_ptrs.cpp LIBS cdrc)

# Known Bugs

add_dtests(NAME test_bug FILES test_bug.cpp LIBS cdrc)

# Internals

add_dtests(NAME test_sticky_counter FILES test_sticky_counter.cpp LIBS cdrc)
add_dtests(NAME test_weak_ptr_mini FILES test_weak_ptr_mini.cpp LIBS cdrc)
add_dtests(NAME test_weak_ptr_leak FILES test_weak_ptr_leak.cpp LIBS cdrc)

# Temporaily Disabled Folly Tests

# add_dtests(NAME test_folly FILES test_folly.cpp LIBS cdrc) # Folly
# add_dtests(NAME test_hazptr_stack FILES test_hazptr_stack.cpp LIBS cdrc) # Folly
# add_dtests(NAME test_rcu_stack FILES test_rcu_stack.cpp LIBS cdrc) # Folly
# add_dtests(NAME test_jss_arc FILES test_jss_arc.cpp LIBS cdrc) # experimental/atomic
# add_dtests(NAME test_rc_rideables FILES test_rc_rideables.cpp LIBS cdrc) # rideables
