#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <variant>

#include <xenon/asm/binary.hpp>
#include <xenon/asm/model.hpp>
#include <xenon/asm/opcode.hpp>
#include <xenon/module/module_file.hpp>
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

xenon::Module makeAddModule()
{
	xenon::Module module;
	module.Name    = "Math";
	module.Imports = {"Core"};

	xenon::Function add;
	add.Name      = "add";
	add.Signature = "double add(double, double)";
	add.Arity     = 2;

	add.Body.write(xenon::Instruction::Ldl, 1);
	writeU16(add.Body, 0);
	add.Body.write(xenon::Instruction::Ldl, 1);
	writeU16(add.Body, 1);
	add.Body.write(xenon::Instruction::Add, 1);
	add.Body.write(xenon::Instruction::Ret, 1);

	auto attribute    = std::make_shared<xenon::AttributeString>();
	attribute->Key    = "version";
	attribute->Value  = "1.0";
	add.Attributes.push_back(std::move(attribute));

	module.Functions.push_back(std::move(add));

	xenon::Type point;
	point.Name = "Point";
	point.Kind = xenon::TypeKind::Struct;

	xenon::Field x;
	x.Type           = "double";
	x.Name           = "x";
	x.AccessModifier = xenon::Access::Public;
	point.Fields.push_back(std::move(x));

	xenon::Field y;
	y.Type           = "double";
	y.Name           = "y";
	y.AccessModifier = xenon::Access::Public;
	point.Fields.push_back(std::move(y));

	xenon::Method length;
	length.Name           = "length";
	length.Signature      = "double length()";
	length.AccessModifier = xenon::Access::Public;
	const auto zero = length.Body.addConstant(0.0);
	length.Body.write(xenon::Instruction::Const, 1);
	writeU16(length.Body, zero);
	length.Body.write(xenon::Instruction::Ret, 1);
	point.Methods.push_back(std::move(length));

	module.Types.push_back(std::move(point));

	xenon::Global pi;
	pi.Type         = "double";
	pi.Name         = "pi";
	pi.Constant     = true;
	pi.InitialValue = 3.14159;
	module.Globals.push_back(std::move(pi));

	return module;
}

void testRoundTrip()
{
	const auto module = makeAddModule();

	std::stringstream buffer;
	writeModuleFile(buffer, module);

	xenon::Module loaded;
	readModuleFile(buffer, loaded);

	check(loaded.Name == "Math", "module name did not round-trip");
	check(loaded.Imports.size() == 1 && loaded.Imports[0] == "Core", "imports did not round-trip");
	check(loaded.Functions.size() == 1, "function count did not round-trip");
	check(loaded.Types.size() == 1, "type count did not round-trip");
	check(loaded.Globals.size() == 1, "global count did not round-trip");

	const auto& add = loaded.Functions.front();
	check(add.Name == "add", "function name did not round-trip");
	check(add.Signature == "double add(double, double)", "function signature did not round-trip");
	check(add.Arity == 2, "function arity did not round-trip");
	check(add.Body.code().size() == 8, "function body size did not round-trip");
	check(add.Body.constants().empty(), "function constants did not round-trip");
	check(add.Attributes.size() == 1, "function attributes did not round-trip");
	check(add.Attributes.front()->Kind == xenon::AttributeKind::String, "attribute kind did not round-trip");
	check(static_cast<const xenon::AttributeString&>(*add.Attributes.front()).Value == "1.0",
		  "attribute value did not round-trip");

	const auto& point = loaded.Types.front();
	check(point.Name == "Point" && point.Kind == xenon::TypeKind::Struct, "type did not round-trip");
	check(point.Fields.size() == 2 && point.Fields[0].Name == "x" &&
			  point.Fields[1].AccessModifier == xenon::Access::Public,
		  "fields did not round-trip");
	check(point.Methods.size() == 1 && point.Methods.front().Name == "length" &&
			  point.Methods.front().Body.code().size() == 4,
		  "methods did not round-trip");

	check(loaded.Globals.front().Name == "pi" && loaded.Globals.front().Constant, "global did not round-trip");
	const auto* pi = std::get_if<double>(&loaded.Globals.front().InitialValue);
	check(pi != nullptr && std::abs(*pi - 3.14159) < 1e-12, "global value did not round-trip");
}

void testVmLoadsModule()
{
	const auto module = makeAddModule();

	xenon::VirtualMachine vm;
	vm.load(module);

	check(vm.hasFunction("add"), "VM did not load 'add'");
	check(vm.findFunction("add") != nullptr, "findFunction('add') failed");
	check(vm.findType("Point") != nullptr, "findType('Point') failed");
	check(vm.findMethod("Point::length") != nullptr, "findMethod('Point::length') failed");
	check(!vm.hasFunction("missing"), "VM found a function that does not exist");

	xenon::VirtualStack methodStack;
	check(vm.run("Point::length", methodStack) == xenon::InterpretResult::Ok, "run(method) failed");

	xenon::VirtualStack stack;
	stack.push(1.5);
	stack.push(3.0);
	const auto result = vm.run("add", stack);
	check(result == xenon::InterpretResult::Ok, "run('add') did not return Ok");

	const auto* number = std::get_if<double>(&vm.lastValue());
	check(number != nullptr, "result is not a number");
	if (number != nullptr)
	{
		check(std::abs(*number - 4.5) < 1e-12, "1.5 + 3.0 did not equal 4.5");
	}
}

void testWideOperandRoundTrip()
{
	xenon::Module module;
	module.Name = "Wide";

	xenon::Function function;
	function.Name      = "main";
	function.Signature = "double main()";
	for (int i = 0; i <= 300; ++i)
	{
		(void)function.Body.addConstant(static_cast<double>(i));
	}
	function.Body.write(xenon::Instruction::Const, 1);
	writeU16(function.Body, 300);
	function.Body.write(xenon::Instruction::Ret, 1);
	module.Functions.push_back(std::move(function));

	std::stringstream buffer;
	writeModuleFile(buffer, module);

	xenon::Module loaded;
	readModuleFile(buffer, loaded);

	xenon::VirtualMachine vm;
	vm.load(loaded);
	xenon::VirtualStack stack;
	check(vm.run("main", stack) == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&vm.lastValue());
	check(number != nullptr && *number == 300.0, "16-bit constant index did not round-trip");
}

void testCorruptFileThrows()
{
	std::stringstream buffer("not a module file");
	xenon::Module    module;
	bool             threw = false;
	try
	{
		readModuleFile(buffer, module);
	}
	catch (const xenon::ModuleFileError&)
	{
		threw = true;
	}
	check(threw, "reading a corrupt module did not throw ModuleFileError");
}

int main()
{
	testRoundTrip();
	testVmLoadsModule();
	testWideOperandRoundTrip();
	testCorruptFileThrows();

	if (g_failures == 0)
	{
		std::cout << "All tests passed.\n";
		return EXIT_SUCCESS;
	}

	std::cerr << g_failures << " test(s) failed.\n";
	return EXIT_FAILURE;
}
