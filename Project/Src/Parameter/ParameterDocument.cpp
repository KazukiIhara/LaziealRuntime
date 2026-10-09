#include "LGF/Parameter.h"

#include <Windows.h>

#include <fstream>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

using namespace LGF;

namespace {

	using Json = nlohmann::json;

	ParameterIoResult MakeError(std::string message) {
		return ParameterIoResult{
			.succeeded = false,
			.message = std::move(message),
		};
	}

	std::vector<std::string> SplitPath(std::string_view path) {
		std::vector<std::string> result;
		std::size_t begin = 0u;
		while (begin < path.size()) {
			const std::size_t end = path.find_first_of("/\\", begin);
			const std::size_t length =
				end == std::string_view::npos ? path.size() - begin : end - begin;
			if (length > 0u) {
				result.emplace_back(path.substr(begin, length));
			}
			if (end == std::string_view::npos) {
				break;
			}
			begin = end + 1u;
		}
		return result;
	}

	const ParameterNode* FindNode(
		const ParameterNode& root,
		const std::vector<std::string>& path) {
		const ParameterNode* current = &root;
		for (const std::string& name : path) {
			const auto iterator = current->children.find(name);
			if (iterator == current->children.end()) {
				return nullptr;
			}
			current = &iterator->second;
		}
		return current;
	}

	ParameterNode* FindNode(
		ParameterNode& root,
		const std::vector<std::string>& path) {
		return const_cast<ParameterNode*>(
			FindNode(std::as_const(root), path));
	}

	std::optional<ParameterType> ParseParameterType(std::string_view value) {
		if (value == "Bool") {
			return ParameterType::Bool;
		}
		if (value == "Int32") {
			return ParameterType::Int32;
		}
		if (value == "Float") {
			return ParameterType::Float;
		}
		if (value == "Vec2" || value == "Vector2") {
			return ParameterType::Vector2;
		}
		if (value == "Vec3" || value == "Vector3") {
			return ParameterType::Vector3;
		}
		if (value == "Vec4" || value == "Vector4") {
			return ParameterType::Vector4;
		}
		if (value == "String") {
			return ParameterType::String;
		}
		return std::nullopt;
	}

	Vector2 ReadVector2(const Json& json) {
		return Vector2{
			.x = json.at("x").get<float>(),
			.y = json.at("y").get<float>(),
		};
	}

	Vector3 ReadVector3(const Json& json) {
		return Vector3{
			.x = json.at("x").get<float>(),
			.y = json.at("y").get<float>(),
			.z = json.at("z").get<float>(),
		};
	}

	Vector4 ReadVector4(const Json& json) {
		return Vector4{
			.x = json.at("x").get<float>(),
			.y = json.at("y").get<float>(),
			.z = json.at("z").get<float>(),
			.w = json.at("w").get<float>(),
		};
	}

	ParameterValue ReadValue(const Json& json, ParameterType type) {
		switch (type) {
		case ParameterType::Bool:
			return json.get<bool>();
		case ParameterType::Int32:
			return json.get<int32_t>();
		case ParameterType::Float:
			return json.get<float>();
		case ParameterType::Vector2:
			return ReadVector2(json);
		case ParameterType::Vector3:
			return ReadVector3(json);
		case ParameterType::Vector4:
			return ReadVector4(json);
		case ParameterType::String:
			return json.get<std::string>();
		default:
			throw std::runtime_error("Unknown parameter type");
		}
	}

	ParameterEditorSettings ReadEditorSettings(const Json& json) {
		ParameterEditorSettings result{};
		if (!json.is_object()) {
			throw std::runtime_error("The editor field must be an object");
		}
		if (json.contains("min")) {
			result.minimum = json.at("min").get<float>();
		}
		if (json.contains("max")) {
			result.maximum = json.at("max").get<float>();
		}
		if (json.contains("step")) {
			result.step = json.at("step").get<float>();
		}
		result.label = json.value("label", std::string{});
		result.tooltip = json.value("tooltip", std::string{});
		return result;
	}

