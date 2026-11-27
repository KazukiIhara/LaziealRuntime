#pragma once

#include <LGF/Graphics/Camera3D.h>
#include <LGF/Graphics/RenderTargetTexture.h>
#include <LGF/GameObject.h>

namespace LevelEditorPreview {

	void UpdateOrbitCamera(LGF::Camera3D& camera, float& distance, const LGF::Vector3& target, bool hovered);
	void DrawWorld3D(const LGF::GameObjectWorld& world, LGF::RenderTargetTexture& target, const LGF::Camera3D& camera, const LGF::Vector2& size);

}
