#include "WindowSystem.h"

#include "Core/Config/RuntimeConfig.h"

using namespace LGF;

WindowSystem::WindowSystem() {
}

WindowSystem::~WindowSystem() = default;

bool WindowSystem::Initialize(const WindowConfig& config) {
	const Win32Window::Setting setting{
		.wndSize = { config.width, config.height },
		.wndName = config.title,
		.isFullScreen = config.fullscreen,
	};
	return window_.Initialize(setting);
}

bool WindowSystem::Finalize() {
	return window_.Finalize();
}

void WindowSystem::ProcessMessage() {
	isCloseRequested_ = !window_.ProcessMessage();
}

Win32Window& WindowSystem::GetWindow() {
	return window_;
}

bool WindowSystem::IsCloseRequested() const {
	return isCloseRequested_;
}
