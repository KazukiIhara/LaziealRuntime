#pragma once

#include <filesystem>

namespace LGF {

	using MainFunction = void(*)();

	int Run(MainFunction mainFunction);
	int Run(
		MainFunction mainFunction,
		const std::filesystem::path& configFilePath);

}

void Main();
