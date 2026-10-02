#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <Windows.h>

namespace LGF {

	/// <summary>
	/// ウィンドウクラス　
	/// </summary>
	class Win32Window {
	public:
		using RawInputHandler = std::function<void(void*)>;
		using MessageHandler = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;

		struct WndSize {
			uint32_t width{};
			uint32_t height{};
		};

		struct Setting {
			WndSize wndSize{ 800,600 };
			std::wstring wndName{ L"LGF" };
			bool isFullScreen = false;
		};
	public:
		Win32Window();
		~Win32Window();

		bool Initialize(const Setting& setting = Setting{});
		bool Finalize();

		// メッセージの処理
		bool ProcessMessage();

		// ウィンドウハンドルの取得
		HWND GetHwnd() const;
		// ウィンドウクラスの取得
		WNDCLASS GetWndClass() const;
		// ウィンドウサイズの取得
		Setting GetWndSetting()const;
		bool IsResized()const;
		void ClearResizeFlag();
		bool SetTitle(std::wstring_view title);
		bool Resize(uint32_t width, uint32_t height);
		void SetFullscreen(bool fullscreen);

		// フルスクリーンモード切り替え
		void ToggleFullScreen();
		WndSize GetSize() const;
		std::wstring GetTitle() const;
		bool IsFullscreen() const;
		void SetRawInputHandler(RawInputHandler handler);
		void SetMessageHandler(MessageHandler handler);

	private:

		// ゲームウィンドウの作成
		void CreateGameWindow(int32_t clientWidth, int32_t clientHeight,
			const std::wstring& windowName = L"LGF", UINT windowStyle = WS_OVERLAPPEDWINDOW);

		// ウィンドウプロシージャ
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
		void OnResize(uint32_t width, uint32_t height);

	private:
		// ウィンドウハンドル
		HWND hwnd_ = nullptr;
		// ウィンドウクラス
		WNDCLASS wc_{};
		// ウィンドウ設定
		Setting setting_;
		bool isResized_ = false;
		// ウィンドウモード時の位置とサイズ
		RECT windowRect_{};
		// Comの初期化判別フラグ
		bool isCOMInitialized_ = false;
		RawInputHandler rawInputHandler_;
		MessageHandler messageHandler_;
	};
}
