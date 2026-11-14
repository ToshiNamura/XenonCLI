#include <xenon/module/module_file.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace xenon
{
	namespace
	{
		constexpr char          Magic[4] = {'X', 'E', 'N', 'M'};
		constexpr std::uint32_t Version  = 5;
		constexpr std::uint32_t MaxCount = 1u << 22;

		void checkCount(std::uint32_t count)
		{
			if (count > MaxCount)
			{
				throw ModuleFileError("malformed module file (count too large)");
			}
		}

		void writeU32(std::ostream& os, std::uint32_t value)
		{
			const char bytes[4] = {
				static_cast<char>(value & 0xff),
				static_cast<char>((value >> 8) & 0xff),
				static_cast<char>((value >> 16) & 0xff),
				static_cast<char>((value >> 24) & 0xff),
			};
			os.write(bytes, 4);
		}

		void writeU64(std::ostream& os, std::uint64_t value)
		{
			const char bytes[8] = {
				static_cast<char>(value & 0xff),         static_cast<char>((value >> 8) & 0xff),
				static_cast<char>((value >> 16) & 0xff), static_cast<char>((value >> 24) & 0xff),
				static_cast<char>((value >> 32) & 0xff), static_cast<char>((value >> 40) & 0xff),
				static_cast<char>((value >> 48) & 0xff), static_cast<char>((value >> 56) & 0xff),
			};
			os.write(bytes, 8);
		}

		void writeI32(std::ostream& os, std::int32_t value)
		{
			writeU32(os, static_cast<std::uint32_t>(value));
		}

		void writeString(std::ostream& os, std::string_view value)
		{
			writeU32(os, static_cast<std::uint32_t>(value.size()));
			if (!value.empty())
			{
				os.write(value.data(), static_cast<std::streamsize>(value.size()));
			}
		}

		void writeValue(std::ostream& os, const Value& value)
		{
			if (const auto* boolean = std::get_if<bool>(&value))
			{
				os.put(1);
				os.put(*boolean ? 1 : 0);
				return;
			}

			if (const auto* number = std::get_if<double>(&value))
			{
				os.put(2);
				std::uint64_t bits = 0;
				std::memcpy(&bits, number, sizeof bits);
				writeU64(os, bits);
				return;
			}

			if (const auto* ref = std::get_if<Ref>(&value))
			{
				os.put(3);
				writeU32(os, ref->id);
				return;
			}
			os.put(0); // Null
		}

		void writeBinary(std::ostream& os, const XenonBinary& binary)
		{
			const auto& code = binary.code();
			writeU32(os, static_cast<std::uint32_t>(code.size()));
			if (!code.empty())
			{
				os.write(reinterpret_cast<const char*>(code.data()),
						 static_cast<std::streamsize>(code.size()));
			}

			const auto& constants = binary.constants();
			writeU32(os, static_cast<std::uint32_t>(constants.size()));
			for (const auto& constant : constants)
			{
				writeValue(os, constant);
			}

			const auto& strings = binary.strings();
			writeU32(os, static_cast<std::uint32_t>(strings.size()));
			for (const auto& string : strings)
			{
				writeString(os, string);
			}

			const auto& lines = binary.lines();
			writeU32(os, static_cast<std::uint32_t>(lines.size()));
			for (const int line : lines)
			{
				writeI32(os, line);
			}
		}

		void writeAttributes(std::ostream& os, const AttributeList& attributes)
		{
			writeU32(os, static_cast<std::uint32_t>(attributes.size()));
			for (const auto& attribute : attributes)
			{
				os.put(static_cast<char>(static_cast<std::uint8_t>(attribute->Kind)));
				writeString(os, attribute->Key);
				switch (attribute->Kind)
				{
					case AttributeKind::None:
						break;
					case AttributeKind::Integer:
						writeU64(os, static_cast<std::uint64_t>(
										 static_cast<const AttributeInteger&>(*attribute).Value));
						break;
					case AttributeKind::Float:
					{
						std::uint64_t bits = 0;
						const double value = static_cast<const AttributeFloat&>(*attribute).Value;
						std::memcpy(&bits, &value, sizeof bits);
						writeU64(os, bits);
						break;
					}
					case AttributeKind::String:
						writeString(os, static_cast<const AttributeString&>(*attribute).Value);
						break;
					case AttributeKind::Object:
						writeString(os, static_cast<const AttributeObject&>(*attribute).Object);
						break;
				}
			}
		}

		void writeTemplates(std::ostream& os, const std::vector<TemplateParam>& parameters)
		{
			writeU32(os, static_cast<std::uint32_t>(parameters.size()));
			for (const auto& parameter : parameters)
			{
				os.put(parameter.IsValue ? 1 : 0);
				writeString(os, parameter.TypeName);
				writeString(os, parameter.DefaultType);
				writeString(os, parameter.ValueType);
				writeString(os, parameter.DefaultValue);
			}
		}

		void writeExecutable(std::ostream& os, const Function& function)
		{
			writeString(os, function.Name);
			writeString(os, function.Signature);
			os.put(function.Exported ? 1 : 0);
			os.put(function.Defined ? 1 : 0);
			writeU32(os, static_cast<std::uint32_t>(function.Arity));
			writeU32(os, static_cast<std::uint32_t>(function.Locals));
			writeBinary(os, function.Body);
			writeAttributes(os, function.Attributes);
			writeTemplates(os, function.TemplateParams);
		}

		void writeExecutable(std::ostream& os, const Method& method)
		{
			writeString(os, method.Name);
			writeString(os, method.Signature);
			os.put(static_cast<char>(static_cast<std::uint8_t>(method.AccessModifier)));
			os.put(method.Exported ? 1 : 0);
			os.put(method.Defined ? 1 : 0);
			writeU32(os, static_cast<std::uint32_t>(method.Arity));
			writeU32(os, static_cast<std::uint32_t>(method.Locals));
			writeBinary(os, method.Body);
			writeAttributes(os, method.Attributes);
			writeTemplates(os, method.TemplateParams);
		}

		std::uint32_t readU32(std::istream& is)
		{
			char bytes[4] = {};
			is.read(bytes, 4);
			if (is.gcount() != 4)
			{
				throw ModuleFileError("truncated module file");
			}
			return static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[0])) |
				   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[1])) << 8) |
				   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[2])) << 16) |
				   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[3])) << 24);
		}

		std::uint64_t readU64(std::istream& is)
		{
			char bytes[8] = {};
			is.read(bytes, 8);
			if (is.gcount() != 8)
			{
				throw ModuleFileError("truncated module file");
			}
			std::uint64_t value = 0;
			for (int i = 0; i < 8; ++i)
			{
				value |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(bytes[i])) << (8 * i);
			}
			return value;
		}

		std::int32_t readI32(std::istream& is)
		{
			return static_cast<std::int32_t>(readU32(is));
		}

		std::string readString(std::istream& is)
		{
			const auto length = readU32(is);
			checkCount(length);
			std::string value(length, '\0');
			if (length != 0)
			{
				is.read(value.data(), static_cast<std::streamsize>(length));
				if (is.gcount() != static_cast<std::streamsize>(length))
				{
					throw ModuleFileError("truncated module file");
				}
			}
			return value;
		}

		Value readValue(std::istream& is)
		{
			const int tag = is.get();
			switch (tag)
			{
				case 0:
					return Null{};
				case 1:
				{
					const int boolean = is.get();
					if (boolean < 0)
					{
						throw ModuleFileError("truncated module file");
					}
					return Value{boolean != 0};
				}
				case 2:
				{
					const auto bits = readU64(is);
					double number   = 0.0;
					std::memcpy(&number, &bits, sizeof number);
					return Value{number};
				}
				case 3:
					return Value{Ref{readU32(is)}};
				default:
					throw ModuleFileError("unknown value tag in module file");
			}
		}

		XenonBinary readBinary(std::istream& is)
		{
			XenonBinary binary;

			const auto codeSize = readU32(is);
			checkCount(codeSize);
			std::vector<std::uint8_t> code(codeSize);
			if (codeSize != 0)
			{
				is.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(codeSize));
				if (is.gcount() != static_cast<std::streamsize>(codeSize))
				{
					throw ModuleFileError("truncated module file");
				}
			}

			const auto constantCount = readU32(is);
			checkCount(constantCount);
			for (std::uint32_t i = 0; i < constantCount; ++i)
			{
				(void)binary.addConstant(readValue(is));
			}

			const auto stringCount = readU32(is);
			checkCount(stringCount);
			for (std::uint32_t i = 0; i < stringCount; ++i)
			{
				(void)binary.addString(readString(is));
			}

			const auto lineCount = readU32(is);
			checkCount(lineCount);
			std::vector<int> lines(lineCount);
			for (std::uint32_t i = 0; i < lineCount; ++i)
			{
				lines[i] = readI32(is);
			}

			for (std::size_t i = 0; i < code.size(); ++i)
			{
				const int line = i < lines.size() ? lines[i] : 0;
				binary.write(code[i], line);
			}

			return binary;
		}

		void readAttributes(std::istream& is, AttributeList& attributes)
		{
			const auto count = readU32(is);
			checkCount(count);
			attributes.reserve(count);
			for (std::uint32_t i = 0; i < count; ++i)
			{
				const int kind = is.get();
				if (kind < 0 || kind > static_cast<int>(AttributeKind::Object))
				{
					throw ModuleFileError("invalid attribute kind in module file");
				}

				const std::string key = readString(is);
				std::unique_ptr<Attribute> attribute;
				switch (static_cast<AttributeKind>(kind))
				{
					case AttributeKind::None:
					{
						auto base = std::make_unique<Attribute>();
						base->Key  = key;
						attribute  = std::move(base);
						break;
					}
					case AttributeKind::Integer:
					{
						auto value    = std::make_unique<AttributeInteger>();
						value->Key    = key;
						value->Value  = static_cast<std::int64_t>(readU64(is));
						attribute     = std::move(value);
						break;
					}
					case AttributeKind::Float:
					{
						const auto bits = readU64(is);
						auto value      = std::make_unique<AttributeFloat>();
						value->Key      = key;
						std::memcpy(&value->Value, &bits, sizeof value->Value);
						attribute       = std::move(value);
						break;
					}
					case AttributeKind::String:
					{
						auto value   = std::make_unique<AttributeString>();
						value->Key   = key;
						value->Value = readString(is);
						attribute    = std::move(value);
						break;
					}
					case AttributeKind::Object:
					{
						auto value   = std::make_unique<AttributeObject>();
						value->Key   = key;
						value->Object = readString(is);
						attribute    = std::move(value);
						break;
					}
				}
				attributes.push_back(std::move(attribute));
			}
		}

		void readTemplates(std::istream& is, std::vector<TemplateParam>& parameters)
		{
			const auto count = readU32(is);
			checkCount(count);
			parameters.reserve(count);
			for (std::uint32_t i = 0; i < count; ++i)
			{
				const int isValue = is.get();
				if (isValue < 0)
				{
					throw ModuleFileError("truncated module file");
				}
				TemplateParam parameter;
				parameter.IsValue      = isValue != 0;
				parameter.TypeName     = readString(is);
				parameter.DefaultType  = readString(is);
				parameter.ValueType    = readString(is);
				parameter.DefaultValue = readString(is);
				parameters.push_back(std::move(parameter));
			}
		}

		void readExecutable(std::istream& is, Function& function)
		{
			function.Name          = readString(is);
			function.Signature     = readString(is);
			const int exported     = is.get();
			const int defined      = is.get();
			if (exported < 0 || defined < 0)
			{
				throw ModuleFileError("truncated module file");
			}
			function.Exported      = exported != 0;
			function.Defined       = defined != 0;
			function.Arity         = readU32(is);
			function.Locals        = readU32(is);
			function.Body          = readBinary(is);
			readAttributes(is, function.Attributes);
			readTemplates(is, function.TemplateParams);
		}

		void readExecutable(std::istream& is, Method& method)
		{
			method.Name      = readString(is);
			method.Signature = readString(is);
			const int access = is.get();
			const int exported = is.get();
			const int defined  = is.get();
			if (access < 0 || exported < 0 || defined < 0 ||
				access > static_cast<int>(Access::Public))
			{
				throw ModuleFileError("invalid method in module file");
			}
			method.AccessModifier = static_cast<Access>(access);
			method.Exported       = exported != 0;
			method.Defined        = defined != 0;
			method.Arity          = readU32(is);
			method.Locals         = readU32(is);
			method.Body           = readBinary(is);
			readAttributes(is, method.Attributes);
			readTemplates(is, method.TemplateParams);
		}
	}
}

