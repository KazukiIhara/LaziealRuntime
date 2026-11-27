#pragma once

#include "PreviewRender.h"

#include <LGF/Level.h>
#include <LGF/UI/UILayout2D.h>

#include <optional>

class LevelPreview {
public:
	LevelPreview();

	void Load(const LGF::LevelData& level, const LGF::Font2D& font);
	void Draw();

private:
	LGF::GameObjectWorld world_;
	std::optional<LGF::UILayoutInstance2D> ui_;
	LGF::RenderTargetTexture target_;
	LGF::Camera3D camera_;
	LGF::Vector2 gameSize_{ 1280.0f, 720.0f };
	float cameraDistance_ = 8.5f;
	bool hasWorld3D_{};
	bool testButtons_{};
	std::string worldError_;
	std::string uiError_;
};
