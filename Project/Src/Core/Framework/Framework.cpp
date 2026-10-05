#include "Framework.h"

#include "Core/Config/RuntimeConfig.h"
#include "Core/Time/TimeSystem.h"
#include "Core/Window/System/WindowSystem.h"
#include "Input/System/InputSystem.h"
#include "LGF/Graphics/GraphicsSystem.h"

using namespace LGF;

Framework::Framework() = default;

Framework::~Framework() = default;

bool Framework::Initialize(const RuntimeConfig& config) {
	windowSystem_ = std::make_unique<WindowSystem>();
	if (!windowSystem_->Initialize(config.window)) {
		return false;
	}

	Win32Window& window = windowSystem_->GetWindow();
	const Win32Window::Setting windowSetting = window.GetSetting();

	inputSystem_ = std::make_unique<InputSystem>();
	if (!inputSystem_->Initialize(window.GetHwnd(), config.input)) {
		windowSystem_->Finalize();
		return false;
	}

	graphicsSystem_ = std::make_unique<GraphicsSystem>();
	const GraphicsInitDesc graphicsDesc{
		.windowHandle = window.GetHwnd(),
		.width = windowSetting.size.width,
		.height = windowSetting.size.height,
		.assetRoot = std::filesystem::absolute(config.assetRoot).lexically_normal(),
	};
	if (!graphicsSystem_->Initialize(graphicsDesc)) {
		inputSystem_->Finalize();
		windowSystem_->Finalize();
		return false;
	}

	window.SetRawInputHandler([this](void* rawInputHandle) {
		inputSystem_->HandleRawInput(rawInputHandle);
	});
	window.SetMessageHandler(
		[this](HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) {
			return graphicsSystem_->HandleWindowMessage(
				windowHandle,
				message,
				wParam,
				lParam);
		});

	timeSystem_ = std::make_unique<TimeSystem>();
	if (!timeSystem_->Initialize()) {
		window.SetRawInputHandler({});
		window.SetMessageHandler({});
		graphicsSystem_->Finalize();
		inputSystem_->Finalize();
		windowSystem_->Finalize();
		return false;
	}

	return true;
}

bool Framework::Update() {
	if (isFrameStarted_) {
		EndFrame();
		isFrameStarted_ = false;
	}

	windowSystem_->ProcessMessage();
	if (windowSystem_->IsCloseRequested()) {
		return false;
	}

	Win32Window& window = windowSystem_->GetWindow();
	if (window.IsResized()) {
		const Win32Window::Setting setting = window.GetSetting();
		if (!graphicsSystem_->Resize(setting.size.width, setting.size.height)) {
			return false;
		}
		window.ClearResizeFlag();
	}

	BeginFrame();
	isFrameStarted_ = true;
	return true;
}

bool Framework::Finalize() {
	if (isFrameStarted_) {
		EndFrame();
		isFrameStarted_ = false;
	}

	bool succeeded = true;
	if (timeSystem_) {
		succeeded = timeSystem_->Finalize() && succeeded;
	}
	if (windowSystem_) {
		Win32Window& window = windowSystem_->GetWindow();
		window.SetRawInputHandler({});
		window.SetMessageHandler({});
	}
	if (inputSystem_) {
		succeeded = inputSystem_->Finalize() && succeeded;
	}
	if (graphicsSystem_) {
		succeeded = graphicsSystem_->Finalize() && succeeded;
	}
	if (windowSystem_) {
		succeeded = windowSystem_->Finalize() && succeeded;
	}

	timeSystem_.reset();
	inputSystem_.reset();
	graphicsSystem_.reset();
	windowSystem_.reset();
	return succeeded;
}

double Framework::GetDeltaTime() const {
	return timeSystem_ ? timeSystem_->GetDeltaTime() : 0.0;
}

bool Framework::SetWindowTitle(std::wstring_view title) {
	return windowSystem_ && windowSystem_->GetWindow().SetTitle(title);
}

bool Framework::ResizeWindow(uint32_t width, uint32_t height) {
	return windowSystem_ && windowSystem_->GetWindow().Resize(width, height);
}

bool Framework::SetWindowFullscreen(bool fullscreen) {
	if (!windowSystem_) {
		return false;
	}
	windowSystem_->GetWindow().SetFullscreen(fullscreen);
	return true;
}

bool Framework::ToggleWindowFullscreen() {
	if (!windowSystem_) {
		return false;
	}
	windowSystem_->GetWindow().ToggleFullscreen();
	return true;
}

uint32_t Framework::GetWindowWidth() const {
	return windowSystem_ ? windowSystem_->GetWindow().GetSize().width : 0u;
}

uint32_t Framework::GetWindowHeight() const {
	return windowSystem_ ? windowSystem_->GetWindow().GetSize().height : 0u;
}

std::wstring Framework::GetWindowTitle() const {
	return windowSystem_ ? windowSystem_->GetWindow().GetTitle() : std::wstring{};
}

bool Framework::IsWindowFullscreen() const {
	return windowSystem_ && windowSystem_->GetWindow().IsFullscreen();
}

void Framework::BeginFrame() {
	timeSystem_->BeginFrame();
	inputSystem_->BeginFrame();
	graphicsSystem_->BeginFrame();
}

void Framework::EndFrame() {
	inputSystem_->EndFrame();
	graphicsSystem_->EndFrame();
}
