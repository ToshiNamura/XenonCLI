cmake_minimum_required(VERSION 3.24)

project(
    XenonCLI
    VERSION 0.1.0
    DESCRIPTION "Runtime for C++-like scripts compiled to the Xenon bytecode VM"
    LANGUAGES CXX)

include(GNUInstallDirs)

option(XENON_BUILD_TESTS "Build XenonCLI unit tests" ON)
option(XENON_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Common compiler options for every target (library, CLI, tests).
if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc)
    if(XENON_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(XENON_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

# ---------------------------------------------------------------------------
# xenon.core — Xenon ASM, byte-code values, the front end, modules and the VM.
# ---------------------------------------------------------------------------
add_library(xenon_core
    src/asm/assembler.cpp
    src/asm/binary.cpp
    src/asm/disassembler.cpp
    src/asm/opcode.cpp
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
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

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
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(xenon_env PUBLIC cxx_std_23)

# ---------------------------------------------------------------------------
# xenon.packer — `.xenon` archives and manifests (JAR-like packaging).
# ---------------------------------------------------------------------------
add_library(xenon_packer
    src/packer/manifest.cpp
    src/packer/packer.cpp)

add_library(xenon::packer ALIAS xenon_packer)

target_link_libraries(xenon_packer PUBLIC xenon::core)

target_include_directories(xenon_packer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(xenon_packer PUBLIC cxx_std_23)

# ---------------------------------------------------------------------------
# xenon — the executable: compiles, assembles, disassembles, packs and runs.
# ---------------------------------------------------------------------------
add_executable(xenon
    src/cli/main.cpp)

target_link_libraries(xenon PRIVATE xenon::core xenon::env xenon::packer)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(XENON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
install(TARGETS xenon RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/xenon DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
