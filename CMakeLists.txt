cmake_minimum_required(VERSION 3.30)
project(ff LANGUAGES C CXX)

# Enable testing
include(CTest)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dir: ${LLVM_INCLUDE_DIRS}")

# Add LLVM directories to the search paths
list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include_directories(${LLVM_INCLUDE_DIRS})

# Apply LLVM preprocessor definitions (important for ABI compatibility)
add_definitions(${LLVM_DEFINITIONS})

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SRC_FILES
    src/main.cpp
)

# Main executable
add_executable(ffc ${SRC_FILES})

target_link_libraries(ffc LLVM)

# Tests
#if (BUILD_TESTING)
#    add_executable(lexer_test_success tests/lexer/test_success.cpp)
#    target_link_libraries(lexer_test_success lexer_lib)
#endif()

