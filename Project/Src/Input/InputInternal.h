#pragma once

#include "LGF/Input.h"

namespace LGF::Input::Internal {

	bool IsTriggered(KeyCode code);
	bool IsPressed(KeyCode code);
	bool IsReleased(KeyCode code);
	uint32_t GetPressedFrames(KeyCode code);

	bool IsTriggered(MouseButtonCode code);
	bool IsPressed(MouseButtonCode code);
	bool IsReleased(MouseButtonCode code);
	uint32_t GetPressedFrames(MouseButtonCode code);

	Vector2 GetMouseDelta();
	float GetMouseWheelDelta();
	Vector2 GetCursorPos();
	Vector2 GetCursorDelta();

	bool IsGamepadConnected(uint32_t gamepadIndex);
	GamepadType GetGamepadType(uint32_t gamepadIndex);
	std::string GetGamepadName(uint32_t gamepadIndex);
	Vector2 GetGamepadLeftStick(uint32_t gamepadIndex);
	Vector2 GetGamepadRightStick(uint32_t gamepadIndex);
	float GetGamepadLeftTrigger(uint32_t gamepadIndex);
	float GetGamepadRightTrigger(uint32_t gamepadIndex);

	bool IsTriggered(uint32_t gamepadIndex, GamepadButtonCode code);
	bool IsPressed(uint32_t gamepadIndex, GamepadButtonCode code);
	bool IsReleased(uint32_t gamepadIndex, GamepadButtonCode code);
	uint32_t GetPressedFrames(uint32_t gamepadIndex, GamepadButtonCode code);

	bool HasGamepadGyroscope(uint32_t gamepadIndex, GamepadSensorType sensor);
	bool HasGamepadAccelerometer(uint32_t gamepadIndex, GamepadSensorType sensor);
	Vector3 GetGamepadGyroscope(uint32_t gamepadIndex, GamepadSensorType sensor);
	Vector3 GetGamepadAcceleration(uint32_t gamepadIndex, GamepadSensorType sensor);
	Vector3 GetGamepadRotationDelta(uint32_t gamepadIndex, GamepadSensorType sensor);

}
