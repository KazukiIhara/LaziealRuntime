#include "LGF/Window.h"

#include "Core/Framework/Framework.h"

namespace LGF::Detail {
	Framework& GetFramework();
}

bool LGF::Window::SetTitle(std::wstring_view title) {
	return Detail::GetFramework().SetWindowTitle(title);
}

bool LGF::Window::Resize(uint32_t width, uint32_t height) {
	return Detail::GetFramework().ResizeWindow(width, height);
}

bool LGF::Window::SetFullscreen(bool fullscreen) {
	return Detail::GetFramework().SetWindowFullscreen(fullscreen);
}

bool LGF::Window::ToggleFullscreen() {
	return Detail::GetFramework().ToggleWindowFullscreen();
}

LGF::Window::Size LGF::Window::GetSize() {
	const Framework& framework = Detail::GetFramework();
	return {
		.width = framework.GetWindowWidth(),
		.height = framework.GetWindowHeight(),
	};
}

std::wstring LGF::Window::GetTitle() {
	return Detail::GetFramework().GetWindowTitle();
}

bool LGF::Window::IsFullscreen() {
	return Detail::GetFramework().IsWindowFullscreen();
}
