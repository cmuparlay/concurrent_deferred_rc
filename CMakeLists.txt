# -------------------------------------------------------------------
#       Build system for Concurrent Deferred Reference Counting
# -------------------------------------------------------------------
# Requirements:
#   - CMake version 3.14+
# -------------------------------------------------------------------

cmake_minimum_required(VERSION 3.14)
project(CDRC VERSION 0.2.0
        DESCRIPTION "Atomic reference counting for C++"
        LANGUAGES CXX)

include(CheckCXXCompilerFlag)
include(GNUInstallDirs)

# Set a default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug Release RelWithDebInfo MinSizeRel)" FORCE)
  message(STATUS "No build type specified. Defaulted to DEBUG.")
  message(STATUS "To specify a build type, add -DCMAKE_BUILD_TYPE=<DEBUG/RELEASE/RELWITHDEBINFO/MINSIZEREL>")
endif(NOT CMAKE_BUILD_TYPE)

# Make sure -fno-omit-frame-pointer is set for profiling
# if(NOT MSVC)
#   set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")
# else()
#   set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Oy-")
# endif()

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb")

message(STATUS "CDRC VERSION ${CDRC_VERSION}")
message(STATUS "---------------------------- General configuration -----------------------------")
message(STATUS "CMake Generator:                ${CMAKE_GENERATOR}")
message(STATUS "Compiler:                       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type:                     ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_FLAGS:                ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG:          ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE:        ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS          ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_INSTALL_PREFIX:           ${CMAKE_INSTALL_PREFIX}" )

# Set module path
list(APPEND CMAKE_MODULE_PATH "${CDRC_SOURCE_DIR}/cmake")

# -------------------------------------------------------------------
#                       Library definition

add_library(cdrc INTERFACE)
set(CDRC_INCLUDE_DIR "${CDRC_SOURCE_DIR}/include")
target_include_directories(cdrc INTERFACE
  $<BUILD_INTERFACE:${CDRC_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(cdrc INTERFACE cxx_std_20)

# -------------------------------------------------------------------
#                         Installation

install(TARGETS cdrc
        EXPORT cdrc_Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        
include(CMakePackageConfigHelpers)
write_basic_package_version_file("CdrcConfigVersion.cmake"
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY SameMajorVersion)
                                 
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/CdrcConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/CdrcConfig.cmake"
  INSTALL_DESTINATION
  ${CMAKE_INSTALL_DATAROOTDIR}/cdrc/cmake)

install(EXPORT cdrc_Targets
        FILE CdrcTargets.cmake
        NAMESPACE Cdrc::
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cdrc/cmake)
        
install(FILES "${PROJECT_BINARY_DIR}/CdrcConfig.cmake"
              "${PROJECT_BINARY_DIR}/CdrcConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cdrc/cmake)
        
install(DIRECTORY ${CDRC_INCLUDE_DIR}/cdrc DESTINATION include)

message(STATUS "---------------------------------- Unit Tests ----------------------------------")

# Link against system threads
find_package(Threads REQUIRED)
target_link_libraries(cdrc INTERFACE Threads::Threads)

if(LINUX)
  # Enable 16 byte CAS
  target_compile_definitions(cdrc INTERFACE -DMCX16)
  target_compile_options(cdrc INTERFACE -mcx16)

  # The following might not be needed for 16-byte atomics
  target_compile_options(cdrc INTERFACE "-march=native")
endif()

# User option to build unit tests
option(CDRC_TEST  "Build unit tests"       ON)

if (CDRC_TEST)
  # Set CMake options for GoogleTest
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)

  # Download and configure GoogleTest
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY  https://github.com/google/googletest.git
    GIT_TAG         main
  )
  FetchContent_GetProperties(googletest)
  if(NOT googletest_POPULATED)
    message(STATUS "testing: Configuring GoogleTest")
    FetchContent_Populate(googletest)  
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "")
    add_subdirectory(${googletest_SOURCE_DIR}
                     ${googletest_BINARY_DIR}
                     EXCLUDE_FROM_ALL)
  endif()
  # Include test targets
  message(STATUS "testing: Enabled")
  include(CTest)
  add_subdirectory(test)
else()
  message(STATUS "testing: Disabled (enable with -DCDRC_TEST=On)")
endif()

message(STATUS "---------------------------------- Benchmarks ----------------------------------")

# User option to build benchmarks
option(CDRC_BENCHMARK    "Build microbenchmarks"  OFF)

if (CDRC_BENCHMARK)
  # Benchmark should not run its own unit tests
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

  # Download Benchmark library
  include(FetchContent)
  FetchContent_Declare(benchmark
    GIT_REPOSITORY  https://github.com/google/benchmark.git
    GIT_TAG         v1.7.1
  )
  FetchContent_GetProperties(benchmark)
  if(NOT benchmark_POPULATED)
    message(STATUS "benchmarks: Configuring Google Benchmark")
    FetchContent_Populate(benchmark)
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "")
    add_subdirectory(${benchmark_SOURCE_DIR}
                     ${benchmark_BINARY_DIR}
                     EXCLUDE_FROM_ALL)
  endif()

  # Include benchmark targets
  message(STATUS "benchmarks: Enabled")
  add_subdirectory(benchmarks)

else()
  message(STATUS "benchmarks: Disabled (enable with -DCDRC_BENCHMARK=On)")
endif()

# -------------------------------------------------------------------
#                           Figures

if(LINUX)
  add_subdirectory(results)
endif()
