#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "xenon/bytecode/chunk.hpp"
#include "xenon/bytecode/opcode.hpp"
#include "xenon/runtime/error.hpp"
#include "xenon/runtime/vm.hpp"

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

xenon::Chunk make_add_chunk()
{
    using namespace xenon;

    Chunk chunk;
    const auto a = chunk.add_constant(1.2);
    const auto b = chunk.add_constant(3.4);
    chunk.write(OpCode::Constant, 1);
    chunk.write(static_cast<std::uint8_t>(a), 1);
    chunk.write(OpCode::Constant, 1);
    chunk.write(static_cast<std::uint8_t>(b), 1);
    chunk.write(OpCode::Add, 1);
    chunk.write(OpCode::Return, 1);
    return chunk;
}

void test_addition()
{
    using namespace xenon;

    VM vm;
    std::vector<Value> stack;
    const auto result = vm.interpret(make_add_chunk(), stack);
    check(result == InterpretResult::Ok, "interpret() did not return Ok");

    const auto* number = std::get_if<double>(&vm.last_value());
    check(number != nullptr, "result is not a number");
    if (number != nullptr)
    {
        check(std::abs(*number - 4.6) < 1e-12, "1.2 + 3.4 did not equal 4.6");
    }
}

void test_division_by_zero()
{
    using namespace xenon;

    Chunk chunk;
    const auto a = chunk.add_constant(10.0);
    const auto b = chunk.add_constant(0.0);
    chunk.write(OpCode::Constant, 1);
    chunk.write(static_cast<std::uint8_t>(a), 1);
    chunk.write(OpCode::Constant, 1);
    chunk.write(static_cast<std::uint8_t>(b), 1);
    chunk.write(OpCode::Divide, 1);
    chunk.write(OpCode::Return, 1);

    VM vm;
    std::vector<Value> stack;
    bool threw = false;
    try
    {
        (void)vm.interpret(chunk, stack);
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
    test_addition();
    test_division_by_zero();

    if (g_failures == 0)
    {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
