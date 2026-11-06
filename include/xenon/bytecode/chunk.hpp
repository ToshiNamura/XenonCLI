#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "xenon/bytecode/opcode.hpp"
#include "xenon/runtime/value.hpp"

namespace xenon
{

/// A contiguous block of executable bytecode together with its constant pool.
///
/// Instructions are encoded as a 1-byte opcode optionally followed by operands.
/// Currently only `OpCode::Constant` has an operand: a 1-byte index into the
/// constant pool (limiting a chunk to 256 constants — a known early constraint).
class Chunk
{
  public:
    /// Appends a raw byte to the code stream.
    void write(std::uint8_t byte, int line);

    /// Appends an opcode to the code stream.
    void write(OpCode op, int line);

    /// Adds `value` to the constant pool and returns its index.
    [[nodiscard]] std::size_t add_constant(Value value);

    /// Returns the current code stream length (the next byte offset).
    [[nodiscard]] std::size_t code_position() const noexcept
    {
        return code_.size();
    }

    /// Overwrites two bytes at `offset` (little-endian, used to patch jumps).
    void patch_u16(std::size_t offset, std::uint16_t value);

    [[nodiscard]] const std::vector<std::uint8_t>& code() const noexcept
    {
        return code_;
    }
    [[nodiscard]] const std::vector<Value>& constants() const noexcept
    {
        return constants_;
    }
    [[nodiscard]] const std::vector<int>& lines() const noexcept
    {
        return lines_;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return code_.size();
    }

  private:
    std::vector<std::uint8_t> code_;
    std::vector<Value> constants_;
    std::vector<int> lines_; ///< Source line for every byte (diagnostics).
};

} // namespace xenon
