cmake_minimum_required(VERSION 3.24)

project(
    XenonCLI
    VERSION 0.1.0
    DESCRIPTION "Runtime for C++-like scripts compiled to the Xenon bytecode VM"
    LANGUAGES CXX)

option(XENON_BUILD_TESTS "Build XenonCLI unit tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Common compiler options for every target (library, CLI, tests).
if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Core: the Xenon runtime library (bytecode, value, VM, compiler front end)
# ---------------------------------------------------------------------------
add_library(xenon_core
    src/bytecode/chunk.cpp
    src/bytecode/disassembler.cpp
    src/bytecode/opcode.cpp
    src/compiler/codegen.cpp
    src/compiler/compiler.cpp
    src/compiler/lexer.cpp
    src/compiler/module_parser.cpp
    src/compiler/parser.cpp
    src/module/module_file.cpp
    src/runtime/env.cpp
    src/runtime/object.cpp
    src/runtime/value.cpp
    src/runtime/vm.cpp)

add_library(xenon::core ALIAS xenon_core)

target_include_directories(xenon_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_compile_features(xenon_core PUBLIC cxx_std_23)

# ---------------------------------------------------------------------------
# Executable: the `xenon` CLI
# ---------------------------------------------------------------------------
add_executable(xenon src/main.cpp)
target_link_libraries(xenon PRIVATE xenon::core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(XENON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
