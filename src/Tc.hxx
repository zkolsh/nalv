#pragma once
/* Tc.hxx:  Procedures for converting C type names into their Haskell names. */

#include <map>
#include <string>

#include <clang-c/Index.h>

using HSType = std::string;

struct BasicType {
	std::string_view Name;
	std::string_view Source;
};

/* Lookup table for types that can be used directly in CApiFFI. */
inline const std::map<std::string, BasicType> FundamentalTypes{
	{"bool", {"CBool", "Foreign.C.Types"}},
	{"_Bool", {"CBool", "Foreign.C.Types"}},
	{"char", {"CChar", "Foreign.C.Types"}},
	{"clock_t", {"CClock", "Foreign.C.Types"}},
	{"double", {"CDouble", "Foreign.C.Types"}},
	{"FILE", {"CFile", "Foreign.C.Types"}},
	{"float", {"CFloat", "Foreign.C.Types"}},
	{"fpos_t", {"CFpos", "Foreign.C.Types"}},
	{"int16_t", {"CShort", "Foreign.C.Types"}},
	{"int32_t", {"CInt", "Foreign.C.Types"}},
	{"int64_t", {"CLLong", "Foreign.C.Types"}},
	{"int8_t", {"CChar", "Foreign.C.Types"}},
	{"int", {"CInt", "Foreign.C.Types"}},
	{"intmax_t", {"CIntMax", "Foreign.C.Types"}},
	{"intptr_t", {"CIntPtr", "Foreign.C.Types"}},
	{"jmp_buf", {"CJmpBuf", "Foreign.C.Types"}},
	{"long", {"CLong", "Foreign.C.Types"}},
	{"long long", {"CLLong", "Foreign.C.Types"}},
	{"ptrdiff_t", {"CPtrdiff", "Foreign.C.Types"}},
	{"short", {"CShort", "Foreign.C.Types"}},
	{"sig_atomic_t", {"CSigAtomic", "Foreign.C.Types"}},
	{"signed char", {"CSChar", "Foreign.C.Types"}},
	{"size_t", {"CSize", "Foreign.C.Types"}},
	{"ssize_t", {"CSsize", "Foreign.C.Types"}},
	{"suseconds_t", {"CSUSeconds", "Foreign.C.Types"}},
	{"time_t", {"CTime", "Foreign.C.Types"}},
	{"uint16_t", {"CUShort", "Foreign.C.Types"}},
	{"uint32_t", {"CUInt", "Foreign.C.Types"}},
	{"uint64_t", {"CULLong", "Foreign.C.Types"}},
	{"uint8_t", {"CUChar", "Foreign.C.Types"}},
	{"uintmax_t", {"CUIntMax", "Foreign.C.Types"}},
	{"uintptr_t", {"CUIntPtr", "Foreign.C.Types"}},
	{"unsigned char", {"CUChar", "Foreign.C.Types"}},
	{"unsigned int", {"CUInt", "Foreign.C.Types"}},
	{"unsigned long", {"CULong", "Foreign.C.Types"}},
	{"unsigned long long", {"CULLong", "Foreign.C.Types"}},
	{"unsigned short", {"CUShort", "Foreign.C.Types"}},
	{"useconds_t", {"CUSeconds", "Foreign.C.Types"}},
	{"wchar_t", {"CWChar", "Foreign.C.Types"}},
	{"void", {"()", "Prelude"}},
};

HSType ToHSType(CXType type);

HSType HSTypeName(const std::string_view cname);
std::string HSBindingName(const std::string_view cname);
#define HSFunctionName(x) HSBindingName((x))
std::string HSPatternName(const std::string_view cname);
