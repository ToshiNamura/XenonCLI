#include "xenon/bytecode/chunk.hpp"

#include <utility>

namespace xenon
{

void Chunk::write(std::uint8_t byte, int line)
{
    code_.push_back(byte);
    lines_.push_back(line);
}

void Chunk::write(OpCode op, int line)
{
    write(static_cast<std::uint8_t>(op), line);
}

std::size_t Chunk::add_constant(Value value)
{
    constants_.push_back(std::move(value));
    return constants_.size() - 1;
}

void Chunk::patch_u16(std::size_t offset, std::uint16_t value)
{
    code_[offset] = static_cast<std::uint8_t>(value & 0xff);
    code_[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

} // namespace xenon
