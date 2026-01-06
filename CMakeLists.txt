cmake_minimum_required(VERSION 3.5)
project(ccbpf)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend/include
    ${CMAKE_CURRENT_SOURCE_DIR}/bpf/include
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
    ${CMAKE_CURRENT_SOURCE_DIR}/test/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ir/include
)

file(GLOB LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/frontend/src/*.c")
file(GLOB LIB_BPF_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/bpf/src/*.c")
file(GLOB LIB_BACK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/backend/src/*.c")
file(GLOB LIB_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test/src/*.c")
file(GLOB LIB_IR_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ir/src/*.c")

add_executable(ccbpf  
    ${LIB_SOURCES}
    ${LIB_BPF_SOURCES}
    ${LIB_BACK_SOURCES}
    ${LIB_TEST_SOURCES}
    ${LIB_IR_SOURCES}
)

add_custom_target(cleanup
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/ccbpf
    COMMENT "Cleaning up build files"
)