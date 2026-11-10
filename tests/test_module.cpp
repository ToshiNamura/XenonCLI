#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>

#include <xenon/bytecode/opcode.hpp>
#include <xenon/module/module.hpp>
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

xenon::Module makeAddModule()
{
	xenon::Module module;
	module.Name = "Math";
	module.Imports = {"Core"};

	xenon::Symbol add;
	add.Name      = "add";
	add.Kind      = xenon::SymbolKind::Function;
	add.Signature = "double add(double, double)";

	const auto a = add.Body.addConstant(1.2);
	const auto b = add.Body.addConstant(3.4);
	add.Body.write(xenon::OpCode::Constant, 1);
	add.Body.write(static_cast<std::uint8_t>(a), 1);
	add.Body.write(xenon::OpCode::Constant, 1);
	add.Body.write(static_cast<std::uint8_t>(b), 1);
	add.Body.write(xenon::OpCode::Add, 1);
	add.Body.write(xenon::OpCode::Return, 1);

	module.Symbols.push_back(std::move(add));

	xenon::Symbol point;
	point.Name      = "Point";
	point.Kind      = xenon::SymbolKind::Struct;
	point.Signature = "struct Point { double x; double y; }";
	module.Symbols.push_back(std::move(point));

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
	check(loaded.Symbols.size() == 2, "symbol count did not round-trip");

	check(loaded.Symbols[0].Name == "add", "first symbol name did not round-trip");
	check(loaded.Symbols[0].Kind == xenon::SymbolKind::Function, "first symbol kind did not round-trip");
	check(loaded.Symbols[0].Signature == "double add(double, double)", "first symbol signature did not round-trip");
	check(loaded.Symbols[0].Body.code().size() == 6, "function body code size did not round-trip");
	check(loaded.Symbols[0].Body.constants().size() == 2, "function body constants did not round-trip");

	check(loaded.Symbols[1].Name == "Point" && loaded.Symbols[1].Kind == xenon::SymbolKind::Struct, "second symbol did not round-trip");
}

void testVmLoadsSymbols()
{
	const auto module = makeAddModule();

	xenon::VirtualMachine vm;
	vm.load(module);

	check(vm.symbols().size() == 2, "VM did not load all symbols");
	check(vm.find("add") != nullptr, "VM could not find 'add'");
	check(vm.find("missing") == nullptr, "VM found a symbol that does not exist");

	xenon::VirtualStack stack;
	const auto result = vm.run("add", stack);
	check(result == xenon::InterpretResult::Ok, "run('add') did not return Ok");

	const auto* number = std::get_if<double>(&vm.lastValue());
	check(number != nullptr, "result is not a number");
	if (number != nullptr)
	{
		check(std::abs(*number - 4.6) < 1e-12, "1.2 + 3.4 did not equal 4.6");
	}
}

void testCorruptFileThrows()
{
	std::stringstream buffer("not a module file");
	xenon::Module module;
	bool threw = false;
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
	testVmLoadsSymbols();
	testCorruptFileThrows();

	if (g_failures == 0)
	{
		std::cout << "All tests passed.\n";
		return EXIT_SUCCESS;
	}

	std::cerr << g_failures << " test(s) failed.\n";
	return EXIT_FAILURE;
}
