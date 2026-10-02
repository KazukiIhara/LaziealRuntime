#pragma once

#include "Core/Config/RuntimeConfig.h"

#include <filesystem>
#include <optional>

namespace LGF {

	std::optional<RuntimeConfig> LoadRuntimeConfig(
		const std::filesystem::path& filePath);

}
