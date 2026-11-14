#pragma once

#ifndef XENON__ASM_MODEL_HPP
#define XENON__ASM_MODEL_HPP 1

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <xenon/asm/binary.hpp>
#include <xenon/runtime/value.hpp>

namespace xenon
{
	/// Member access classifier (`private`, `protected`, `public`).
	enum class Access : std::uint8_t
	{
		Private,
		Protected,
		Public,
	};

	/// Kind of a declared type.
	enum class TypeKind : std::uint8_t
	{
		Class,
		Struct,
		Enum,
	};

	/// Kind of an attribute value.
	enum class AttributeKind : std::uint8_t
	{
		None,    ///< Single attribute (key only).
		Integer, ///< Integer number.
		Float,   ///< Floating-point number.
		String,  ///< Text value.
		Object,  ///< Reference to another declaration.
	};

	/// Base attribute: the key plus the kind of the value (the value lives in a subclass).
	struct Attribute
	{
		std::string   Key;
		AttributeKind Kind = AttributeKind::None;
		virtual ~Attribute() = default;
	};

	using AttributeList = std::vector<std::shared_ptr<Attribute>>;

	struct AttributeInteger final : Attribute
	{
		AttributeInteger() { Kind = AttributeKind::Integer; }
		std::int64_t Value = 0;
	};

	struct AttributeFloat final : Attribute
	{
		AttributeFloat() { Kind = AttributeKind::Float; }
		double Value = 0.0;
	};

	struct AttributeString final : Attribute
	{
		AttributeString() { Kind = AttributeKind::String; }
		std::string Value;
	};

	struct AttributeObject final : Attribute
	{
		AttributeObject() { Kind = AttributeKind::Object; }
		std::string Object; ///< Name of the declaration the attribute refers to.
	};

	/// A base type (inheritance).
	struct Parent
	{
		Access      AccessModifier = Access::Private;
		std::string TypeName;
	};

	/// A template parameter: a type parameter or a value parameter.
	struct TemplateParam
	{
		std::string TypeName;     ///< Type parameter name (empty for a value parameter).
		std::string DefaultType;  ///< Default type (`= default`).
		std::string ValueType;    ///< Type of a value parameter.
		std::string DefaultValue; ///< Default value (`= default`).
		bool        IsValue = false;
	};

	/// A top-level function with an executable body.
	struct Function
	{
		std::string                     Name;
		std::string                     Signature;
		std::vector<std::string>        Params; ///< Parameter types, in order.
		bool                            Exported = false;
		std::size_t                     Arity    = 0;
		std::size_t                     Locals   = 0;
		bool                            Defined  = true; ///< False for a declaration without a body (`.hint`/`.extern`).
		XenonBinary                     Body;
		AttributeList                   Attributes;
		std::vector<TemplateParam>      TemplateParams;
	};

	/// Sentinel for "no string constant".
	inline constexpr std::size_t NoString = static_cast<std::size_t>(-1);

	/// A field of a class or struct.
	struct Field
	{
		std::string                     Type;
		std::string                     Name;
		Access                          AccessModifier = Access::Private; ///< Public by default in a struct.
		bool                            Exported       = false;
		bool                            Constant       = false;
		Value                           InitialValue   = Null{};
		std::size_t                     InitialString  = NoString; ///< Index into `Module::Strings`.
		AttributeList                   Attributes;
		std::vector<TemplateParam>      TemplateParams;
	};

	/// A method: an independent unit (signature, access and executable body).
	struct Method
	{
		std::string                     Name;
		std::string                     Signature;
		std::vector<std::string>        Params; ///< Parameter types, in order.
		Access                          AccessModifier = Access::Private;
		bool                            Exported       = false;
		std::size_t                     Arity          = 0;
		std::size_t                     Locals         = 0;
		bool                            Defined        = true; ///< False for a declaration without a body.
		XenonBinary                     Body;
		AttributeList                   Attributes;
		std::vector<TemplateParam>      TemplateParams;
	};

	/// A class, struct or enum.
	struct Type
	{
		std::string                     Name;
		TypeKind                        Kind     = TypeKind::Class;
		bool                            Exported = false;
		bool                            Defined  = true; ///< False for a declaration without a body.
		std::vector<Field>              Fields;
		std::vector<Method>             Methods;
		std::vector<std::string>        Enumerators; ///< Enumerators of an enum.
		std::vector<Parent>             Parents;
		AttributeList                   Attributes;
		std::vector<TemplateParam>      TemplateParams;
	};

	/// A global variable or constant.
	struct Global
	{
		std::string                     Type;
		std::string                     Name;
		bool                            Exported      = false;
		bool                            Constant      = false;
		Value                           InitialValue  = Null{};
		std::size_t                     InitialString = NoString; ///< Index into `Module::Strings`.
		AttributeList                   Attributes;
		std::vector<TemplateParam>      TemplateParams;
	};

	/// A compiled module — the Xenon ASM artifact stored in a `.module` file.
	///
	/// The module owns the whole declaration tree: functions, types (with fields
	/// and methods) and globals. Executable code lives in the `XenonBinary` bodies
	/// of functions and methods.
	struct Module
	{
		std::string              Name;
		std::vector<std::string> Imports;
		AttributeList            Attributes; ///< Module attributes (`.meta`).
		std::vector<std::string> Strings;    ///< String constants used by initializers.
		std::vector<Function>    Functions;
		std::vector<Type>        Types;
		std::vector<Global>      Globals;

		/// Returns the top-level function named `name`, or `nullptr`.
		[[nodiscard]] const Function* findFunction(std::string_view name) const noexcept
		{
			for (const auto& function : Functions)
			{
				if (function.Name == name)
				{
					return &function;
				}
			}
			return nullptr;
		}

		/// Returns the type named `name`, or `nullptr`.
		[[nodiscard]] const Type* findType(std::string_view name) const noexcept
		{
			for (const auto& type : Types)
			{
				if (type.Name == name)
				{
					return &type;
				}
			}
			return nullptr;
		}

		/// Returns the global named `name`, or `nullptr`.
		[[nodiscard]] const Global* findGlobal(std::string_view name) const noexcept
		{
			for (const auto& global : Globals)
			{
				if (global.Name == name)
				{
					return &global;
				}
			}
			return nullptr;
		}

		/// Returns the method `Type::name` (or `name` when the type is omitted), or `nullptr`.
		[[nodiscard]] const Method* findMethod(std::string_view qualifiedName) const noexcept
		{
			const auto separator = qualifiedName.rfind("::");
			std::string_view typeName = {};
			std::string_view methodName = qualifiedName;
			if (separator != std::string_view::npos)
			{
				typeName   = qualifiedName.substr(0, separator);
				methodName = qualifiedName.substr(separator + 2);
			}

			for (const auto& type : Types)
			{
				if (!typeName.empty() && type.Name != typeName)
				{
					continue;
				}
				for (const auto& method : type.Methods)
				{
					if (method.Name == methodName)
					{
						return &method;
					}
				}
			}
			return nullptr;
		}
	};
}

#endif // !XENON__ASM_MODEL_HPP
