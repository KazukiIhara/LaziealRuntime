#pragma once

#include "LGF/GameObject.h"

#include <filesystem>
#include <variant>

namespace LGF {

	struct ModelRendererDesc {
		std::filesystem::path modelPath;
	};

	struct SpriteRendererDesc {
		std::filesystem::path texturePath;
		Vector2 pivot{ 0.5f, 0.5f };
	};

	using GameObjectComponentDesc = std::variant<Transform3D, ModelRendererDesc, Transform2D, SpriteRendererDesc>;

	struct GameObjectDesc {
		std::string id;
		std::string name;
		bool isEnabled = true;
		std::vector<GameObjectComponentDesc> components;
	};

	struct WorldLayout {
		std::vector<GameObjectDesc> objects;
	};

	WorldLayout LoadWorldLayout(const std::filesystem::path& filePath);
	void SaveWorldLayout(const std::filesystem::path& filePath, const WorldLayout& layout);

	/// <summary>配置からWorldを作る。アセットの相対パスはassetDirectoryを基準に解決する。</summary>
	GameObjectWorld CreateGameObjectWorld(const WorldLayout& layout, const std::filesystem::path& assetDirectory);

}
