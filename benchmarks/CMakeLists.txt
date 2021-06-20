include(CheckCXXCompilerFlag)

set(NUMA_COMMAND numactl -i all)

function(add_benchmark TARGET)
  add_executable(${TARGET} ${TARGET}.cpp)
  target_link_libraries(${TARGET} concurrent_deferred_rc)
  target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wfatal-errors)
  target_compile_features(${TARGET} PRIVATE cxx_std_20)

  check_cxx_compiler_flag("-fconcepts" SUPPORTS_CONCEPTS)
  if (SUPPORTS_CONCEPTS)
    target_compile_options(${TARGET} PRIVATE -fconcepts)
  endif()

  add_custom_target(numactl_${TARGET}
    COMMAND ${NUMA_COMMAND} ${CMAKE_CURRENT_BINARY_DIR}/${TARGET} -t 128 -a arc -r 5
  )
  add_dependencies(numactl_${TARGET} ${TARGET})
  message(STATUS "Added benchmark target " ${TARGET})
endfunction()

add_benchmark(bench_ref_count)
add_benchmark(bench_stack)


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
