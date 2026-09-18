#include "Window.h"

#include <utility>

using namespace LGF;

Window::Window() {
}

Window::~Window() {
}

bool Window::Initialize(const Window::Setting& setting) {
	// 設定を取得
	setting_ = setting;
	const bool startFullscreen = setting_.isFullScreen;
	setting_.isFullScreen = false;

	// COMを初期化
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// 初期化失敗を確認
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
		return false;
	}

	// COM初期化成功を記録
	if (SUCCEEDED(hr)) {
		isCOMInitialized_ = true;
	}

	// システムタイマー分解能をあげる
	timeBeginPeriod(1);

	// ゲームウィンドウの作成
	CreateGameWindow(
		setting.wndSize.width,
		setting.wndSize.height,
		setting.wndName,
		WS_OVERLAPPEDWINDOW
	);

	// ウィンドウ生成失敗を確認
	if (hwnd_ == nullptr) {
		timeEndPeriod(1);

		if (isCOMInitialized_) {
			CoUninitialize();
			isCOMInitialized_ = false;
		}

		return false;
	}

	if (startFullscreen) {
		ToggleFullScreen();
	}

	return true;
}

bool Window::Finalize() {
	// ウィンドウを破棄
	if (hwnd_ != nullptr) {
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}

	// ウィンドウクラスを登録解除
	if (wc_.lpszClassName != nullptr && wc_.hInstance != nullptr) {
		UnregisterClass(wc_.lpszClassName, wc_.hInstance);
	}

	// タイマー分解能を戻す
	timeEndPeriod(1);

	// COMを終了
	if (isCOMInitialized_) {
		CoUninitialize();
		isCOMInitialized_ = false;
	}

	return true;
}

bool Window::ProcessMessage() {
	// メッセージ
	MSG msg{};

	// ウィンドウメッセージを処理
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		// 終了メッセージを検出
		if (msg.message == WM_QUIT) {
			return false;
		}

		// メッセージを変換して送る
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return true;
}

HWND Window::GetHwnd() const {
	return hwnd_;
}

WNDCLASS Window::GetWndClass() const {
	return wc_;
}

Window::Setting Window::GetWndSetting() const {
	return setting_;
}

bool Window::IsResized() const {
	return isResized_;
}

void Window::ClearResizeFlag() {
	isResized_ = false;
}

void Window::ToggleFullScreen() {
	// 無効なウィンドウは処理しない
	if (hwnd_ == nullptr) {
		return;
	}

	// 現在のフルスクリーン状態を確認
	if (!setting_.isFullScreen) {
		// 現在のウィンドウ位置とサイズを保存
		GetWindowRect(hwnd_, &windowRect_);

		// モニター情報を取得
		HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(MONITORINFO);
		GetMonitorInfo(monitor, &monitorInfo);

		// フルスクリーン用スタイルに変更
		SetWindowLongPtr(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);

		// モニター全体に合わせて表示
		SetWindowPos(
			hwnd_,
			HWND_TOP,
			monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER
		);

		// フルスクリーンフラグを設定
		setting_.isFullScreen = true;
	} else {
		// ウィンドウスタイルを元に戻す
		SetWindowLongPtr(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);

		// 保存した位置とサイズに戻す
		SetWindowPos(
			hwnd_,
			HWND_TOP,
			windowRect_.left,
			windowRect_.top,
			windowRect_.right - windowRect_.left,
			windowRect_.bottom - windowRect_.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER
		);

		// 通常表示に戻す
		ShowWindow(hwnd_, SW_RESTORE);

		// フルスクリーンフラグを解除
		setting_.isFullScreen = false;
	}
}

void Window::SetRawInputHandler(RawInputHandler handler) {
	rawInputHandler_ = std::move(handler);
}

void Window::SetMessageHandler(MessageHandler handler) {
	messageHandler_ = std::move(handler);
}

void Window::CreateGameWindow(int32_t clientWidth, int32_t clientHeight, const std::wstring& windowName, UINT windowStyle) {
	// ウィンドウクラス情報を設定
	wc_ = {};

	// ウィンドウプロシージャを設定
	wc_.lpfnWndProc = WindowProc;

	// ウィンドウクラス名を設定
	wc_.lpszClassName = L"LGFWindowClass";

	// インスタンスハンドルを設定
	wc_.hInstance = GetModuleHandle(nullptr);

	// カーソルを設定
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスを登録
	if (RegisterClass(&wc_) == 0) {
		hwnd_ = nullptr;
		return;
	}

	// クライアントサイズを設定
	RECT wrc{ 0, 0, clientWidth, clientHeight };

	// ウィンドウ枠を含めたサイズに変換
	AdjustWindowRect(&wrc, windowStyle, false);

	// ウィンドウを生成
	hwnd_ = CreateWindow(
		wc_.lpszClassName,
		windowName.c_str(),
		windowStyle,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc_.hInstance,
		this
	);

	// 生成失敗を確認
	if (hwnd_ == nullptr) {
		return;
	}

	// ウィンドウを表示
	ShowWindow(hwnd_, SW_SHOW);

}

LRESULT Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_NCCREATE) {
		const CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lparam);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
	}

	Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (msg == WM_INPUT && window != nullptr && window->rawInputHandler_) {
		window->rawInputHandler_(reinterpret_cast<void*>(lparam));
		return 0;
	}
	if (window != nullptr && window->messageHandler_ &&
		window->messageHandler_(hwnd, msg, wparam, lparam)) {
		return true;
	}

	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		case WM_SIZE:
			if (window != nullptr && wparam != SIZE_MINIMIZED) {
				const uint32_t width = static_cast<uint32_t>(LOWORD(lparam));
				const uint32_t height = static_cast<uint32_t>(HIWORD(lparam));
				window->OnResize(width, height);
			}
			return 0;

		case WM_DESTROY:
			// OSに対してアプリの終了を伝える
			PostQuitMessage(0);
			return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Window::OnResize(uint32_t width, uint32_t height) {
	if (width == 0 || height == 0) {
		return;
	}

	if (setting_.wndSize.width == width && setting_.wndSize.height == height) {
		return;
	}

	setting_.wndSize.width = width;
	setting_.wndSize.height = height;
	isResized_ = true;
}
