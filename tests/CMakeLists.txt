include(CheckCXXCompilerFlag)

function(add_my_test TARGET)
  add_executable(${TARGET} ${TARGET}.cpp)
  target_link_libraries(${TARGET} concurrent_deferred_rc)

  # Support earlier versions of GCC with experimental concepts
  check_cxx_compiler_flag("-fconcepts" SUPPORTS_CONCEPTS)
  if (SUPPORTS_CONCEPTS)
    target_compile_options(${TARGET} PRIVATE -fconcepts)
  endif()

  if(NOT MSVC)
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wfatal-errors)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(${TARGET} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${TARGET} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    endif (CMAKE_BUILD_TYPE STREQUAL "Debug")
  endif()

  add_test(${TARGET} ${TARGET})
  message(STATUS "Added test target " ${TARGET})
endfunction()

function(add_memory_reclamation_test TARGET)
  add_executable(${TARGET} ${TARGET}.cpp)
  target_link_libraries(${TARGET} concurrent_deferred_rc)
  target_include_directories(${TARGET} PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks/memory_reclamation/src")
  target_include_directories(${TARGET} PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks/memory_reclamation/ext/parharness")
  target_include_directories(${TARGET} PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks/memory_reclamation/src/trackers")

  if(NOT MSVC)
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wfatal-errors)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(${TARGET} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${TARGET} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    endif (CMAKE_BUILD_TYPE STREQUAL "Debug")
  endif()

  add_test(${TARGET} ${TARGET})
  message(STATUS "Added test target " ${TARGET})
endfunction()

# List all test cases
add_my_test(test_marked_ptrs)
add_my_test(test_example_linked_list)
add_my_test(test_example_stack)
add_my_test(test_weak_ptrs)

# The benchmarks only work on Linux
if(LINUX)
  add_my_test(test_benchmark_stack)
  add_my_test(test_benchmark_queue)
  add_memory_reclamation_test(test_rc_ridables)
endif()

# Test folly
if(FOLLY_AVAILABLE)
  add_my_test(test_folly)
endif()

# Test just::threads if it is available
if(JUST_THREADS_AVAILABLE)
  add_my_test(test_jss_arc)
endif()
