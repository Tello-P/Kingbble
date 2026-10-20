# Quackle side of the Quackle match (scripts/quackle_match.py builds it):
#   cmake -S scripts/quackle_match -B build/quackle_helper -DQUACKLE_DIR=~/src/quackle
cmake_minimum_required(VERSION 3.16)
project(kingbble_quackle_helper LANGUAGES CXX)

set(QUACKLE_DIR "$ENV{HOME}/src/quackle" CACHE PATH "Quackle checkout")
set(CMAKE_BUILD_TYPE Release CACHE STRING "")
add_subdirectory(${QUACKLE_DIR} libquackle)
add_subdirectory(${QUACKLE_DIR}/quackleio quackleio)

add_executable(quackle_helper main.cpp)
target_link_libraries(quackle_helper quackleio libquackle)
target_compile_definitions(quackle_helper PRIVATE QUACKLE_DATA="${QUACKLE_DIR}/data")
