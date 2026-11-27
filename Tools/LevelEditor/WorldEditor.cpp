#include "WorldEditor.h"

#include <DirectXMath.h>

#include <LGF/Graphics/Graphics2D.h>
#include <LGF/Graphics/Rect2D.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <ImGuizmo.h>

#include "EditorWidgets.h"
#include "PreviewRender.h"

using namespace LGF;
using namespace LevelEditorWidgets;

namespace {

	constexpr Vector2 kRenderSize{ 640.0f, 360.0f };

	template <class T>
	auto FindComponent(auto& object) {
		const auto component = std::ranges::find_if(object.components, [](const auto& value) {
			return std::holds_alternative<T>(value);
		});
		return component == object.components.end() ? nullptr : std::get_if<T>(&*component);
	}

	bool EditVector3(const char* label, Vector3& value, float speed) {
		float components[]{ value.x, value.y, value.z };
		if (!ImGui::DragFloat3(label, components, speed)) {
			return false;
		}
		value = { components[0], components[1], components[2] };
		return true;
	}

	float GetScale(float original, const Matrix4x4& matrix, std::size_t axis) {
		const auto& row = matrix.m[axis];
		return std::copysign(std::hypot(row[0], row[1], row[2]), original);
	}

}

void WorldEditor::Open(WorldLayout layout) {
	layout_ = std::move(layout);
	selected_.reset();
	show3D_ = layout_.objects.empty() || std::ranges::any_of(layout_.objects, [](const auto& object) {
		return FindComponent<Transform3D>(object) != nullptr;
	});
	operation_ = Operation::Move;
	cameraTarget_ = {};
	cameraDistance_ = 8.5f;
	camera_.yaw = 0.0f;
	camera_.pitch = 18.0f * Math::DEG_TO_RAD;
	camera_.param.useWindowAspectRatio = false;
	camera_.param.aspectRatio = kRenderSize.x / kRenderSize.y;
	rebuildPreview_ = true;
}

WorldLayout& WorldEditor::GetLayout() {
	return layout_;
}

