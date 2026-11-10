#pragma once

#ifndef XENON__MODULE_PARSER_HPP
#define XENON__MODULE_PARSER_HPP 1

#include <string>
#include <string_view>

#include <xenon/compiler/module.hpp>

namespace xenon
{
	enum class ModuleParseResult
	{
		Ok,
		Error,
	};

	/// Parses the module header of a Xenon source file: the `export module`
	/// declaration, the `import` dependencies and the `#include` directives.
	///
	/// The header ends at the first statement that is not an import or an include;
	/// everything after that is the module body (handled by later compiler stages).
	class ModuleParser
	{
	public:
		/// Parses `source` and fills `out`. Returns `Ok` on success.
		ModuleParseResult parse(std::string_view source, ModuleHeader& out);

		/// Human-readable description of the last parse error.
		[[nodiscard]] const std::string& lastError() const noexcept
		{
			return _error;
		}

	private:
		std::string _error;
	};
}

#endif // !XENON__MODULE_PARSER_HPP
