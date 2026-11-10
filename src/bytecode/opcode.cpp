#include <xenon/bytecode/opcode.hpp>

#define XENON_OPCODE_NAME_CASE(opcode) case xenon::OpCode::opcode: return #opcode

const char* xenon::opcodeName(xenon::OpCode op)
{
	switch (op)
	{
		XENON_OPCODE_NAME_CASE(Constant);
		XENON_OPCODE_NAME_CASE(LoadLocal);
		XENON_OPCODE_NAME_CASE(StoreLocal);
		XENON_OPCODE_NAME_CASE(Pop);
		XENON_OPCODE_NAME_CASE(Add);
		XENON_OPCODE_NAME_CASE(Subtract);
		XENON_OPCODE_NAME_CASE(Multiply);
		XENON_OPCODE_NAME_CASE(Divide);
		XENON_OPCODE_NAME_CASE(Negate);
		XENON_OPCODE_NAME_CASE(Not);
		XENON_OPCODE_NAME_CASE(Equal);
		XENON_OPCODE_NAME_CASE(NotEqual);
		XENON_OPCODE_NAME_CASE(Less);
		XENON_OPCODE_NAME_CASE(LessEqual);
		XENON_OPCODE_NAME_CASE(Greater);
		XENON_OPCODE_NAME_CASE(GreaterEqual);
		XENON_OPCODE_NAME_CASE(Jump);
		XENON_OPCODE_NAME_CASE(JumpIfFalse);
		XENON_OPCODE_NAME_CASE(Call);
		XENON_OPCODE_NAME_CASE(Return);
	}
	return "Unknown";
}

#undef XENON_OPCODE_NAME_CASE
