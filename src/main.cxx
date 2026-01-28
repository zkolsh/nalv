#include <cassert>
#include <iostream>
#include <filesystem>

#include "CReader.hxx"
#include "IR.hxx"
#include "Generator.hxx"

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: " << argv[0] << " <C source file>" << std::endl;
		return EXIT_FAILURE;
	};

	TranslationUnit bindings = FromSourceFile(argv[1]);
	const std::string outName = HSTypeName(std::filesystem::path(argv[1]).stem().string()) + ".hs";
	BindingsToFile("build/" + outName, bindings);

	return EXIT_SUCCESS;
};
