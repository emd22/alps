cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME alps)

project(${BUILD_NAME} C)

set(C_WARNING_FLAGS "-Wall")
set(C_FLAGS "${C_DEBUG_FLAGS} ${C_WARNING_FLAGS}")

file(GLOB_RECURSE SOURCES "src/*.c")
file(GLOB_RECURSE HEADERS "src/*.h")

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/src
)

add_executable(${BUILD_NAME} ${SOURCES} ${HEADERS})
set_target_properties(${BUILD_NAME} PROPERTIES COMPILE_FLAGS ${C_FLAGS})