bool WorldEditor::Draw(const std::filesystem::path& directory, const AssetPicker& pickAsset) {
	bool changed = false;
	if (ImGui::BeginTable("World layout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("Objects", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 290.0f);
		ImGui::TableNextColumn();
		if (ImGui::BeginChild("World objects")) {
			changed = DrawObjectList();
		}
		ImGui::EndChild();
		ImGui::TableNextColumn();
		constexpr ImGuiWindowFlags previewFlags = ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		if (ImGui::BeginChild("World preview", {}, ImGuiChildFlags_None, previewFlags)) {
			changed |= DrawPreview(directory);
		}
		ImGui::EndChild();
		ImGui::TableNextColumn();
		if (ImGui::BeginChild("World inspector")) {
			changed |= DrawInspector(pickAsset);
		}
		ImGui::EndChild();
		ImGui::EndTable();
	}
	return changed;
}

void WorldEditor::AddGameObject() {
	const std::string prefix = "GameObject";
	GameObjectDesc object{ .id = prefix };
	for (std::size_t number = 2u; std::ranges::find(layout_.objects, object.id, &GameObjectDesc::id) != layout_.objects.end(); ++number) {
		object.id = prefix + std::to_string(number);
	}
	object.name = object.id;
	layout_.objects.push_back(std::move(object));
	selected_ = layout_.objects.size() - 1u;
	rebuildPreview_ = true;
}

bool WorldEditor::DrawObjectList() {
	ImGui::SeparatorText("Objects");
	const bool added = ImGui::Button("Add GameObject");
	if (added) {
		AddGameObject();
	}
	for (std::size_t index = 0u; index < layout_.objects.size(); ++index) {
		const auto& object = layout_.objects[index];
		ImGui::PushID(static_cast<int>(index));
		const std::string label = object.name.empty() || object.name == object.id ? object.id : object.name + " (" + object.id + ")";
		if (ImGui::Selectable(label.c_str(), selected_ == index)) {
			selected_ = index;
			if (FindComponent<Transform3D>(object)) {
				show3D_ = true;
			} else if (FindComponent<Transform2D>(object)) {
				show3D_ = false;
			}
		}
		ImGui::PopID();
	}
	return added;
}

bool WorldEditor::DrawInspector(const AssetPicker& pickAsset) {
	ImGui::SeparatorText("Inspector");
	if (!selected_) {
		return false;
	}

	bool changed = false;
	ImGui::PushID(static_cast<int>(*selected_));
	GameObjectDesc& object = layout_.objects[*selected_];
	std::string id = object.id;
	if (EditText("Id", id) && !id.empty() && std::ranges::find(layout_.objects, id, &GameObjectDesc::id) == layout_.objects.end()) {
		object.id = std::move(id);
		rebuildPreview_ = true;
		changed = true;
	}
	changed |= EditText("Name", object.name);
	changed |= ImGui::Checkbox("Enabled", &object.isEnabled);
	if (auto* transform = FindComponent<Transform3D>(object)) {
		ImGui::SeparatorText("Transform3D");
		ImGui::PushID("Transform3D");
		changed |= EditVector3("Position", transform->translation, 0.02f);
		float rotation[]{ transform->rotation.x, transform->rotation.y, transform->rotation.z, transform->rotation.w };
		if (ImGui::DragFloat4("Quaternion", rotation, 0.01f, -1.0f, 1.0f)) {
			const Quaternion value{ rotation[0], rotation[1], rotation[2], rotation[3] };
			if (Math::LengthSq(value) > Math::EPSILON) {
				transform->rotation = Math::Normalize(value);
				changed = true;
			}
		}
		changed |= EditVector3("Scale", transform->scale, 0.01f);
		ImGui::PopID();
	}
	if (auto* model = FindComponent<ModelRendererDesc>(object)) {
		ImGui::SeparatorText("ModelRenderer");
		ImGui::TextWrapped("%s", GetPathText(model->modelPath).c_str());
		if (ImGui::Button("Choose model...")) {
			if (const auto path = pickAsset(true)) {
				model->modelPath = *path;
				rebuildPreview_ = true;
				changed = true;
			}
		}
	}
	if (auto* transform = FindComponent<Transform2D>(object)) {
		ImGui::SeparatorText("Transform2D");
		ImGui::PushID("Transform2D");
		changed |= EditVector2("Position", transform->translation);
		changed |= ImGui::DragFloat("Rotation (rad)", &transform->rotation, 0.01f);
		changed |= EditVector2("Scale", transform->scale, 0.01f);
		ImGui::PopID();
	}
	if (auto* sprite = FindComponent<SpriteRendererDesc>(object)) {
		ImGui::SeparatorText("SpriteRenderer");
		changed |= EditVector2("Pivot", sprite->pivot, 0.01f, 0.0f, 1.0f);
		ImGui::TextWrapped("%s", GetPathText(sprite->texturePath).c_str());
		if (ImGui::Button("Choose texture...")) {
			if (const auto path = pickAsset(false)) {
				sprite->texturePath = *path;
				rebuildPreview_ = true;
				changed = true;
			}
		}
	}
	ImGui::Spacing();
	ImGui::BeginDisabled();
	ImGui::Button("Add Component");
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Remove object")) {
		layout_.objects.erase(layout_.objects.begin() + static_cast<std::ptrdiff_t>(*selected_));
		selected_.reset();
		rebuildPreview_ = true;
		changed = true;
	} else if (changed && !rebuildPreview_) {
		SyncObject(object);
	}
	ImGui::PopID();
	return changed;
}

void WorldEditor::SyncObject(const GameObjectDesc& description) {
	GameObject* object = preview_.Get(preview_.Find(description.id));
	if (!object) {
		return;
	}
	object->SetName(description.name);
	object->SetEnabled(description.isEnabled);
	if (const auto* transform = FindComponent<Transform3D>(description)) {
		object->SetComponent(*transform);
	}
	if (const auto* transform = FindComponent<Transform2D>(description)) {
		object->SetComponent(*transform);
	}
	if (auto* renderer = object->GetComponent<SpriteRenderer>()) {
		renderer->pivot = FindComponent<SpriteRendererDesc>(description)->pivot;
	}
}

