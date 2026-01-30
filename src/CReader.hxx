#pragma once
/* CReader.hxx:  Procedures to generate IR from C source */

#include <filesystem>
#include <optional>
#include <utility>

#include <clang-c/Index.h>
#include "IR.hxx"

TranslationUnit FromSourceFile(const std::filesystem::path filename);

/* Collect top-level bindings in a CXCursor to emit later */
CXChildVisitResult TopLevelVisit(CXCursor cursor, CXCursor, CXClientData clientData);

std::optional<Variable> ParseVariable(CXCursor cursor);
std::optional<Typedef> ParseRegularTypedef(CXCursor cursor);
std::optional<std::pair<Struct, Typedef>> ParseStructTypedef(CXCursor cursor);
std::optional<std::pair<Enum, Typedef>> ParseEnumTypedef(CXCursor cursor);
std::optional<Field> ParseStructField(CXCursor cursor);
std::optional<Struct> ParseStruct(CXCursor cursor, bool allowAnonymous = false);
std::optional<Constant> ParseEnumConstant(CXCursor cursor);
std::optional<Enum> ParseEnum(CXCursor cursor);
std::optional<Function> ParseFunction(CXCursor cursor);
