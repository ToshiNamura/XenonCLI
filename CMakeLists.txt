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
# xenon.core — shared base: definitions, bytecode, values, modules, compiler,
#              virtual stack/heap/machine and the Runtime executor.
# ---------------------------------------------------------------------------
add_library(xenon_core
    src/bytecode/disassembler.cpp
    src/bytecode/opcode.cpp
    src/bytecode/xenon_binary.cpp
    src/compiler/codegen.cpp
    src/compiler/compiler.cpp
    src/compiler/lexer.cpp
    src/compiler/module_parser.cpp
    src/compiler/parser.cpp
    src/module/module_file.cpp
    src/runtime/runtime.cpp
    src/runtime/value.cpp
    src/runtime/virtual_heap.cpp
    src/runtime/virtual_machine.cpp
    src/runtime/virtual_stack.cpp)

add_library(xenon::core ALIAS xenon_core)

target_include_directories(xenon_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_compile_features(xenon_core PUBLIC cxx_std_23)

# ---------------------------------------------------------------------------
# xenon.env — the Xenon API environment (Object, String, Vector, Environment).
# ---------------------------------------------------------------------------
add_library(xenon_env
    src/env/environment.cpp
    src/env/env_module.cpp
    src/env/object.cpp
    src/env/string.cpp)

add_library(xenon::env ALIAS xenon_env)

target_link_libraries(xenon_env PUBLIC xenon::core)

target_include_directories(xenon_env
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_compile_features(xenon_env PUBLIC cxx_std_23)

# ---------------------------------------------------------------------------
# xenon — the executable: compiles, packs and executes byte-code.
# ---------------------------------------------------------------------------
add_executable(xenon
    src/main.cpp
    src/packer/manifest.cpp
    src/packer/packer.cpp)

target_link_libraries(xenon PRIVATE xenon::core xenon::env)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(XENON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
