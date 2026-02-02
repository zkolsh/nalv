#include "Tc.hxx"
#include <clang-c/Index.h>
#include <vector>
#include <iostream>

static inline std::string Parens(const std::string s) {
	if (s.find(' ') != std::string::npos) {
		return "(" + s + ")";
	} else {
		return s;
	};
};

std::string HSBindingName(const std::string_view cname) {
	std::string o{};
	if (cname.empty()) return o;
	o.reserve(cname.size());

	size_t wordStart = 0;
	o += std::tolower(cname[0]);
	for (size_t i = 1; i < cname.length(); i++) {
		if (cname[i] == '_') {
			wordStart = i + 1;
			continue;
		};
		
		if (wordStart == i) {
			o += std::toupper(cname[i]);
		} else {
			o += cname[i];
		};
	};

	static const std::vector<std::string_view> keywords{
		"data", "type", "module", "import", "newtype", "deriving", "instance", "let", "in", "case", "where"
	};

	for (const auto& k : keywords) {
		if (o == k) {
			o += "'";
			break;
		};
	};

	return o;
};

std::string HSTypeName(const std::string_view cname) {
	if (cname.empty()) {
		std::cerr << ":: HSTypeName called with an empty string." << std::endl;
		return "";
	};

	std::string o{};
	o.reserve(cname.size());

	size_t wordStart = 0;
	o += std::toupper(cname[0]);
	for (size_t i = 1; i < cname.length(); i++) {
		if (cname[i] == '_') {
			wordStart = i + 1;
			continue;
		};

		if (wordStart == i) {
			o += std::toupper(cname[i]);
		} else {
			o += cname[i];
		};
	};

	return o;
};

std::string HSPatternName(const std::string_view cname) {
	if (cname.empty()) {
		std::cerr << ":: HSPatternName called with an empty string." << std::endl;
		return "";
	};

	std::string o{cname};
	o[0] = std::toupper(o.at(0));
	return o;
};

static HSType FunctionType(CXType type) {
	std::string o{};
	
	int argCount = clang_getNumArgTypes(type);
	for (int i = 0; i < argCount; i++) {
		std::string arg = ToHSType(clang_getArgType(type, i));
		o += arg + " -> ";
	};

	HSType rt = ToHSType(clang_getResultType(type));

	return o + "IO " + Parens(rt);
};

