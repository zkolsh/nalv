#include "CReader.hxx"
#include <cassert>
#include <iostream>

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

	for (const auto& struct_ : bindings.DataTypes) {
		bindings.UnknownNames.erase(struct_.BoundName);
		bindings.UnknownNames.erase(struct_.BoundName + "_");
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
		if (clang_Cursor_isAnonymous(cursor)) {
			return CXChildVisit_Continue;
		};

		CXString cursorName = clang_getCursorSpelling(cursor);
		std::string name = HSTypeName(clang_getCString(cursorName));
		clang_disposeString(cursorName);

		if (name.empty()) {
			std::cerr << "StructDecl: empty underlying name." << std::endl;
			break;
		};

		if (!clang_isCursorDefinition(cursor)) {
			if (!name.empty()) {
				bindings->UnknownNames.insert(name + "_");
			};
		} else {
			bindings->DataTypes.emplace(cursor);
			bindings->KnownNames.insert(name);
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

		CXString cursorName = clang_getCursorSpelling(cursor);
		HSType name = HSTypeName(clang_getCString(cursorName));
		if (name != "Bool") {
			bindings->KnownNames.insert(name);
			bindings->Enumerations.emplace(cursor);
		};
		clang_disposeString(cursorName);

		break;
	};

	/* This is probably not possible, but trivial to implement with little risk. */
	case CXCursor_EnumConstantDecl: {
		CXCursor canon = clang_getCanonicalCursor(cursor);
		if (!clang_equalCursors(cursor, canon)) {
			break;
		};

		CXString cursorName = clang_getCursorSpelling(cursor);
		HSType name = HSTypeName(clang_getCString(cursorName));

		/* Boolean types are sometimes defined manually.  Do not emit them. */
		if (name != "True" && name != "False") {
			long long value = clang_getEnumConstantDeclValue(cursor);
			bindings->Constants.emplace(name, std::to_string(value));
		};

		clang_disposeString(cursorName);
		break;
	};

	case CXCursor_FunctionDecl: {
		CXCursor canon = clang_getCanonicalCursor(cursor);
		if (!clang_equalCursors(cursor, canon)) {
			break;
		};

		if (clang_Cursor_isVariadic(cursor)) {
			break;
		};

		bindings->Functions.emplace(cursor);
		break;
	};

	case CXCursor_VarDecl: {
		CXCursor canon = clang_getCanonicalCursor(cursor);
		if (!clang_equalCursors(cursor, canon)) {
			break;
		};

		bindings->Bindings.emplace(cursor);
		break;
	};

	case CXCursor_TypedefDecl: {
		if (clang_Cursor_isAnonymous(cursor)) {
			return CXChildVisit_Continue;
		};

		CXCursor canon = clang_getCanonicalCursor(cursor);
		if (!clang_equalCursors(cursor, canon)) {
			break;
		};

		Typedef t{cursor};
		if (bindings->KnownNames.contains(t.BoundName)) {
			break;
		};

		if (t.SourceName.empty() == true) {
			std::cerr << "TypedefDecl: empty underlying name." << std::endl;
			break;
		};

		//FIXME: hacky
		if (t.BoundName.find("VaListTag") != std::string::npos
		 || t.SourceName.find("VaListTag") != std::string::npos) {
			break;
		};

		/* Already in Prelude */
		if (t.BoundName == "Bool") {
			break;
		};

		if (t.SourceName.find("Ptr ") != std::string::npos
		 || t.SourceName.find("ConstPtr ") != std::string::npos
		 || t.SourceName.find("FunPtr ") != std::string::npos
		 || bindings->UnknownNames.contains(t.SourceName)
		 || FundamentalTypes.contains(t.SourceName)) {
			bindings->KnownNames.insert(t.BoundName);
			bindings->TypeAliases.insert(std::move(t));
		} else if (bindings->UnknownNames.contains(t.SourceName + "_")) {
			t.SourceName += "_";
			bindings->KnownNames.insert(t.BoundName);
			bindings->TypeAliases.insert(std::move(t));
		} else {
			bindings->KnownNames.insert(t.BoundName);
			bindings->UnknownNames.insert(t.SourceName);
			bindings->TypeAliases.insert(std::move(t));
		};

		break;
	};

	default:
		break;
	};

	return CXChildVisit_Continue;
};
