#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace LGF {

	struct RuntimeConfig;
	class GraphicsSystem;
	class InputSystem;
	class TimeSystem;
	class WindowSystem;

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

		double GetDeltaTime() const;
		bool SetWindowTitle(std::wstring_view title);
		bool ResizeWindow(uint32_t width, uint32_t height);
		bool SetWindowFullscreen(bool fullscreen);
		bool ToggleWindowFullscreen();
		uint32_t GetWindowWidth() const;
		uint32_t GetWindowHeight() const;
		std::wstring GetWindowTitle() const;
		bool IsWindowFullscreen() const;

	private:
		void BeginFrame();
		void EndFrame();

		std::unique_ptr<WindowSystem> windowSystem_;
		std::unique_ptr<GraphicsSystem> graphicsSystem_;
		std::unique_ptr<InputSystem> inputSystem_;
		std::unique_ptr<TimeSystem> timeSystem_;
		bool isFrameStarted_ = false;
	};

}
