#pragma once
/* IR.hxx:  Intermediate representaiton of C symbols after parsing */

#include <string>
#include <utility>
#include <vector>
#include <set>

#include <clang-c/Index.h>
#include "Tc.hxx"

struct Variable {
	Variable() = default;
	Variable(CXCursor cursor);
	auto operator<=>(const Variable&) const = default;

	HSType Type;
	std::string BoundName;
	std::string SourceHeader;
	std::string SourceName;
};

struct Constant {
	auto operator<=>(const Constant&) const = default;

	std::string BoundName;
	std::string Value;
};

struct Typedef {
	Typedef(CXCursor definition);
	auto operator<=>(const Typedef&) const = default;

	std::string BoundName;
	std::string SourceName;
};

struct Field {
	auto operator<=>(const Field&) const = default;

	long long Offset;
	long long Size;
	HSType Type;
	std::string BoundName;
};

struct Struct {
	Struct(CXCursor definition);
	auto operator<=>(const Struct&) const = default;

	std::string BoundName;
	std::string SourceName;
	std::string SourceHeader;
	long long Alignment;
	long long Size;
	std::vector<Field> Fields;
};

struct Enum {
	Enum(CXCursor definition);
	auto operator<=>(const Enum&) const = default;

	std::string BoundName;
	HSType Type;
	std::vector<Constant> Members;
};

struct Function {
	Function(CXCursor declaration);
	auto operator<=>(const Function&) const = default;

	HSType ReturnType;
	std::string BoundName;
	std::string SourceHeader;
	std::string SourceName;
	std::vector<std::pair<HSType, std::string>> Arguments;
};

struct TranslationUnit {
	std::set<Constant> Constants;
	std::set<Enum> Enumerations;
	std::set<Function> Functions;
	std::set<Struct> DataTypes;
	std::set<Typedef> TypeAliases;
	std::set<Variable> Bindings;

	std::set<std::string_view> RequiredModules;
	std::set<std::string> KnownNames;
	std::set<std::string> UnknownNames;
};
