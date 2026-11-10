#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>

#include <xenon/bytecode/opcode.hpp>
#include <xenon/bytecode/xenon_binary.hpp>
#include <xenon/module/module.hpp>
#include <xenon/runtime/error.hpp>
#include <xenon/runtime/runtime.hpp>

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
	xenon::Runtime runtime;

	const auto point = runtime.newObject("Point");
	runtime.setField(point, "x", 1.5);
	runtime.setField(point, "y", 2.5);

	check(runtime.objectKind(point) == xenon::ObjectKind::Object, "wrong object kind");
	check(runtime.className(point) == "Point", "wrong class name");

	const xenon::Value fieldX = runtime.getField(point, "x");
	const auto* x = std::get_if<double>(&fieldX);
	check(x != nullptr && std::abs(*x - 1.5) < 1e-12, "getField('x') wrong");
	check(runtime.heap().size() == 1, "heap size wrong");
}

void testArraysAndStrings()
{
	xenon::Runtime runtime;

	const auto arr = runtime.newArray(3);
	runtime.setElement(arr, 0, 10.0);
	runtime.setElement(arr, 2, 30.0);

	check(runtime.arrayLength(arr) == 3, "array length wrong");
	const xenon::Value elem0 = runtime.getElement(arr, 0);
	const auto* first = std::get_if<double>(&elem0);
	check(first != nullptr && std::abs(*first - 10.0) < 1e-12, "array[0] wrong");

	const auto str = runtime.newString("hello");
	check(runtime.objectKind(str) == xenon::ObjectKind::String, "string kind wrong");
	check(runtime.stringValue(str) == "hello", "string value wrong");
}

void testStack()
{
	xenon::Runtime runtime;

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
	check(xenon::Runtime::isNumber(xenon::Value{1.5}), "isNumber(1.5) false");
	check(xenon::Runtime::isBoolean(xenon::Value{true}), "isBoolean(true) false");
	check(xenon::Runtime::isNull(xenon::Value{xenon::Null{}}), "isNull(null) false");
	check(xenon::Runtime::isRef(xenon::Value{xenon::Ref{1}}), "isRef(ref) false");
	check(!xenon::Runtime::isNumber(xenon::Value{true}), "isNumber(true) true");
}

void testRunFunction()
{
	xenon::XenonBinary body;
	const auto a = body.addConstant(2.0);
	const auto b = body.addConstant(3.0);
	body.write(xenon::OpCode::Constant, 1);
	body.write(static_cast<std::uint8_t>(a), 1);
	body.write(xenon::OpCode::Constant, 1);
	body.write(static_cast<std::uint8_t>(b), 1);
	body.write(xenon::OpCode::Add, 1);
	body.write(xenon::OpCode::Return, 1);

	xenon::Module module;
	module.Name = "Math";
	xenon::Symbol add;
	add.Name      = "add";
	add.Kind      = xenon::SymbolKind::Function;
	add.Signature = "double add(double, double)";
	add.Body      = std::move(body);
	module.Symbols.push_back(std::move(add));

	xenon::Runtime runtime;
	runtime.addModule(module);

	check(runtime.find("add") != nullptr, "runtime.find('add') failed");
	check(runtime.run("add") == xenon::InterpretResult::Ok, "runtime.run('add') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 5.0) < 1e-12, "run result wrong");
}

void testBadFieldThrows()
{
	xenon::Runtime runtime;
	const auto obj = runtime.newObject("X");

	bool threw = false;
	try
	{
		(void)runtime.getField(obj, "missing");
	}
	catch (const xenon::RuntimeError&)
	{
		threw = true;
	}
	check(threw, "missing field did not throw RuntimeError");
}

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
