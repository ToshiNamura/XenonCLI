#include <xenon/module/module_file.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace xenon
{
	constexpr char Magic[4] = {'X', 'E', 'N', 'M'};
	constexpr std::uint32_t Version = 4;

	constexpr std::uint32_t MaxCount = 1u << 22;

	void checkCount(std::uint32_t count)
	{
		if (count > xenon::MaxCount)
		{
			throw xenon::ModuleFileError("malformed module file (count too large)");
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
		xenon::writeU32(os, static_cast<std::uint32_t>(value));
	}

	void writeString(std::ostream& os, std::string_view value)
	{
		xenon::writeU32(os, static_cast<std::uint32_t>(value.size()));
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
			xenon::writeU64(os, bits);
			return;
		}

		if (const auto* ref = std::get_if<Ref>(&value))
		{
			os.put(3);
			xenon::writeU32(os, ref->id);
			return;
		}
		os.put(0); // Null
	}

	void writeBinary(std::ostream& os, const xenon::XenonBinary& binary)
	{
		const auto& code = binary.code();
		xenon::writeU32(os, static_cast<std::uint32_t>(code.size()));
		if (!code.empty())
		{
			os.write(reinterpret_cast<const char*>(code.data()), static_cast<std::streamsize>(code.size()));
		}

		const auto& constants = binary.constants();
		xenon::writeU32(os, static_cast<std::uint32_t>(constants.size()));
		for (const auto& constant : constants)
		{
			xenon::writeValue(os, constant);
		}

		const auto& strings = binary.strings();
		xenon::writeU32(os, static_cast<std::uint32_t>(strings.size()));
		for (const auto& string : strings)
		{
			xenon::writeString(os, string);
		}

		const auto& lines = binary.lines();
		xenon::writeU32(os, static_cast<std::uint32_t>(lines.size()));
		for (const int line : lines)
		{
			xenon::writeI32(os, line);
		}
	}

	std::uint32_t readU32(std::istream& is)
	{
		char bytes[4] = {};
		is.read(bytes, 4);
		if (is.gcount() != 4)
		{
			throw xenon::ModuleFileError("truncated module file");
		}
		return static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[0]))         |
			   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[1])) << 8)  |
			   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[2])) << 16) |
			   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[3])) << 24);
	}

	std::uint64_t readU64(std::istream& is)
	{
		char bytes[8] = {};
		is.read(bytes, 8);
		if (is.gcount() != 8)
		{
			throw xenon::ModuleFileError("truncated module file");
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
		return static_cast<std::int32_t>(xenon::readU32(is));
	}

	std::string readString(std::istream& is)
	{
		const auto length = xenon::readU32(is);
		xenon::checkCount(length);
		std::string value(length, '\0');
		if (length != 0)
		{
			is.read(value.data(), static_cast<std::streamsize>(length));
			if (is.gcount() != static_cast<std::streamsize>(length))
			{
				throw xenon::ModuleFileError("truncated module file");
			}
		}
		return value;
	}

	xenon::Value readValue(std::istream& is)
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
				const auto bits = xenon::readU64(is);
				double number   = 0.0;
				std::memcpy(&number, &bits, sizeof number);
				return Value{number};
			}
			case 3:
				return Value{Ref{xenon::readU32(is)}};
			default:
				throw ModuleFileError("unknown value tag in module file");
		}
	}

	xenon::XenonBinary readBinary(std::istream& is)
	{
		xenon::XenonBinary binary;

		const auto codeSize = xenon::readU32(is);
		xenon::checkCount(codeSize);
		std::vector<std::uint8_t> code(codeSize);
		if (codeSize != 0)
		{
			is.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(codeSize));
			if (is.gcount() != static_cast<std::streamsize>(codeSize))
			{
				throw xenon::ModuleFileError("truncated module file");
			}
		}

		const auto constantCount = xenon::readU32(is);
		xenon::checkCount(constantCount);
		for (std::uint32_t i = 0; i < constantCount; ++i)
		{
			(void)binary.addConstant(xenon::readValue(is));
		}

		const auto stringCount = xenon::readU32(is);
		xenon::checkCount(stringCount);
		for (std::uint32_t i = 0; i < stringCount; ++i)
		{
			(void)binary.addString(xenon::readString(is));
		}

		const auto lineCount = xenon::readU32(is);
		xenon::checkCount(lineCount);
		std::vector<int> lines(lineCount);
		for (std::uint32_t i = 0; i < lineCount; ++i)
		{
			lines[i] = xenon::readI32(is);
		}

		for (std::size_t i = 0; i < code.size(); ++i)
		{
			const int line = i < lines.size() ? lines[i] : 0;
			binary.write(code[i], line);
		}

		return binary;
	}
}

