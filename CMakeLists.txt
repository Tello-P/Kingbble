cmake_minimum_required(VERSION 3.20)
project(kingbble LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(KINGBBLE_BUILD_TESTS "Build unit tests" ON)
if(WIN32)
  set(tools_default OFF)
else()
  set(tools_default ON)
endif()
option(KINGBBLE_TOOLS "Build the research tools (POSIX only), not just the bridge" ${tools_default})
# Release packages: link the C++ runtime in, so the bridge runs on systems with an
# older libstdc++ (Linux) or with no MinGW DLLs at all (Windows: one .exe).
option(KINGBBLE_STATIC_RUNTIME "Link the C++ runtime statically" OFF)
if(MINGW)
  add_link_options(-static)
elseif(KINGBBLE_STATIC_RUNTIME AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_link_options(-static-libstdc++ -static-libgcc)
endif()
# Release speed: -O3, link-time optimization and a CPU level with AVX2 (any x86
# from about 2015 on, GitHub's runners included); "" builds for any x86-64.
set(KINGBBLE_ARCH "x86-64-v3" CACHE STRING "-march for Release builds, empty for none")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  string(REPLACE "-O2" "-O3" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  if(KINGBBLE_ARCH AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    add_compile_options(-march=${KINGBBLE_ARCH})
  endif()
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_ok OUTPUT ipo_error)
  if(ipo_ok)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
endif()

enable_testing()
add_subdirectory(engine)
