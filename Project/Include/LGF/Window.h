#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace LGF::Window {

	struct Size final {
		uint32_t width = 0u;
		uint32_t height = 0u;
	};

	bool SetTitle(std::wstring_view title);
	bool Resize(uint32_t width, uint32_t height);
	bool SetFullscreen(bool fullscreen);
	bool ToggleFullscreen();

	Size GetSize();
	std::wstring GetTitle();
	bool IsFullscreen();

}
