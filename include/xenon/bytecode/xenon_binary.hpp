#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "xenon/bytecode/opcode.hpp"
#include "xenon/runtime/value.hpp"

namespace xenon
{

/// A contiguous block of executable binary code together with its constant
/// pool — the unit of code the virtual machine executes.
///
/// Instructions are encoded as a 1-byte opcode optionally followed by operands.
/// Currently only `OpCode::Constant` has an operand: a 1-byte index into the
/// constant pool (limiting a binary to 256 constants — a known early constraint).
class XenonBinary
{
  public:
    /// Appends a raw byte to the code stream.
    void write(std::uint8_t byte, int line);

    /// Appends an opcode to the code stream.
    void write(OpCode op, int line);

    /// Adds `value` to the constant pool and returns its index.
    [[nodiscard]] std::size_t addConstant(Value value);

    /// Returns the current code stream length (the next byte offset).
    [[nodiscard]] std::size_t codePosition() const noexcept
    {
        return _code.size();
    }

    /// Overwrites two bytes at `offset` (little-endian, used to patch jumps).
    void patchU16(std::size_t offset, std::uint16_t value);

    [[nodiscard]] const std::vector<std::uint8_t>& code() const noexcept
    {
        return _code;
    }
    [[nodiscard]] const std::vector<Value>& constants() const noexcept
    {
        return _constants;
    }
    [[nodiscard]] const std::vector<int>& lines() const noexcept
    {
        return _lines;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return _code.size();
    }

  private:
    std::vector<std::uint8_t> _code;
    std::vector<Value> _constants;
    std::vector<int> _lines; ///< Source line for every byte (diagnostics).
};

} // namespace xenon
