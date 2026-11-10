#include <xenon/env/env_module.hpp>

#include <utility>

xenon::Module xenon::environmentModule()
{
	xenon::Module module;
	module.Name = "xenon.env";

	xenon::Symbol object;
	object.Name      = "Object";
	object.Kind      = xenon::SymbolKind::Class;
	object.Signature = "class Object";
	module.Symbols.push_back(std::move(object));

	xenon::Symbol string;
	string.Name      = "String";
	string.Kind      = xenon::SymbolKind::Class;
	string.Signature = "class String";
	module.Symbols.push_back(std::move(string));

	xenon::Symbol vector;
	vector.Name      = "Vector";
	vector.Kind      = xenon::SymbolKind::Class;
	vector.Signature = "template class Vector<Type>";
	module.Symbols.push_back(std::move(vector));

	xenon::Symbol environment;
	environment.Name      = "Environment";
	environment.Kind      = xenon::SymbolKind::Class;
	environment.Signature = "class Environment";
	module.Symbols.push_back(std::move(environment));

	return module;
}
