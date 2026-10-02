#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace LGF {

	struct WindowConfig final {
		std::wstring title = L"LGF";
		uint32_t width = 800u;
		uint32_t height = 600u;
		bool fullscreen = false;
	};

	struct RuntimeConfig final {
		std::filesystem::path assetRoot = "Assets";
		WindowConfig window{};
	};

}
