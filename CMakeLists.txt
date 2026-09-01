cmake_minimum_required(VERSION 3.30)
project(quail LANGUAGES C CXX)

# Enable testing
include(CTest)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dir: ${LLVM_INCLUDE_DIRS}")

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

# Add LLVM directories to the search paths
list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include_directories(${LLVM_INCLUDE_DIRS})

# Apply LLVM preprocessor definitions (important for ABI compatibility)
add_definitions(${LLVM_DEFINITIONS})

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate parser
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

FLEX_TARGET(
    Lexer
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/scanner.l
    ${GENERATED_DIR}/scanner.cpp
    DEFINES_FILE ${GENERATED_DIR}/scanner.hpp
)

BISON_TARGET(
    Parser
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser.y
    ${GENERATED_DIR}/parser.cpp
    DEFINES_FILE ${GENERATED_DIR}/parser.hpp
)

ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

add_library(parser STATIC
    ${BISON_Parser_OUTPUTS}
    ${FLEX_Lexer_OUTPUTS}
)

target_include_directories(parser
    PUBLIC
        ${GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/compiler
)

target_compile_options(parser PRIVATE
    -Wall -Wextra -Wpedantic
)


# Source files
set(SRC_FILES
    compiler/main.cpp
    compiler/types.cpp
    compiler/context.cpp
    compiler/ast.cpp
    compiler/error.cpp
    compiler/instructions.cpp
    compiler/binop.cpp
    compiler/enviroment.cpp
    compiler/generator.cpp
    compiler/cli.cpp
    compiler/driver.cpp
    compiler/parse_driver.cpp
    compiler/file_manager.cpp
    compiler/graph_function.cpp
)

# Main executable
add_executable(qc ${SRC_FILES})

set_property(TARGET qc PROPERTY COMPILE_WARNING_AS_ERROR ON)

target_compile_options(qc PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(qc
    PRIVATE
        parser
        LLVM
)

# Tests
if(BUILD_TESTING)
    add_subdirectory(test)
endif()

