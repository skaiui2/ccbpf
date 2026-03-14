cmake_minimum_required(VERSION 3.5)
project(nodeB C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccBPF/vm/bpf/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccBPF
    ${CMAKE_CURRENT_SOURCE_DIR}/../mg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/include
)

file(GLOB NODEB_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

file(GLOB VM_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccBPF/vm/bpf/source/*.c
)

file(GLOB MG_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../mg/source/*.c
)

file(GLOB LIB_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/source/*.c
)

add_executable(nodeB
    ${NODEB_SRC}
    ${VM_SRC}
    ${MG_SRC}
    ${LIB_SRC}
)

target_link_libraries(nodeB m)
