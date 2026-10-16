#include "RuntimeConfigLoader.h"

#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

namespace {

	std::string_view Trim(std::string_view value) {
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
			value.remove_prefix(1u);
		}
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
			value.remove_suffix(1u);
		}
		return value;
	}

	std::string ToLower(std::string_view value) {
		std::string result(value);
		std::ranges::transform(result, result.begin(), [](unsigned char character) {
			return static_cast<char>(std::tolower(character));
		});
		return result;
	}

	bool ParseUint32(std::string_view value, uint32_t& result) {
		value = Trim(value);
		uint32_t parsed = 0u;
		const auto [end, error] = std::from_chars(
			value.data(),
			value.data() + value.size(),
			parsed);
		if (error != std::errc{} || end != value.data() + value.size() || parsed == 0u) {
			return false;
		}
		result = parsed;
		return true;
	}

	bool ParseBool(std::string_view value, bool& result) {
		const std::string normalized = ToLower(Trim(value));
		if (normalized == "true" || normalized == "1") {
			result = true;
			return true;
		}
		if (normalized == "false" || normalized == "0") {
			result = false;
			return true;
		}
		return false;
	}

	std::optional<std::string> WideToUtf8(std::wstring_view value) {
		if (value.empty()) {
			return std::string{};
		}
		if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
			return std::nullopt;
		}

		const int sourceSize = static_cast<int>(value.size());
		const int requiredSize = WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			value.data(),
			sourceSize,
			nullptr,
			0,
			nullptr,
			nullptr);
		if (requiredSize <= 0) {
			return std::nullopt;
		}

		std::string result(static_cast<std::size_t>(requiredSize), '\0');
		if (WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			value.data(),
			sourceSize,
			result.data(),
			requiredSize,
			nullptr,
			nullptr) != requiredSize) {
			return std::nullopt;
		}
		return result;
	}

	bool WriteRuntimeConfig(
		const std::filesystem::path& filePath,
		const LGF::RuntimeConfig& config) {
		const std::filesystem::path parentPath = filePath.parent_path();
		if (!parentPath.empty()) {
			std::error_code error;
			std::filesystem::create_directories(parentPath, error);
			if (error) {
				return false;
			}
		}

		const std::optional<std::string> assetRoot =
			WideToUtf8(config.assetRoot.wstring());
		const std::optional<std::string> windowTitle =
			WideToUtf8(config.window.title);
		if (!assetRoot || !windowTitle) {
			return false;
		}

		std::filesystem::path temporaryPath = filePath;
		temporaryPath += ".tmp";
		{
			std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
			if (!file) {
				return false;
			}

			file << std::boolalpha
				<< "[Runtime]\n"
				<< "assetRoot = " << *assetRoot << "\n\n"
				<< "[Graphics]\n"
				<< "vsync = " << config.graphics.vsync << '\n'
				<< "frameContextCount = " << config.graphics.frameContextCount << "\n\n"
				<< "[Window]\n"
				<< "title = " << *windowTitle << '\n'
				<< "width = " << config.window.width << '\n'
				<< "height = " << config.window.height << '\n'
				<< "fullscreen = " << config.window.fullscreen << "\n\n"
				<< "[Input.Gamepad]\n"
				<< "enabled = " << config.input.gamepad.enabled << '\n'
				<< "combineJoyCons = " << config.input.gamepad.combineJoyCons << '\n'
				<< "verticalJoyCons = " << config.input.gamepad.verticalJoyCons << '\n'
				<< "backgroundInput = " << config.input.gamepad.backgroundInput << "\n\n"
				<< "[Input.Gamepad.JoyCon.Left]\n"
				<< "gyroscopeEnabled = "
				<< config.input.gamepad.joyCon.left.gyroscopeEnabled << '\n'
				<< "accelerometerEnabled = "
				<< config.input.gamepad.joyCon.left.accelerometerEnabled << "\n\n"
				<< "[Input.Gamepad.JoyCon.Right]\n"
				<< "gyroscopeEnabled = "
				<< config.input.gamepad.joyCon.right.gyroscopeEnabled << '\n'
				<< "accelerometerEnabled = "
				<< config.input.gamepad.joyCon.right.accelerometerEnabled << '\n';
			if (!file) {
				return false;
			}
		}

		if (!MoveFileExW(
			temporaryPath.c_str(),
			filePath.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			std::error_code ignored;
			std::filesystem::remove(temporaryPath, ignored);
			return false;
		}
		return true;
	}

	std::optional<std::wstring> Utf8ToWide(std::string_view value) {
		if (value.empty()) {
			return std::wstring{};
		}
		if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
			return std::nullopt;
		}

		const int sourceSize = static_cast<int>(value.size());
		const int requiredSize = MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			value.data(),
			sourceSize,
			nullptr,
			0);
		if (requiredSize <= 0) {
			return std::nullopt;
		}

		std::wstring result(static_cast<std::size_t>(requiredSize), L'\0');
		if (MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			value.data(),
			sourceSize,
			result.data(),
			requiredSize) != requiredSize) {
			return std::nullopt;
		}
		return result;
	}

}

