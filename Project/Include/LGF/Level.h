#pragma once

#include "LGF/Graphics/Transform2D.h"
#include "LGF/UI/UIWorld.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace LGF {

	class GameObjectWorld;

	/// <summary>2Dレベルに配置するオブジェクトのデータ。</summary>
	struct LevelObject2D {
		/// <summary>同じレベル内で一意のID。</summary>
		std::string id;
		/// <summary>アプリケーションが解釈するオブジェクト種別。</summary>
		std::string type;
		Transform2D transform{};
		/// <summary>アプリケーションが解決するアセットパス。参照がない場合は空文字列。</summary>
		std::string assetPath;
	};

	/// <summary>1つのJSONファイルに保存する2Dレベルのデータ。</summary>
	struct LevelData2D {
		std::string name;
		std::vector<LevelObject2D> objects;
	};

	/// <summary>UIとワールドの配置ファイルを参照するレベルデータ。相対パスはこのファイルの場所が基準。</summary>
	struct LevelData {
		std::filesystem::path uiLayoutPath;
		std::filesystem::path worldLayoutPath;
	};

	struct LevelIoResult {
		bool succeeded = false;
		std::string message;

		explicit operator bool() const {
			return succeeded;
		}
	};

	/// <summary>JSONからレベルを読み込む。失敗時はlevelを変更しない。</summary>
	LevelIoResult LoadLevel2D(const std::filesystem::path& filePath, LevelData2D& level);

	/// <summary>レベルをJSONに保存する。検証と書き込みが成功した後に既存ファイルを置き換える。</summary>
	LevelIoResult SaveLevel2D(const std::filesystem::path& filePath, const LevelData2D& level);

	LevelIoResult LoadLevelData(const std::filesystem::path& filePath, LevelData& level);
	LevelIoResult SaveLevelData(const std::filesystem::path& filePath, const LevelData& level);

	/// <summary>使用するレベルを切り替える。相対パスの起点はAssets/LevelDatas。</summary>
	LevelIoResult SetLevelData(const std::filesystem::path& fileName);
	void ClearLevelData();

	/// <summary>現在のレベルを取得する。参照ファイルのパスは解決済み。</summary>
	const LevelData* GetLevelData();
	UIWorld* GetLevelUIWorld();
	UIHandle FindLevelUIElement(std::string_view id);
	GameObjectWorld* GetLevelGameObjectWorld();

}
