#include "xenon/bytecode/disassembler.hpp"

#include <iomanip>
#include <ostream>

#include "xenon/bytecode/opcode.hpp"

namespace xenon
{
namespace
{

/// Number of operand bytes following the opcode.
std::size_t operand_size(OpCode op)
{
    switch (op)
    {
    case OpCode::Constant:
    case OpCode::LoadLocal:
    case OpCode::StoreLocal:
    case OpCode::Call:
        return 1;
    case OpCode::Jump:
    case OpCode::JumpIfFalse:
        return 2;
    default:
        return 0;
    }
}

} // namespace

void disassemble(const Chunk& chunk, std::ostream& os)
{
    const auto& code = chunk.code();
    const auto& constants = chunk.constants();
    const auto& lines = chunk.lines();

    os << "== Xenon bytecode (" << code.size() << " bytes) ==\n";

    for (std::size_t offset = 0; offset < code.size();)
    {
        const auto op = static_cast<OpCode>(code[offset]);
        const int line = offset < lines.size() ? lines[offset] : -1;
        const std::size_t size = 1 + operand_size(op);

        os << std::right << std::setw(4) << offset << " | " << std::setw(4) << line << " | "
           << std::left << std::setw(12) << opcode_name(op);

        if (operand_size(op) == 1 && offset + 1 < code.size())
        {
            const auto operand = code[offset + 1];
            os << ' ' << static_cast<unsigned>(operand);
            if (op == OpCode::Constant && operand < constants.size())
            {
                os << "  ; " << constants[operand];
            }
        }
        else if (operand_size(op) == 2 && offset + 2 < code.size())
        {
            const auto operand = static_cast<std::int16_t>(
                code[offset + 1] | (static_cast<std::uint16_t>(code[offset + 2]) << 8));
            os << ' ' << operand;
        }

        os << '\n';
        offset += size;
    }
}

} // namespace xenon
