#pragma once

#include <array>
#include <filesystem>
#include <string>

#include "LGF/Animation/Easing.h"
#include "LGF/Graphics/Color.h"
#include "LGF/Graphics/SceneTransition.h"

namespace LGF {

	/// <summary>
	/// シーン遷移の時間と描画方法を設定する。
	/// </summary>
	struct SceneTransition {
		float fadeOutDuration = 0.25f;
		float fadeInDuration = 0.25f;
		EasingType fadeOutEasing = EasingType::EaseInCubic;
		EasingType fadeInEasing = EasingType::EaseOutCubic;
		std::string pipeline = "SceneTransitionFade";
		Vector4 color = Color::Black;
		std::array<Vector4, kSceneTransitionParameterCount> parameters{};
	};

	/// <summary>
	/// Jsonからシーン遷移設定を読み込む。
	/// </summary>
	SceneTransition LoadSceneTransition(const std::filesystem::path& filePath);

}
