#include "xenon/bytecode/opcode.hpp"

namespace xenon
{

const char* opcodeName(OpCode op)
{
    switch (op)
    {
    case OpCode::Constant:
        return "Constant";
    case OpCode::LoadLocal:
        return "LoadLocal";
    case OpCode::StoreLocal:
        return "StoreLocal";
    case OpCode::Pop:
        return "Pop";
    case OpCode::Add:
        return "Add";
    case OpCode::Subtract:
        return "Subtract";
    case OpCode::Multiply:
        return "Multiply";
    case OpCode::Divide:
        return "Divide";
    case OpCode::Negate:
        return "Negate";
    case OpCode::Not:
        return "Not";
    case OpCode::Equal:
        return "Equal";
    case OpCode::NotEqual:
        return "NotEqual";
    case OpCode::Less:
        return "Less";
    case OpCode::LessEqual:
        return "LessEqual";
    case OpCode::Greater:
        return "Greater";
    case OpCode::GreaterEqual:
        return "GreaterEqual";
    case OpCode::Jump:
        return "Jump";
    case OpCode::JumpIfFalse:
        return "JumpIfFalse";
    case OpCode::Call:
        return "Call";
    case OpCode::Return:
        return "Return";
    }
    return "Unknown";
}

} // namespace xenon
