#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <Windows.h>

#include "LGF/Input.h"

namespace LGF {

	struct InputConfig;
	class GamepadSystem;

	class InputSystem {
	public:
		InputSystem();
		~InputSystem();

		bool Initialize(void* windowHandle, const InputConfig& config);
		bool Finalize();

		void BeginFrame();
		void EndFrame();

		bool IsTriggered(KeyCode code) const;
		bool IsPressed(KeyCode code) const;
		bool IsReleased(KeyCode code) const;
		uint32_t GetPressedFrames(KeyCode code) const;

		bool IsTriggered(MouseButtonCode code) const;
		bool IsPressed(MouseButtonCode code) const;
		bool IsReleased(MouseButtonCode code) const;
		uint32_t GetPressedFrames(MouseButtonCode code) const;

		Vector2 GetMouseDelta() const;
		float GetMouseWheelDelta() const;
		Vector2 GetCursorPos() const;
		Vector2 GetCursorDelta() const;

		bool IsGamepadConnected(uint32_t gamepadIndex) const;
		GamepadType GetGamepadType(uint32_t gamepadIndex) const;
		std::string GetGamepadName(uint32_t gamepadIndex) const;
		Vector2 GetGamepadLeftStick(uint32_t gamepadIndex) const;
		Vector2 GetGamepadRightStick(uint32_t gamepadIndex) const;
		float GetGamepadLeftTrigger(uint32_t gamepadIndex) const;
		float GetGamepadRightTrigger(uint32_t gamepadIndex) const;

		bool IsTriggered(uint32_t gamepadIndex, GamepadButtonCode code) const;
		bool IsPressed(uint32_t gamepadIndex, GamepadButtonCode code) const;
		bool IsReleased(uint32_t gamepadIndex, GamepadButtonCode code) const;
		uint32_t GetPressedFrames(uint32_t gamepadIndex, GamepadButtonCode code) const;

		bool HasGamepadGyroscope(uint32_t gamepadIndex, GamepadSensorType sensor) const;
		bool HasGamepadAccelerometer(uint32_t gamepadIndex, GamepadSensorType sensor) const;
		Vector3 GetGamepadGyroscope(uint32_t gamepadIndex, GamepadSensorType sensor) const;
		Vector3 GetGamepadAcceleration(uint32_t gamepadIndex, GamepadSensorType sensor) const;
		Vector3 GetGamepadRotationDelta(uint32_t gamepadIndex, GamepadSensorType sensor) const;

		void HandleRawInput(void* rawInputHandle);

	private:
		bool RegisterRawInput(HWND hwnd);
		void UpdateCursorPos();
		void UpdatePressedFrames();
		void HandleKeyboardInput(const RAWKEYBOARD& keyboard);
		void HandleMouseInput(const RAWMOUSE& mouse);
		KeyCode ToKeyCode(USHORT virtualKey) const;
		size_t ToIndex(KeyCode code) const;
		size_t ToIndex(MouseButtonCode code) const;

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

		std::unique_ptr<GamepadSystem> gamepadSystem_;
	};

}
