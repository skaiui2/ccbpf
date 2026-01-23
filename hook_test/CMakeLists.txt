cmake_minimum_required(VERSION 3.5)
project(hook_test)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../bpf/include
)

file(GLOB LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB LIB_BPF_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../bpf/src/*.c")

add_executable(hook_test  
    ${LIB_SOURCES}
    ${LIB_BPF_SOURCES}
)

add_custom_target(cleanup
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/hook_test
    COMMENT "Cleaning up build files"
)