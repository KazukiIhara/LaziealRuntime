#include "LGF/Runtime.h"

#include <cstdlib>

#include "Core/Framework/Framework.h"

namespace LGF::Detail {
	Framework& GetFramework();
}

int LGF::Run(const RuntimeConfig& config, MainFunction mainFunction) {
	if (mainFunction == nullptr) {
		return EXIT_FAILURE;
	}

	auto& framework = Detail::GetFramework();
	if (!framework.Initialize(config)) {
		return EXIT_FAILURE;
	}

	mainFunction();
	return framework.Finalize() ? EXIT_SUCCESS : EXIT_FAILURE;
}
