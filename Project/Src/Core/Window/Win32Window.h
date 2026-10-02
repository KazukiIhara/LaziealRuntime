#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <Windows.h>

namespace LGF {

	/// <summary>
	/// Win32ウィンドウの生成とメッセージ処理を管理する。
	/// </summary>
	class Win32Window {
	public:
		using RawInputHandler = std::function<void(void*)>;
		using MessageHandler = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;

		struct Size {
			uint32_t width{};
			uint32_t height{};
		};

		struct Setting {
			Size size{ 800, 600 };
			std::wstring title{ L"LGF" };
			bool fullscreen = false;
		};

		Win32Window();
		~Win32Window();

		bool Initialize(const Setting& setting = Setting{});
		bool Finalize();

		bool ProcessMessage();

		HWND GetHwnd() const;
		Setting GetSetting() const;
		bool IsResized() const;
		void ClearResizeFlag();
		bool SetTitle(std::wstring_view title);
		bool Resize(uint32_t width, uint32_t height);
		void SetFullscreen(bool fullscreen);
		void ToggleFullscreen();
		Size GetSize() const;
		std::wstring GetTitle() const;
		bool IsFullscreen() const;
		void SetRawInputHandler(RawInputHandler handler);
		void SetMessageHandler(MessageHandler handler);

	private:
		void CreateGameWindow(int32_t clientWidth, int32_t clientHeight,
			const std::wstring& windowName = L"LGF", UINT windowStyle = WS_OVERLAPPEDWINDOW);
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
		void OnResize(uint32_t width, uint32_t height);

		HWND hwnd_ = nullptr;
		WNDCLASS wc_{};
		Setting setting_;
		bool isResized_ = false;
		RECT windowRect_{};
		bool isCOMInitialized_ = false;
		RawInputHandler rawInputHandler_;
		MessageHandler messageHandler_;
	};
}
