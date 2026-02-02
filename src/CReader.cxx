#include <cassert>
#include <climits>
#include <iostream>

#include <clang-c/Index.h>
#include "CReader.hxx"

#define GUARD(condition) do { if (!(condition)) return std::nullopt; } while (0)

TranslationUnit FromSourceFile(const std::filesystem::path filename) {
	assert(std::filesystem::exists(filename));
	CXIndex index = clang_createIndex(0, 1);

	const char* args[] = {
		"-I/usr/include/",
		"-I/usr/lib/clang/21/include/"
	};

	CXTranslationUnit tu = clang_parseTranslationUnit(index, filename.c_str(),
		args, 2, nullptr, 0, CXTranslationUnit_None);
	assert(tu);

	CXCursor root = clang_getTranslationUnitCursor(tu);
	TranslationUnit bindings{};
	clang_visitChildren(root, TopLevelVisit, &bindings);

	for (const auto& st : bindings.DataTypes) {
		bindings.UnknownNames.erase(st.BoundName);
		bindings.UnknownNames.erase(st.BoundName + "_");
	};

	for (const auto& e : bindings.Enumerations) {
		bindings.UnknownNames.erase(e.BoundName);
	};

	for (const auto& name : bindings.KnownNames) {
		bindings.UnknownNames.erase(name);
	};

	for (const auto& [key, fundamental] : FundamentalTypes) {
		bindings.UnknownNames.erase(std::string(fundamental.Name));
	};
	
	clang_disposeTranslationUnit(tu);
	clang_disposeIndex(index);

	bindings.RequiredModules.emplace("Foreign");
	bindings.RequiredModules.emplace("Foreign.C.ConstPtr");
	bindings.RequiredModules.emplace("Foreign.C.Types");
	bindings.RequiredModules.emplace("Foreign.Ptr");
	return bindings;
};

