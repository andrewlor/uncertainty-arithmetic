cmake_minimum_required(VERSION 3.20)
project(uncertainty_arithmetic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # lets clangd understand your project

# Everything except main() lives in a library so the tests can link against it.
add_library(uncertainty_core src/csv.cpp src/evaluator.cpp src/parser.cpp)
target_include_directories(uncertainty_core PUBLIC src)
target_compile_options(uncertainty_core PUBLIC -Wall -Wextra -Wpedantic)
target_compile_options(uncertainty_core PUBLIC $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
target_link_options(uncertainty_core PUBLIC $<$<CONFIG:Debug>:-fsanitize=address,undefined>)

add_executable(uncertainty_arithmetic src/main.cpp)
target_link_libraries(uncertainty_arithmetic PRIVATE uncertainty_core)

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(googletest)

  add_executable(unit_tests
    tests/parser_test.cpp
    tests/evaluator_test.cpp
    tests/csv_test.cpp
    tests/cli_test.cpp
  )
  target_link_libraries(unit_tests PRIVATE uncertainty_core GTest::gtest_main)
  # The CLI tests run the real binary, so they need to know where it is.
  target_compile_definitions(unit_tests PRIVATE
    UA_BINARY="$<TARGET_FILE:uncertainty_arithmetic>")
  add_dependencies(unit_tests uncertainty_arithmetic)

  include(GoogleTest)
  gtest_discover_tests(unit_tests)
endif()
