#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace LGF {

	struct WindowConfig {
		std::wstring title = L"LGF";
		uint32_t width = 800u;
		uint32_t height = 600u;
		bool fullscreen = false;
	};

	struct GamepadConfig {
		bool enabled = true;
		bool combineJoyCons = false;
		bool verticalJoyCons = true;
		bool backgroundInput = false;
	};

	struct InputConfig {
		GamepadConfig gamepad{};
	};

	struct RuntimeConfig {
		std::filesystem::path assetRoot = "Assets";
		WindowConfig window{};
		InputConfig input{};
	};

}
