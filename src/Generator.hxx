#pragma once

#include <filesystem>

#include "IR.hxx"

constexpr std::string_view Tab = "        ";

#if !defined(CRLF)
constexpr std::string_view LineEnd = "\n";
#else
constexpr std::string_view LineEnd = "\r\n";
#endif

void BindingsToFile(const std::filesystem::path filename, const TranslationUnit& bindings);
