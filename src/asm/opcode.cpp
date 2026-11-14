#include <xenon/asm/opcode.hpp>

const char* xenon::mnemonic(Instruction op) noexcept
{
	switch (op)
	{
		case Instruction::Const: return "const";
		case Instruction::Str:   return "str";
		case Instruction::New:   return "new";
		case Instruction::Getf:  return "getf";
		case Instruction::Setf:  return "setf";
		case Instruction::Ldg:   return "ldg";
		case Instruction::Stg:   return "stg";
		case Instruction::Ldl:   return "ldl";
		case Instruction::Stl:   return "stl";
		case Instruction::Pop:   return "pop";
		case Instruction::Add:   return "add";
		case Instruction::Sub:   return "sub";
		case Instruction::Mul:   return "mul";
		case Instruction::Div:   return "div";
		case Instruction::Neg:   return "neg";
		case Instruction::Not:   return "not";
		case Instruction::Equ:   return "equ";
		case Instruction::Neq:   return "neq";
		case Instruction::Lss:   return "lss";
		case Instruction::Leq:   return "leq";
		case Instruction::Gtr:   return "gtr";
		case Instruction::Geq:   return "geq";
		case Instruction::Jmp:   return "jmp";
		case Instruction::Jif:   return "jif";
		case Instruction::Call:  return "call";
		case Instruction::Ret:   return "ret";
		case Instruction::Jmpa:  return "jmpa";
		case Instruction::Jifa:  return "jifa";
		case Instruction::Mod:   return "mod";
		case Instruction::BAnd:  return "and";
		case Instruction::BOr:   return "or";
		case Instruction::BXor:  return "xor";
		case Instruction::BNot:  return "bnot";
		case Instruction::Shl:   return "shl";
		case Instruction::Shr:   return "shr";
	}
	return "?";
}

const char* xenon::mnemonic(Directive op) noexcept
{
	switch (op)
	{
		case Directive::Meta:      return ".meta";
		case Directive::Types:     return ".types";
		case Directive::Data:      return ".data";
		case Directive::Text:      return ".text";
		case Directive::Extern:    return ".extern";
		case Directive::Module:    return ".module";
		case Directive::Import:    return ".import";
		case Directive::Class:     return ".class";
		case Directive::Struct:    return ".struct";
		case Directive::Enum:      return ".enum";
		case Directive::Parent:    return ".parent";
		case Directive::Field:     return ".field";
		case Directive::Method:    return ".method";
		case Directive::Item:      return ".item";
		case Directive::Variable:  return ".variable";
		case Directive::Constant:  return ".constant";
		case Directive::Function:  return ".function";
		case Directive::Local:     return ".local";
		case Directive::Attribute: return ".attribute";
		case Directive::Template:  return ".template";
		case Directive::End:       return ".end";
		case Directive::Locals:    return ".locals";
	}
	return ".?";
}

xenon::OperandKind xenon::operandKind(Instruction op) noexcept
{
	switch (op)
	{
		case Instruction::Const:
			return OperandKind::Constant;
		case Instruction::Str:
		case Instruction::New:
		case Instruction::Ldg:
		case Instruction::Stg:
		case Instruction::Call:
			return OperandKind::String;
		case Instruction::Getf:
		case Instruction::Setf:
			return OperandKind::Field;
		case Instruction::Ldl:
		case Instruction::Stl:
			return OperandKind::Local;
		case Instruction::Jmp:
		case Instruction::Jif:
			return OperandKind::Relative;
		case Instruction::Jmpa:
		case Instruction::Jifa:
			return OperandKind::Address;
		default:
			return OperandKind::None;
	}
}

std::size_t xenon::operandSize(Instruction op) noexcept
{
	switch (operandKind(op))
	{
		case OperandKind::Constant:
		case OperandKind::String:
		case OperandKind::Field:
		case OperandKind::Local:
		case OperandKind::Relative:
			return 2;
		case OperandKind::Address:
			return 4;
		default:
			return 0;
	}
}

bool xenon::instructionFromMnemonic(std::string_view name, Instruction& out) noexcept
{
	static constexpr struct { std::string_view Name; Instruction Op; } table[] = {
		{"const", Instruction::Const}, {"str", Instruction::Str},
		{"new", Instruction::New},     {"getf", Instruction::Getf},
		{"setf", Instruction::Setf},   {"ldg", Instruction::Ldg},
		{"stg", Instruction::Stg},     {"ldl", Instruction::Ldl},
		{"stl", Instruction::Stl},     {"pop", Instruction::Pop},
		{"add", Instruction::Add},     {"sub", Instruction::Sub},
		{"mul", Instruction::Mul},     {"div", Instruction::Div},
		{"neg", Instruction::Neg},     {"not", Instruction::Not},
		{"equ", Instruction::Equ},     {"neq", Instruction::Neq},
		{"lss", Instruction::Lss},     {"leq", Instruction::Leq},
		{"gtr", Instruction::Gtr},     {"geq", Instruction::Geq},
		{"jmp", Instruction::Jmp},     {"jif", Instruction::Jif},
		{"call", Instruction::Call},   {"ret", Instruction::Ret},
		{"jmpa", Instruction::Jmpa},   {"jifa", Instruction::Jifa},
		{"mod", Instruction::Mod},     {"and", Instruction::BAnd},
		{"or", Instruction::BOr},      {"xor", Instruction::BXor},
		{"bnot", Instruction::BNot},   {"shl", Instruction::Shl},
		{"shr", Instruction::Shr},
	};
	for (const auto& entry : table)
	{
		if (name == entry.Name)
		{
			out = entry.Op;
			return true;
		}
	}
	return false;
}

bool xenon::directiveFromMnemonic(std::string_view name, Directive& out) noexcept
{
	static constexpr struct { std::string_view Name; Directive Op; } table[] = {
		{".meta", Directive::Meta},           {".types", Directive::Types},
		{".data", Directive::Data},           {".text", Directive::Text},
		{".extern", Directive::Extern},       {".module", Directive::Module},
		{".import", Directive::Import},       {".class", Directive::Class},
		{".struct", Directive::Struct},       {".enum", Directive::Enum},
		{".parent", Directive::Parent},       {".field", Directive::Field},
		{".method", Directive::Method},       {".item", Directive::Item},
		{".variable", Directive::Variable},   {".constant", Directive::Constant},
		{".function", Directive::Function},   {".local", Directive::Local},
		{".attribute", Directive::Attribute}, {".template", Directive::Template},
		{".end", Directive::End},             {".locals", Directive::Locals},
	};
	for (const auto& entry : table)
	{
		if (name == entry.Name)
		{
			out = entry.Op;
			return true;
		}
	}
	return false;
}
