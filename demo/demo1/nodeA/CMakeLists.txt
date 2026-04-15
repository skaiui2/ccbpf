cmake_minimum_required(VERSION 3.5)
project(nodeA C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")

# ccBPF core include dirs
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccBPF/compiler/frontend/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccBPF/compiler/backend/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccBPF/compiler/ir/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccBPF
    ${CMAKE_CURRENT_SOURCE_DIR}/../mg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ccBPF compiler sources
file(GLOB CC_FRONTEND_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccBPF/compiler/frontend/source/*.c
)
file(GLOB CC_BACKEND_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccBPF/compiler/backend/source/*.c
)
file(GLOB CC_IR_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccBPF/compiler/ir/source/*.c
)

# mg allocator
file(GLOB MG_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../mg/source/*.c
)

# lib utils
file(GLOB LIB_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/source/*.c
)

# nodeA sources
file(GLOB NODEA_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_executable(nodeA
    ${NODEA_SRC}
    ${CC_FRONTEND_SRC}
    ${CC_BACKEND_SRC}
    ${CC_IR_SRC}
    ${MG_SRC}
    ${LIB_SRC}
)

target_link_libraries(nodeA m)
