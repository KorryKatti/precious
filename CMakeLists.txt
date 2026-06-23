cmake_minimum_required(VERSION 4.3.3)

project(precious)

set(CMAKE_CXX_STANDARD 23)

file(GLOB PRECIOUS_FILES "src/*.precious")
file(GLOB ASM_FILES "*.asm")
file(GLOB HEADER_FILES "src/*.hpp")

add_executable(precious src/main.cpp ${PRECIOUS_FILES} ${ASM_FILES} ${HEADER_FILES})

add_custom_command(
  TARGET precious
  POST_BUILD
  COMMAND precious src/test.precious
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Compiling test.precious")
