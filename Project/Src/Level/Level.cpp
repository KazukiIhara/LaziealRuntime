#include "LGF/Level.h"

#include "File/JsonFile.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

using namespace LGF;

namespace {

	using Json = nlohmann::json;
	constexpr int kLevelVersion = 1;

	LevelIoResult MakeError(const std::filesystem::path& filePath, const char* message) {
		const std::u8string pathText = filePath.u8string();
		return LevelIoResult{
			.message = std::string(reinterpret_cast<const char*>(pathText.data()), pathText.size()) + ": " + message,
		};
	}

	float ReadFloat(const Json& value) {
		if (!value.is_number()) {
			throw std::runtime_error("Transform values must be numbers.");
		}
		const double number = value.get<double>();
		constexpr double maxFloat = std::numeric_limits<float>::max();
		if (!std::isfinite(number) || number < -maxFloat || number > maxFloat) {
			throw std::runtime_error("Transform values must be finite floats.");
		}
		return static_cast<float>(number);
	}

	Vector2 ReadVector2(const Json& value) {
		if (!value.is_array() || value.size() != 2u) {
			throw std::runtime_error("Translation and Scale must contain two numbers.");
		}
		return Vector2{ ReadFloat(value[0]), ReadFloat(value[1]) };
	}

	Transform2D ReadTransform(const Json& value) {
		if (!value.is_object()) {
			throw std::runtime_error("Transform must be an object.");
		}

		Transform2D transform{};
		if (value.contains("Translation")) {
			transform.translation = ReadVector2(value.at("Translation"));
		}
		if (value.contains("Rotation")) {
			transform.rotation = ReadFloat(value.at("Rotation"));
		}
		if (value.contains("Scale")) {
			transform.scale = ReadVector2(value.at("Scale"));
		}
		return transform;
	}

	void ValidateLevel(const LevelData2D& level) {
		std::unordered_set<std::string_view> ids;
		for (const LevelObject2D& object : level.objects) {
			if (object.id.empty()) {
				throw std::runtime_error("Object Id must not be empty.");
			}
			if (!ids.emplace(object.id).second) {
				throw std::runtime_error("Duplicate object Id: " + object.id);
			}
			if (object.type.empty()) {
				throw std::runtime_error("Object Type must not be empty: " + object.id);
			}

			const Transform2D& transform = object.transform;
			if (!std::isfinite(transform.translation.x) || !std::isfinite(transform.translation.y) ||
				!std::isfinite(transform.rotation) ||
				!std::isfinite(transform.scale.x) || !std::isfinite(transform.scale.y)) {
				throw std::runtime_error("Object Transform must be finite: " + object.id);
			}
		}
	}

	LevelData2D ReadLevel(const Json& root) {
		const Json& version = root.at("Version");
		if (!version.is_number_integer() || version != kLevelVersion) {
			throw std::runtime_error("Unsupported 2D level version.");
		}

		const Json& objects = root.at("Objects");
		if (!objects.is_array()) {
			throw std::runtime_error("Objects must be an array.");
		}

		LevelData2D level{ .name = root.value("Name", std::string{}) };
		level.objects.reserve(objects.size());
		for (const Json& value : objects) {
			try {
				level.objects.push_back(LevelObject2D{
					.id = value.at("Id").get<std::string>(),
					.type = value.at("Type").get<std::string>(),
					.transform = ReadTransform(value.value("Transform", Json::object())),
					.assetPath = value.value("AssetPath", std::string{}),
				});
			} catch (const std::exception& error) {
				throw std::runtime_error("Objects[" + std::to_string(level.objects.size()) + "]: " + error.what());
			}
		}
		ValidateLevel(level);
		return level;
	}

	Json WriteLevel(const LevelData2D& level) {
		Json objects = Json::array();
		for (const LevelObject2D& object : level.objects) {
			const Transform2D& transform = object.transform;
			objects.push_back({
				{ "Id", object.id },
				{ "Type", object.type },
				{ "Transform", {
					{ "Translation", { transform.translation.x, transform.translation.y } },
					{ "Rotation", transform.rotation },
					{ "Scale", { transform.scale.x, transform.scale.y } },
				} },
				{ "AssetPath", object.assetPath },
			});
		}
		return Json{
			{ "Version", kLevelVersion },
			{ "Name", level.name },
			{ "Objects", std::move(objects) },
		};
	}

}

LevelIoResult LGF::LoadLevel2D(const std::filesystem::path& filePath, LevelData2D& level) {
	try {
		std::ifstream file(filePath, std::ios::binary);
		if (!file) {
			return MakeError(filePath, "Failed to open the level file.");
		}
		level = ReadLevel(Json::parse(file));
		return LevelIoResult{ .succeeded = true };
	} catch (const std::exception& error) {
		return MakeError(filePath, error.what());
	}
}

LevelIoResult LGF::SaveLevel2D(const std::filesystem::path& filePath, const LevelData2D& level) {
	try {
		ValidateLevel(level);
		Internal::WriteJsonFile(filePath, WriteLevel(level).dump(2));
		return LevelIoResult{ .succeeded = true };
	} catch (const std::exception& error) {
		return MakeError(filePath, error.what());
	}
}

LevelIoResult LGF::LoadLevelData(const std::filesystem::path& filePath, LevelData& level) {
	try {
		std::ifstream file(filePath, std::ios::binary);
		if (!file) {
			return MakeError(filePath, "Failed to open the level file.");
		}
		const Json root = Json::parse(file);
		if (root.value("Type", std::string("Level")) != "Level") {
			return MakeError(filePath, "Expected a Level document.");
		}
		const Json& version = root.at("Version");
		if (!version.is_number_integer() || version != kLevelVersion) {
			return MakeError(filePath, "Unsupported level data version.");
		}
		const auto readPath = [&root](const char* name) {
			const std::string path = root.at(name).get<std::string>();
			return std::filesystem::path(std::u8string(path.begin(), path.end()));
		};
		level = LevelData{
			.uiLayoutPath = readPath("UI"),
			.worldLayoutPath = readPath("World"),
		};
		return LevelIoResult{ .succeeded = true };
	} catch (const std::exception& error) {
		return MakeError(filePath, error.what());
	}
}

LevelIoResult LGF::SaveLevelData(const std::filesystem::path& filePath, const LevelData& level) {
	try {
		const auto writePath = [](const std::filesystem::path& path) {
			const auto text = path.generic_u8string();
			return std::string(reinterpret_cast<const char*>(text.data()), text.size());
		};
		const Json root{
			{ "Type", "Level" },
			{ "Version", kLevelVersion },
			{ "UI", writePath(level.uiLayoutPath) },
			{ "World", writePath(level.worldLayoutPath) },
		};
		Internal::WriteJsonFile(filePath, root.dump(2));
		return LevelIoResult{ .succeeded = true };
	} catch (const std::exception& error) {
		return MakeError(filePath, error.what());
	}
}
