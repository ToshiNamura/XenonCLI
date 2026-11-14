#pragma once

#ifndef XENON__ASM_ASSEMBLER_HPP
#define XENON__ASM_ASSEMBLER_HPP 1

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <xenon/asm/model.hpp>

namespace xenon
{
	/// Собирает текстовый Xenon ASM (`.asm`) в модель `Module`.
	///
	/// Грамматика описана в `docs/xenon-asm.md`: одна директива/инструкция на
	/// строку, `;` — комментарий. Инструкции с операндом принимают числовой
	/// операнд (индекс пула, слот или смещение/адрес перехода).
	class Assembler
	{
	public:
		/// Собирает `source` в `out`. Возвращает `true` при успехе.
		[[nodiscard]] bool assemble(std::string_view source, Module& out);

		/// Описание последней ошибки.
		[[nodiscard]] const std::string& lastError() const noexcept
		{
			return _error;
		}

	private:
		struct Error {};

		struct Token
		{
			std::string Text;
			bool        IsString = false;
			bool        IsNumber = false;
			bool        IsIdentifier = false;
		};

		enum class Section
		{
			None,
			Meta,
			Types,
			Data,
			Text,
			Extern,
		};

		enum class Scope
		{
			Section,
			Type,
			Function,
			Method,
		};

		[[noreturn]] void fail(const std::string& message);

		[[nodiscard]] std::vector<Token> tokenize(std::string_view line);

		void processLine(std::string_view line);
		void parseDirective(const std::vector<Token>& tokens);
		void parseTypeDecl(const std::vector<Token>& tokens);
		void parseField(const std::vector<Token>& tokens);
		void parseMethod(const std::vector<Token>& tokens);
		void parseItem(const std::vector<Token>& tokens);
		void parseFunction(const std::vector<Token>& tokens);
		void parseVariable(const std::vector<Token>& tokens);
		void parseConstant(const std::vector<Token>& tokens);
		void parseLocal(const std::vector<Token>& tokens);
		void parseLocals(const std::vector<Token>& tokens);
		void parseAttribute(const std::vector<Token>& tokens);
		void parseTemplate(const std::vector<Token>& tokens);
		void parseParent(const std::vector<Token>& tokens);
		void parseInstruction(const std::vector<Token>& tokens);
		void closeScope();
		void clearPending();

		[[nodiscard]] std::size_t parseParams(const std::vector<Token>& tokens, std::size_t openIdx,
											  std::vector<std::string>& paramTypes);
		[[nodiscard]] Value parseValue(const Token& token);

		void assignGlobalValue(Global& global, const Token& token);

		void assignFieldValue(Field& field, const Token& token);

		static bool parseAccess(const std::string& text, Access& out);
		static void finishParamGroup(std::vector<std::string>& group,
									 std::vector<std::string>& paramTypes,
									 std::size_t& arity);
		static std::string join(const std::vector<std::string>& items, std::string_view sep);

		std::string              _error;
		std::size_t              _line = 0;
		Module*             _out = nullptr;
		Section                  _section = Section::None;
		std::vector<Scope>       _scopes;
		Type*               _currentType = nullptr;
		Function*           _currentFunction = nullptr;
		Method*             _currentMethod = nullptr;
		AttributeList       _pendingAttributes;
		std::vector<TemplateParam> _pendingTemplates;
		std::vector<Parent>        _pendingParents;
	};
}

#endif // !XENON__ASM_ASSEMBLER_HPP
