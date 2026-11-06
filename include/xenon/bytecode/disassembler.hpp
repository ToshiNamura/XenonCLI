#pragma once

#include <iosfwd>

#include "xenon/bytecode/chunk.hpp"

namespace xenon
{

/// Writes a human-readable listing of `chunk`'s bytecode to `os`.
void disassemble(const Chunk& chunk, std::ostream& os);

} // namespace xenon