HSType ToHSType(CXType type) {
	if (type.kind == CXType_Attributed) {
		return ToHSType(clang_Type_getModifiedType(type));
	};

	if (type.kind == CXType_Elaborated) {
		return ToHSType(clang_Type_getNamedType(type));
	};

	if (type.kind == CXType_Unexposed) {
		CXType canon = clang_getCanonicalType(type);
		if (canon.kind != CXType_Unexposed) {
			return ToHSType(canon);
		};
	};

	/* Fundamental types by name */
	/* In some situations, these types might fall into the typedef case, which
	 * would incorrectly convert, for example, uint8_t into Uint8T. */
	CXString spelling = clang_getTypeSpelling(type);
	std::string name = clang_getCString(spelling);
	clang_disposeString(spelling);
	if (FundamentalTypes.contains(name)) {
		return std::string(FundamentalTypes.at(name).Name);
	};

	std::string o{};

	switch (type.kind) {
	case CXType_Invalid: /* FALLTHROUGH */
	case CXType_Unexposed:
		o += "NalvUnknownType";
		break;

	case CXType_Pointer: {
		CXType pointee = clang_getPointeeType(type);
		CXType canonPointee = clang_getCanonicalType(pointee);

		if (canonPointee.kind == CXType_FunctionProto
		 || canonPointee.kind == CXType_FunctionNoProto) {
			/* Function pointers. */
			o += "FunPtr " + Parens(FunctionType(canonPointee));
		} else {
			/* Regular pointers. */
			HSType innerType = ToHSType(pointee);

			if (clang_Type_getSizeOf(pointee) < 0
			 && innerType.back() != '_'
			 && pointee.kind == canonPointee.kind
			 && canonPointee.kind != CXType_Void) {
				innerType += "_";
			};

			HSType pointer = clang_isConstQualifiedType(pointee)?
				"ConstPtr" : "Ptr";

			o += pointer + " " + Parens(innerType);
		};

		break;
	};

	case CXType_FunctionProto: /* FALLTHROUGH */
	case CXType_FunctionNoProto: {
		o += FunctionType(type);
		break;
	};

	/* libclang makes a difference between char* and char[], but CApiFFI doesn't. */
	case CXType_IncompleteArray:
	case CXType_VariableArray:
	case CXType_DependentSizedArray:
	case CXType_ConstantArray: {
		CXType pointee = clang_getArrayElementType(type);
		HSType innerType = Parens(ToHSType(pointee));

		HSType pointer = clang_isConstQualifiedType(pointee)?
			"ConstPtr" : "Ptr";

		o = pointer + " " + innerType;
		break;
	};

	/* Fundamental types by type kind */
	case CXType_Void: /* FALLTHROUGH */
	case CXType_Bool: /* FALLTHROUGH */
	case CXType_Char_U: /* FALLTHROUGH */
	case CXType_UChar: /* FALLTHROUGH */
	case CXType_Char16: /* FALLTHROUGH */
	case CXType_Char32: /* FALLTHROUGH */
	case CXType_UShort: /* FALLTHROUGH */
	case CXType_UInt: /* FALLTHROUGH */
	case CXType_ULong: /* FALLTHROUGH */
	case CXType_ULongLong: /* FALLTHROUGH */
	case CXType_UInt128: /* FALLTHROUGH */
	case CXType_Char_S: /* FALLTHROUGH */
	case CXType_SChar: /* FALLTHROUGH */
	case CXType_WChar: /* FALLTHROUGH */
	case CXType_Short: /* FALLTHROUGH */
	case CXType_Int: /* FALLTHROUGH */
	case CXType_Long: /* FALLTHROUGH */
	case CXType_LongLong: /* FALLTHROUGH */
	case CXType_Int128: /* FALLTHROUGH */
	case CXType_Float: /* FALLTHROUGH */
	case CXType_Double: /* FALLTHROUGH */
	case CXType_LongDouble: /* FALLTHROUGH */
	case CXType_NullPtr: /* FALLTHROUGH */
	case CXType_Overload: /* FALLTHROUGH */
	case CXType_Dependent: /* FALLTHROUGH */
	case CXType_Float128: /* FALLTHROUGH */
	case CXType_Half: /* FALLTHROUGH */
	case CXType_Float16: /* FALLTHROUGH */
	case CXType_ShortAccum: /* FALLTHROUGH */
	case CXType_Accum: /* FALLTHROUGH */
	case CXType_LongAccum: /* FALLTHROUGH */
	case CXType_UShortAccum: /* FALLTHROUGH */
	case CXType_UAccum: /* FALLTHROUGH */
	case CXType_ULongAccum: /* FALLTHROUGH */
	case CXType_BFloat16: /* FALLTHROUGH */
	case CXType_Ibm128: {
		type = clang_getUnqualifiedType(type);

		CXString spelling = clang_getTypeSpelling(type);
		std::string name = clang_getCString(spelling);

		if (FundamentalTypes.contains(name)) {
			o += FundamentalTypes.at(name).Name;
		} else {
			std::cerr << ":: builtin type not found on global table: "
			          << name << std::endl;
			o += HSTypeName(name);
		};

		clang_disposeString(spelling);
		break;
	};

	/* Type aliases should be emitted by Nalv, so they can be used normally. */
	case CXType_Typedef: {
		CXString source = clang_getTypedefName(type);
		std::string name = clang_getCString(source);
		clang_disposeString(source);
		o += HSTypeName(name);
		break;
	};

	/* Any other name we don't know should have gone through these same
	 * mechanisms, and the only thing we can do is hope that the
	 * equivalent name is valid. */
	default: {
		type = clang_getUnqualifiedType(type);

		CXString spelling = clang_getTypeSpelling(type);
		std::string name = clang_getCString(spelling);

		const std::string_view prefixes[] = {"struct ", "enum ", "union "};
		for (const auto& pfx : prefixes) {
			if (name.find(pfx) == 0) {
				name.erase(0, pfx.length());
			};
		};

		if (FundamentalTypes.contains(name)) {
			o += FundamentalTypes.at(name).Name;
		} else {
			o += HSTypeName(name);
		};

		clang_disposeString(spelling);
		break;
	};
	};

	return o;
};
