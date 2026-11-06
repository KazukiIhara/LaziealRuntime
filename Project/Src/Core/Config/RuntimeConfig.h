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

	struct GamepadSensorConfig {
		bool gyroscopeEnabled = true;
		bool accelerometerEnabled = true;
	};

	struct JoyConConfig {
		GamepadSensorConfig left{};
		GamepadSensorConfig right{};
	};

	struct GamepadConfig {
		bool enabled = true;
		bool combineJoyCons = false;
		bool verticalJoyCons = true;
		bool backgroundInput = false;
		JoyConConfig joyCon{};
	};

	struct InputConfig {
		GamepadConfig gamepad{};
	};

	struct GraphicsConfig {
		bool vsync = true;
		bool multiFrameContextEnabled = true;
	};

	struct AudioConfig {
		bool enabled = true;
		float masterVolume = 1.0f;
	};

	struct RuntimeConfig {
		std::filesystem::path assetRoot = "Assets";
		WindowConfig window{};
		GraphicsConfig graphics{};
		AudioConfig audio{};
		InputConfig input{};
	};

}
