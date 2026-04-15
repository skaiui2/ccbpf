cmake_minimum_required(VERSION 3.5)
project(nodeC C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf/compiler/frontend/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf/compiler/backend/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf/compiler/ir/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

file(GLOB CC_FRONTEND_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf/compiler/frontend/source/*.c
)
file(GLOB CC_BACKEND_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf/compiler/backend/source/*.c
)
file(GLOB CC_IR_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccbpf/compiler/ir/source/*.c
)

file(GLOB MG_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mg/source/*.c
)

file(GLOB LIB_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/source/*.c
)

file(GLOB nodeC_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_executable(nodeC
    ${nodeC_SRC}
    ${CC_FRONTEND_SRC}
    ${CC_BACKEND_SRC}
    ${CC_IR_SRC}
    ${MG_SRC}
    ${LIB_SRC}
)

target_link_libraries(nodeC m)