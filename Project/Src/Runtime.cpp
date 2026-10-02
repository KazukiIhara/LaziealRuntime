#include "LGF/Runtime.h"

#include <cstdlib>
#include <optional>

#include "Core/Config/RuntimeConfigLoader.h"
#include "Core/Framework/Framework.h"

namespace LGF::Detail {
	Framework& GetFramework();
}

int LGF::Run(MainFunction mainFunction) {
	if (mainFunction == nullptr) {
		return EXIT_FAILURE;
	}
	const std::optional<RuntimeConfig> config =
		LoadRuntimeConfig("RuntimeSetting.ini");
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
