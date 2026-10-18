add_library(scrabble_engine
  src/tiles.cpp
  src/rack.cpp
  src/bag.cpp
  src/board.cpp
  src/gaddag.cpp
  src/move.cpp
  src/movegen.cpp
  src/game.cpp
  src/greedy_bot.cpp
  src/leaves.cpp
  src/equity.cpp
  src/equity_bot.cpp
  src/sim_bot.cpp
  src/gcg.cpp
  src/wordmap.cpp
  src/kwg.cpp
  src/cgp.cpp
)

add_executable(kingbble_selfplay apps/selfplay.cpp)
target_link_libraries(kingbble_selfplay PRIVATE scrabble_engine)
add_executable(kingbble_buildkwg apps/buildkwg.cpp)
target_link_libraries(kingbble_buildkwg PRIVATE scrabble_engine)
add_executable(kingbble_positions apps/positions.cpp)
target_link_libraries(kingbble_positions PRIVATE scrabble_engine)
find_package(Threads REQUIRED)
target_link_libraries(scrabble_engine PUBLIC Threads::Threads)
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
    tests/gaddag_test.cpp
    tests/movegen_test.cpp
    tests/game_test.cpp
    tests/gcg_test.cpp
    tests/wordmap_test.cpp
    tests/kwg_test.cpp
    tests/cgp_test.cpp
    tests/leaves_test.cpp
    tests/equity_test.cpp
    tests/sim_bot_test.cpp
  )
  target_compile_definitions(engine_tests PRIVATE
    KINGBBLE_FIXTURES="${PROJECT_SOURCE_DIR}/lexica/fixtures")
  target_link_libraries(engine_tests PRIVATE scrabble_engine GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(engine_tests)
endif()
