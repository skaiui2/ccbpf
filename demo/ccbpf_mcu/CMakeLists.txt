cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG -g0" CACHE STRING "" FORCE)

# Set the project name
set(CMAKE_PROJECT_NAME ccbpf_mcu)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})


file(GLOB LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/lib/source/*.c")
file(GLOB MG_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/mg/source/*.c")
file(GLOB FS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/fs/source/*.c")
file(GLOB COMPILER_BACKEND_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/ccBPF/compiler/backend/source/*.c")
file(GLOB COMPILER_FRONTEND_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/ccBPF/compiler/frontend/source/*.c")
file(GLOB COMPILER_IR_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/ccBPF/compiler/ir/source/*.c")
file(GLOB VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/ccBPF/vm/bpf/source/*.c")
file(GLOB SHELL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/shell/source/*.c")
file(GLOB VIM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/vim/source/*.c")
file(GLOB SHELL_PORT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/port/shell_port/*.c")
file(GLOB FS_PORT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/port/fs_port/*.c")

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        ${LIB_SOURCES}
        ${MG_SOURCES}
        ${FS_SOURCES}
        ${COMPILER_BACKEND_SOURCES}
        ${COMPILER_FRONTEND_SOURCES}
        ${COMPILER_IR_SOURCES}
        ${VM_SOURCES}
        ${SHELL_SOURCES}
        ${VIM_SOURCES}
        ${SHELL_PORT_SOURCES}
        ${FS_PORT_SOURCES}
)


include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/lib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/mg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/fs/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/ccBPF/compiler/frontend/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/ccBPF/compiler/backend/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/ccBPF/compiler/ir/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/ccBPF/vm/bpf/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/shell/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/vim/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/ccBPF
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/port/fs_port/
        ${CMAKE_CURRENT_SOURCE_DIR}/tool/port/shell_port/
)


# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