void xenon::writeModuleFile(std::ostream& os, const Module& module)
{
	os.write(Magic, 4);
	writeU32(os, Version);
	writeString(os, module.Name);

	writeU32(os, static_cast<std::uint32_t>(module.Imports.size()));
	for (const auto& import : module.Imports)
	{
		writeString(os, import);
	}

	writeAttributes(os, module.Attributes);

	writeU32(os, static_cast<std::uint32_t>(module.Functions.size()));
	for (const auto& function : module.Functions)
	{
		writeExecutable(os, function);
	}

	writeU32(os, static_cast<std::uint32_t>(module.Types.size()));
	for (const auto& type : module.Types)
	{
		writeString(os, type.Name);
		os.put(static_cast<char>(static_cast<std::uint8_t>(type.Kind)));
		os.put(type.Exported ? 1 : 0);
		os.put(type.Defined ? 1 : 0);

		writeU32(os, static_cast<std::uint32_t>(type.Fields.size()));
		for (const auto& field : type.Fields)
		{
			writeString(os, field.Type);
			writeString(os, field.Name);
			os.put(static_cast<char>(static_cast<std::uint8_t>(field.AccessModifier)));
			os.put(field.Exported ? 1 : 0);
			writeAttributes(os, field.Attributes);
			writeTemplates(os, field.TemplateParams);
		}

		writeU32(os, static_cast<std::uint32_t>(type.Methods.size()));
		for (const auto& method : type.Methods)
		{
			writeExecutable(os, method);
		}

		writeU32(os, static_cast<std::uint32_t>(type.Enumerators.size()));
		for (const auto& enumerator : type.Enumerators)
		{
			writeString(os, enumerator);
		}

		writeU32(os, static_cast<std::uint32_t>(type.Parents.size()));
		for (const auto& parent : type.Parents)
		{
			os.put(static_cast<char>(static_cast<std::uint8_t>(parent.AccessModifier)));
			writeString(os, parent.TypeName);
		}

		writeAttributes(os, type.Attributes);
		writeTemplates(os, type.TemplateParams);
	}

	writeU32(os, static_cast<std::uint32_t>(module.Globals.size()));
	for (const auto& global : module.Globals)
	{
		writeString(os, global.Type);
		writeString(os, global.Name);
		os.put(global.Exported ? 1 : 0);
		os.put(global.Constant ? 1 : 0);
		writeValue(os, global.InitialValue);
		writeAttributes(os, global.Attributes);
		writeTemplates(os, global.TemplateParams);
	}
}

