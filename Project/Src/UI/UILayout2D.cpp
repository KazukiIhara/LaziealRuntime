#include "LGF/UI/UILayout2D.h"

#include "File/JsonFile.h"

#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

using namespace LGF;

namespace {

	using Json = nlohmann::json;
	constexpr int kVersion = 1;
	constexpr std::string_view kTypeNames[]{ "Canvas", "Panel", "Label", "Image", "Button" };

	std::size_t TypeIndex(UIElementType type) {
		const auto index = static_cast<std::size_t>(type);
		if (index >= std::size(kTypeNames)) {
			throw std::runtime_error("Invalid UI element type.");
		}
		return index;
	}

	UIElementType ReadType(const std::string& name) {
		for (std::size_t index = 0u; index < std::size(kTypeNames); ++index) {
			if (name == kTypeNames[index]) {
				return static_cast<UIElementType>(index);
			}
		}
		throw std::runtime_error("Unknown UI element type: " + name);
	}

	float ReadNumber(const Json& value) {
		if (!value.is_number()) {
			throw std::runtime_error("UI values must be numbers.");
		}
		const double number = value.get<double>();
		constexpr double maximum = std::numeric_limits<float>::max();
		if (!std::isfinite(number) || number < -maximum || number > maximum) {
			throw std::runtime_error("UI values must be finite floats.");
		}
		return static_cast<float>(number);
	}

	Vector2 ReadVector2(const Json& value) {
		if (!value.is_array() || value.size() != 2u) {
			throw std::runtime_error("Expected two UI coordinates.");
		}
		return { ReadNumber(value[0]), ReadNumber(value[1]) };
	}

	Vector4 ReadColor(const Json& value) {
		if (!value.is_array() || value.size() != 4u) {
			throw std::runtime_error("Expected four UI color components.");
		}
		return { ReadNumber(value[0]), ReadNumber(value[1]), ReadNumber(value[2]), ReadNumber(value[3]) };
	}

	struct Hierarchy {
		std::vector<std::size_t> roots;
		std::vector<std::vector<std::size_t>> children;
		std::vector<std::size_t> parents;
	};

	Hierarchy GetHierarchy(const UILayout2D& layout) {
		if (!std::isfinite(layout.size.x) || !std::isfinite(layout.size.y) || layout.size.x <= 0.0f || layout.size.y <= 0.0f) {
			throw std::runtime_error("UI layout size must be positive and finite.");
		}
		const std::size_t count = layout.elements.size();
		Hierarchy result{ {}, std::vector<std::vector<std::size_t>>(count), std::vector<std::size_t>(count, count) };
		std::unordered_map<std::string_view, std::size_t> indices;
		for (std::size_t index = 0u; index < count; ++index) {
			const auto& element = layout.elements[index];
			TypeIndex(element.type);
			if (element.id.empty() || !indices.emplace(element.id, index).second) {
				throw std::runtime_error("UI element IDs must be nonempty and unique: " + element.id);
			}
			const auto& [position, size, anchor, pivot] = element.transform;
			const auto& [r, g, b, a] = element.color;
			const float values[]{ position.x, position.y, size.x, size.y, anchor.x, anchor.y, pivot.x, pivot.y, r, g, b, a };
			for (const float value : values) {
				if (!std::isfinite(value)) {
					throw std::runtime_error("UI element values must be finite: " + element.id);
				}
			}
			if (size.x < 0.0f || size.y < 0.0f) {
				throw std::runtime_error("UI element size must be nonnegative: " + element.id);
			}
		}
		for (std::size_t index = 0u; index < count; ++index) {
			const auto& element = layout.elements[index];
			if (element.type == UIElementType::Canvas) {
				if (!element.parentId.empty()) {
					throw std::runtime_error("A canvas cannot have a parent: " + element.id);
				}
				result.roots.push_back(index);
				continue;
			}
			const auto parent = indices.find(element.parentId);
			if (parent == indices.end()) {
				throw std::runtime_error("Missing UI parent for " + element.id);
			}
			const UIElementType type = layout.elements[parent->second].type;
			if (type != UIElementType::Canvas && type != UIElementType::Panel) {
				throw std::runtime_error("A UI parent must be a canvas or panel: " + element.parentId);
			}
			result.parents[index] = parent->second;
			result.children[parent->second].push_back(index);
		}
		std::vector<std::size_t> reachable = result.roots;
		for (std::size_t cursor = 0u; cursor < reachable.size(); ++cursor) {
			const auto& children = result.children[reachable[cursor]];
			reachable.insert(reachable.end(), children.begin(), children.end());
		}
		if (reachable.size() != count) {
			throw std::runtime_error("UI parent hierarchy contains a cycle.");
		}
		return result;
	}

	Json WriteVector2(const Vector2& value) {
		return Json::array({ value.x, value.y });
	}

}

