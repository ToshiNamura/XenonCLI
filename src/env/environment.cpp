#include <xenon/env/environment.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

void xenon::Environment::registerClass(std::string typeName, Factory factory)
{
	_classes[std::move(typeName)] = std::move(factory);
}

bool xenon::Environment::hasClass(std::string_view typeName) const
{
	return _classes.find(std::string(typeName)) != _classes.end();
}

std::shared_ptr<xenon::Object> xenon::Environment::create(std::string_view typeName) const
{
	const auto it = _classes.find(std::string(typeName));
	if (it == _classes.end())
	{
		throw std::runtime_error("unknown Xenon class: " + std::string(typeName));
	}
	return it->second();
}

std::string xenon::Environment::getType() const
{
	return "Environment";
}

std::string xenon::Environment::toString() const
{
	return "Environment(" + std::to_string(_classes.size()) + " classes)";
}
