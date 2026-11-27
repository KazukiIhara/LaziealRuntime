#include "LGF/WorldLayout.h"

#include "File/JsonFile.h"

#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <type_traits>

#include <nlohmann/json.hpp>

using namespace LGF;

namespace {

	using Json = nlohmann::json;
	constexpr int kWorldLayoutVersion = 1;

	float ReadNumber(const Json& value) {
		if (!value.is_number()) {
			throw std::runtime_error("Transform components must be numbers.");
		}
		const double number = value.get<double>();
		constexpr double maximum = std::numeric_limits<float>::max();
		if (!std::isfinite(number) || number < -maximum || number > maximum) {
			throw std::runtime_error("Transform components must be finite floats.");
		}
		return static_cast<float>(number);
	}

	Vector3 ReadVector3(const Json& value) {
		if (!value.is_array() || value.size() != 3u) {
			throw std::runtime_error("Position and Scale must contain three numbers.");
		}
		return { ReadNumber(value[0]), ReadNumber(value[1]), ReadNumber(value[2]) };
	}

	Vector2 ReadVector2(const Json& value) {
		if (!value.is_array() || value.size() != 2u) {
			throw std::runtime_error("2D Position, Scale and Pivot must contain two numbers.");
		}
		return { ReadNumber(value[0]), ReadNumber(value[1]) };
	}

	std::filesystem::path ReadAssetPath(const Json& value) {
		const std::string path = value.get<std::string>();
		return std::filesystem::path(std::u8string(path.begin(), path.end()));
	}

	std::string WriteAssetPath(const std::filesystem::path& path) {
		const std::u8string text = path.generic_u8string();
		return std::string(reinterpret_cast<const char*>(text.data()), text.size());
	}

	Json WriteComponent(const GameObjectComponentDesc& component) {
		return std::visit([](const auto& value) -> Json {
			using T = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<T, Transform3D>) {
				const auto& [x, y, z, w] = value.rotation;
				return {
					{ "Type", "Transform3D" },
					{ "Position", { value.translation.x, value.translation.y, value.translation.z } },
					{ "Rotation", { x, y, z, w } },
					{ "Scale", { value.scale.x, value.scale.y, value.scale.z } },
				};
			} else if constexpr (std::is_same_v<T, Transform2D>) {
				return {
					{ "Type", "Transform2D" },
					{ "Position", { value.translation.x, value.translation.y } },
					{ "Rotation", value.rotation },
					{ "Scale", { value.scale.x, value.scale.y } },
				};
			} else if constexpr (std::is_same_v<T, ModelRendererDesc>) {
				return { { "Type", "ModelRenderer" }, { "Model", WriteAssetPath(value.modelPath) } };
			} else {
				return {
					{ "Type", "SpriteRenderer" },
					{ "Texture", WriteAssetPath(value.texturePath) },
					{ "Pivot", { value.pivot.x, value.pivot.y } },
				};
			}
		}, component);
	}

	Quaternion ReadRotation(const Json& value) {
		if (!value.is_array() || value.size() != 4u) {
			throw std::runtime_error("Rotation must be a quaternion with four numbers.");
		}
		const double x = ReadNumber(value[0]);
		const double y = ReadNumber(value[1]);
		const double z = ReadNumber(value[2]);
		const double w = ReadNumber(value[3]);
		const double length = std::sqrt(x * x + y * y + z * z + w * w);
		if (length == 0.0) {
			throw std::runtime_error("Rotation quaternion must be nonzero.");
		}
		return {
			static_cast<float>(x / length),
			static_cast<float>(y / length),
			static_cast<float>(z / length),
			static_cast<float>(w / length),
		};
	}

	GameObjectComponentDesc ReadComponent(const Json& value) {
		const std::string type = value.at("Type").get<std::string>();
		if (type == "Transform3D") {
			Transform3D transform{};
			if (value.contains("Position")) {
				transform.translation = ReadVector3(value.at("Position"));
			}
			if (value.contains("Rotation")) {
				transform.rotation = ReadRotation(value.at("Rotation"));
			}
			if (value.contains("Scale")) {
				transform.scale = ReadVector3(value.at("Scale"));
			}
			return transform;
		}
		if (type == "ModelRenderer") {
			return ModelRendererDesc{ ReadAssetPath(value.at("Model")) };
		}
		if (type == "Transform2D") {
			Transform2D transform{};
			if (value.contains("Position")) {
				transform.translation = ReadVector2(value.at("Position"));
			}
			if (value.contains("Rotation")) {
				transform.rotation = ReadNumber(value.at("Rotation"));
			}
			if (value.contains("Scale")) {
				transform.scale = ReadVector2(value.at("Scale"));
			}
			return transform;
		}
		if (type == "SpriteRenderer") {
			SpriteRendererDesc renderer{ .texturePath = ReadAssetPath(value.at("Texture")) };
			if (value.contains("Pivot")) {
				renderer.pivot = ReadVector2(value.at("Pivot"));
			}
			return renderer;
		}
		throw std::runtime_error("Unknown GameObject component: " + type);
	}

	void ValidateWorldLayout(const WorldLayout& layout) {
		std::unordered_set<std::string_view> ids;
		for (const GameObjectDesc& object : layout.objects) {
			if (object.id.empty() || !ids.emplace(object.id).second) {
				throw std::runtime_error("GameObject IDs must be nonempty and unique: " + object.id);
			}
			std::array<bool, std::variant_size_v<GameObjectComponentDesc>> hasComponent{};
			bool hasTransform3D = false;
			bool hasTransform2D = false;
			bool hasModelRenderer = false;
			bool hasSpriteRenderer = false;
			for (const GameObjectComponentDesc& component : object.components) {
				if (component.valueless_by_exception()) {
					throw std::runtime_error("Invalid GameObject component: " + object.id);
				}
				if (hasComponent[component.index()]) {
					throw std::runtime_error("Duplicate GameObject component: " + object.id);
				}
				hasComponent[component.index()] = true;
				if (const auto* model = std::get_if<ModelRendererDesc>(&component)) {
					hasModelRenderer = true;
					if (model->modelPath.empty()) {
						throw std::runtime_error("ModelRenderer requires a model path: " + object.id);
					}
				} else if (const auto* sprite = std::get_if<SpriteRendererDesc>(&component)) {
					hasSpriteRenderer = true;
					if (sprite->texturePath.empty()) {
						throw std::runtime_error("SpriteRenderer requires a texture path: " + object.id);
					}
					const Vector2& pivot = sprite->pivot;
					if (!(pivot.x >= 0.0f && pivot.x <= 1.0f && pivot.y >= 0.0f && pivot.y <= 1.0f)) {
						throw std::runtime_error("SpriteRenderer Pivot must be between zero and one: " + object.id);
					}
				} else if (std::holds_alternative<Transform3D>(component)) {
					hasTransform3D = true;
				} else {
					hasTransform2D = true;
				}
			}

			if (hasModelRenderer && !hasTransform3D) {
				throw std::runtime_error("ModelRenderer requires Transform3D: " + object.id);
			}
			if (hasSpriteRenderer && !hasTransform2D) {
				throw std::runtime_error("SpriteRenderer requires Transform2D: " + object.id);
			}
		}
	}

}

