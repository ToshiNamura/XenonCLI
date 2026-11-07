#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>

#include "xenon/bytecode/opcode.hpp"
#include "xenon/bytecode/xenon_binary.hpp"
#include "xenon/module/module.hpp"
#include "xenon/runtime/error.hpp"
#include "xenon/runtime/runtime.hpp"

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

void testObjects()
{
    using namespace xenon;

    Runtime runtime;

    const auto point = runtime.newObject("Point");
    runtime.setField(point, "x", 1.5);
    runtime.setField(point, "y", 2.5);

    check(runtime.objectKind(point) == ObjectKind::Object, "wrong object kind");
    check(runtime.className(point) == "Point", "wrong class name");

    const Value fieldX = runtime.getField(point, "x");
    const auto* x = std::get_if<double>(&fieldX);
    check(x != nullptr && std::abs(*x - 1.5) < 1e-12, "getField('x') wrong");
    check(runtime.heap().size() == 1, "heap size wrong");
}

void testArraysAndStrings()
{
    using namespace xenon;

    Runtime runtime;

    const auto arr = runtime.newArray(3);
    runtime.setElement(arr, 0, 10.0);
    runtime.setElement(arr, 2, 30.0);

    check(runtime.arrayLength(arr) == 3, "array length wrong");
    const Value elem0 = runtime.getElement(arr, 0);
    const auto* first = std::get_if<double>(&elem0);
    check(first != nullptr && std::abs(*first - 10.0) < 1e-12, "array[0] wrong");

    const auto str = runtime.newString("hello");
    check(runtime.objectKind(str) == ObjectKind::String, "string kind wrong");
    check(runtime.stringValue(str) == "hello", "string value wrong");
}

void testStack()
{
    using namespace xenon;

    Runtime runtime;

    runtime.push(1.0);
    runtime.push(2.0);
    runtime.push(3.0);
    check(runtime.stackSize() == 3, "stack size wrong");
    check(std::get_if<double>(&runtime.peek()) != nullptr, "peek wrong type");

    const auto top = runtime.pop();
    const auto* number = std::get_if<double>(&top);
    check(number != nullptr && std::abs(*number - 3.0) < 1e-12, "pop wrong");
    check(runtime.stackSize() == 2, "stack size after pop wrong");
}

void testTypePredicates()
{
    using namespace xenon;

    check(Runtime::isNumber(Value{1.5}), "isNumber(1.5) false");
    check(Runtime::isBoolean(Value{true}), "isBoolean(true) false");
    check(Runtime::isNull(Value{Null{}}), "isNull(null) false");
    check(Runtime::isRef(Value{Ref{1}}), "isRef(ref) false");
    check(!Runtime::isNumber(Value{true}), "isNumber(true) true");
}

void testRunFunction()
{
    using namespace xenon;

    XenonBinary body;
    const auto a = body.addConstant(2.0);
    const auto b = body.addConstant(3.0);
    body.write(OpCode::Constant, 1);
    body.write(static_cast<std::uint8_t>(a), 1);
    body.write(OpCode::Constant, 1);
    body.write(static_cast<std::uint8_t>(b), 1);
    body.write(OpCode::Add, 1);
    body.write(OpCode::Return, 1);

    Module module;
    module.Name = "Math";
    Symbol add;
    add.Name = "add";
    add.Kind = SymbolKind::Function;
    add.Signature = "double add(double, double)";
    add.Body = std::move(body);
    module.Symbols.push_back(std::move(add));

    Runtime runtime;
    runtime.addModule(module);

    check(runtime.find("add") != nullptr, "runtime.find('add') failed");
    check(runtime.run("add") == InterpretResult::Ok, "runtime.run('add') failed");

    const auto* number = std::get_if<double>(&runtime.lastValue());
    check(number != nullptr && std::abs(*number - 5.0) < 1e-12, "run result wrong");
}

void testBadFieldThrows()
{
    using namespace xenon;

    Runtime runtime;
    const auto obj = runtime.newObject("X");

    bool threw = false;
    try
    {
        (void)runtime.getField(obj, "missing");
    }
    catch (const RuntimeError&)
    {
        threw = true;
    }
    check(threw, "missing field did not throw RuntimeError");
}

} // namespace

int main()
{
    testObjects();
    testArraysAndStrings();
    testStack();
    testTypePredicates();
    testRunFunction();
    testBadFieldThrows();

    if (g_failures == 0)
    {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
