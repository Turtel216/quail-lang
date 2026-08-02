if(POLICY CMP0205)
  cmake_policy(SET CMP0205 NEW)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Locate lit test runner
find_program(LIT_EXECUTABLE NAMES lit llvm-lit HINTS ${LLVM_TOOLS_BINARY_DIR})
if(LIT_EXECUTABLE)
  set(LIT_COMMAND ${LIT_EXECUTABLE})
  message(STATUS "Found lit executable: ${LIT_EXECUTABLE}")
else()
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import lit"
    RESULT_VARIABLE LIT_MODULE_RESULT
    OUTPUT_QUIET ERROR_QUIET
  )
  if(LIT_MODULE_RESULT EQUAL 0)
    set(LIT_COMMAND ${Python3_EXECUTABLE} -m lit)
    message(STATUS "Found python lit module via ${Python3_EXECUTABLE}")
  else()
    message(WARNING "Neither lit executable nor python 'lit' module was found.")
    set(LIT_COMMAND ${Python3_EXECUTABLE} -m lit)
  endif()
endif()

# Locate FileCheck tool
find_program(FILECHECK_EXECUTABLE NAMES FileCheck HINTS ${LLVM_TOOLS_BINARY_DIR})
if(FILECHECK_EXECUTABLE)
  message(STATUS "Found FileCheck: ${FILECHECK_EXECUTABLE}")
else()
  message(WARNING "FileCheck executable not found.")
endif()

set(QC_EXECUTABLE "${CMAKE_BINARY_DIR}/qc${CMAKE_EXECUTABLE_SUFFIX}")

# Ensure prelude and runtime are accessible in binary test execution directories
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/e2e)

file(CREATE_LINK ${CMAKE_SOURCE_DIR}/prelude ${CMAKE_BINARY_DIR}/prelude SYMBOLIC COPY_ON_ERROR)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/prelude ${CMAKE_CURRENT_BINARY_DIR}/prelude SYMBOLIC COPY_ON_ERROR)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/prelude ${CMAKE_CURRENT_BINARY_DIR}/e2e/prelude SYMBOLIC COPY_ON_ERROR)

file(CREATE_LINK ${CMAKE_SOURCE_DIR}/runtime ${CMAKE_BINARY_DIR}/runtime SYMBOLIC COPY_ON_ERROR)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/runtime ${CMAKE_CURRENT_BINARY_DIR}/runtime SYMBOLIC COPY_ON_ERROR)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/runtime ${CMAKE_CURRENT_BINARY_DIR}/e2e/runtime SYMBOLIC COPY_ON_ERROR)

# Configure lit site config
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  @ONLY
)

# Custom target for running lit tests
add_custom_target(check-quail
  COMMAND ${LIT_COMMAND} -v ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS qc
  USES_TERMINAL
  COMMENT "Running Quail lit end-to-end test suite"
)

add_custom_target(check-lit DEPENDS check-quail)
