#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace LGF {

	struct WindowConfig {
		std::wstring title = L"LGF";
		uint32_t width = 800;
		uint32_t height = 600;
		bool fullscreen = false;
	};

	struct RuntimeConfig {
		std::filesystem::path assetRoot = "Assets";
		WindowConfig window{};
	};

	using MainFunction = void(*)();

	/// <summary>
	/// Runtimeを初期化し、アプリケーション本体を実行して終了処理を行う。
	/// 独自のエントリーポイントを使用するホストからも呼び出せる。
	/// </summary>
	int Run(const RuntimeConfig& config, MainFunction mainFunction);

}

/// <summary>
/// アプリケーション側で実装するRuntime設定関数。
/// </summary>
LGF::RuntimeConfig ConfigureRuntime();

/// <summary>
/// アプリケーション側で実装するメイン関数。
/// </summary>
void Main();
