add_library(scrabble_engine
  src/tiles.cpp
  src/rack.cpp
  src/bag.cpp
  src/board.cpp
)
target_include_directories(scrabble_engine PUBLIC include)
target_compile_options(scrabble_engine PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>)

if(SCRABBLE_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(engine_tests
    tests/tiles_test.cpp
    tests/rack_test.cpp
    tests/bag_test.cpp
    tests/board_test.cpp
  )
  target_link_libraries(engine_tests PRIVATE scrabble_engine GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(engine_tests)
endif()
