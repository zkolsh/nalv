#include "Tc.hxx"
#include <vector>
#include <iostream>

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

HSType ToHSType(CXType type) {
	std::string o{};
	type = clang_getCanonicalType(type);

	switch (type.kind) {
	case CXType_Invalid: /* FALLTHROUGH */
	case CXType_Unexposed:
		o += "NalvUnknownType";
		break;

	case CXType_Pointer: {
		CXType pointee = clang_getPointeeType(type);
		pointee = clang_getCanonicalType(pointee);

		if (pointee.kind == CXType_FunctionProto
		 || pointee.kind == CXType_FunctionNoProto) {
			/* Function pointers. */
			o += "FunPtr (";

			int argCount = clang_getNumArgTypes(pointee);
			for (int i = 0; i < argCount; i++) {
				std::string arg = ToHSType(clang_getArgType(pointee, i));
				o += arg + " -> ";
			};

			HSType rt = ToHSType(clang_getResultType(pointee));
			if (rt.find(' ') != std::string::npos) {
				rt = "(" + rt + ")";
			};

			o += "IO " + rt + ")";
		} else {
			/* Regular pointers. */
			HSType innerType = ToHSType(pointee);

			if (innerType.find(' ') != std::string::npos) {
				innerType = "(" + innerType + ")";
			};

			HSType pointer = clang_isConstQualifiedType(pointee)?
				"ConstPtr" : "Ptr";

			o += pointer + " " + innerType;
		};

		break;
	};

	/* libclang makes a difference between char* and char[], but CApiFFI doesn't. */
	case CXType_IncompleteArray:
	case CXType_VariableArray:
	case CXType_DependentSizedArray:
	case CXType_ConstantArray: {
		CXType pointee = clang_getArrayElementType(type);
		HSType innerType = ToHSType(pointee);

		if (innerType.find(' ') != std::string::npos) {
			innerType = "(" + innerType + ")";
		};

		HSType pointer = clang_isConstQualifiedType(pointee)?
			"ConstPtr" : "Ptr";

		o = pointer + " " + innerType;
		break;
	};

	/* Fundamental types */
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
	case CXType_Typedef: /* FALLTHROUGH */

	/* Structs, enums and anything else we don't know:  Hope that the 
	 * Haskell name already exists, and/or that an opaque data type
	 * will be emitted for it. */
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
