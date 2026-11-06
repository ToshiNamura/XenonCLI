#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>

#include "xenon/bytecode/chunk.hpp"
#include "xenon/bytecode/opcode.hpp"
#include "xenon/module/module.hpp"
#include "xenon/runtime/env.hpp"
#include "xenon/runtime/error.hpp"

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

void test_objects()
{
    using namespace xenon;

    Env env;

    const auto point = env.new_object("Point");
    env.set_field(point, "x", 1.5);
    env.set_field(point, "y", 2.5);

    check(env.object_kind(point) == ObjectKind::Object, "wrong object kind");
    check(env.class_name(point) == "Point", "wrong class name");

    const Value field_x = env.get_field(point, "x");
    const auto* x = std::get_if<double>(&field_x);
    check(x != nullptr && std::abs(*x - 1.5) < 1e-12, "get_field('x') wrong");
    check(env.heap().size() == 1, "heap size wrong");
}

void test_arrays_and_strings()
{
    using namespace xenon;

    Env env;

    const auto arr = env.new_array(3);
    env.set_element(arr, 0, 10.0);
    env.set_element(arr, 2, 30.0);

    check(env.array_length(arr) == 3, "array length wrong");
    const Value elem0 = env.get_element(arr, 0);
    const auto* first = std::get_if<double>(&elem0);
    check(first != nullptr && std::abs(*first - 10.0) < 1e-12, "array[0] wrong");

    const auto str = env.new_string("hello");
    check(env.object_kind(str) == ObjectKind::String, "string kind wrong");
    check(env.string_value(str) == "hello", "string value wrong");
}

void test_stack()
{
    using namespace xenon;

    Env env;

    env.push(1.0);
    env.push(2.0);
    env.push(3.0);
    check(env.stack_size() == 3, "stack size wrong");
    check(std::get_if<double>(&env.peek()) != nullptr, "peek wrong type");

    const auto top = env.pop();
    const auto* number = std::get_if<double>(&top);
    check(number != nullptr && std::abs(*number - 3.0) < 1e-12, "pop wrong");
    check(env.stack_size() == 2, "stack size after pop wrong");
}

void test_type_predicates()
{
    using namespace xenon;

    check(Env::is_number(Value{1.5}), "is_number(1.5) false");
    check(Env::is_boolean(Value{true}), "is_boolean(true) false");
    check(Env::is_null(Value{Null{}}), "is_null(null) false");
    check(Env::is_ref(Value{Ref{1}}), "is_ref(ref) false");
    check(!Env::is_number(Value{true}), "is_number(true) true");
}

void test_run_function()
{
    using namespace xenon;

    Chunk body;
    const auto a = body.add_constant(2.0);
    const auto b = body.add_constant(3.0);
    body.write(OpCode::Constant, 1);
    body.write(static_cast<std::uint8_t>(a), 1);
    body.write(OpCode::Constant, 1);
    body.write(static_cast<std::uint8_t>(b), 1);
    body.write(OpCode::Add, 1);
    body.write(OpCode::Return, 1);

    Module module;
    module.name = "Math";
    Symbol add;
    add.name = "add";
    add.kind = SymbolKind::Function;
    add.signature = "double add(double, double)";
    add.body = std::move(body);
    module.symbols.push_back(std::move(add));

    Env env;
    env.load(module);

    check(env.find("add") != nullptr, "env.find('add') failed");
    check(env.run("add") == InterpretResult::Ok, "env.run('add') failed");

    const auto* number = std::get_if<double>(&env.last_value());
    check(number != nullptr && std::abs(*number - 5.0) < 1e-12, "run result wrong");
}

void test_bad_field_throws()
{
    using namespace xenon;

    Env env;
    const auto obj = env.new_object("X");

    bool threw = false;
    try
    {
        (void)env.get_field(obj, "missing");
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
    test_objects();
    test_arrays_and_strings();
    test_stack();
    test_type_predicates();
    test_run_function();
    test_bad_field_throws();

    if (g_failures == 0)
    {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
