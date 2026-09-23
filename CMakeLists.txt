cmake_minimum_required(VERSION 3.20)
project(uncertainty_arithmetic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # lets clangd understand your project

add_executable(uncertainty_arithmetic src/main.cpp src/evaluator.cpp src/parser.cpp)
target_compile_options(uncertainty_arithmetic PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(uncertainty_arithmetic PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
target_link_options(uncertainty_arithmetic PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)