cmake_minimum_required(VERSION 3.14)
project(runtime)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable (runtime src/test.c src/runtime.c src/stack.c)
