#pragma once

#include <LGF/Math/Vector2.h>

#include <filesystem>
#include <string>

#include <imgui.h>

namespace LevelEditorWidgets {

	inline std::string GetPathText(const std::filesystem::path& path) {
		const std::u8string text = path.generic_u8string();
		return std::string(reinterpret_cast<const char*>(text.data()), text.size());
	}

	inline bool EditVector2(const char* label, LGF::Vector2& value, float speed = 1.0f, float minimum = 0.0f, float maximum = 0.0f) {
		float components[]{ value.x, value.y };
		if (!ImGui::DragFloat2(label, components, speed, minimum, maximum)) {
			return false;
		}
		value = { components[0], components[1] };
		return true;
	}

	inline bool EditText(const char* label, std::string& text) {
		return ImGui::InputText(
			label,
			text.data(),
			text.capacity() + 1u,
			ImGuiInputTextFlags_CallbackResize,
			[](ImGuiInputTextCallbackData* data) {
				auto& value = *static_cast<std::string*>(data->UserData);
				value.resize(static_cast<std::size_t>(data->BufTextLen));
				data->Buf = value.data();
				return 0;
			},
			&text);
	}

}
