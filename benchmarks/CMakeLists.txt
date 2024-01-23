include(CheckCXXCompilerFlag)

set(NUMA_COMMAND numactl -i all)

# Link boost::program_options
find_package(Boost 1.40 COMPONENTS program_options REQUIRED)

function(add_benchmark TARGET)
  add_executable(${TARGET} ${TARGET}.cpp)
  target_link_libraries(${TARGET} cdrc)
  target_link_libraries(${TARGET} Boost::program_options)
  target_compile_features(${TARGET} PRIVATE cxx_std_20)

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

  message(STATUS "Added benchmark target " ${TARGET})
endfunction()

add_benchmark(bench_ref_count)
add_benchmark(bench_stack)
add_benchmark(bench_queue)

# -------------------------------------------------------------------
#          External Benchmarks (from the IBR/WFE benchmark suite)

# Build the memory reclamation benchmark suite from its own Makefile
# since it does not come with a CMake configuration.
include(ExternalProject)
ExternalProject_Add(
  memory_reclamation
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/memory_reclamation
  BUILD_IN_SOURCE TRUE
  CONFIGURE_COMMAND make clean
  INSTALL_COMMAND ""
)