CXChildVisitResult TopLevelVisit(CXCursor cursor, CXCursor, CXClientData clientData) {
	CXSourceLocation location = clang_getCursorLocation(cursor);
	if (!clang_Location_isFromMainFile(location)) {
		return CXChildVisit_Continue;
	};

	TranslationUnit* bindings = reinterpret_cast<TranslationUnit*>(clientData);

	switch (clang_getCursorKind(cursor)) {
	case CXCursor_StructDecl: {
		if (!clang_isCursorDefinition(cursor)) {
			CXString cursorName = clang_getCursorSpelling(cursor);
			std::string name = HSTypeName(clang_getCString(cursorName));
			clang_disposeString(cursorName);

			if (!name.empty()) {
				bindings->UnknownNames.insert(name + "_");
			};

			break;
		};

		std::optional<Struct> st = ParseStruct(cursor);
		if (st.has_value()) {
			bindings->DataTypes.emplace(st.value());
			bindings->KnownNames.insert(st.value().BoundName);
			bindings->UnknownNames.erase(st.value().BoundName);
		} else {
			CXString name = clang_getCursorSpelling(cursor);
			std::cout << "StructDecl: failed to emit "
			          << clang_getCString(name) << std::endl;
			clang_disposeString(name);
		};

		break;
	};

	case CXCursor_UnionDecl: {
		if (clang_Cursor_isAnonymous(cursor)) {
			return CXChildVisit_Continue;
		};

		CXString cursorName = clang_getCursorSpelling(cursor);
		std::string name = HSTypeName(clang_getCString(cursorName));

		if (!name.empty()) {
			bindings->UnknownNames.insert(name + "_");
		} else {
			std::cerr << "UnionDecl: empty underlying name." << std::endl;
			break;
		};

		std::cerr << "unimplemented: CXCursor_UnionDecl" << std::endl;

		clang_disposeString(cursorName);
		break;
	};

	case CXCursor_EnumDecl: {
		CXCursor canon = clang_getCanonicalCursor(cursor);
		if (!clang_equalCursors(cursor, canon)) {
			break;
		};

		std::optional<Enum> e = ParseEnum(cursor);
		if (e.has_value()) {
			bindings->Enumerations.emplace(e.value());
			bindings->KnownNames.insert(e.value().BoundName);
		} else {
			CXString name = clang_getCursorSpelling(cursor);
			std::cout << "EnumDecl: failed to emit "
			          << clang_getCString(name) << std::endl;
			clang_disposeString(name);
		};

		break;
	};

	/* This is probably not possible, but trivial to implement with little risk. */
	case CXCursor_EnumConstantDecl: {
		CXCursor canon = clang_getCanonicalCursor(cursor);
		if (!clang_equalCursors(cursor, canon)) {
			break;
		};

		std::optional<Constant> c = ParseEnumConstant(cursor);
		if (c.has_value()) {
			bindings->Constants.emplace(c.value());
		} else {
			CXString name = clang_getCursorSpelling(cursor);
			std::cout << "EnumDecl: failed to emit "
			          << clang_getCString(name) << std::endl;
			clang_disposeString(name);
		};

		break;
	};

	case CXCursor_FunctionDecl: {
		CXCursor canon = clang_getCanonicalCursor(cursor);
		if (!clang_equalCursors(cursor, canon)) {
			break;
		};

		std::optional<Function> fun = ParseFunction(cursor);
		if (fun.has_value()) {
			bindings->Functions.emplace(fun.value());
		} else {
			CXString name = clang_getCursorSpelling(cursor);
			std::cout << "FunctionDecl: failed to emit "
			          << clang_getCString(name) << std::endl;
			clang_disposeString(name);
		};
		break;
	};

	case CXCursor_VarDecl: {
		CXCursor canon = clang_getCanonicalCursor(cursor);
		if (!clang_equalCursors(cursor, canon)) {
			break;
		};

		std::optional<Variable> var = ParseVariable(cursor);
		if (var.has_value()) {
			bindings->Bindings.emplace(var.value());
		} else {
			CXString name = clang_getCursorSpelling(cursor);
			std::cout << "VarDecl: failed to emit "
			          << clang_getCString(name) << std::endl;
			clang_disposeString(name);
		};

		break;
	};

	case CXCursor_TypedefDecl: {
		CXCursor canon = clang_getCanonicalCursor(cursor);
		if (!clang_equalCursors(cursor, canon)) {
			break;
		};

		std::optional<std::pair<Struct, Typedef>> tstruct = ParseStructTypedef(cursor);
		if (tstruct.has_value()) {
			auto& [st, td] = tstruct.value();

			if (st.Size >= 0) {
				bindings->DataTypes.insert(st);
			} else if (!st.BoundName.empty()) {
				bindings->UnknownNames.insert(st.BoundName);
			};

			if (td.BoundName == td.SourceName) {
				break;
			};

			const std::string_view prefixes[] = {
				"ConstPtr ",
				"Ptr "
			};

			HSType source = td.SourceName;
			for (const auto& pfx : prefixes) {
				if (source.find(pfx) == 0) {
					source.erase(0, pfx.length());
				};
			};

			if (!bindings->KnownNames.contains(source)
			 && !FundamentalTypes.contains(source)) {
				bindings->UnknownNames.insert(source);
			};

			bindings->KnownNames.insert(td.BoundName);
			bindings->TypeAliases.insert(td);
			break;
		};

		std::optional<std::pair<Enum, Typedef>> tenum = ParseEnumTypedef(cursor);
		if (tenum.has_value()) {
			auto& [e, td] = tenum.value();

			bindings->Enumerations.insert(e);
			if (!e.BoundName.empty()) {
				bindings->KnownNames.insert(e.BoundName);
			};

			if (td.BoundName == td.SourceName) {
				break;
			};

			bindings->TypeAliases.insert(td);
			break;
		};

		std::optional<Typedef> treg = ParseRegularTypedef(cursor);
		if (treg.has_value()) {
			Typedef& td = treg.value();

			if (bindings->KnownNames.contains(td.BoundName)) {
				break;
			};

			if (bindings->UnknownNames.contains(td.SourceName + "_")
			 || td.SourceName == td.BoundName) {
				td.SourceName += "_";
			};

			bindings->KnownNames.insert(td.BoundName);
			bindings->TypeAliases.insert(td);

			bool isTrivialOpaque = true;
			const std::string_view prefixes[] = {
				"ConstPtr ",
				"FunPtr ",
				"Ptr "
			};

			for (const auto& pfx : prefixes) {
				if (td.SourceName.find(pfx) == 0) {
					isTrivialOpaque = false;
					break;
				};
			};

			if (!bindings->KnownNames.contains(td.SourceName)
			 && !FundamentalTypes.contains(td.SourceName)
			 && isTrivialOpaque) {
				bindings->UnknownNames.insert(td.SourceName);
			};

			break;
		};
		
		/* All branches end with a break, so you shouldn't be here. */
		CXString name = clang_getCursorSpelling(cursor);
		std::cout << "TypedefDecl: failed to emit "
			  << clang_getCString(name) << std::endl;
		clang_disposeString(name);

		break;
	};

	default:
		break;
	};

	return CXChildVisit_Continue;
};

