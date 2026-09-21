cmake_minimum_required(VERSION 3.24)
project(aquarium_rules LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(aquarium_rules
  rules/src/Nickname.cpp
)
target_include_directories(aquarium_rules PUBLIC rules/include)
# Match the stricter Unreal build early: same sources are compiled inside UE with warnings.
target_compile_options(aquarium_rules PUBLIC -Wall -Wextra -Wshadow)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.4
)
FetchContent_MakeAvailable(Catch2)

enable_testing()
add_executable(rules_tests
  tests/test_vec2.cpp
  tests/test_nickname.cpp
  tests/test_catalog.cpp
  tests/test_session.cpp
  tests/test_steering.cpp
  tests/test_motion.cpp
  tests/test_bounds.cpp
  tests/test_flee.cpp
  tests/test_wander.cpp
  tests/test_swimplane.cpp
  tests/test_swimanimation.cpp
  tests/test_heading.cpp
  tests/test_boids.cpp
  tests/test_obstacles.cpp
  tests/test_facing.cpp
  tests/test_audio.cpp
  tests/test_reaction.cpp
  tests/test_bubbles.cpp
  tests/test_catch.cpp
  tests/test_evade.cpp
  tests/test_dash.cpp
  tests/test_stamp.cpp
  tests/test_impact.cpp
)
target_link_libraries(rules_tests PRIVATE aquarium_rules Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(rules_tests)