	ParameterNode ReadNode(const Json& json) {
		if (!json.is_object()) {
			throw std::runtime_error("A parameter node must be an object");
		}

		const bool hasType = json.contains("type");
		const bool hasValue = json.contains("value");
		if (hasType || hasValue) {
			if (!hasType || !hasValue) {
				throw std::runtime_error("A parameter must contain type and value");
			}

			const std::string typeName = json.at("type").get<std::string>();
			const std::optional<ParameterType> type = ParseParameterType(typeName);
			if (!type) {
				throw std::runtime_error("Unknown parameter type: " + typeName);
			}

			ParameterNode result{};
			result.data = ParameterData{
				.value = ReadValue(json.at("value"), *type),
			};
			if (json.contains("editor")) {
				result.data->editor = ReadEditorSettings(json.at("editor"));
			}
			return result;
		}

		ParameterNode result{};
		for (auto iterator = json.begin(); iterator != json.end(); ++iterator) {
			result.children.emplace(iterator.key(), ReadNode(iterator.value()));
		}
		return result;
	}

	Json WriteVector(const Vector2& value) {
		return Json{
			{ "x", value.x },
			{ "y", value.y },
		};
	}

	Json WriteVector(const Vector3& value) {
		return Json{
			{ "x", value.x },
			{ "y", value.y },
			{ "z", value.z },
		};
	}

	Json WriteVector(const Vector4& value) {
		return Json{
			{ "x", value.x },
			{ "y", value.y },
			{ "z", value.z },
			{ "w", value.w },
		};
	}

	Json WriteValue(const ParameterValue& value) {
		return std::visit([](const auto& current) -> Json {
			using T = std::decay_t<decltype(current)>;
			if constexpr (
				std::is_same_v<T, Vector2> ||
				std::is_same_v<T, Vector3> ||
				std::is_same_v<T, Vector4>) {
				return WriteVector(current);
			} else {
				return current;
			}
		}, value);
	}

	Json WriteEditorSettings(const ParameterEditorSettings& editor) {
		Json result = Json::object();
		if (editor.minimum) {
			result["min"] = *editor.minimum;
		}
		if (editor.maximum) {
			result["max"] = *editor.maximum;
		}
		if (editor.step) {
			result["step"] = *editor.step;
		}
		if (!editor.label.empty()) {
			result["label"] = editor.label;
		}
		if (!editor.tooltip.empty()) {
			result["tooltip"] = editor.tooltip;
		}
		return result;
	}

	Json WriteNode(const ParameterNode& node) {
		if (node.data) {
			if (!node.children.empty()) {
				throw std::runtime_error("A parameter node cannot contain children");
			}
			Json result{
				{ "type", ToString(GetParameterType(node.data->value)) },
				{ "value", WriteValue(node.data->value) },
			};
			const Json editor = WriteEditorSettings(node.data->editor);
			if (!editor.empty()) {
				result["editor"] = editor;
			}
			return result;
		}

		Json result = Json::object();
		for (const auto& [name, child] : node.children) {
			result[name] = WriteNode(child);
		}
		return result;
	}

	bool RemoveNode(
		ParameterNode& node,
		const std::vector<std::string>& path,
		std::size_t index) {
		auto iterator = node.children.find(path[index]);
		if (iterator == node.children.end()) {
			return false;
		}

		if (index + 1u == path.size()) {
			node.children.erase(iterator);
			return true;
		}

		if (!RemoveNode(iterator->second, path, index + 1u)) {
			return false;
		}
		if (!iterator->second.data && iterator->second.children.empty()) {
			node.children.erase(iterator);
		}
		return true;
	}

}

ParameterIoResult ParameterDocument::Load(const std::filesystem::path& filePath) {
	std::ifstream file(filePath, std::ios::binary);
	if (!file) {
		return MakeError("Failed to open parameter file: " + filePath.string());
	}

	try {
		Json json;
		file >> json;
		ParameterNode loadedRoot = ReadNode(json);
		if (loadedRoot.data) {
			return MakeError("The root of a parameter file must be an object");
		}
		root_ = std::move(loadedRoot);
		return ParameterIoResult{ .succeeded = true };
	} catch (const std::exception& error) {
		return MakeError(
			"Failed to load parameter file " + filePath.string() + ": " + error.what());
	}
}

