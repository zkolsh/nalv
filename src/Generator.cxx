#include "Generator.hxx"

#include <algorithm>
#include <fstream>
#include <iostream>

void BindingsToFile(std::filesystem::path filename, const TranslationUnit& bindings) {
	std::ofstream file(filename);
	file.exceptions(std::ios::badbit | std::ios::failbit);

	std::vector<std::string_view> extensions{};

	if (!bindings.Functions.empty() || !bindings.Bindings.empty()) {
		extensions.push_back("CApiFFI");
	};

	if (!bindings.Enumerations.empty() || !bindings.Constants.empty()) {
		extensions.push_back("PatternSynonyms");
	};

	if (!bindings.DataTypes.empty()) {
		extensions.push_back("OverloadedRecordDot");
	};

	if (bindings.DataTypes.size() > 1) {
		extensions.push_back("DuplicateRecordFields");
	};

	std::sort(extensions.begin(), extensions.end());
	for (const auto& e : extensions) {
		file << "{-# LANGUAGE " << e << " #-}" << LineEnd;
	};

	file << "module Bindings where" << LineEnd;

	for (const auto& name : bindings.RequiredModules) {
		if (name == "Prelude") continue;
		file << "import " << name << LineEnd;
	};

	file << LineEnd;

	for (const auto& name : bindings.UnknownNames) {
		if (name.empty()) {
			std::cerr << ":: Attempted to bind empty opaque!" << std::endl;
			continue;
		};

		file << "data " << name << LineEnd;
	};

	if (!bindings.UnknownNames.empty()) {
		file << LineEnd;
	};

	for (const Constant& c : bindings.Constants) {
		file << "pattern " << c.BoundName << " :: (Eq a, Bits a, Num a) => a" << LineEnd;
		file << "pattern " << c.BoundName << " = " << c.Value << LineEnd;
	};

	if (!bindings.Constants.empty()) {
		file << LineEnd;
	};

	for (const Enum& e : bindings.Enumerations) {
		if (!e.BoundName.empty()) {
			file << "type " << e.BoundName << " = " << e.Type << LineEnd;
		};

		for (const Constant& c : e.Members) {
			file << "pattern " << c.BoundName << " :: (Eq a, Bits a, Num a) => a" << LineEnd;
			file << "pattern " << c.BoundName << " = " << c.Value << LineEnd;
		};

		file << LineEnd;
	};

	for (const Typedef& alias : bindings.TypeAliases) {
		file << "type " << alias.BoundName << " = " << alias.SourceName << LineEnd;
	};

	if (!bindings.TypeAliases.empty()) {
		file << LineEnd;
	};

	for (const Struct& dt : bindings.DataTypes) {
		std::string_view rep = dt.Fields.size() == 1? "newtype" : "data";

		file << rep << " {-# CTYPE \"" << dt.SourceHeader << "\" \""
		     << dt.SourceName << "\" #-} " << dt.BoundName << " = "
		     << dt.BoundName << " {" << LineEnd;

		for (size_t i = 0; i < dt.Fields.size(); i++) {
			const Field& f = dt.Fields.at(i);
			file << Tab << f.BoundName << " :: " << f.Type 
			     << ((i + 1 == dt.Fields.size())? "" : ",") << LineEnd;
		};

		file << "} deriving (Eq, Show)" << LineEnd;
		file << LineEnd;

		file << "instance Storable " << dt.BoundName << " where" << LineEnd;
		file << Tab << "sizeOf _ = " << dt.Size << LineEnd;
		file << Tab << "alignment _ = " << dt.Alignment << LineEnd;

		const std::string_view peek_ = "peek ptr = ";
		const size_t indentColumns = peek_.length() + dt.BoundName.length() + 1;
		const std::string indent(indentColumns, ' ');

		file << Tab << peek_ << dt.BoundName << " ";
		for (size_t i = 0; i < dt.Fields.size(); i++) {
			[[unlikely]] if (i == 0) {
				file << "<$> ";
			} else {
				file << Tab << indent << "<*> ";
			};

			file << "peekByteOff ptr "
			     << dt.Fields.at(i).Offset << LineEnd;
		};

		file << Tab << "poke ptr x = do" << LineEnd;
		for (const auto& field : dt.Fields) {
			file << Tab << Tab << "pokeByteOff ptr "
			     << field.Offset << " x." << field.BoundName << LineEnd;
		};

		file << LineEnd;
	};

	for (const Variable& var : bindings.Bindings) {
		file << "foreign import capi \"" << var.SourceHeader << " " << var.SourceName
		     << "\" " << var.BoundName << " :: " << var.Type << LineEnd;
	};

	if (!bindings.Bindings.empty()) {
		file << LineEnd;
	};

	for (const Function& fn : bindings.Functions) {
		file << "foreign import capi \"" << fn.SourceHeader << " " << fn.SourceName
		     << "\" " << fn.BoundName << " :: ";

		for (const auto& hstype : fn.Arguments) {
			file << hstype.first << " -> ";
		};

		if (fn.ReturnType.find(' ') != std::string::npos) {
			file << "IO (" << fn.ReturnType << ")";
		} else {
			file << "IO " << fn.ReturnType;
		};

		file << LineEnd;
	};
};
