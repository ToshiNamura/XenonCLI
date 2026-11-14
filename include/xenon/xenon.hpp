#pragma once

#ifndef XENON__XENON_HPP
#define XENON__XENON_HPP 1

// xenon.core — shared definitions, Xenon ASM, compiler, modules and executor.
#include <xenon/core/defines.hpp>

#include <xenon/asm/assembler.hpp>
#include <xenon/asm/binary.hpp>
#include <xenon/asm/disassembler.hpp>
#include <xenon/asm/model.hpp>
#include <xenon/asm/opcode.hpp>

#include <xenon/compiler/ast.hpp>
#include <xenon/compiler/codegen.hpp>
#include <xenon/compiler/compiler.hpp>
#include <xenon/compiler/lexer.hpp>
#include <xenon/compiler/module_header.hpp>
#include <xenon/compiler/module_parser.hpp>
#include <xenon/compiler/parser.hpp>

#include <xenon/module/module_file.hpp>

#include <xenon/runtime/error.hpp>
#include <xenon/runtime/heap_object.hpp>
#include <xenon/runtime/runtime.hpp>
#include <xenon/runtime/value.hpp>
#include <xenon/runtime/virtual_heap.hpp>
#include <xenon/runtime/virtual_machine.hpp>
#include <xenon/runtime/virtual_stack.hpp>

// xenon.env — the Xenon API object hierarchy.
#include <xenon/env/environment.hpp>
#include <xenon/env/env_module.hpp>
#include <xenon/env/object.hpp>
#include <xenon/env/string.hpp>
#include <xenon/env/vector.hpp>

// xenon — the packer (used by the `xenon` executable).
#include <xenon/packer/manifest.hpp>
#include <xenon/packer/packer.hpp>

#endif // !XENON__XENON_HPP
