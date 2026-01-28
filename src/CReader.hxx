#pragma once
/* CReader.hxx:  Procedures to generate IR from C source */

#include <filesystem>

#include <clang-c/Index.h>
#include "IR.hxx"

TranslationUnit FromSourceFile(const std::filesystem::path filename);

/* Collect top-level bindings in a CXCursor to emit later */
CXChildVisitResult TopLevelVisit(CXCursor cursor, CXCursor, CXClientData clientData);