std::optional<Variable> ParseVariable(CXCursor cursor) {
	Variable var{};
	CXString name = clang_getCursorSpelling(cursor);
	var.SourceName = clang_getCString(name);
	var.BoundName = HSBindingName(var.SourceName);
	clang_disposeString(name);
	GUARD(!var.SourceName.empty());
	GUARD(!var.BoundName.empty());

	CXType type = clang_getCursorType(cursor);
	var.Type = ToHSType(type);
	GUARD(!var.Type.empty());

	CXSourceLocation source = clang_getCursorLocation(cursor);
	CXFile file;
	clang_getExpansionLocation(source, &file, nullptr, nullptr, nullptr);
	if (file) {
		CXString filename = clang_getFileName(file);
		var.SourceHeader = clang_getCString(filename);
		clang_disposeString(filename);
	};

	GUARD(!var.SourceHeader.empty());
	return var;
};

std::optional<Typedef> ParseRegularTypedef(CXCursor cursor) {
	GUARD(!clang_Cursor_isAnonymous(cursor));

	Typedef td{};
	CXString alias = clang_getCursorSpelling(cursor);
	td.BoundName = HSTypeName(clang_getCString(alias));
	clang_disposeString(alias);

	CXType underlying = clang_getTypedefDeclUnderlyingType(cursor);
	td.SourceName = ToHSType(underlying);
	if (underlying.kind == CXType_FunctionProto
	 || underlying.kind == CXType_FunctionNoProto) {
		td.SourceName = "FunPtr (" + td.SourceName + ")";
	};

	GUARD(!td.SourceName.empty());
	GUARD(!td.BoundName.empty());
	GUARD(td.BoundName != td.SourceName);
	return td;
};

std::optional<std::pair<Struct, Typedef>> ParseStructTypedef(CXCursor cursor) {
	GUARD(clang_getCursorKind(cursor) == CXCursor_TypedefDecl);
	Typedef td{};

	CXString alias = clang_getCursorSpelling(cursor);
	td.BoundName = HSTypeName(clang_getCString(alias));
	clang_disposeString(alias);
	GUARD(!td.BoundName.empty());

	CXType underlyingType = clang_getTypedefDeclUnderlyingType(cursor);
	const bool isStructPointer = underlyingType.kind == CXType_Pointer;
	CXType targetType = isStructPointer ? clang_getPointeeType(underlyingType)
	                                    : underlyingType;

	CXCursor underlying = clang_getTypeDeclaration(targetType);
	GUARD(clang_getCursorKind(underlying) == CXCursor_StructDecl);

	std::optional<Struct> st = ParseStruct(underlying, true);
	GUARD(st.has_value());

	if (isStructPointer) {
		td.SourceName = "Ptr " + st.value().BoundName;
	} else if (st.value().BoundName.empty()) {
		st.value().BoundName = td.BoundName;
		td.SourceName = td.BoundName;
	} else {
		td.SourceName = st.value().BoundName;
	};

	return std::make_pair(st.value(), td);
};

