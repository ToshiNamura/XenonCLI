#pragma once

#include <cstdint>

namespace xenon
{

/// Instruction set of the Xenon virtual machine.
enum class OpCode : std::uint8_t
{
    Constant,     ///< Push a constant from the chunk's constant pool (1-byte operand).
    String,       ///< Push a string literal as a heap String object (1-byte operand).
    LoadLocal,    ///< Push a local variable (1-byte slot operand).
    StoreLocal,   ///< Pop into a local variable (1-byte slot operand).
    Pop,          ///< Discard the top of the stack.
    Add,          ///< Pop b, pop a, push a + b.
    Subtract,     ///< Pop b, pop a, push a - b.
    Multiply,     ///< Pop b, pop a, push a * b.
    Divide,       ///< Pop b, pop a, push a / b.
    Negate,       ///< Pop a, push -a.
    Not,          ///< Pop a, push !a (boolean).
    Equal,        ///< Pop b, pop a, push a == b.
    NotEqual,     ///< Pop b, pop a, push a != b.
    Less,         ///< Pop b, pop a, push a < b.
    LessEqual,    ///< Pop b, pop a, push a <= b.
    Greater,      ///< Pop b, pop a, push a > b.
    GreaterEqual, ///< Pop b, pop a, push a >= b.
    Jump,         ///< Unconditional relative jump (2-byte signed operand).
    JumpIfFalse,  ///< Pop a; jump if a is falsy (2-byte signed operand).
    Call,         ///< Call a function (1-byte function index operand).
    Return,       ///< Pop the top of the stack and exit with it as the result.
};

/// Returns the mnemonic of `op` (e.g. `"Add"`).
[[nodiscard]] const char* opcodeName(OpCode op);

} // namespace xenon
