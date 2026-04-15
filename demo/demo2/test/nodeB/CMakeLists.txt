cmake_minimum_required(VERSION 3.10)
project(nodeB C)

set(CMAKE_BUILD_TYPE Debug)

file(GLOB NODEB_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

set(SCP_CORE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scp.c
)

file(GLOB DATASTRUCT_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/source/*.c
)

file(GLOB MG_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mg/source/*.c
)

set(CCBPF_VM
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf/vm/bpf/source/ccbpf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf/vm/bpf/source/bpf_filter.c
)

add_executable(nodeB
    ${NODEB_SRC}
    ${SCP_CORE}
    ${DATASTRUCT_SRC}
    ${MG_SRC}
    ${CCBPF_VM}
)

target_include_directories(nodeB PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf/vm/bpf/include
)

find_package(Threads REQUIRED)

target_link_libraries(nodeB PRIVATE
    Threads::Threads
    rt
)
