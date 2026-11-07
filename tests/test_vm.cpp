#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>

#include "xenon/bytecode/opcode.hpp"
#include "xenon/bytecode/xenon_binary.hpp"
#include "xenon/runtime/error.hpp"
#include "xenon/runtime/virtual_machine.hpp"
#include "xenon/runtime/virtual_stack.hpp"

namespace
{

int g_failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++g_failures;
    }
}

xenon::XenonBinary makeAddBinary()
{
    using namespace xenon;

    XenonBinary binary;
    const auto a = binary.addConstant(1.2);
    const auto b = binary.addConstant(3.4);
    binary.write(OpCode::Constant, 1);
    binary.write(static_cast<std::uint8_t>(a), 1);
    binary.write(OpCode::Constant, 1);
    binary.write(static_cast<std::uint8_t>(b), 1);
    binary.write(OpCode::Add, 1);
    binary.write(OpCode::Return, 1);
    return binary;
}

void testAddition()
{
    using namespace xenon;

    VirtualMachine vm;
    VirtualStack stack;
    const auto result = vm.interpret(makeAddBinary(), stack);
    check(result == InterpretResult::Ok, "interpret() did not return Ok");

    const auto* number = std::get_if<double>(&vm.lastValue());
    check(number != nullptr, "result is not a number");
    if (number != nullptr)
    {
        check(std::abs(*number - 4.6) < 1e-12, "1.2 + 3.4 did not equal 4.6");
    }
}

void testDivisionByZero()
{
    using namespace xenon;

    XenonBinary binary;
    const auto a = binary.addConstant(10.0);
    const auto b = binary.addConstant(0.0);
    binary.write(OpCode::Constant, 1);
    binary.write(static_cast<std::uint8_t>(a), 1);
    binary.write(OpCode::Constant, 1);
    binary.write(static_cast<std::uint8_t>(b), 1);
    binary.write(OpCode::Divide, 1);
    binary.write(OpCode::Return, 1);

    VirtualMachine vm;
    VirtualStack stack;
    bool threw = false;
    try
    {
        (void)vm.interpret(binary, stack);
    }
    catch (const RuntimeError&)
    {
        threw = true;
    }
    check(threw, "division by zero did not throw RuntimeError");
}

} // namespace

int main()
{
    testAddition();
    testDivisionByZero();

    if (g_failures == 0)
    {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
