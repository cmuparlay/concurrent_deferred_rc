include(dtest)

# ------------------------------ Range and Slice -------------------------------

add_dtests(NAME test_benchmark_queue FILES test_benchmark_queue.cpp LIBS cdrc)
add_dtests(NAME test_benchmark_stack FILES test_benchmark_stack.cpp LIBS cdrc)