void xenon::readModuleFile(std::istream& is, Module& out)
{
	char magic[4] = {};
	is.read(magic, 4);
	if (is.gcount() != 4 || std::memcmp(magic, Magic, 4) != 0)
	{
		throw ModuleFileError("not a Xenon module file (bad magic)");
	}

	const auto version = readU32(is);
	if (version != Version)
	{
		throw ModuleFileError("unsupported module file version");
	}

	out      = Module{};
	out.Name = readString(is);

	const auto importCount = readU32(is);
	checkCount(importCount);
	out.Imports.reserve(importCount);
	for (std::uint32_t i = 0; i < importCount; ++i)
	{
		out.Imports.emplace_back(readString(is));
	}

	readAttributes(is, out.Attributes);

	const auto functionCount = readU32(is);
	checkCount(functionCount);
	out.Functions.reserve(functionCount);
	for (std::uint32_t i = 0; i < functionCount; ++i)
	{
		Function function;
		readExecutable(is, function);
		out.Functions.push_back(std::move(function));
	}

	const auto typeCount = readU32(is);
	checkCount(typeCount);
	out.Types.reserve(typeCount);
	for (std::uint32_t i = 0; i < typeCount; ++i)
	{
		Type type;
		type.Name = readString(is);

		const int kind     = is.get();
		const int exported = is.get();
		const int defined  = is.get();
		if (kind < 0 || kind > static_cast<int>(TypeKind::Enum) || exported < 0 || defined < 0)
		{
			throw ModuleFileError("invalid type in module file");
		}
		type.Kind     = static_cast<TypeKind>(kind);
		type.Exported = exported != 0;
		type.Defined  = defined != 0;

		const auto fieldCount = readU32(is);
		checkCount(fieldCount);
		type.Fields.reserve(fieldCount);
		for (std::uint32_t f = 0; f < fieldCount; ++f)
		{
			Field field;
			field.Type      = readString(is);
			field.Name      = readString(is);
			const int access = is.get();
			const int isExported = is.get();
			if (access < 0 || access > static_cast<int>(Access::Public) || isExported < 0)
			{
				throw ModuleFileError("invalid field in module file");
			}
			field.AccessModifier = static_cast<Access>(access);
			field.Exported       = isExported != 0;
			readAttributes(is, field.Attributes);
			readTemplates(is, field.TemplateParams);
			type.Fields.push_back(std::move(field));
		}

		const auto methodCount = readU32(is);
		checkCount(methodCount);
		type.Methods.reserve(methodCount);
		for (std::uint32_t m = 0; m < methodCount; ++m)
		{
			Method method;
			readExecutable(is, method);
			type.Methods.push_back(std::move(method));
		}

		const auto enumeratorCount = readU32(is);
		checkCount(enumeratorCount);
		type.Enumerators.reserve(enumeratorCount);
		for (std::uint32_t e = 0; e < enumeratorCount; ++e)
		{
			type.Enumerators.emplace_back(readString(is));
		}

		const auto parentCount = readU32(is);
		checkCount(parentCount);
		type.Parents.reserve(parentCount);
		for (std::uint32_t p = 0; p < parentCount; ++p)
		{
			const int access = is.get();
			if (access < 0 || access > static_cast<int>(Access::Public))
			{
				throw ModuleFileError("invalid parent in module file");
			}
			Parent parent;
			parent.AccessModifier = static_cast<Access>(access);
			parent.TypeName       = readString(is);
			type.Parents.push_back(std::move(parent));
		}

		readAttributes(is, type.Attributes);
		readTemplates(is, type.TemplateParams);
		out.Types.push_back(std::move(type));
	}

	const auto globalCount = readU32(is);
	checkCount(globalCount);
	out.Globals.reserve(globalCount);
	for (std::uint32_t i = 0; i < globalCount; ++i)
	{
		Global global;
		global.Type = readString(is);
		global.Name = readString(is);
		const int exported = is.get();
		const int constant = is.get();
		if (exported < 0 || constant < 0)
		{
			throw ModuleFileError("truncated module file");
		}
		global.Exported     = exported != 0;
		global.Constant     = constant != 0;
		global.InitialValue = readValue(is);
		readAttributes(is, global.Attributes);
		readTemplates(is, global.TemplateParams);
		out.Globals.push_back(std::move(global));
	}
}

void xenon::writeModuleFile(const std::string& path, const Module& module)
{
	std::ofstream output(path, std::ios::binary);
	if (!output)
	{
		throw ModuleFileError("cannot open module file for writing: " + path);
	}
	writeModuleFile(output, module);
}

void xenon::readModuleFile(const std::string& path, Module& out)
{
	std::ifstream input(path, std::ios::binary);
	if (!input)
	{
		throw ModuleFileError("cannot open module file for reading: " + path);
	}
	readModuleFile(input, out);
}
