cmake_minimum_required(VERSION 3.20)
project(kingbble LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(SCRABBLE_BUILD_TESTS "Build unit tests" ON)

enable_testing()
add_subdirectory(engine)
