#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "xenon/bytecode/opcode.hpp"
#include "xenon/module/module.hpp"
#include "xenon/module/module_file.hpp"
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

xenon::Module make_add_module()
{
    using namespace xenon;

    Module module;
    module.name = "Math";
    module.imports = {"Core"};

    Symbol add;
    add.name = "add";
    add.kind = SymbolKind::Function;
    add.signature = "double add(double, double)";

    const auto a = add.body.add_constant(1.2);
    const auto b = add.body.add_constant(3.4);
    add.body.write(OpCode::Constant, 1);
    add.body.write(static_cast<std::uint8_t>(a), 1);
    add.body.write(OpCode::Constant, 1);
    add.body.write(static_cast<std::uint8_t>(b), 1);
    add.body.write(OpCode::Add, 1);
    add.body.write(OpCode::Return, 1);

    module.symbols.push_back(std::move(add));

    Symbol point;
    point.name = "Point";
    point.kind = SymbolKind::Struct;
    point.signature = "struct Point { double x; double y; }";
    module.symbols.push_back(std::move(point));

    return module;
}

void test_round_trip()
{
    using namespace xenon;

    const auto module = make_add_module();

    std::stringstream buffer;
    write_module_file(buffer, module);

    Module loaded;
    read_module_file(buffer, loaded);

    check(loaded.name == "Math", "module name did not round-trip");
    check(loaded.imports.size() == 1 && loaded.imports[0] == "Core", "imports did not round-trip");
    check(loaded.symbols.size() == 2, "symbol count did not round-trip");

    check(loaded.symbols[0].name == "add", "first symbol name did not round-trip");
    check(loaded.symbols[0].kind == SymbolKind::Function, "first symbol kind did not round-trip");
    check(loaded.symbols[0].signature == "double add(double, double)",
          "first symbol signature did not round-trip");
    check(loaded.symbols[0].body.code().size() == 6, "function body code size did not round-trip");
    check(loaded.symbols[0].body.constants().size() == 2,
          "function body constants did not round-trip");

    check(loaded.symbols[1].name == "Point" && loaded.symbols[1].kind == SymbolKind::Struct,
          "second symbol did not round-trip");
}

void test_vm_loads_symbols()
{
    using namespace xenon;

    const auto module = make_add_module();

    VM vm;
    vm.load(module);

    check(vm.symbols().size() == 2, "VM did not load all symbols");
    check(vm.find("add") != nullptr, "VM could not find 'add'");
    check(vm.find("missing") == nullptr, "VM found a symbol that does not exist");

    std::vector<Value> stack;
    const auto result = vm.run("add", stack);
    check(result == InterpretResult::Ok, "run('add') did not return Ok");

    const auto* number = std::get_if<double>(&vm.last_value());
    check(number != nullptr, "result is not a number");
    if (number != nullptr)
    {
        check(std::abs(*number - 4.6) < 1e-12, "1.2 + 3.4 did not equal 4.6");
    }
}

void test_corrupt_file_throws()
{
    using namespace xenon;

    std::stringstream buffer("not a module file");
    Module module;
    bool threw = false;
    try
    {
        read_module_file(buffer, module);
    }
    catch (const ModuleFileError&)
    {
        threw = true;
    }
    check(threw, "reading a corrupt module did not throw ModuleFileError");
}

} // namespace

int main()
{
    test_round_trip();
    test_vm_loads_symbols();
    test_corrupt_file_throws();

    if (g_failures == 0)
    {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
