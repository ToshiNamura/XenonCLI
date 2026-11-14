#pragma once

#ifndef XENON__ASM_BINARY_HPP
#define XENON__ASM_BINARY_HPP 1

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <xenon/asm/opcode.hpp>
#include <xenon/runtime/value.hpp>

namespace xenon
{
	/// A contiguous block of executable Xenon ASM together with its constant and
	/// string pools — the unit of code the virtual machine executes.
	///
	/// Instructions are encoded as a 1-byte opcode followed by an optional
	/// little-endian operand. Operands that index a pool are 16-bit, so a body can
	/// address up to 65535 constants, strings, globals, functions or fields.
	class XenonBinary
	{
	public:
		/// Largest value an operand can hold.
		static constexpr std::size_t MaxOperand = 0xFFFF;

		/// Appends a raw byte to the code stream.
		void write(std::uint8_t byte, int line);

		/// Appends an opcode to the code stream.
		void write(Instruction op, int line);

		/// Adds `value` to the constant pool and returns its index.
		[[nodiscard]] std::size_t addConstant(Value value);

		/// Adds `value` to the string pool and returns its index.
		[[nodiscard]] std::size_t addString(std::string value);

		/// Returns the current code stream length (the next byte offset).
		[[nodiscard]] std::size_t codePosition() const noexcept
		{
			return _code.size();
		}

		/// Overwrites two bytes at `offset` (little-endian, used to patch jumps).
		void patchU16(std::size_t offset, std::uint16_t value);

		[[nodiscard]] const std::vector<std::uint8_t>& code() const noexcept
		{
			return _code;
		}

		[[nodiscard]] const std::vector<Value>& constants() const noexcept
		{
			return _constants;
		}

		[[nodiscard]] const std::vector<std::string>& strings() const noexcept
		{
			return _strings;
		}

		[[nodiscard]] const std::vector<int>& lines() const noexcept
		{
			return _lines;
		}

		[[nodiscard]] std::size_t size() const noexcept
		{
			return _code.size();
		}

	private:
		std::vector<std::uint8_t> _code;
		std::vector<Value>        _constants;
		std::vector<std::string>  _strings; ///< String literals and names referenced by instructions.
		std::vector<int>          _lines;   ///< Source line for every byte (diagnostics).
	};
}

#endif // !XENON__ASM_BINARY_HPP
