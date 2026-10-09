#include "LGF/Runtime.h"

#include <cstdlib>
#include <optional>

#include "Core/Config/RuntimeConfigLoader.h"
#include "Core/Framework/Framework.h"

namespace LGF::Detail {
	Framework& GetFramework();
}

int LGF::Run(MainFunction mainFunction) {
	return Run(mainFunction, "RuntimeSetting.ini");
}

int LGF::Run(
	MainFunction mainFunction,
	const std::filesystem::path& configFilePath) {
	if (mainFunction == nullptr) {
		return EXIT_FAILURE;
	}
	const std::optional<RuntimeConfig> config =
		LoadRuntimeConfig(configFilePath);
	if (!config) {
		return EXIT_FAILURE;
	}

	auto& framework = Detail::GetFramework();
	if (!framework.Initialize(*config)) {
		return EXIT_FAILURE;
	}

	mainFunction();
	return framework.Finalize() ? EXIT_SUCCESS : EXIT_FAILURE;
}
