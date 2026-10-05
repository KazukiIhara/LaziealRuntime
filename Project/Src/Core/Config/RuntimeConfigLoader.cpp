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
	std::ifstream file(filePath);
	if (!file) {
		return std::nullopt;
	}

	RuntimeConfig config{};
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
		}
	}

	return config;
}
