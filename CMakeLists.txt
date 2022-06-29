# -------------------------------------------------------------------
#       Build system for Concurrent Deferred Reference Counting
# -------------------------------------------------------------------
# Requirements:
#   - CMake version 3.5
#   - G++-9 or a compatible C++ compiler
# -------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5)
project(CONCURRENT_DEFERRED_RC VERSION 2.0
        DESCRIPTION "Atomic reference counting for C++"
        LANGUAGES CXX)

# Set a default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

message(STATUS "--------------- General configuration -------------")
message(STATUS "CMake Generator:                ${CMAKE_GENERATOR}")
message(STATUS "Compiler:                       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type:                     ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_FLAGS:                ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG:          ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE:        ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS          ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_INSTALL_PREFIX:           ${CMAKE_INSTALL_PREFIX}" )

# Make sure -fno-omit-frame-pointer is set for profiling
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")

# -------------------------------------------------------------------
#                       Library definition

add_library(concurrent_deferred_rc INTERFACE)
set(CONCURRENT_DEFERRED_RC_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
target_include_directories(concurrent_deferred_rc INTERFACE ${CONCURRENT_DEFERRED_RC_INCLUDE_DIR})
target_compile_features(concurrent_deferred_rc INTERFACE cxx_std_20)

# Link against system threads
find_package(Threads REQUIRED)
target_link_libraries(concurrent_deferred_rc INTERFACE Threads::Threads)

if(NOT MSVC)
  # Enable 16 byte CAS
  target_compile_definitions(concurrent_deferred_rc INTERFACE -DMCX16)
  target_compile_options(concurrent_deferred_rc INTERFACE -mcx16)

  # The following might not be needed for 16-byte atomics
  target_compile_options(concurrent_deferred_rc INTERFACE "-march=native")
endif()

# Configure support for just::threads, if available
# Defines the flag JUST_THREADS_AVAILABLE in cmake,
# and the compile definition ARC_JUST_THREADS_AVAILABLE
# for C++ targets.
find_library(JUST_THREADS_LIB NAMES justthread)
find_path(JUST_THREADS_INCLUDE NAMES jss/experimental_concurrency.hpp PATH_SUFFIXES justthread)
if(NOT JUST_THREADS_LIB)
  message(STATUS "Could not find just::threads library. Certain targets will not be built")
elseif(NOT JUST_THREADS_INCLUDE)
  message(STATUS "Could not find just::threads includes. Certain targets will not be built")
else()
  message(STATUS "just::threads found -- lib: " ${JUST_THREADS_LIB} " include: " ${JUST_THREADS_INCLUDE})
  set(JUST_THREADS_AVAILABLE TRUE)
  add_library(jss INTERFACE)
  target_include_directories(jss INTERFACE ${JUST_THREADS_INCLUDE})
  target_link_libraries(jss INTERFACE ${JUST_THREADS_LIB} atomic)
  target_compile_features(jss INTERFACE cxx_std_17)
  target_compile_definitions(concurrent_deferred_rc INTERFACE -DARC_JUST_THREADS_AVAILABLE)
  target_link_libraries(concurrent_deferred_rc INTERFACE jss)
endif()

# Configure support for folly, if available
# Defines the flag FOLLY_AVAILABLE in cmake,
# and the compile definition ARC_FOLLY_AVAILABLE
# for C++ targets.
find_library(FOLLY_LIB NAMES folly)
find_path(FOLLY_INCLUDE NAMES folly/concurrency/AtomicSharedPtr.h PATH_SUFFIXES folly)
if(NOT FOLLY_LIB)
  message(STATUS "Could not find folly library. Certain targets will not be built")
elseif(NOT FOLLY_INCLUDE)
  message(STATUS "Could not find folly includes. Certain targets will not be built")
else()
  message(STATUS "folly found -- lib: " ${FOLLY_LIB} " include: " ${FOLLY_INCLUDE})
  set(FOLLY_AVAILABLE TRUE)
  add_library(fss INTERFACE)
  target_include_directories(fss INTERFACE ${FOLLY_INCLUDE})
  target_link_libraries(fss INTERFACE ${FOLLY_LIB} glog dl pthread double-conversion)
  target_compile_features(fss INTERFACE cxx_std_14)
  target_compile_definitions(concurrent_deferred_rc INTERFACE -DARC_FOLLY_AVAILABLE)
  target_link_libraries(concurrent_deferred_rc INTERFACE fss)
endif()

# -------------------------------------------------------------------
#                             Tests

enable_testing()
add_subdirectory(tests)

# -------------------------------------------------------------------
#                           Benchmarks

if(LINUX)
  add_subdirectory(benchmarks)
endif()

# -------------------------------------------------------------------
#                           Figures

if(LINUX)
  add_subdirectory(results)
endif()