std::optional<std::pair<Enum, Typedef>> ParseEnumTypedef(CXCursor cursor) {
	GUARD(clang_getCursorKind(cursor) == CXCursor_TypedefDecl);
	Typedef td{};

	CXString alias = clang_getCursorSpelling(cursor);
	td.BoundName = HSTypeName(clang_getCString(alias));
	clang_disposeString(alias);
	GUARD(!td.BoundName.empty());

	CXType underlyingType = clang_getTypedefDeclUnderlyingType(cursor);
	CXCursor underlying = clang_getTypeDeclaration(underlyingType);
	const bool isDefinition = clang_isCursorDefinition(underlying);

	GUARD(clang_getCursorKind(underlying) == CXCursor_EnumDecl);
	std::optional<Enum> e = ParseEnum(underlying);
	if (!isDefinition) {
		CXString spelling = clang_getCursorSpelling(underlying);
		std::string name = clang_getCString(spelling);
		clang_disposeString(spelling);

		Enum fallback{};
		fallback.BoundName = "";
		fallback.Members.clear();
		td.SourceName = e.value_or(fallback).BoundName;
		return std::make_pair(e.value_or(fallback), td);
	};

	GUARD(e.has_value());
	td.SourceName = e.value().BoundName;
	return std::make_pair(e.value(), td);
};

std::optional<Field> ParseStructField(CXCursor cursor) {
	GUARD(clang_getCursorKind(cursor) == CXCursor_FieldDecl);
	GUARD(!clang_Cursor_isAnonymous(cursor));
	Field field{};

	CXType fieldType = clang_getCursorType(cursor);
	long long size = clang_Type_getSizeOf(fieldType);
	long long offset = clang_Cursor_getOffsetOfField(cursor) / 8;
	CXString name = clang_getCursorSpelling(cursor);

	field.BoundName = HSFunctionName(clang_getCString(name));
	field.Type = ToHSType(fieldType);
	field.Size = size;
	field.Offset = offset;

	clang_disposeString(name);
	GUARD(size > 0);
	GUARD(offset >= 0);
	GUARD(!field.BoundName.empty());
	return field;
};

std::optional<Struct> ParseStruct(CXCursor cursor, bool allowAnonymous) {
	GUARD(clang_getCursorKind(cursor) == CXCursor_StructDecl);
	if (!allowAnonymous) {
		GUARD(!clang_Cursor_isAnonymous(cursor));
	};

	Struct st{};

	CXString structName = clang_getCursorSpelling(cursor);
	st.SourceName = clang_getCString(structName);
	st.BoundName = HSTypeName(st.SourceName);
	clang_disposeString(structName);

	CXSourceLocation source = clang_getCursorLocation(cursor);
	CXFile file;
	clang_getExpansionLocation(source, &file, nullptr, nullptr, nullptr);
	if (file) {
		CXString filename = clang_getFileName(file);
		st.SourceHeader = clang_getCString(filename);
		clang_disposeString(filename);
	};

	CXType structType = clang_getCursorType(cursor);
	st.Size = clang_Type_getSizeOf(structType);
	st.Alignment = clang_Type_getAlignOf(structType);
	GUARD(!st.BoundName.empty());
	GUARD(!st.SourceName.empty());
	if (st.Size <= 0) {
		if (st.BoundName.back() != '_') {
			st.BoundName += "_";
		};

		return st;
	};

	GUARD(st.Alignment > 0);

	auto ret = clang_visitChildren(cursor, [](CXCursor child, CXCursor, CXClientData cd) {
		Struct* self = reinterpret_cast<Struct*>(cd);
		std::optional<Field> f = ParseStructField(child);
		if (!f.has_value()) {
			return CXChildVisit_Break;
		};

		self->Fields.emplace_back(f.value());
		return CXChildVisit_Continue;

	}, reinterpret_cast<CXClientData>(&st));

	GUARD(ret == 0);
	return st;
};

