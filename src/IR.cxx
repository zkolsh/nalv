#include "IR.hxx"

Variable::Variable(CXCursor cursor) {
	CXString name = clang_getCursorSpelling(cursor);
	SourceName = clang_getCString(name);
	BoundName = HSBindingName(SourceName);
	clang_disposeString(name);

	CXType type = clang_getCursorType(cursor);
	Type = ToHSType(type);

	SourceHeader = "nalv_unknown_header.h";

	CXSourceLocation source = clang_getCursorLocation(cursor);
	CXFile file;
	clang_getExpansionLocation(source, &file, nullptr, nullptr, nullptr);
	if (file) {
		CXString filename = clang_getFileName(file);
		SourceHeader = clang_getCString(filename);
		clang_disposeString(filename);
	};
};

Typedef::Typedef(CXCursor cursor) {
	CXString alias = clang_getCursorSpelling(cursor);
	BoundName = HSTypeName(clang_getCString(alias));
	clang_disposeString(alias);

	CXType underlying = clang_getTypedefDeclUnderlyingType(cursor);
	SourceName = ToHSType(underlying);
};

Struct::Struct(CXCursor cursor) {
	CXString structName = clang_getCursorSpelling(cursor);
	SourceName = clang_getCString(structName);
	BoundName = HSTypeName(SourceName);

	CXSourceLocation source = clang_getCursorLocation(cursor);
	CXFile file;
	clang_getExpansionLocation(source, &file, nullptr, nullptr, nullptr);
	if (file) {
		CXString filename = clang_getFileName(file);
		SourceHeader = clang_getCString(filename);
		clang_disposeString(filename);
	};

	CXType structType = clang_getCursorType(cursor);
	Size = clang_Type_getSizeOf(structType);
	Alignment = clang_Type_getAlignOf(structType);

	clang_visitChildren(cursor, [](CXCursor child, CXCursor, CXClientData cd) {
		Struct* self = reinterpret_cast<Struct*>(cd);
		if (clang_getCursorKind(child) != CXCursor_FieldDecl) {
			return CXChildVisit_Continue;
		};

		CXType fieldType = clang_getCursorType(child);
		long long size = clang_Type_getSizeOf(fieldType);
		long long offset = clang_Cursor_getOffsetOfField(child) / 8;

		CXString name = clang_getCursorSpelling(child);
		self->Fields.emplace_back(offset, size, ToHSType(fieldType),
			HSFunctionName(clang_getCString(name)));

		clang_disposeString(name);
		return CXChildVisit_Continue;
	}, this);

	clang_disposeString(structName);
};

Enum::Enum(CXCursor cursor) {
	CXString enumName = clang_getCursorSpelling(cursor);
	BoundName = HSTypeName(clang_getCString(enumName));

	CXType underlyingType = clang_getEnumDeclIntegerType(cursor);
	Type = ToHSType(underlyingType);

	clang_visitChildren(cursor, [](CXCursor child, CXCursor, CXClientData clientData) {
		Enum* self = reinterpret_cast<Enum*>(clientData);
		if (clang_getCursorKind(child) != CXCursor_EnumConstantDecl) {
			return CXChildVisit_Continue;
		};

		CXString spelling = clang_getCursorSpelling(child);
		HSType name = HSPatternName(clang_getCString(spelling));
		long long value = clang_getEnumConstantDeclValue(child);

		self->Members.emplace_back(name, std::to_string(value));

		clang_disposeString(spelling);
		return CXChildVisit_Continue;
	}, this);

	clang_disposeString(enumName);
};

Function::Function(CXCursor cursor) {
	CXString fnName = clang_getCursorSpelling(cursor);
	ReturnType = ToHSType(clang_getCursorResultType(cursor));
	SourceName = clang_getCString(fnName);
	BoundName = HSFunctionName(SourceName);

	SourceHeader = "nalv_unknown_header.h";

	CXSourceLocation source = clang_getCursorLocation(cursor);
	CXFile file;
	clang_getExpansionLocation(source, &file, nullptr, nullptr, nullptr);
	if (file) {
		CXString filename = clang_getFileName(file);
		SourceHeader = clang_getCString(filename);
		clang_disposeString(filename);
	};

	const int argCount = clang_Cursor_getNumArguments(cursor);
	Arguments.resize(argCount);
	for (int i = 0; i < argCount; i++) {
		CXCursor arg = clang_Cursor_getArgument(cursor, i);
		CXType type = clang_getCursorType(arg);
		CXString name = clang_getCursorSpelling(arg);
		Arguments.at(i) = std::make_pair(ToHSType(type), clang_getCString(name));
		clang_disposeString(name);
	};

	clang_disposeString(fnName);
};
