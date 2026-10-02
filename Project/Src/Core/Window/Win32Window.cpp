#include "Win32Window.h"

#include <limits>
#include <utility>

using namespace LGF;

Win32Window::Win32Window() {
}

Win32Window::~Win32Window() {
}

bool Win32Window::Initialize(const Setting& setting) {
	setting_ = setting;
	const bool startFullscreen = setting_.fullscreen;
	setting_.fullscreen = false;

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
		return false;
	}

	if (SUCCEEDED(hr)) {
		isCOMInitialized_ = true;
	}

	timeBeginPeriod(1);

	CreateGameWindow(
		setting.size.width,
		setting.size.height,
		setting.title,
		WS_OVERLAPPEDWINDOW
	);

	if (hwnd_ == nullptr) {
		timeEndPeriod(1);

		if (isCOMInitialized_) {
			CoUninitialize();
			isCOMInitialized_ = false;
		}

		return false;
	}

	if (startFullscreen) {
		ToggleFullscreen();
	}

	return true;
}

bool Win32Window::Finalize() {
	if (hwnd_ != nullptr) {
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}

	if (wc_.lpszClassName != nullptr && wc_.hInstance != nullptr) {
		UnregisterClass(wc_.lpszClassName, wc_.hInstance);
	}

	timeEndPeriod(1);

	if (isCOMInitialized_) {
		CoUninitialize();
		isCOMInitialized_ = false;
	}

	return true;
}

bool Win32Window::ProcessMessage() {
	MSG msg{};

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			return false;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return true;
}

HWND Win32Window::GetHwnd() const {
	return hwnd_;
}

Win32Window::Setting Win32Window::GetSetting() const {
	return setting_;
}

bool Win32Window::IsResized() const {
	return isResized_;
}

void Win32Window::ClearResizeFlag() {
	isResized_ = false;
}

bool Win32Window::SetTitle(std::wstring_view title) {
	if (hwnd_ == nullptr) {
		return false;
	}

	const std::wstring ownedTitle(title);
	if (SetWindowTextW(hwnd_, ownedTitle.c_str()) == FALSE) {
		return false;
	}
	setting_.title = ownedTitle;
	return true;
}

bool Win32Window::Resize(uint32_t width, uint32_t height) {
	if (hwnd_ == nullptr || width == 0u || height == 0u ||
		width > static_cast<uint32_t>(std::numeric_limits<LONG>::max()) ||
		height > static_cast<uint32_t>(std::numeric_limits<LONG>::max()) ||
		setting_.fullscreen) {
		return false;
	}

	RECT windowRect{
		.left = 0,
		.top = 0,
		.right = static_cast<LONG>(width),
		.bottom = static_cast<LONG>(height),
	};
	const DWORD windowStyle = static_cast<DWORD>(GetWindowLongPtr(hwnd_, GWL_STYLE));
	if (AdjustWindowRect(&windowRect, windowStyle, FALSE) == FALSE) {
		return false;
	}

	return SetWindowPos(
		hwnd_,
		nullptr,
		0,
		0,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
}

void Win32Window::SetFullscreen(bool fullscreen) {
	if (setting_.fullscreen != fullscreen) {
		ToggleFullscreen();
	}
}

void Win32Window::ToggleFullscreen() {
	if (hwnd_ == nullptr) {
		return;
	}

	if (!setting_.fullscreen) {
		GetWindowRect(hwnd_, &windowRect_);

		HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(MONITORINFO);
		GetMonitorInfo(monitor, &monitorInfo);

		SetWindowLongPtr(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);

		SetWindowPos(
			hwnd_,
			HWND_TOP,
			monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER
		);

		setting_.fullscreen = true;
	} else {
		SetWindowLongPtr(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);

		SetWindowPos(
			hwnd_,
			HWND_TOP,
			windowRect_.left,
			windowRect_.top,
			windowRect_.right - windowRect_.left,
			windowRect_.bottom - windowRect_.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER
		);

		ShowWindow(hwnd_, SW_RESTORE);
		setting_.fullscreen = false;
	}
}

Win32Window::Size Win32Window::GetSize() const {
	return setting_.size;
}

std::wstring Win32Window::GetTitle() const {
	return setting_.title;
}

bool Win32Window::IsFullscreen() const {
	return setting_.fullscreen;
}

void Win32Window::SetRawInputHandler(RawInputHandler handler) {
	rawInputHandler_ = std::move(handler);
}

void Win32Window::SetMessageHandler(MessageHandler handler) {
	messageHandler_ = std::move(handler);
}

void Win32Window::CreateGameWindow(
	int32_t clientWidth,
	int32_t clientHeight,
	const std::wstring& windowName,
	UINT windowStyle) {
	wc_ = {};
	wc_.lpfnWndProc = WindowProc;
	wc_.lpszClassName = L"LGFWindowClass";
	wc_.hInstance = GetModuleHandle(nullptr);
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	if (RegisterClass(&wc_) == 0) {
		hwnd_ = nullptr;
		return;
	}

	RECT wrc{ 0, 0, clientWidth, clientHeight };
	AdjustWindowRect(&wrc, windowStyle, false);

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

	if (hwnd_ == nullptr) {
		return;
	}

	ShowWindow(hwnd_, SW_SHOW);
}

LRESULT Win32Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_NCCREATE) {
		const CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lparam);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
	}

	Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (msg == WM_INPUT && window != nullptr && window->rawInputHandler_) {
		window->rawInputHandler_(reinterpret_cast<void*>(lparam));
		return 0;
	}
	if (window != nullptr && window->messageHandler_ &&
		window->messageHandler_(hwnd, msg, wparam, lparam)) {
		return true;
	}

	switch (msg) {
		case WM_SIZE:
			if (window != nullptr && wparam != SIZE_MINIMIZED) {
				const uint32_t width = static_cast<uint32_t>(LOWORD(lparam));
				const uint32_t height = static_cast<uint32_t>(HIWORD(lparam));
				window->OnResize(width, height);
			}
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Win32Window::OnResize(uint32_t width, uint32_t height) {
	if (width == 0 || height == 0) {
		return;
	}

	if (setting_.size.width == width && setting_.size.height == height) {
		return;
	}

	setting_.size.width = width;
	setting_.size.height = height;
	isResized_ = true;
}
