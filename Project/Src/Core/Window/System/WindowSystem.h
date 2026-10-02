#pragma once

#include "Core/Window/Win32Window.h"

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

		Win32Window& GetWindow();

		bool IsCloseRequested() const;

	private:
		Win32Window window_;
		bool isCloseRequested_ = false;
	};
}
