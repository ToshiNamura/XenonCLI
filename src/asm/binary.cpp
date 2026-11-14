#include <xenon/asm/binary.hpp>

#include <utility>

void xenon::XenonBinary::write(std::uint8_t byte, int line)
{
	_code.push_back(byte);
	_lines.push_back(line);
}

void xenon::XenonBinary::write(xenon::Instruction op, int line)
{
	write(static_cast<std::uint8_t>(op), line);
}

std::size_t xenon::XenonBinary::addConstant(xenon::Value value)
{
	_constants.push_back(std::move(value));
	return _constants.size() - 1;
}

std::size_t xenon::XenonBinary::addString(std::string value)
{
	_strings.push_back(std::move(value));
	return _strings.size() - 1;
}

void xenon::XenonBinary::patchU16(std::size_t offset, std::uint16_t value)
{
	_code[offset]      = static_cast<std::uint8_t>(value & 0xff);
	_code[offset + 1]  = static_cast<std::uint8_t>((value >> 8) & 0xff);
}