std::optional<LGF::RuntimeConfig> LGF::LoadRuntimeConfig(
	const std::filesystem::path& filePath) {
	RuntimeConfig config{};
	std::ifstream file(filePath);
	if (!file) {
		std::error_code error;
		if (std::filesystem::exists(filePath, error) || error) {
			return std::nullopt;
		}

		if (!WriteRuntimeConfig(filePath, config)) {
			return std::nullopt;
		}
	} else {
		std::string section;
		std::string line;
		bool firstLine = true;
		while (std::getline(file, line)) {
			if (firstLine) {
				firstLine = false;
				if (line.starts_with("\xEF\xBB\xBF")) {
					line.erase(0u, 3u);
				}
			}

			const std::string_view trimmed = Trim(line);
			if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#') {
				continue;
			}
			if (trimmed.front() == '[' && trimmed.back() == ']') {
				section = ToLower(Trim(trimmed.substr(1u, trimmed.size() - 2u)));
				continue;
			}

			const std::size_t separator = trimmed.find('=');
			if (separator == std::string_view::npos) {
				return std::nullopt;
			}
			const std::string key = ToLower(Trim(trimmed.substr(0u, separator)));
			const std::string_view value = Trim(trimmed.substr(separator + 1u));

			if (section == "runtime" && key == "assetroot") {
				if (value.empty()) {
					return std::nullopt;
				}
				const std::optional<std::wstring> assetRoot = Utf8ToWide(value);
				if (!assetRoot) {
					return std::nullopt;
				}
				config.assetRoot = *assetRoot;
			} else if (section == "graphics" && key == "vsync") {
				if (!ParseBool(value, config.graphics.vsync)) {
					return std::nullopt;
				}
			} else if (section == "graphics" && key == "framecontextcount") {
				if (!ParseUint32(value, config.graphics.frameContextCount) ||
					(config.graphics.frameContextCount != 1u &&
					 config.graphics.frameContextCount != 2u)) {
					return std::nullopt;
				}
			} else if (section == "window" && key == "title") {
				const std::optional<std::wstring> title = Utf8ToWide(value);
				if (!title) {
					return std::nullopt;
				}
				config.window.title = *title;
			} else if (section == "window" && key == "width") {
				if (!ParseUint32(value, config.window.width)) {
					return std::nullopt;
				}
			} else if (section == "window" && key == "height") {
				if (!ParseUint32(value, config.window.height)) {
					return std::nullopt;
				}
			} else if (section == "window" && key == "fullscreen") {
				if (!ParseBool(value, config.window.fullscreen)) {
					return std::nullopt;
				}
			} else if (section == "input.gamepad" && key == "enabled") {
				if (!ParseBool(value, config.input.gamepad.enabled)) {
					return std::nullopt;
				}
			} else if (section == "input.gamepad" && key == "combinejoycons") {
				if (!ParseBool(value, config.input.gamepad.combineJoyCons)) {
					return std::nullopt;
				}
			} else if (section == "input.gamepad" && key == "verticaljoycons") {
				if (!ParseBool(value, config.input.gamepad.verticalJoyCons)) {
					return std::nullopt;
				}
			} else if (section == "input.gamepad" && key == "backgroundinput") {
				if (!ParseBool(value, config.input.gamepad.backgroundInput)) {
					return std::nullopt;
				}
			} else if (section == "input.gamepad.joycon.left" && key == "gyroscopeenabled") {
				if (!ParseBool(value, config.input.gamepad.joyCon.left.gyroscopeEnabled)) {
					return std::nullopt;
				}
			} else if (section == "input.gamepad.joycon.left" && key == "accelerometerenabled") {
				if (!ParseBool(value, config.input.gamepad.joyCon.left.accelerometerEnabled)) {
					return std::nullopt;
				}
			} else if (section == "input.gamepad.joycon.right" && key == "gyroscopeenabled") {
				if (!ParseBool(value, config.input.gamepad.joyCon.right.gyroscopeEnabled)) {
					return std::nullopt;
				}
			} else if (section == "input.gamepad.joycon.right" && key == "accelerometerenabled") {
				if (!ParseBool(value, config.input.gamepad.joyCon.right.accelerometerEnabled)) {
					return std::nullopt;
				}
			}
		}

		file.close();
		if (!WriteRuntimeConfig(filePath, config)) {
			return std::nullopt;
		}
	}

	if (config.assetRoot.is_relative()) {
		const std::filesystem::path configDirectory =
			std::filesystem::absolute(filePath).parent_path();
		config.assetRoot =
			(configDirectory / config.assetRoot).lexically_normal();
	}

	return config;
}