void WorldEditor::RebuildPreview(const std::filesystem::path& directory) {
	rebuildPreview_ = false;
	previewError_.clear();
	try {
		preview_ = CreateGameObjectWorld(layout_, directory);
	} catch (const std::exception& error) {
		preview_ = {};
		previewError_ = error.what();
	}
}

bool WorldEditor::DrawPreview(const std::filesystem::path& directory) {
	if (rebuildPreview_) {
		RebuildPreview(directory);
	}

	if (ImGui::RadioButton("3D", show3D_)) {
		show3D_ = true;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("2D", !show3D_)) {
		show3D_ = false;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Move", operation_ == Operation::Move)) {
		operation_ = Operation::Move;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", operation_ == Operation::Rotate)) {
		operation_ = Operation::Rotate;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", operation_ == Operation::Scale)) {
		operation_ = Operation::Scale;
	}
	if (show3D_) {
		if (selected_) {
			if (const auto* transform = FindComponent<Transform3D>(layout_.objects[*selected_])) {
				if (ImGui::Button("Focus selection")) {
					cameraTarget_ = transform->translation;
				}
			}
		}
		ImGui::TextUnformatted("Right drag: orbit   Wheel: zoom");
	} else {
		EditVector2("Canvas size", canvasSize_, 1.0f, 16.0f, 8192.0f);
	}
	if (!previewError_.empty()) {
		ImGui::TextWrapped("%s", previewError_.c_str());
		if (ImGui::Button("Retry preview")) {
			rebuildPreview_ = true;
		}
	}

	const Vector2 logicalSize = show3D_ ? kRenderSize : canvasSize_;
	if (!(logicalSize.x > 0.0f && logicalSize.y > 0.0f)) {
		return false;
	}
	const ImVec2 available = ImGui::GetContentRegionAvail();
	const float scale = std::min(available.x / logicalSize.x, available.y / logicalSize.y);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 size(logicalSize.x * scale, logicalSize.y * scale);
	if (scale <= 0.0f || !Graphics2D::SetDrawingViewport({ { origin.x, origin.y }, { size.x, size.y } }, logicalSize)) {
		return false;
	}
	ImGui::Dummy(size);
	const ImVec2 end(origin.x + size.x, origin.y + size.y);
	if (show3D_) {
		LevelEditorPreview::UpdateOrbitCamera(
			camera_,
			cameraDistance_,
			cameraTarget_,
			ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(origin, end) && !ImGuizmo::IsUsing());
	}
	bool changed = false;
	if (selected_) {
		auto& object = layout_.objects[*selected_];
		changed = DrawGizmo(origin, size, object);
		if (changed) {
			SyncObject(object);
		}
	}

	if (show3D_) {
		LevelEditorPreview::DrawWorld3D(preview_, target_, camera_, logicalSize);
	} else {
		Rect2D{ {}, logicalSize }.Draw(Vector4{ 0.035f, 0.045f, 0.065f, 1.0f });
		preview_.Draw2D();
	}
	ImGui::GetWindowDrawList()->AddRect(origin, end, IM_COL32(85, 100, 120, 255));
	return changed;
}

