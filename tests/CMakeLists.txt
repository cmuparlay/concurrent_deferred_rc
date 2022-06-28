include(CheckCXXCompilerFlag)

function(add_my_test TARGET)
  add_executable(${TARGET} ${TARGET}.cpp)
  target_link_libraries(${TARGET} concurrent_deferred_rc)
  target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wfatal-errors -fsanitize=address,undefined)
  target_link_options(${TARGET} PRIVATE -fsanitize=address,undefined)

  check_cxx_compiler_flag("-fconcepts" SUPPORTS_CONCEPTS)
  if (SUPPORTS_CONCEPTS)
    target_compile_options(${TARGET} PRIVATE -fconcepts)
  endif()

  add_test(${TARGET} ${TARGET})
  message(STATUS "Added test target " ${TARGET})
endfunction()

function(add_memory_reclamation_test TARGET)
  add_executable(${TARGET} ${TARGET}.cpp)
  target_link_libraries(${TARGET} concurrent_deferred_rc)
  target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wfatal-errors -fsanitize=address,undefined)
  target_link_options(${TARGET} PRIVATE -fsanitize=address,undefined)
  target_include_directories(${TARGET} PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks/memory_reclamation/src")
  target_include_directories(${TARGET} PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks/memory_reclamation/ext/parharness")
  target_include_directories(${TARGET} PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks/memory_reclamation/src/trackers")

  check_cxx_compiler_flag("-fconcepts" SUPPORTS_CONCEPTS)
  if (SUPPORTS_CONCEPTS)
    target_compile_options(${TARGET} PRIVATE -fconcepts)
  endif()

  add_test(${TARGET} ${TARGET})
  message(STATUS "Added test target " ${TARGET})
endfunction()

# List all test cases
add_my_test(test_marked_ptrs)
add_my_test(test_stack)
add_my_test(test_example_linked_list)
add_my_test(test_example_stack)
add_my_test(test_weak_ptrs)
add_my_test(test_queue)

add_memory_reclamation_test(test_rc_ridables)

# Test folly
if(FOLLY_AVAILABLE)
  add_my_test(test_folly)
endif()

# Test just::threads if it is available
if(JUST_THREADS_AVAILABLE)
  add_my_test(test_jss_arc)
endif()