WorldLayout LGF::LoadWorldLayout(const std::filesystem::path& filePath) {
	std::ifstream file(filePath, std::ios::binary);
	if (!file) {
		throw std::runtime_error("Failed to open the world layout.");
	}
	const Json root = Json::parse(file);
	if (root.value("Type", std::string("WorldLayout")) != "WorldLayout") {
		throw std::runtime_error("Expected a WorldLayout document.");
	}
	const Json& version = root.at("Version");
	if (!version.is_number_integer() || version != kWorldLayoutVersion) {
		throw std::runtime_error("Unsupported world layout version.");
	}
	const Json& objects = root.at("Objects");
	if (!objects.is_array()) {
		throw std::runtime_error("World layout Objects must be an array.");
	}

	WorldLayout layout;
	layout.objects.reserve(objects.size());
	for (const Json& value : objects) {
		GameObjectDesc object{
			.id = value.at("Id").get<std::string>(),
			.name = value.value("Name", std::string{}),
			.isEnabled = value.value("Enabled", true),
		};
		const Json& components = value.at("Components");
		if (!components.is_array()) {
			throw std::runtime_error("GameObject Components must be an array: " + object.id);
		}
		for (const Json& component : components) {
			object.components.push_back(ReadComponent(component));
		}
		layout.objects.push_back(std::move(object));
	}
	ValidateWorldLayout(layout);
	return layout;
}

void LGF::SaveWorldLayout(const std::filesystem::path& filePath, const WorldLayout& layout) {
	ValidateWorldLayout(layout);
	Json objects = Json::array();
	for (const GameObjectDesc& object : layout.objects) {
		Json components = Json::array();
		for (const GameObjectComponentDesc& component : object.components) {
			Json value = WriteComponent(component);
			// 読み込みと同じ数値の制約を適用してからファイルを置き換える。
			ReadComponent(value);
			components.push_back(std::move(value));
		}
		objects.push_back({
			{ "Id", object.id },
			{ "Name", object.name },
			{ "Enabled", object.isEnabled },
			{ "Components", std::move(components) },
		});
	}
	const Json root{
		{ "Type", "WorldLayout" },
		{ "Version", kWorldLayoutVersion },
		{ "Objects", std::move(objects) },
	};
	Internal::WriteJsonFile(filePath, root.dump(2));
}

GameObjectWorld LGF::CreateGameObjectWorld(const WorldLayout& layout, const std::filesystem::path& assetDirectory) {
	ValidateWorldLayout(layout);
	GameObjectWorld world;
	for (const GameObjectDesc& desc : layout.objects) {
		GameObject* object = world.Get(world.Add(desc.id, desc.name, desc.isEnabled));
		if (!object) {
			throw std::runtime_error("Failed to add GameObject: " + desc.id);
		}
		for (const GameObjectComponentDesc& component : desc.components) {
			if (const auto* transform3D = std::get_if<Transform3D>(&component)) {
				object->SetComponent(*transform3D);
			} else if (const auto* transform2D = std::get_if<Transform2D>(&component)) {
				object->SetComponent(*transform2D);
			} else if (const auto* renderer = std::get_if<ModelRendererDesc>(&component)) {
				const auto modelPath = (assetDirectory / renderer->modelPath).lexically_normal();
				try {
					Model model(modelPath);
					if (!model.IsValid()) {
						throw std::runtime_error("The model is invalid.");
					}
					object->SetComponent(ModelRenderer{ model });
				} catch (const std::exception& error) {
					throw std::runtime_error("Failed to load the model for " + desc.id + ": " + error.what());
				}
			} else {
				const auto& sprite = std::get<SpriteRendererDesc>(component);
				const auto texturePath = (assetDirectory / sprite.texturePath).lexically_normal();
				try {
					object->SetComponent(SpriteRenderer{
						.texture = Texture(texturePath, TextureLoadDesc::Color()),
						.pivot = sprite.pivot,
					});
				} catch (const std::exception& error) {
					throw std::runtime_error("Failed to load the sprite for " + desc.id + ": " + error.what());
				}
			}
		}
	}
	return world;
}
