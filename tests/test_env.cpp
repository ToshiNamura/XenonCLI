#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>

#include <xenon/asm/binary.hpp>
#include <xenon/asm/model.hpp>
#include <xenon/asm/opcode.hpp>
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

void writeU16(xenon::XenonBinary& binary, std::size_t value)
{
	binary.write(static_cast<std::uint8_t>(value & 0xff), 1);
	binary.write(static_cast<std::uint8_t>((value >> 8) & 0xff), 1);
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
	const auto*         x      = std::get_if<double>(&fieldX);
	check(x != nullptr && std::abs(*x - 1.5) < 1e-12, "getField('x') wrong");
	check(runtime.heap().size() == 1, "heap size wrong");
}

void testArraysAndStrings()
{
	xenon::Runtime runtime;

	const auto array = runtime.newArray(3);
	runtime.setElement(array, 0, 10.0);
	runtime.setElement(array, 2, 30.0);

	check(runtime.arrayLength(array) == 3, "array length wrong");
	const xenon::Value element = runtime.getElement(array, 0);
	const auto*        first   = std::get_if<double>(&element);
	check(first != nullptr && std::abs(*first - 10.0) < 1e-12, "array[0] wrong");

	const auto text = runtime.newString("hello");
	check(runtime.objectKind(text) == xenon::ObjectKind::String, "string kind wrong");
	check(runtime.stringValue(text) == "hello", "string value wrong");
}

void testStack()
{
	xenon::Runtime runtime;

	runtime.push(1.0);
	runtime.push(2.0);
	runtime.push(3.0);
	check(runtime.stackSize() == 3, "stack size wrong");
	check(std::get_if<double>(&runtime.peek()) != nullptr, "peek wrong type");

	const auto  top    = runtime.pop();
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
	body.write(xenon::Instruction::Ldl, 1);
	writeU16(body, 0);
	body.write(xenon::Instruction::Ldl, 1);
	writeU16(body, 1);
	body.write(xenon::Instruction::Add, 1);
	body.write(xenon::Instruction::Ret, 1);

	xenon::Module module;
	module.Name = "Math";

	xenon::Function add;
	add.Name      = "add";
	add.Signature = "double add(double, double)";
	add.Arity     = 2;
	add.Body      = std::move(body);
	module.Functions.push_back(std::move(add));

	xenon::Runtime runtime;
	runtime.addModule(module);

	check(runtime.findFunction("add") != nullptr, "runtime.findFunction('add') failed");
	check(runtime.hasFunction("add"), "runtime.hasFunction('add') failed");
	runtime.push(2.0);
	runtime.push(3.0);
	check(runtime.run("add") == xenon::InterpretResult::Ok, "runtime.run('add') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 5.0) < 1e-12, "run result wrong");
}

void testBadFieldThrows()
{
	xenon::Runtime runtime;
	const auto    object = runtime.newObject("X");

	bool threw = false;
	try
	{
		(void)runtime.getField(object, "missing");
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
