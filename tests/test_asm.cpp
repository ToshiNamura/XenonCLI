#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <variant>

#include <xenon/asm/assembler.hpp>
#include <xenon/asm/disassembler.hpp>
#include <xenon/asm/model.hpp>
#include <xenon/asm/opcode.hpp>
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

const std::string kSource = R"(
.meta
    .module Demo
    .import Math
    .attribute version "1.0"
.end

.types
    .attribute marker
    .template <T = double>
    .parent public Base
    .class export Point
        .field public double x
        .field private double y
        .method public double length()
            ldl 0
            ldl 0
            mul
            ret
        .end
    .end

    .enum Color
        .item Red
        .item Green
    .end
.end

.data
    .variable export double scale 2.0
    .constant double version 1.0
.end

.text
    .function export double main()
        const 0.0
        ret
    .end
.end
)";

void testMnemonics()
{
	check(std::string(xenon::mnemonic(xenon::Instruction::Add)) == "add", "mnemonic(add)");
	check(std::string(xenon::mnemonic(xenon::Instruction::Jmpa)) == "jmpa", "mnemonic(jmpa)");
	check(std::string(xenon::mnemonic(xenon::Directive::Class)) == ".class", "mnemonic(.class)");
	check(std::string(xenon::mnemonic(xenon::Directive::Item)) == ".item", "mnemonic(.item)");
	check(std::string(xenon::mnemonic(xenon::Directive::Locals)) == ".locals", "mnemonic(.locals)");
	check(std::string(xenon::mnemonic(xenon::Instruction::Mod)) == "mod", "mnemonic(mod)");
	check(std::string(xenon::mnemonic(xenon::Instruction::BAnd)) == "and", "mnemonic(and)");
	check(std::string(xenon::mnemonic(xenon::Instruction::BOr)) == "or", "mnemonic(or)");
	check(std::string(xenon::mnemonic(xenon::Instruction::BXor)) == "xor", "mnemonic(xor)");
	check(std::string(xenon::mnemonic(xenon::Instruction::BNot)) == "bnot", "mnemonic(bnot)");
	check(std::string(xenon::mnemonic(xenon::Instruction::Shl)) == "shl", "mnemonic(shl)");
	check(std::string(xenon::mnemonic(xenon::Instruction::Shr)) == "shr", "mnemonic(shr)");

	xenon::Instruction parsed = xenon::Instruction::Add;
	check(xenon::instructionFromMnemonic("shr", parsed) && parsed == xenon::Instruction::Shr,
		  "instructionFromMnemonic(shr)");
	check(xenon::instructionFromMnemonic("mod", parsed) && parsed == xenon::Instruction::Mod,
		  "instructionFromMnemonic(mod)");
}

void testOpcodeRanges()
{
	check(!xenon::isDirective(0x00), "0x00 is an instruction");
	check(!xenon::isDirective(0x7F), "0x7F is an instruction");
	check(xenon::isDirective(0x80), "0x80 is a directive");
	check(xenon::isDirective(0xFF), "0xFF is a directive");
}

void testOperandSizes()
{
	check(xenon::operandSize(xenon::Instruction::Const) == 2, "const has a u16 operand");
	check(xenon::operandKind(xenon::Instruction::Const) == xenon::OperandKind::Constant,
		  "const operand kind");
	check(xenon::operandSize(xenon::Instruction::Jmp) == 2, "jmp has an i16 operand");
	check(xenon::operandSize(xenon::Instruction::Jmpa) == 4, "jmpa has a u32 operand");
	check(xenon::operandSize(xenon::Instruction::Add) == 0, "add has no operand");
	check(xenon::operandSize(xenon::Instruction::Mod) == 0, "mod has no operand");
	check(xenon::operandSize(xenon::Instruction::BAnd) == 0, "and has no operand");
	check(xenon::operandSize(xenon::Instruction::BOr) == 0, "or has no operand");
	check(xenon::operandSize(xenon::Instruction::BXor) == 0, "xor has no operand");
	check(xenon::operandSize(xenon::Instruction::BNot) == 0, "bnot has no operand");
	check(xenon::operandSize(xenon::Instruction::Shl) == 0, "shl has no operand");
	check(xenon::operandSize(xenon::Instruction::Shr) == 0, "shr has no operand");
}

void testModel()
{
	xenon::Module module;
	module.Name = "Demo";

	xenon::Function function;
	function.Name      = "main";
	function.Signature = "double main()";
	function.Exported  = true;

	auto attribute   = std::make_shared<xenon::AttributeString>();
	attribute->Key   = "version";
	attribute->Value = "1.0";
	function.Attributes.push_back(std::move(attribute));

	module.Functions.push_back(std::move(function));

	check(module.Functions.size() == 1, "module has one function");
	check(module.Functions.front().Exported, "function is exported");
	check(module.Functions.front().Attributes.size() == 1, "function has one attribute");

	const auto& attr = module.Functions.front().Attributes.front();
	check(attr->Kind == xenon::AttributeKind::String, "attribute kind is String");

	const auto* text = dynamic_cast<const xenon::AttributeString*>(attr.get());
	check(text != nullptr && text->Value == "1.0", "attribute value via dynamic_cast");
}

