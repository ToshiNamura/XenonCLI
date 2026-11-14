#pragma once

#ifndef XENON__ASM_DISASSEMBLER_HPP
#define XENON__ASM_DISASSEMBLER_HPP 1

#include <iosfwd>
#include <string>

#include <xenon/asm/binary.hpp>
#include <xenon/asm/model.hpp>

namespace xenon
{
	/// Writes a textual Xenon ASM listing of `binary` to `os`.
	void disassembleBody(const XenonBinary& binary, std::ostream& os);

	/// Writes `module` as textual Xenon ASM (sections, declarations and bodies).
	void disassemble(const Module& module, std::ostream& os);

	/// Returns `module` as textual Xenon ASM.
	[[nodiscard]] std::string disassemble(const Module& module);
}

#endif // !XENON__ASM_DISASSEMBLER_HPP