UILayout2D LGF::LoadUILayout2D(const std::filesystem::path& filePath) {
	std::ifstream file(filePath, std::ios::binary);
	if (!file) {
		throw std::runtime_error("Failed to open the UI layout.");
	}
	const Json root = Json::parse(file);
	if (root.value("Type", std::string("UILayout2D")) != "UILayout2D") {
		throw std::runtime_error("Expected a UILayout2D document.");
	}
	const Json& version = root.at("Version");
	if (!version.is_number_integer() || version != kVersion) {
		throw std::runtime_error("Unsupported UI layout version.");
	}
	UILayout2D layout{ .size = ReadVector2(root.at("Size")) };
	const Json& elements = root.at("Elements");
	if (!elements.is_array()) {
		throw std::runtime_error("UI Elements must be an array.");
	}
	for (const Json& value : elements) {
		const auto type = ReadType(value.at("Type").get<std::string>());
		const Vector4 defaultColor = type == UIElementType::Panel ? UIPanelDesc{}.color :
			(type == UIElementType::Button ? UIButtonStyle{}.normalColor : Color::White);
		layout.elements.push_back({
			.id = value.at("Id").get<std::string>(),
			.parentId = value.value("Parent", std::string{}),
			.type = type,
			.transform = {
				.position = ReadVector2(value.value("Position", WriteVector2({}))),
				.size = ReadVector2(value.value("Size", WriteVector2({}))),
				.anchor = ReadVector2(value.value("Anchor", WriteVector2({}))),
				.pivot = ReadVector2(value.value("Pivot", WriteVector2({}))),
			},
			.isVisible = value.value("Visible", true),
			.isEnabled = value.value("Enabled", true),
			.text = value.value("Text", std::string{}),
			.texturePath = value.value("Texture", std::string{}),
			.color = value.contains("Color") ? ReadColor(value.at("Color")) : defaultColor,
		});
	}
	GetHierarchy(layout);
	return layout;
}

void LGF::SaveUILayout2D(const std::filesystem::path& filePath, const UILayout2D& layout) {
	GetHierarchy(layout);
	Json elements = Json::array();
	for (const auto& element : layout.elements) {
		const auto& [position, size, anchor, pivot] = element.transform;
		const auto& [r, g, b, a] = element.color;
		elements.push_back({
			{ "Id", element.id }, { "Parent", element.parentId }, { "Type", kTypeNames[TypeIndex(element.type)] },
			{ "Position", WriteVector2(position) }, { "Size", WriteVector2(size) },
			{ "Anchor", WriteVector2(anchor) }, { "Pivot", WriteVector2(pivot) },
			{ "Visible", element.isVisible }, { "Enabled", element.isEnabled },
			{ "Text", element.text }, { "Texture", element.texturePath }, { "Color", { r, g, b, a } },
		});
	}
	const Json root{
		{ "Type", "UILayout2D" },
		{ "Version", kVersion },
		{ "Size", WriteVector2(layout.size) },
		{ "Elements", std::move(elements) },
	};
	Internal::WriteJsonFile(filePath, root.dump(2));
}

UILayoutInstance2D LGF::CreateUIWorld2D(const UILayout2D& layout, const Font2D& font, const std::function<Texture(std::string_view)>& resolveTexture) {
	const Hierarchy hierarchy = GetHierarchy(layout);
	UILayoutInstance2D instance;
	instance.handles.resize(layout.elements.size());
	std::vector<std::size_t> pending = hierarchy.roots;
	for (std::size_t cursor = 0u; cursor < pending.size(); ++cursor) {
		const std::size_t index = pending[cursor];
		const auto& element = layout.elements[index];
		auto& handle = instance.handles[index];
		if (element.type == UIElementType::Canvas) {
			handle = instance.world.AddCanvas({
				.transform = element.transform,
				.isVisible = element.isVisible,
				.isEnabled = element.isEnabled,
			});
		} else {
			const UIElementDesc desc{ instance.handles[hierarchy.parents[index]], element.isVisible, element.isEnabled };
			switch (element.type) {
			case UIElementType::Panel:
				handle = instance.world.AddPanel({ .element = desc, .transform = element.transform, .color = element.color });
				break;
			case UIElementType::Label:
				handle = instance.world.AddLabel({ .element = desc, .text = element.text, .transform = element.transform, .font = font, .color = element.color });
				break;
			case UIElementType::Image:
				handle = instance.world.AddImage({
					.element = desc, .transform = element.transform,
					.texture = element.texturePath.empty() ? Texture{} : resolveTexture(element.texturePath),
					.color = element.color,
				});
				break;
			case UIElementType::Button: {
				UIButtonStyle style;
				style.normalColor = element.color;
				handle = instance.world.AddButton({ .element = desc, .text = element.text, .transform = element.transform, .font = font, .style = style });
				break;
			}
			case UIElementType::Canvas:
				break;
			}
		}
		const auto& children = hierarchy.children[index];
		pending.insert(pending.end(), children.begin(), children.end());
	}
	return instance;
}
