#pragma once

#ifndef XENON__DISASSEMBLER_HPP
#define XENON__DISASSEMBLER_HPP 1

#include <iosfwd>

#include "xenon/bytecode/xenon_binary.hpp"

namespace xenon
{

/// Writes a human-readable listing of `binary`'s bytecode to `os`.
void disassemble(const XenonBinary& binary, std::ostream& os);

} // namespace xenon

#endif // !XENON__DISASSEMBLER_HPP
