#include "PreviewRender.h"

#include <LGF/Graphics/DirectionalLight.h>
#include <LGF/Graphics/Graphics3D.h>
#include <LGF/Graphics/Sprite.h>

#include <algorithm>
#include <cmath>

#include <imgui.h>

using namespace LGF;

void LevelEditorPreview::UpdateOrbitCamera(Camera3D& camera, float& distance, const Vector3& target, bool hovered) {
	if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
		const ImVec2 delta = ImGui::GetIO().MouseDelta;
		camera.yaw -= delta.x * 0.01f;
		camera.pitch = std::clamp(camera.pitch + delta.y * 0.01f, -1.5f, 1.5f);
	}
	if (hovered) {
		distance = std::clamp(distance * std::exp(-ImGui::GetIO().MouseWheel * 0.13f), 0.1f, 500.0f);
	}
	const float cosPitch = std::cos(camera.pitch);
	const Vector3 forward{ std::sin(camera.yaw) * cosPitch, -std::sin(camera.pitch), std::cos(camera.yaw) * cosPitch };
	camera.translation = target - forward * distance;
}

void LevelEditorPreview::DrawWorld3D(const GameObjectWorld& world, RenderTargetTexture& target, const Camera3D& camera, const Vector2& size) {
	if (!target.IsValid()) {
		target = RenderTargetTexture({
			.width = 640u,
			.height = 360u,
			.clearColor = { 0.035f, 0.045f, 0.065f, 1.0f },
		});
	}
	const DirectionalLight light({ -0.45f, -1.0f, 0.35f }, Color::White, 2.0f);
	light.SetScene();
	Graphics3D::BeginRenderPass(target, camera);
	world.Draw3D();
	Graphics3D::EndRenderPass();
	Sprite{ target.GetTexture(), {}, size }.Draw();
}
