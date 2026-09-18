#pragma once

#include "Core/Window/Window.h"

namespace LGF {
	struct WindowConfig;

	/// <summary>
	/// ウィンドウ管理クラス
	/// </summary>
	class WindowSystem {
	public:
		WindowSystem();
		~WindowSystem();

		bool Initialize(const WindowConfig& config);
		bool Finalize();

		void ProcessMessage();

		Window& GetWindow();

		bool IsCloseRequested() const;

	private:
		Window window_;
		bool isCloseRequested_ = false;
	};
}