void testAssembler()
{
	xenon::Module    module;
	xenon::Assembler assembler;
	check(assembler.assemble(kSource, module), std::string("assemble failed: ") + assembler.lastError());

	check(module.Name == "Demo", "module name");
	check(module.Imports.size() == 1 && module.Imports.front() == "Math", "module imports");
	check(module.Attributes.size() == 1, "module attributes");

	check(module.Types.size() == 2, "two types");

	const xenon::Type& point = module.Types[0];
	check(point.Name == "Point", "type name");
	check(point.Kind == xenon::TypeKind::Class, "type kind");
	check(point.Exported, "type exported");
	check(point.Parents.size() == 1 && point.Parents.front().TypeName == "Base", "type parent");
	check(point.TemplateParams.size() == 1 && point.TemplateParams.front().TypeName == "T",
		  "type template");
	check(point.Fields.size() == 2, "two fields");
	check(point.Fields[0].AccessModifier == xenon::Access::Public, "first field public");
	check(point.Fields[1].AccessModifier == xenon::Access::Private, "second field private");
	check(point.Methods.size() == 1, "one method");
	check(point.Methods.front().Name == "length", "method name");
	check(point.Methods.front().Signature == "double length()", "method signature");
	check(point.Methods.front().Body.code().size() == 8, "method body size");

	const xenon::Type& color = module.Types[1];
	check(color.Kind == xenon::TypeKind::Enum, "color is enum");
	check(color.Enumerators.size() == 2, "two enumerators");

	check(module.Globals.size() == 2, "two globals");
	check(!module.Globals[0].Constant, "variable is not constant");
	check(module.Globals[1].Constant, "constant is constant");

	check(module.Functions.size() == 1, "one function");
	check(module.Functions.front().Name == "main", "function name");
	check(module.Functions.front().Arity == 0, "function arity");
	check(module.Functions.front().Body.code().size() == 4, "function body size");
}

void testAssemblerErrors()
{
	const std::string source = ".text\n    add\n.end\n";
	xenon::Module     module;
	xenon::Assembler  assembler;
	check(!assembler.assemble(source, module), "instruction outside body should fail");

	const std::string unclosed = ".text\n    .function double main()\n";
	check(!assembler.assemble(unclosed, module), "unclosed scope should fail");

	const std::string missingModule = ".text\n.end\n";
	check(assembler.assemble(missingModule, module), "empty module should assemble");
}

void testAssembledRun()
{
	xenon::Module    module;
	xenon::Assembler assembler;
	check(assembler.assemble(kSource, module), std::string("assemble failed: ") + assembler.lastError());

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && *number == 0.0, "assembled main() result wrong");
}

void testArithmeticBodies()
{
	const std::string source = R"(
.meta
    .module Operators
.end

.text
    .function double main()
        const 7.0
        const 4.0
        mod
        const 3.0
        and
        const 8.0
        or
        const 1.0
        xor
        bnot
        const 1.0
        shl
        const 2.0
        shr
        ret
    .end
.end
)";

	xenon::Module    module;
	xenon::Assembler assembler;
	check(assembler.assemble(source, module), std::string("assemble failed: ") + assembler.lastError());
	check(module.Functions.front().Body.code().size() == 29, "arithmetic body size");

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	// 7 mod 4 = 3; 3 and 3 = 3; 3 or 8 = 11; 11 xor 1 = 10; ~10 = -11; -11 << 1 = -22; -22 >> 2 = -6
	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && *number == -6.0, "arithmetic instruction result wrong");
}

void testDisassembler()
{
	xenon::Module    module;
	xenon::Assembler assembler;
	check(assembler.assemble(kSource, module), std::string("assemble failed: ") + assembler.lastError());

	const std::string listing = xenon::disassemble(module);
	check(listing.find(".module Demo") != std::string::npos, "listing has no module name");
	check(listing.find(".class export Point") != std::string::npos, "listing has no class");
	check(listing.find(".function export double main()") != std::string::npos, "listing has no main");
	check(listing.find("const 0.0") != std::string::npos, "listing has no constant");

	xenon::Module    roundTripped;
	xenon::Assembler second;
	check(second.assemble(listing, roundTripped),
		  std::string("re-assembling the listing failed: ") + second.lastError());
	check(roundTripped.Name == module.Name, "round-tripped module name");
	check(roundTripped.Functions.size() == module.Functions.size(), "round-tripped function count");
	check(roundTripped.Types.size() == module.Types.size(), "round-tripped type count");
	check(roundTripped.Types[0].Methods.size() == 1 &&
			  roundTripped.Types[0].Methods.front().Body.code().size() == 8,
		  "round-tripped method body");
}

int main()
{
	try
	{
		testMnemonics();
		testOpcodeRanges();
		testOperandSizes();
		testModel();
		testAssembler();
		testAssemblerErrors();
		testAssembledRun();
		testArithmeticBodies();
		testDisassembler();
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
