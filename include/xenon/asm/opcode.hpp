#pragma once

#ifndef XENON__ASM_OPCODE_HPP
#define XENON__ASM_OPCODE_HPP 1

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace xenon
{
	/// Инструкции VM (диапазон опкодов `0x00..0x7F`).
	///
	/// `Jmpa`/`Jifa` — адресные варианты переходов рядом с относительными
	/// `Jmp`/`Jif`.
	enum class Instruction : std::uint8_t
	{
		Const = 0,  ///< const — положить константу (u16 индекс пула констант).
		Str   = 1,  ///< str   — создать String, положить Ref (u16 индекс строки).
		New   = 2,  ///< new   — создать объект, положить Ref (u16 индекс имени типа).
		Getf  = 3,  ///< getf  — снять объект, положить поле (u16 индекс поля в типе).
		Setf  = 4,  ///< setf  — снять значение и объект, записать поле (u16 индекс поля).
		Ldg   = 5,  ///< ldg   — положить глобальную переменную (u16 индекс имени).
		Stg   = 6,  ///< stg   — записать вершину в глобальную (u16 индекс имени).
		Ldl   = 7,  ///< ldl   — положить локальную переменную (u16 слот).
		Stl   = 8,  ///< stl   — записать вершину в локальную (u16 слот).
		Pop   = 9,  ///< pop   — снять вершину.
		Add   = 10, ///< add
		Sub   = 11, ///< sub
		Mul   = 12, ///< mul
		Div   = 13, ///< div
		Neg   = 14, ///< neg
		Not   = 15, ///< not
		Equ   = 16, ///< equ
		Neq   = 17, ///< neq
		Lss   = 18, ///< lss
		Leq   = 19, ///< leq
		Gtr   = 20, ///< gtr
		Geq   = 21, ///< geq
		Jmp   = 22, ///< jmp  — относительный безусловный переход (i16).
		Jif   = 23, ///< jif  — относительный условный переход (i16).
		Call  = 24, ///< call — вызов функции (u16 индекс имени).
		Ret   = 25, ///< ret  — вернуть вершину как результат.
		Jmpa  = 26, ///< jmpa — адресный безусловный переход (u32).
		Jifa  = 27, ///< jifa — адресный условный переход (u32).
		Mod   = 28, ///< mod  — остаток от деления (`std::fmod`).
		BAnd  = 29, ///< and  — побитовое И над целыми значениями.
		BOr   = 30, ///< or   — побитовое ИЛИ.
		BXor  = 31, ///< xor  — побитовое исключающее ИЛИ.
		BNot  = 32, ///< bnot — побитовое НЕ.
		Shl   = 33, ///< shl  — сдвиг влево.
		Shr   = 34, ///< shr  — сдвиг вправо.
	};

	/// How the assembler and disassembler interpret an instruction operand.
	enum class OperandKind : std::uint8_t
	{
		None,     ///< No operand.
		Constant, ///< u16 index into the constant pool of the body.
		String,   ///< u16 index into the string pool of the body.
		Field,    ///< u16 index of a field in the object's type.
		Local,    ///< u16 local slot.
		Relative, ///< i16 relative code offset.
		Address,  ///< u32 absolute code address.
	};

	/// Директивы (диапазон опкодов `0x80..0xFF`).
	enum class Directive : std::uint8_t
	{
		Meta      = 0x80, ///< .meta
		Types     = 0x81, ///< .types
		Data      = 0x82, ///< .data
		Text      = 0x83, ///< .text
		Extern    = 0x84, ///< .extern
		Module    = 0x85, ///< .module
		Import    = 0x86, ///< .import
		Class     = 0x87, ///< .class
		Struct    = 0x88, ///< .struct
		Enum      = 0x89, ///< .enum
		Parent    = 0x8A, ///< .parent
		Field     = 0x8B, ///< .field
		Method    = 0x8C, ///< .method
		Item      = 0x8D, ///< .item
		Variable  = 0x8E, ///< .variable
		Constant  = 0x8F, ///< .constant
		Function  = 0x90, ///< .function
		Local     = 0x91, ///< .local
		Attribute = 0x92, ///< .attribute
		Template  = 0x93, ///< .template
		End       = 0x94, ///< .end
		Locals    = 0x95, ///< .locals — задать число локальных слотов явно.
	};

	/// True, если опкод `op` — директива (диапазон `0x80..0xFF`).
	[[nodiscard]] constexpr bool isDirective(std::uint8_t op) noexcept
	{
		return op >= 0x80;
	}

	/// Возвращает мнемонику инструкции (например, `"add"`).
	[[nodiscard]] const char* mnemonic(Instruction op) noexcept;

	/// Возвращает имя директивы (например, `".class"`).
	[[nodiscard]] const char* mnemonic(Directive op) noexcept;

	/// Размер операнда инструкции в байтах (для дизассемблера).
	[[nodiscard]] std::size_t operandSize(Instruction op) noexcept;

	/// Вид операнда инструкции (для ассемблера и дизассемблера).
	[[nodiscard]] OperandKind operandKind(Instruction op) noexcept;

	/// Обратный разбор мнемоники: `"add"` → `Instruction::Add`.
	[[nodiscard]] bool instructionFromMnemonic(std::string_view name, Instruction& out) noexcept;

	/// Обратный разбор имени директивы: `".class"` → `Directive::Class`.
	[[nodiscard]] bool directiveFromMnemonic(std::string_view name, Directive& out) noexcept;
}

#endif // !XENON__ASM_OPCODE_HPP
