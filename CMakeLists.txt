cmake_minimum_required(VERSION 3.20)
project(kingbble LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(SCRABBLE_BUILD_TESTS "Build unit tests" ON)
# Release speed: -O3, link-time optimization and a CPU level with AVX2 (any x86
# from about 2015 on, GitHub's runners included); "" builds for any x86-64.
set(SCRABBLE_ARCH "x86-64-v3" CACHE STRING "-march for Release builds, empty for none")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  string(REPLACE "-O2" "-O3" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  if(SCRABBLE_ARCH AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    add_compile_options(-march=${SCRABBLE_ARCH})
  endif()
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_ok OUTPUT ipo_error)
  if(ipo_ok)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
endif()

enable_testing()
add_subdirectory(engine)
