#pragma once

#include "LGF/UI/UIWorld.h"

#include <filesystem>
#include <functional>

namespace LGF {

	/// <summary>画面上のUI要素を作るためのデータ。親はCanvasまたはPanel。</summary>
	struct UILayoutElement2D {
		std::string id;
		std::string parentId;
		UIElementType type = UIElementType::Canvas;
		UITransform transform{};
		bool isVisible = true;
		bool isEnabled = true;
		std::string text;
		std::string texturePath;
		Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	struct UILayout2D {
		Vector2 size{ 640.0f, 360.0f };
		std::vector<UILayoutElement2D> elements;
	};

	struct UILayoutInstance2D {
		UIWorld world;
		/// <summary>レイアウトの要素と同じ順序で格納する。</summary>
		std::vector<UIHandle> handles;
	};

	UILayout2D LoadUILayout2D(const std::filesystem::path& filePath);
	void SaveUILayout2D(const std::filesystem::path& filePath, const UILayout2D& layout);

	/// <summary>画面用UIWorldを生成する。画像の参照先はresolveTextureで解決する。</summary>
	UILayoutInstance2D CreateUIWorld2D(const UILayout2D& layout, const Font2D& font, const std::function<Texture(std::string_view)>& resolveTexture);

}
