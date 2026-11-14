#include <xenon/asm/disassembler.hpp>

#include <cstddef>
#include <cstdint>
#include <format>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace xenon
{
	namespace
	{
		const char* accessName(Access access) noexcept
		{
			switch (access)
			{
				case Access::Private:   return "private";
				case Access::Protected: return "protected";
				case Access::Public:    return "public";
			}
			return "private";
		}

		std::string escape(std::string_view text)
		{
			std::string result;
			result.reserve(text.size());
			for (const char c : text)
			{
				switch (c)
				{
					case '\\': result += "\\\\"; break;
					case '"':  result += "\\\""; break;
					case '\n': result += "\\n"; break;
					case '\r': result += "\\r"; break;
					case '\t': result += "\\t"; break;
					default:   result += c; break;
				}
			}
			return result;
		}

		std::string formatNumber(double value)
		{
			std::string text = std::format("{}", value);
			if (text.find_first_of(".eE") == std::string::npos)
			{
				text += ".0";
			}
			return text;
		}

		std::string formatValue(const Value& value)
		{
			if (const auto* number = std::get_if<double>(&value))
			{
				return formatNumber(*number);
			}
			return toString(value);
		}

		void writeIndent(std::ostream& os, int depth)
		{
			for (int i = 0; i < depth; ++i)
			{
				os << "    ";
			}
		}

		void writeAttributes(const AttributeList& attributes, std::ostream& os, int depth)
		{
			for (const auto& attribute : attributes)
			{
				writeIndent(os, depth);
				os << ".attribute " << attribute->Key;
				switch (attribute->Kind)
				{
					case AttributeKind::None:
						break;
					case AttributeKind::Integer:
						os << ' ' << static_cast<const AttributeInteger&>(*attribute).Value;
						break;
					case AttributeKind::Float:
						os << ' ' << formatNumber(static_cast<const AttributeFloat&>(*attribute).Value);
						break;
					case AttributeKind::String:
						os << " \"" << escape(static_cast<const AttributeString&>(*attribute).Value) << '"';
						break;
					case AttributeKind::Object:
						os << " .class " << static_cast<const AttributeObject&>(*attribute).Object;
						break;
				}
				os << '\n';
			}
		}

		void writeTemplates(const std::vector<TemplateParam>& parameters, std::ostream& os, int depth)
		{
			for (const auto& parameter : parameters)
			{
				writeIndent(os, depth);
				os << ".template <";
				if (parameter.IsValue)
				{
					os << parameter.ValueType << ' ' << parameter.TypeName;
					if (!parameter.DefaultValue.empty())
					{
						os << " = " << parameter.DefaultValue;
					}
				}
				else
				{
					os << parameter.TypeName;
					if (!parameter.DefaultType.empty())
					{
						os << " = " << parameter.DefaultType;
					}
				}
				os << ">\n";
			}
		}

		void writeFunctionHeader(const Function& function, std::ostream& os, int depth)
		{
			writeAttributes(function.Attributes, os, depth);
			writeTemplates(function.TemplateParams, os, depth);
			writeIndent(os, depth);
			os << ".function";
			if (function.Exported)
			{
				os << " export";
			}
			os << ' ' << function.Signature << '\n';
		}

		void writeMethodHeader(const Method& method, std::ostream& os, int depth)
		{
			writeAttributes(method.Attributes, os, depth);
			writeTemplates(method.TemplateParams, os, depth);
			writeIndent(os, depth);
			os << ".method";
			if (method.Exported)
			{
				os << " export";
			}
			os << ' ' << accessName(method.AccessModifier) << ' ' << method.Signature << '\n';
		}
	}

	void disassembleBody(const XenonBinary& binary, std::ostream& os)
	{
		const auto& code      = binary.code();
		const auto& constants = binary.constants();
		const auto& strings   = binary.strings();

		for (std::size_t offset = 0; offset < code.size();)
		{
			const auto instruction = static_cast<Instruction>(code[offset]);
			const OperandKind kind = operandKind(instruction);
			const std::size_t size = operandSize(instruction);
			if (!isDirective(code[offset]) && mnemonic(instruction) == std::string_view("?"))
			{
				os << "        ; unknown opcode 0x" << std::hex << static_cast<unsigned>(code[offset])
				   << std::dec << '\n';
				++offset;
				continue;
			}

			os << "        " << mnemonic(instruction);

			if (size == 2 && offset + 2 < code.size())
			{
				const auto u16 = static_cast<std::uint16_t>(
					code[offset + 1] | (static_cast<std::uint16_t>(code[offset + 2]) << 8));
				switch (kind)
				{
					case OperandKind::Constant:
						if (u16 < constants.size())
						{
							os << ' ' << formatValue(constants[u16]) << "  ; #" << u16;
						}
						else
						{
							os << ' ' << u16;
						}
						break;
					case OperandKind::String:
						if (u16 < strings.size())
						{
							os << " \"" << escape(strings[u16]) << '"';
						}
						else
						{
							os << ' ' << u16;
						}
						break;
					case OperandKind::Relative:
					{
						const auto relative = static_cast<std::int16_t>(u16);
						const auto target   = static_cast<std::int64_t>(offset) + 3 + relative;
						os << ' ' << relative << "  ; -> " << target;
						break;
					}
					default:
						os << ' ' << u16;
						break;
				}
			}
			else if (size == 4 && offset + 4 < code.size())
			{
				const auto u32 = static_cast<std::uint32_t>(code[offset + 1]) |
								 (static_cast<std::uint32_t>(code[offset + 2]) << 8) |
								 (static_cast<std::uint32_t>(code[offset + 3]) << 16) |
								 (static_cast<std::uint32_t>(code[offset + 4]) << 24);
				os << ' ' << u32;
			}
			else if (size != 0)
			{
				os << "  ; truncated";
			}

			os << '\n';
			offset += 1 + size;
		}
	}

	void disassemble(const Module& module, std::ostream& os)
	{
		os << ".meta\n";
		os << "    .module " << module.Name << '\n';
		for (const auto& import : module.Imports)
		{
			os << "    .import " << import << '\n';
		}
		writeAttributes(module.Attributes, os, 1);
		os << ".end\n\n";

		const bool hasTypes = !module.Types.empty();
		const bool hasDeclarations = [&module]
		{
			for (const auto& type : module.Types)
			{
				if (!type.Defined)
				{
					return true;
				}
			}
			for (const auto& function : module.Functions)
			{
				if (!function.Defined)
				{
					return true;
				}
			}
			return false;
		}();

		if (hasTypes)
		{
			os << ".types\n";
			for (const auto& type : module.Types)
			{
				if (!type.Defined)
				{
					continue;
				}
				writeAttributes(type.Attributes, os, 1);
				writeTemplates(type.TemplateParams, os, 1);
				for (const auto& parent : type.Parents)
				{
					os << "    .parent " << accessName(parent.AccessModifier) << ' ' << parent.TypeName << '\n';
				}
				writeIndent(os, 1);
				switch (type.Kind)
				{
					case TypeKind::Class:  os << ".class "; break;
					case TypeKind::Struct: os << ".struct "; break;
					case TypeKind::Enum:   os << ".enum "; break;
				}
				if (type.Exported)
				{
					os << "export ";
				}
				os << type.Name << '\n';

				for (const auto& field : type.Fields)
				{
					writeAttributes(field.Attributes, os, 2);
					writeTemplates(field.TemplateParams, os, 2);
					writeIndent(os, 2);
					os << ".field";
					if (field.Exported)
					{
						os << " export";
					}
					os << ' ' << accessName(field.AccessModifier) << ' ' << field.Type << ' '
					   << field.Name << '\n';
				}

				for (const auto& enumerator : type.Enumerators)
				{
					os << "        .item " << enumerator << '\n';
				}

				for (const auto& method : type.Methods)
				{
					writeMethodHeader(method, os, 2);
					if (method.Locals != 0)
					{
						os << "            .locals " << method.Locals << '\n';
					}
					disassembleBody(method.Body, os);
					os << "        .end\n";
				}

				os << "    .end\n\n";
			}
			os << ".end\n\n";
		}

		if (!module.Globals.empty())
		{
			os << ".data\n";
			for (const auto& global : module.Globals)
			{
				writeAttributes(global.Attributes, os, 1);
				writeTemplates(global.TemplateParams, os, 1);
				writeIndent(os, 1);
				if (global.Constant)
				{
					os << ".constant";
					if (global.Exported)
					{
						os << " export";
					}
					os << ' ' << global.Type << ' ' << global.Name << ' ' << formatValue(global.InitialValue) << '\n';
				}
				else
				{
					os << ".variable";
					if (global.Exported)
					{
						os << " export";
					}
					os << ' ' << global.Type << ' ' << global.Name;
					if (!std::holds_alternative<Null>(global.InitialValue))
					{
						os << ' ' << formatValue(global.InitialValue);
					}
					os << '\n';
				}
			}
			os << ".end\n\n";
		}

		if (!module.Functions.empty())
		{
			os << ".text\n";
			for (const auto& function : module.Functions)
			{
				if (!function.Defined)
				{
					continue;
				}
				writeFunctionHeader(function, os, 1);
				if (function.Locals != 0)
				{
					os << "        .locals " << function.Locals << '\n';
				}
				disassembleBody(function.Body, os);
				os << "    .end\n\n";
			}
			os << ".end\n\n";
		}

		if (hasDeclarations)
		{
			os << ".extern\n";
			for (const auto& type : module.Types)
			{
				if (type.Defined)
				{
					continue;
				}
				writeAttributes(type.Attributes, os, 1);
				writeTemplates(type.TemplateParams, os, 1);
				writeIndent(os, 1);
				switch (type.Kind)
				{
					case TypeKind::Class:  os << ".class "; break;
					case TypeKind::Struct: os << ".struct "; break;
					case TypeKind::Enum:   os << ".enum "; break;
				}
				if (type.Exported)
				{
					os << "export ";
				}
				os << type.Name << '\n';

				for (const auto& field : type.Fields)
				{
					writeIndent(os, 2);
					os << ".field " << accessName(field.AccessModifier) << ' ' << field.Type << ' '
					   << field.Name << '\n';
				}
				for (const auto& enumerator : type.Enumerators)
				{
					os << "        .item " << enumerator << '\n';
				}
				for (const auto& method : type.Methods)
				{
					writeMethodHeader(method, os, 2);
				}
				os << "    .end\n\n";
			}

			for (const auto& function : module.Functions)
			{
				if (function.Defined)
				{
					continue;
				}
				writeFunctionHeader(function, os, 1);
			}
			os << ".end\n";
		}
	}

	std::string disassemble(const Module& module)
	{
		std::ostringstream os;
		disassemble(module, os);
		return os.str();
	}
}
