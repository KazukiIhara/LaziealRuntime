#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace LGF {

	struct RuntimeConfig;
	struct LevelData;
	struct LevelIoResult;
	struct UIHandle;
	class AudioSystem;
	class GameObjectWorld;
	class GraphicsSystem;
	class InputSystem;
	class TimeSystem;
	class UIWorld;
	class WindowSystem;

	namespace Detail {
		struct LevelRuntime;
	}

	/// <summary>
	/// Runtime層のWindow、Input、Timeと描画フレームワークを接続する。
	/// </summary>
	class Framework {
	public:
		Framework();
		~Framework();

		bool Initialize(const RuntimeConfig& config);
		bool Update();
		bool Finalize();
		LevelIoResult SetLevelData(const std::filesystem::path& fileName);
		void ClearLevelData();
		const LevelData* GetLevelData() const;
		UIWorld* GetLevelUIWorld();
		UIHandle FindLevelUIElement(std::string_view id) const;
		GameObjectWorld* GetLevelGameObjectWorld();

		double GetDeltaTime() const;
		bool SetWindowTitle(std::wstring_view title);
		bool ResizeWindow(uint32_t width, uint32_t height);
		bool SetWindowFullscreen(bool fullscreen);
		bool ToggleWindowFullscreen();
		uint32_t GetWindowWidth() const;
		uint32_t GetWindowHeight() const;
		std::wstring GetWindowTitle() const;
		bool IsWindowFullscreen() const;
		bool IsWindowFocused() const;

	private:
		void BeginFrame();
		void EndFrame();

		std::unique_ptr<WindowSystem> windowSystem_;
		std::unique_ptr<Detail::LevelRuntime> level_;
		std::unique_ptr<AudioSystem> audioSystem_;
		std::unique_ptr<GraphicsSystem> graphicsSystem_;
		std::unique_ptr<InputSystem> inputSystem_;
		std::unique_ptr<TimeSystem> timeSystem_;
		bool isFrameStarted_ = false;
	};

}
