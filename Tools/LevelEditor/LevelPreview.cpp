#include "LevelPreview.h"

#include "EditorWidgets.h"

#include <LGF/Graphics/Graphics2D.h>
#include <LGF/WorldLayout.h>

#include <algorithm>
#include <cmath>

#include <imgui.h>

using namespace LGF;
using namespace LevelEditorWidgets;

LevelPreview::LevelPreview() {
	camera_.pitch = 15.0f * Math::DEG_TO_RAD;
	camera_.param.useWindowAspectRatio = false;
}

void LevelPreview::Load(const LevelData& level, const Font2D& font) {
	world_ = {};
	ui_.reset();
	hasWorld3D_ = false;
	worldError_.clear();
	uiError_.clear();

	if (!level.worldLayoutPath.empty()) {
		try {
			const WorldLayout layout = LoadWorldLayout(level.worldLayoutPath);
			world_ = CreateGameObjectWorld(layout, level.worldLayoutPath.parent_path());
			hasWorld3D_ = std::ranges::any_of(layout.objects, [](const GameObjectDesc& object) {
				return std::ranges::any_of(object.components, [](const GameObjectComponentDesc& component) {
					return std::holds_alternative<ModelRendererDesc>(component);
				});
			});
		} catch (const std::exception& error) {
			worldError_ = GetPathText(level.worldLayoutPath.filename()) + ": " + error.what();
		}
	}
	if (!level.uiLayoutPath.empty()) {
		try {
			const UILayout2D layout = LoadUILayout2D(level.uiLayoutPath);
			const std::filesystem::path directory = level.uiLayoutPath.parent_path();
			ui_ = CreateUIWorld2D(layout, font, [&directory](std::string_view reference) {
				const std::filesystem::path path(std::u8string(reference.begin(), reference.end()));
				return Texture((directory / path).lexically_normal(), TextureLoadDesc::Color());
			});
		} catch (const std::exception& error) {
			uiError_ = GetPathText(level.uiLayoutPath.filename()) + ": " + error.what();
		}
	}
}

void LevelPreview::Draw() {
	ImGui::SeparatorText("Level Preview");
	EditVector2("Game size", gameSize_, 1.0f, 64.0f, 8192.0f);
	if (ui_) {
		ImGui::Checkbox("Test buttons", &testButtons_);
	}
	if (hasWorld3D_) {
		ImGui::TextUnformatted("Right drag: orbit   Wheel: zoom");
	}
	if (!worldError_.empty()) {
		ImGui::TextWrapped("%s", worldError_.c_str());
	}
	if (!uiError_.empty()) {
		ImGui::TextWrapped("%s", uiError_.c_str());
	}

	const ImVec2 available = ImGui::GetContentRegionAvail();
	const float scale = std::min(available.x / gameSize_.x, available.y / gameSize_.y);
	if (!(gameSize_.x > 0.0f && gameSize_.y > 0.0f) || !std::isfinite(scale) || scale <= 0.0f) {
		return;
	}
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 size(gameSize_.x * scale, gameSize_.y * scale);
	if (!Graphics2D::SetDrawingViewport({ { origin.x, origin.y }, { size.x, size.y } }, gameSize_)) {
		return;
	}
	ImGui::Dummy(size);
	const ImVec2 end(origin.x + size.x, origin.y + size.y);
	const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(origin, end);
	if (hasWorld3D_) {
		camera_.param.aspectRatio = gameSize_.x / gameSize_.y;
		LevelEditorPreview::UpdateOrbitCamera(camera_, cameraDistance_, {}, hovered);
		LevelEditorPreview::DrawWorld3D(world_, target_, camera_, gameSize_);
	} else {
		Rect2D{ {}, gameSize_ }.Draw(Vector4{ 0.035f, 0.045f, 0.065f, 1.0f });
	}
	world_.Draw2D();
	if (ui_) {
		const ImVec2 mouse = ImGui::GetMousePos();
		ui_->world.Update(
			{
				.cursorPosition = { (mouse.x - origin.x) / scale, (mouse.y - origin.y) / scale },
				.isCursorPressed = testButtons_ && ImGui::IsMouseDown(ImGuiMouseButton_Left),
				.isCursorEnabled = testButtons_ && hovered,
			},
			gameSize_);
		ui_->world.Draw();
	}
	ImGui::GetWindowDrawList()->AddRect(origin, end, IM_COL32(85, 100, 120, 255));
}
