#pragma once

#ifndef XENON__VECTOR_HPP
#define XENON__VECTOR_HPP 1

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <xenon/env/object.hpp>

namespace xenon
{
	/// A growable list of Xenon objects (host-side analogue of `std::vector`).
	template <typename Type>
	class Vector : public Object
	{
	public:
		Vector() = default;

		explicit Vector(std::vector<Type> values) : _values(std::move(values)) { }

		[[nodiscard]] std::string getType() const override
		{
			return "Vector";
		}

		[[nodiscard]] std::size_t getHash() const override
		{
			return static_cast<std::size_t>(_values.size());
		}

		[[nodiscard]] std::string toString() const override
		{
			return "Vector(" + std::to_string(_values.size()) + ")";
		}

		[[nodiscard]] std::size_t size() const noexcept
		{
			return _values.size();
		}

		[[nodiscard]] bool empty() const noexcept
		{
			return _values.empty();
		}

		void pushBack(Type value)
		{
			_values.push_back(std::move(value));
		}

		[[nodiscard]] Type& operator[](std::size_t index)
		{
			return _values[index];
		}

		[[nodiscard]] const Type& operator[](std::size_t index) const
		{
			return _values[index];
		}

		[[nodiscard]] Type& at(std::size_t index)
		{
			return _values.at(index);
		}

		[[nodiscard]] const Type& at(std::size_t index) const
		{
			return _values.at(index);
		}

		[[nodiscard]] const std::vector<Type>& values() const noexcept
		{
			return _values;
		}

	private:
		std::vector<Type> _values;
	};
}

#endif // !XENON__VECTOR_HPP
