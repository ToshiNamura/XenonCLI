#include "xenon/module/module_file.hpp"

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
namespace
{

constexpr char k_magic[4] = {'X', 'E', 'N', 'M'};
constexpr std::uint32_t k_version = 1;

constexpr std::uint32_t k_max_count = 1u << 22;

void check_count(std::uint32_t count)
{
    if (count > k_max_count)
    {
        throw ModuleFileError("malformed module file (count too large)");
    }
}

void write_u32(std::ostream& os, std::uint32_t value)
{
    const char bytes[4] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff),
    };
    os.write(bytes, 4);
}

void write_u64(std::ostream& os, std::uint64_t value)
{
    const char bytes[8] = {
        static_cast<char>(value & 0xff),         static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff), static_cast<char>((value >> 24) & 0xff),
        static_cast<char>((value >> 32) & 0xff), static_cast<char>((value >> 40) & 0xff),
        static_cast<char>((value >> 48) & 0xff), static_cast<char>((value >> 56) & 0xff),
    };
    os.write(bytes, 8);
}

void write_i32(std::ostream& os, std::int32_t value)
{
    write_u32(os, static_cast<std::uint32_t>(value));
}

void write_string(std::ostream& os, std::string_view value)
{
    write_u32(os, static_cast<std::uint32_t>(value.size()));
    if (!value.empty())
    {
        os.write(value.data(), static_cast<std::streamsize>(value.size()));
    }
}

void write_value(std::ostream& os, const Value& value)
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
        write_u64(os, bits);
        return;
    }
    if (const auto* ref = std::get_if<Ref>(&value))
    {
        os.put(3);
        write_u32(os, ref->id);
        return;
    }
    os.put(0); // Null
}

void write_chunk(std::ostream& os, const Chunk& chunk)
{
    const auto& code = chunk.code();
    write_u32(os, static_cast<std::uint32_t>(code.size()));
    if (!code.empty())
    {
        os.write(reinterpret_cast<const char*>(code.data()),
                 static_cast<std::streamsize>(code.size()));
    }

    const auto& constants = chunk.constants();
    write_u32(os, static_cast<std::uint32_t>(constants.size()));
    for (const auto& constant : constants)
    {
        write_value(os, constant);
    }

    const auto& lines = chunk.lines();
    write_u32(os, static_cast<std::uint32_t>(lines.size()));
    for (const int line : lines)
    {
        write_i32(os, line);
    }
}

std::uint32_t read_u32(std::istream& is)
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

std::uint64_t read_u64(std::istream& is)
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

std::int32_t read_i32(std::istream& is)
{
    return static_cast<std::int32_t>(read_u32(is));
}

std::string read_string(std::istream& is)
{
    const auto length = read_u32(is);
    check_count(length);
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

Value read_value(std::istream& is)
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
        const auto bits = read_u64(is);
        double number = 0.0;
        std::memcpy(&number, &bits, sizeof number);
        return Value{number};
    }
    case 3:
        return Value{Ref{read_u32(is)}};
    default:
        throw ModuleFileError("unknown value tag in module file");
    }
}

Chunk read_chunk(std::istream& is)
{
    Chunk chunk;

    const auto code_size = read_u32(is);
    check_count(code_size);
    std::vector<std::uint8_t> code(code_size);
    if (code_size != 0)
    {
        is.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(code_size));
        if (is.gcount() != static_cast<std::streamsize>(code_size))
        {
            throw ModuleFileError("truncated module file");
        }
    }

    const auto constant_count = read_u32(is);
    check_count(constant_count);
    for (std::uint32_t i = 0; i < constant_count; ++i)
    {
        (void)chunk.add_constant(read_value(is));
    }

    const auto line_count = read_u32(is);
    check_count(line_count);
    std::vector<int> lines(line_count);
    for (std::uint32_t i = 0; i < line_count; ++i)
    {
        lines[i] = read_i32(is);
    }

    for (std::size_t i = 0; i < code.size(); ++i)
    {
        const int line = i < lines.size() ? lines[i] : 0;
        chunk.write(code[i], line);
    }

    return chunk;
}

} // namespace

void write_module_file(std::ostream& os, const Module& module)
{
    os.write(k_magic, 4);
    write_u32(os, k_version);
    write_string(os, module.name);

    write_u32(os, static_cast<std::uint32_t>(module.imports.size()));
    for (const auto& import : module.imports)
    {
        write_string(os, import);
    }

    write_u32(os, static_cast<std::uint32_t>(module.symbols.size()));
    for (const auto& symbol : module.symbols)
    {
        os.put(static_cast<char>(static_cast<std::uint8_t>(symbol.kind)));
        write_string(os, symbol.name);
        write_string(os, symbol.signature);
        write_u32(os, static_cast<std::uint32_t>(symbol.arity));
        write_u32(os, static_cast<std::uint32_t>(symbol.locals));
        write_chunk(os, symbol.body);
    }
}

void read_module_file(std::istream& is, Module& out)
{
    char magic[4] = {};
    is.read(magic, 4);
    if (is.gcount() != 4 || std::memcmp(magic, k_magic, 4) != 0)
    {
        throw ModuleFileError("not a Xenon module file (bad magic)");
    }

    const auto version = read_u32(is);
    if (version != k_version)
    {
        throw ModuleFileError("unsupported module file version");
    }

    out = Module{};
    out.name = read_string(is);

    const auto import_count = read_u32(is);
    check_count(import_count);
    out.imports.reserve(import_count);
    for (std::uint32_t i = 0; i < import_count; ++i)
    {
        out.imports.emplace_back(read_string(is));
    }

    const auto symbol_count = read_u32(is);
    check_count(symbol_count);
    out.symbols.reserve(symbol_count);
    for (std::uint32_t i = 0; i < symbol_count; ++i)
    {
        const int kind = is.get();
        if (kind < 0 || kind > static_cast<int>(SymbolKind::Variable))
        {
            throw ModuleFileError("invalid symbol kind in module file");
        }

        Symbol symbol;
        symbol.kind = static_cast<SymbolKind>(kind);
        symbol.name = read_string(is);
        symbol.signature = read_string(is);
        symbol.arity = read_u32(is);
        symbol.locals = read_u32(is);
        symbol.body = read_chunk(is);
        out.symbols.push_back(std::move(symbol));
    }
}

void write_module_file(const std::string& path, const Module& module)
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        throw ModuleFileError("cannot open module file for writing: " + path);
    }
    write_module_file(output, module);
}

void read_module_file(const std::string& path, Module& out)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw ModuleFileError("cannot open module file for reading: " + path);
    }
    read_module_file(input, out);
}

} // namespace xenon
