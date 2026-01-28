#pragma once
/* CReader.hxx:  Procedures to generate IR from C source */

#include <filesystem>
#include <optional>

#include <clang-c/Index.h>
#include "IR.hxx"

TranslationUnit FromSourceFile(const std::filesystem::path filename);

/* Collect top-level bindings in a CXCursor to emit later */
CXChildVisitResult TopLevelVisit(CXCursor cursor, CXCursor, CXClientData clientData);

std::optional<Variable> ParseVariable(CXCursor cursor);
std::optional<Typedef> ParseTypedef(CXCursor cursor);
std::optional<Field> ParseStructField(CXCursor cursor);
std::optional<Struct> ParseStruct(CXCursor cursor);
std::optional<Constant> ParseEnumConstant(CXCursor cursor);
std::optional<Enum> ParseEnum(CXCursor cursor);
std::optional<Function> ParseFunction(CXCursor cursor);