void xenon::writeModuleFile(std::ostream& os, const Module& module)
{
	os.write(xenon::Magic, 4);
	xenon::writeU32(os, xenon::Version);
	xenon::writeString(os, module.Name);

	xenon::writeU32(os, static_cast<std::uint32_t>(module.Imports.size()));
	for (const auto& import : module.Imports)
	{
		xenon::writeString(os, import);
	}

	xenon::writeU32(os, static_cast<std::uint32_t>(module.Symbols.size()));
	for (const auto& symbol : module.Symbols)
	{
		os.put(static_cast<char>(static_cast<std::uint8_t>(symbol.Kind)));
		os.put(symbol.Defined ? 1 : 0);
		xenon::writeString(os, symbol.Name);
		xenon::writeString(os, symbol.Signature);
		xenon::writeU32(os, static_cast<std::uint32_t>(symbol.Arity));
		xenon::writeU32(os, static_cast<std::uint32_t>(symbol.Locals));
		xenon::writeBinary(os, symbol.Body);
		xenon::writeU32(os, static_cast<std::uint32_t>(symbol.Fields.size()));
		for (const auto& field : symbol.Fields)
		{
			xenon::writeString(os, field);
		}
		xenon::writeU32(os, static_cast<std::uint32_t>(symbol.Enumerators.size()));
		for (const auto& enumerator : symbol.Enumerators)
		{
			xenon::writeString(os, enumerator);
		}
		xenon::writeValue(os, symbol.InitialValue);
	}
}

void xenon::readModuleFile(std::istream& is, Module& out)
{
	char magic[4] = {};
	is.read(magic, 4);
	if (is.gcount() != 4 || std::memcmp(magic, xenon::Magic, 4) != 0)
	{
		throw ModuleFileError("not a Xenon module file (bad magic)");
	}

	const auto version = xenon::readU32(is);
	if (version != xenon::Version)
	{
		throw ModuleFileError("unsupported module file version");
	}

	out      = Module{};
	out.Name = xenon::readString(is);

	const auto importCount = xenon::readU32(is);
	xenon::checkCount(importCount);
	out.Imports.reserve(importCount);
	for (std::uint32_t i = 0; i < importCount; ++i)
	{
		out.Imports.emplace_back(xenon::readString(is));
	}

	const auto symbolCount = xenon::readU32(is);
	xenon::checkCount(symbolCount);
	out.Symbols.reserve(symbolCount);
	for (std::uint32_t i = 0; i < symbolCount; ++i)
	{
		const int kind = is.get();
		if (kind < 0 || kind > static_cast<int>(xenon::SymbolKind::Variable))
		{
			throw xenon::ModuleFileError("invalid symbol kind in module file");
		}

		const int defined = is.get();
		if (defined < 0)
		{
			throw xenon::ModuleFileError("truncated module file");
		}

		Symbol symbol;
		symbol.Kind      = static_cast<xenon::SymbolKind>(kind);
		symbol.Defined   = defined != 0;
		symbol.Name      = xenon::readString(is);
		symbol.Signature = xenon::readString(is);
		symbol.Arity     = xenon::readU32(is);
		symbol.Locals    = xenon::readU32(is);
		symbol.Body      = xenon::readBinary(is);

		const auto fieldCount = xenon::readU32(is);
		xenon::checkCount(fieldCount);
		symbol.Fields.reserve(fieldCount);
		for (std::uint32_t f = 0; f < fieldCount; ++f)
		{
			symbol.Fields.emplace_back(xenon::readString(is));
		}

		const auto enumeratorCount = xenon::readU32(is);
		xenon::checkCount(enumeratorCount);
		symbol.Enumerators.reserve(enumeratorCount);
		for (std::uint32_t e = 0; e < enumeratorCount; ++e)
		{
			symbol.Enumerators.emplace_back(xenon::readString(is));
		}

		symbol.InitialValue = xenon::readValue(is);

		out.Symbols.push_back(std::move(symbol));
	}
}

void xenon::writeModuleFile(const std::string& path, const Module& module)
{
	std::ofstream output(path, std::ios::binary);
	if (!output)
	{
		throw xenon::ModuleFileError("cannot open module file for writing: " + path);
	}
	xenon::writeModuleFile(output, module);
}

void xenon::readModuleFile(const std::string& path, Module& out)
{
	std::ifstream input(path, std::ios::binary);
	if (!input)
	{
		throw xenon::ModuleFileError("cannot open module file for reading: " + path);
	}
	xenon::readModuleFile(input, out);
}
