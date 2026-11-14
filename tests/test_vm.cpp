#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>

#include <xenon/asm/binary.hpp>
#include <xenon/asm/opcode.hpp>
#include <xenon/runtime/error.hpp>
#include <xenon/runtime/virtual_machine.hpp>
#include <xenon/runtime/virtual_stack.hpp>

int g_failures = 0;

void check(bool condition, const std::string& message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		++g_failures;
	}
}

void writeU16(xenon::XenonBinary& binary, std::size_t value)
{
	binary.write(static_cast<std::uint8_t>(value & 0xff), 1);
	binary.write(static_cast<std::uint8_t>((value >> 8) & 0xff), 1);
}

xenon::XenonBinary makeAddBinary()
{
	xenon::XenonBinary binary;
	const auto a = binary.addConstant(1.2);
	const auto b = binary.addConstant(3.4);
	binary.write(xenon::Instruction::Const, 1);
	writeU16(binary, a);
	binary.write(xenon::Instruction::Const, 1);
	writeU16(binary, b);
	binary.write(xenon::Instruction::Add, 1);
	binary.write(xenon::Instruction::Ret, 1);
	return binary;
}

void testAddition()
{
	xenon::VirtualMachine vm;
	xenon::VirtualStack   stack;
	const auto result = vm.interpret(makeAddBinary(), stack);
	check(result == xenon::InterpretResult::Ok, "interpret() did not return Ok");

	const auto* number = std::get_if<double>(&vm.lastValue());
	check(number != nullptr, "result is not a number");
	if (number != nullptr)
	{
		check(std::abs(*number - 4.6) < 1e-12, "1.2 + 3.4 did not equal 4.6");
	}
}

void testDivisionByZero()
{
	xenon::XenonBinary binary;
	const auto a = binary.addConstant(10.0);
	const auto b = binary.addConstant(0.0);
	binary.write(xenon::Instruction::Const, 1);
	writeU16(binary, a);
	binary.write(xenon::Instruction::Const, 1);
	writeU16(binary, b);
	binary.write(xenon::Instruction::Div, 1);
	binary.write(xenon::Instruction::Ret, 1);

	xenon::VirtualMachine vm;
	xenon::VirtualStack   stack;
	bool threw = false;
	try
	{
		(void)vm.interpret(binary, stack);
	}
	catch (const xenon::RuntimeError&)
	{
		threw = true;
	}
	check(threw, "division by zero did not throw RuntimeError");
}

void testWideConstantPool()
{
	xenon::XenonBinary binary;
	std::size_t index = 0;
	for (int i = 0; i <= 300; ++i)
	{
		index = binary.addConstant(static_cast<double>(i));
	}

	binary.write(xenon::Instruction::Const, 1);
	writeU16(binary, index);
	binary.write(xenon::Instruction::Ret, 1);

	xenon::VirtualMachine vm;
	xenon::VirtualStack   stack;
	check(vm.interpret(binary, stack) == xenon::InterpretResult::Ok, "wide constant pool failed");
	const auto* number = std::get_if<double>(&vm.lastValue());
	check(number != nullptr && *number == 300.0, "constant beyond index 255 was not loaded");
}

void testRelativeJump()
{
	xenon::XenonBinary binary;
	const auto condition = binary.addConstant(false);
	const auto skipped   = binary.addConstant(42.0);
	const auto answer    = binary.addConstant(7.0);

	binary.write(xenon::Instruction::Const, 1);
	writeU16(binary, condition);
	binary.write(xenon::Instruction::Jif, 1);
	const auto jumpOffset = binary.codePosition();
	writeU16(binary, 0);

	binary.write(xenon::Instruction::Const, 1);
	writeU16(binary, skipped);
	binary.write(xenon::Instruction::Ret, 1);

	const auto target   = binary.codePosition();
	const auto relative = static_cast<std::int16_t>(target - (jumpOffset + 2));
	binary.patchU16(jumpOffset, static_cast<std::uint16_t>(relative));

	binary.write(xenon::Instruction::Const, 1);
	writeU16(binary, answer);
	binary.write(xenon::Instruction::Ret, 1);

	xenon::VirtualMachine vm;
	xenon::VirtualStack   stack;
	check(vm.interpret(binary, stack) == xenon::InterpretResult::Ok, "interpret() failed");
	const auto* number = std::get_if<double>(&vm.lastValue());
	check(number != nullptr && *number == 7.0, "relative jump landed on the wrong instruction");
}

int main()
{
	try
	{
		testAddition();
		testDivisionByZero();
		testWideConstantPool();
		testRelativeJump();
	}
	catch (const std::exception& error)
	{
		std::cerr << "EXCEPTION: " << error.what() << '\n';
		return EXIT_FAILURE;
	}

	if (g_failures == 0)
	{
		std::cout << "All tests passed.\n";
		return EXIT_SUCCESS;
	}

	std::cerr << g_failures << " test(s) failed.\n";
	return EXIT_FAILURE;
}
