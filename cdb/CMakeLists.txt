cmake_minimum_required(VERSION 3.13)
project(cdb)

set(CMAKE_CXX_STANDARD 17)

set(Clang_DIR /usr/lib/llvm-14/lib/cmake/clang)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(cdb cdb.cpp main.cpp)
target_link_libraries(cdb
    clangTooling
    clangBasic
    clangASTMatchers
    clangRewrite
)

