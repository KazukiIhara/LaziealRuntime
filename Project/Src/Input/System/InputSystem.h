#pragma once

#include <array>
#include <cstdint>
#include <Windows.h>

#include "LGF/Input.h"

namespace LGF {

	class InputSystem {
	public:
		InputSystem();
		~InputSystem();

		bool Initialize(void* windowHandle);
		bool Finalize();

		void BeginFrame();
		void EndFrame();

		bool IsTriggered(KeyCode code)const;
		bool IsPressed(KeyCode code)const;
		bool IsReleased(KeyCode code)const;
		uint32_t GetPressedFrames(KeyCode code)const;

		bool IsTriggered(MouseButtonCode code)const;
		bool IsPressed(MouseButtonCode code)const;
		bool IsReleased(MouseButtonCode code)const;
		uint32_t GetPressedFrames(MouseButtonCode code)const;

		Vector2 GetMouseDelta()const;
		float GetMouseWheelDelta()const;
		Vector2 GetCursorPos()const;
		Vector2 GetCursorDelta()const;

		void HandleRawInput(void* rawInputHandle);

	private:
		bool RegisterRawInput(HWND hwnd);
		void UpdateCursorPos();
		void UpdatePressedFrames();
		void HandleKeyboardInput(const RAWKEYBOARD& keyboard);
		void HandleMouseInput(const RAWMOUSE& mouse);
		KeyCode ToKeyCode(USHORT virtualKey)const;
		size_t ToIndex(KeyCode code)const;
		size_t ToIndex(MouseButtonCode code)const;

	private:
		std::array<bool, static_cast<size_t>(KeyCode::Count)> currentKeys_{};
		std::array<bool, static_cast<size_t>(KeyCode::Count)> previousKeys_{};
		std::array<uint32_t, static_cast<size_t>(KeyCode::Count)> keyPressedFrames_{};

		std::array<bool, static_cast<size_t>(MouseButtonCode::Count)> currentMouseButtons_{};
		std::array<bool, static_cast<size_t>(MouseButtonCode::Count)> previousMouseButtons_{};
		std::array<uint32_t, static_cast<size_t>(MouseButtonCode::Count)> mousePressedFrames_{};

		Vector2 mouseDelta_{};
		float mouseWheelDelta_ = 0.0f;

		HWND hwnd_ = nullptr;
		Vector2 cursorPos_{};
		Vector2 previousCursorPos_{};
	};

}