std::optional<Constant> ParseEnumConstant(CXCursor cursor) {
	GUARD(clang_getCursorKind(cursor) == CXCursor_EnumConstantDecl);
	CXString spelling = clang_getCursorSpelling(cursor);
	HSType name = HSPatternName(clang_getCString(spelling));
	long long value = clang_getEnumConstantDeclValue(cursor);
	clang_disposeString(spelling);

	GUARD(!name.empty());
	GUARD(name != "True");
	GUARD(name != "False");
	GUARD(value != LLONG_MIN);
	return std::optional<Constant>({name, std::to_string(value)});
};

std::optional<Enum> ParseEnum(CXCursor cursor) {
	Enum e{};
	if (clang_Cursor_isAnonymous(cursor)) {
		e.BoundName = "";
	} else {
		CXString enumName = clang_getCursorSpelling(cursor);
		e.BoundName = HSTypeName(clang_getCString(enumName));
		clang_disposeString(enumName);
		GUARD(e.BoundName != "Bool");
		GUARD(!FundamentalTypes.contains(e.BoundName));
	};

	CXType underlyingType = clang_getEnumDeclIntegerType(cursor);
	e.Type = ToHSType(underlyingType);
	GUARD(!e.Type.empty());

	auto ret = clang_visitChildren(cursor, [](CXCursor child, CXCursor, CXClientData cd) {
		Enum* self = reinterpret_cast<Enum*>(cd);
		std::optional<Constant> c = ParseEnumConstant(child);
		if (!c.has_value()) {
			return CXChildVisit_Break;
		};

		self->Members.emplace_back(c.value());
		return CXChildVisit_Continue;
	}, reinterpret_cast<CXClientData>(&e));
	GUARD(ret == 0);

	return e;
};

std::optional<Function> ParseFunction(CXCursor cursor) {
	GUARD(!clang_Cursor_isAnonymous(cursor));
	GUARD(!clang_Cursor_isVariadic(cursor));

	Function fn{};
	CXString fnName = clang_getCursorSpelling(cursor);
	fn.ReturnType = ToHSType(clang_getCursorResultType(cursor));
	fn.SourceName = clang_getCString(fnName);
	fn.BoundName = HSFunctionName(fn.SourceName);
	clang_disposeString(fnName);
	GUARD(!fn.ReturnType.empty());
	GUARD(!fn.SourceName.empty());
	GUARD(!fn.BoundName.empty());

	CXSourceLocation source = clang_getCursorLocation(cursor);
	CXFile file;
	clang_getExpansionLocation(source, &file, nullptr, nullptr, nullptr);
	if (file) {
		CXString filename = clang_getFileName(file);
		fn.SourceHeader = clang_getCString(filename);
		clang_disposeString(filename);
	};

	GUARD(!fn.SourceHeader.empty());

	const int argCount = clang_Cursor_getNumArguments(cursor);
	fn.Arguments.resize(argCount);
	for (int i = 0; i < argCount; i++) {
		CXCursor arg = clang_Cursor_getArgument(cursor, i);
		CXType type = clang_getCursorType(arg);
		CXString name = clang_getCursorSpelling(arg);
		fn.Arguments.at(i) = std::make_pair(ToHSType(type), clang_getCString(name));
		clang_disposeString(name);
		GUARD(!fn.Arguments.at(i).first.empty());
	};

	return fn;
};