ParameterIoResult ParameterDocument::Save(const std::filesystem::path& filePath) const {
	try {
		const std::filesystem::path parent = filePath.parent_path();
		if (!parent.empty()) {
			std::filesystem::create_directories(parent);
		}

		std::filesystem::path temporaryPath = filePath;
		temporaryPath += ".tmp";
		{
			std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
			if (!file) {
				return MakeError(
					"Failed to open parameter file for writing: " + temporaryPath.string());
			}
			file << WriteNode(root_).dump(2) << '\n';
			if (!file) {
				return MakeError("Failed to write parameter file: " + temporaryPath.string());
			}
		}

		if (!MoveFileExW(
			temporaryPath.c_str(),
			filePath.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			const DWORD error = GetLastError();
			std::error_code ignored;
			std::filesystem::remove(temporaryPath, ignored);
			return MakeError(
				"Failed to replace parameter file: Windows error " +
				std::to_string(error));
		}
		return ParameterIoResult{ .succeeded = true };
	} catch (const std::exception& error) {
		return MakeError(
			"Failed to save parameter file " + filePath.string() + ": " + error.what());
	}
}

void ParameterDocument::Clear() {
	root_ = {};
}

bool ParameterDocument::IsEmpty() const {
	return root_.children.empty();
}

bool ParameterDocument::Contains(std::string_view path) const {
	return Find(path) != nullptr;
}

bool ParameterDocument::Set(
	std::string_view path,
	const ParameterValue& value,
	const ParameterEditorSettings& editor) {
	const std::vector<std::string> names = SplitPath(path);
	if (names.empty()) {
		return false;
	}

	ParameterNode* current = &root_;
	for (const std::string& name : names) {
		if (current->data) {
			return false;
		}
		current = &current->children[name];
	}
	if (!current->children.empty()) {
		return false;
	}
	current->data = ParameterData{
		.value = value,
		.editor = editor,
	};
	return true;
}

bool ParameterDocument::Remove(std::string_view path) {
	const std::vector<std::string> names = SplitPath(path);
	return !names.empty() && RemoveNode(root_, names, 0u);
}

ParameterData* ParameterDocument::Find(std::string_view path) {
	const std::vector<std::string> names = SplitPath(path);
	ParameterNode* node = names.empty() ? nullptr : FindNode(root_, names);
	return node && node->data ? &*node->data : nullptr;
}

const ParameterData* ParameterDocument::Find(std::string_view path) const {
	const std::vector<std::string> names = SplitPath(path);
	const ParameterNode* node = names.empty() ? nullptr : FindNode(root_, names);
	return node && node->data ? &*node->data : nullptr;
}

ParameterNode& ParameterDocument::GetRoot() {
	return root_;
}

const ParameterNode& ParameterDocument::GetRoot() const {
	return root_;
}

ParameterType LGF::GetParameterType(const ParameterValue& value) {
	return std::visit([](const auto& current) {
		using T = std::decay_t<decltype(current)>;
		if constexpr (std::is_same_v<T, bool>) {
			return ParameterType::Bool;
		} else if constexpr (std::is_same_v<T, int32_t>) {
			return ParameterType::Int32;
		} else if constexpr (std::is_same_v<T, float>) {
			return ParameterType::Float;
		} else if constexpr (std::is_same_v<T, Vector2>) {
			return ParameterType::Vector2;
		} else if constexpr (std::is_same_v<T, Vector3>) {
			return ParameterType::Vector3;
		} else if constexpr (std::is_same_v<T, Vector4>) {
			return ParameterType::Vector4;
		} else {
			return ParameterType::String;
		}
	}, value);
}

std::string_view LGF::ToString(ParameterType type) {
	switch (type) {
	case ParameterType::Bool: return "Bool";
	case ParameterType::Int32: return "Int32";
	case ParameterType::Float: return "Float";
	case ParameterType::Vector2: return "Vec2";
	case ParameterType::Vector3: return "Vec3";
	case ParameterType::Vector4: return "Vec4";
	case ParameterType::String: return "String";
	default: return "Unknown";
	}
}
