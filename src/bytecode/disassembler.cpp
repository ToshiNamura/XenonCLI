#include <xenon/bytecode/disassembler.hpp>

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ostream>

#include <xenon/bytecode/opcode.hpp>

namespace xenon
{
	/// Number of operand bytes following the opcode.
	std::size_t operandSize(xenon::OpCode op)
	{
		switch (op)
		{
			case xenon::OpCode::Constant:
			case xenon::OpCode::String:
			case xenon::OpCode::New:
			case xenon::OpCode::GetField:
			case xenon::OpCode::SetField:
			case xenon::OpCode::LoadGlobal:
			case xenon::OpCode::StoreGlobal:
			case xenon::OpCode::LoadLocal:
			case xenon::OpCode::StoreLocal:
			case xenon::OpCode::Call:
				return 1;
			case xenon::OpCode::Jump:
			case xenon::OpCode::JumpIfFalse:
				return 2;
			default:
				return 0;
		}
	}
}

void disassemble(const xenon::XenonBinary& binary, std::ostream& os)
{
	const auto& code      = binary.code();
	const auto& constants = binary.constants();
	const auto& strings   = binary.strings();
	const auto& lines     = binary.lines();

	os << "== Xenon bytecode (" << code.size() << " bytes) ==\n";

	for (std::size_t offset = 0; offset < code.size();)
	{
		const auto op = static_cast<xenon::OpCode>(code[offset]);
		const int line = offset < lines.size() ? lines[offset] : -1;
		const std::size_t size = 1 + xenon::operandSize(op);

		os << std::right << std::setw(4) << offset << " | " << std::setw(4) << line << " | "
		   << std::left << std::setw(12) << xenon::opcodeName(op);

		if (xenon::operandSize(op) == 1 && offset + 1 < code.size())
		{
			const auto operand = code[offset + 1];
			os << ' ' << static_cast<unsigned>(operand);
			if (op == xenon::OpCode::Constant && operand < constants.size())
			{
				os << "  ; " << constants[operand];
			}
			else if (op == xenon::OpCode::String && operand < strings.size())
			{
				os << "  ; \"" << strings[operand] << '"';
			}
			else if (op == xenon::OpCode::New && operand < strings.size())
			{
				os << "  ; " << strings[operand];
			}
		}
		else if (xenon::operandSize(op) == 2 && offset + 2 < code.size())
		{
			const auto operand = static_cast<std::int16_t>(
				code[offset + 1] | (static_cast<std::uint16_t>(code[offset + 2]) << 8));
			os << ' ' << operand;
		}

		os << '\n';
		offset += size;
	}
}