bool WorldEditor::DrawGizmo(const ImVec2& origin, const ImVec2& size, GameObjectDesc& object) {
	Transform3D* transform3D = show3D_ ? FindComponent<Transform3D>(object) : nullptr;
	Transform2D* transform2D = show3D_ ? nullptr : FindComponent<Transform2D>(object);
	if (!transform3D && !transform2D) {
		return false;
	}

	const Vector3 translation = transform3D ? transform3D->translation :
		Vector3{ transform2D->translation.x, -transform2D->translation.y, 0.0f };
	const Vector3 originalScale = transform3D ? transform3D->scale :
		Vector3{ transform2D->scale.x, transform2D->scale.y, 1.0f };
	const Quaternion rotation = transform3D ? transform3D->rotation :
		Math::MakeQuaternion(Vector3{ 0.0f, 0.0f, -transform2D->rotation });
	const Vector3 scale = operation_ == Operation::Scale
		? Vector3{ std::max(std::abs(originalScale.x), 0.001f),
			std::max(std::abs(originalScale.y), 0.001f), std::max(std::abs(originalScale.z), 0.001f) }
		: Vector3{ 1.0f, 1.0f, 1.0f };
	Matrix4x4 matrix = Math::MakeAffineMatrix(scale, rotation, translation);
	Matrix4x4 view;
	Matrix4x4 projection;
	if (show3D_) {
		view = Math::Inverse(Math::MakeAffineMatrix(
			{ 1.0f, 1.0f, 1.0f },
			Math::MakeQuaternion(Vector3{ camera_.pitch, camera_.yaw, 0.0f }),
			camera_.translation));
		if (Math::Transform(translation, view).z <= camera_.param.nearClip && !ImGuizmo::IsUsingAny()) {
			return false;
		}
		projection = Math::MakePerspectiveFovMatrix(
			camera_.param.fovY,
			camera_.param.aspectRatio,
			camera_.param.nearClip,
			camera_.param.farClip);
	} else {
		view = Math::MakeTranslateMatrix({ 0.0f, 0.0f, 1.0f });
		projection = { {
			{ 2.0f / canvasSize_.x, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 2.0f / canvasSize_.y, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.5f, 0.0f },
			{ -1.0f, 1.0f, 0.0f, 1.0f },
		} };
	}
	ImGuizmo::OPERATION action = ImGuizmo::TRANSLATE;
	if (operation_ == Operation::Rotate) {
		action = show3D_ ? ImGuizmo::ROTATE : ImGuizmo::ROTATE_Z;
	} else if (operation_ == Operation::Scale) {
		action = show3D_ ? ImGuizmo::SCALE : ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y;
	}
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
	ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);
	ImGuizmo::SetOrthographic(!show3D_);
	ImGuizmo::SetAxisMask(false, false, !show3D_ && operation_ != Operation::Rotate);
	ImGuizmo::PushID(object.id.c_str());
	const bool moved = ImGuizmo::Manipulate(
		&view.m[0][0],
		&projection.m[0][0],
		action,
		operation_ == Operation::Move ? ImGuizmo::WORLD : ImGuizmo::LOCAL,
		&matrix.m[0][0]);
	ImGuizmo::PopID();
	if (!moved) {
		return false;
	}

	if (operation_ == Operation::Move) {
		const auto& row = matrix.m[3];
		if (transform3D) {
			transform3D->translation = { row[0], row[1], row[2] };
		} else {
			transform2D->translation = { row[0], -row[1] };
		}
	} else if (operation_ == Operation::Rotate) {
		if (transform3D) {
			DirectX::XMFLOAT4X4 matrixData;
			std::memcpy(&matrixData, &matrix, sizeof(matrixData));
			DirectX::XMFLOAT4 value;
			DirectX::XMStoreFloat4(&value, DirectX::XMQuaternionRotationMatrix(DirectX::XMLoadFloat4x4(&matrixData)));
			transform3D->rotation = Math::Normalize(Quaternion{ value.x, value.y, value.z, value.w });
		} else {
			transform2D->rotation = -std::atan2(matrix.m[0][1], matrix.m[0][0]);
		}
	} else if (transform3D) {
		transform3D->scale = { GetScale(originalScale.x, matrix, 0u), GetScale(originalScale.y, matrix, 1u), GetScale(originalScale.z, matrix, 2u) };
	} else {
		transform2D->scale = { GetScale(originalScale.x, matrix, 0u), GetScale(originalScale.y, matrix, 1u) };
	}
	return true;
}
