#include "LGF/Input.h"

#include "Input/InputInternal.h"

using namespace LGF;

bool KeyButton::Trigger() const {
	return Input::Internal::IsTriggered(code_);
}

bool KeyButton::Press() const {
	return Input::Internal::IsPressed(code_);
}

bool KeyButton::Release() const {
	return Input::Internal::IsReleased(code_);
}

uint32_t KeyButton::PressedFrames() const {
	return Input::Internal::GetPressedFrames(code_);
}

bool MouseButton::Trigger() const {
	return Input::Internal::IsTriggered(code_);
}

bool MouseButton::Press() const {
	return Input::Internal::IsPressed(code_);
}

bool MouseButton::Release() const {
	return Input::Internal::IsReleased(code_);
}

uint32_t MouseButton::PressedFrames() const {
	return Input::Internal::GetPressedFrames(code_);
}

bool GamepadButton::Trigger() const {
	return Input::Internal::IsTriggered(gamepadIndex_, code_);
}

bool GamepadButton::Press() const {
	return Input::Internal::IsPressed(gamepadIndex_, code_);
}

bool GamepadButton::Release() const {
	return Input::Internal::IsReleased(gamepadIndex_, code_);
}

uint32_t GamepadButton::PressedFrames() const {
	return Input::Internal::GetPressedFrames(gamepadIndex_, code_);
}

bool Gamepad::IsConnected() const {
	return Input::Internal::IsGamepadConnected(index_);
}

GamepadType Gamepad::Type() const {
	return Input::Internal::GetGamepadType(index_);
}

std::string Gamepad::Name() const {
	return Input::Internal::GetGamepadName(index_);
}

Vector2 Gamepad::LeftStick() const {
	return Input::Internal::GetGamepadLeftStick(index_);
}

Vector2 Gamepad::RightStick() const {
	return Input::Internal::GetGamepadRightStick(index_);
}

float Gamepad::LeftTrigger() const {
	return Input::Internal::GetGamepadLeftTrigger(index_);
}

float Gamepad::RightTrigger() const {
	return Input::Internal::GetGamepadRightTrigger(index_);
}

GamepadButton Gamepad::Button(GamepadButtonCode code) const {
	return GamepadButton{ index_, code };
}

bool Gamepad::HasGyroscope(GamepadSensorType sensor) const {
	return Input::Internal::HasGamepadGyroscope(index_, sensor);
}

bool Gamepad::HasAccelerometer(GamepadSensorType sensor) const {
	return Input::Internal::HasGamepadAccelerometer(index_, sensor);
}

Vector3 Gamepad::Gyroscope(GamepadSensorType sensor) const {
	return Input::Internal::GetGamepadGyroscope(index_, sensor);
}

Vector3 Gamepad::Acceleration(GamepadSensorType sensor) const {
	return Input::Internal::GetGamepadAcceleration(index_, sensor);
}

Vector3 Gamepad::RotationDelta(GamepadSensorType sensor) const {
	return Input::Internal::GetGamepadRotationDelta(index_, sensor);
}

namespace {

	std::optional<Gamepad> FindJoyCon(GamepadType type, uint32_t playerIndex) {
		uint32_t matchingIndex = 0u;
		for (uint32_t index = 0u; index < Gamepad::MaxCount; ++index) {
			const Gamepad gamepad{ index };
			if (!gamepad.IsConnected() || gamepad.Type() != type) {
				continue;
			}
			if (matchingIndex == playerIndex) {
				return gamepad;
			}
			++matchingIndex;
		}
		return std::nullopt;
	}

}

std::optional<Gamepad> JoyCon::Left(uint32_t playerIndex) {
	return FindJoyCon(GamepadType::JoyConLeft, playerIndex);
}

std::optional<Gamepad> JoyCon::Right(uint32_t playerIndex) {
	return FindJoyCon(GamepadType::JoyConRight, playerIndex);
}

Vector2 Mouse::Delta() {
	return Input::Internal::GetMouseDelta();
}

float Mouse::WheelDelta() {
	return Input::Internal::GetMouseWheelDelta();
}

Vector2 Cursor::Pos() {
	return Input::Internal::GetCursorPos();
}

Vector2 Cursor::Delta() {
	return Input::Internal::GetCursorDelta();
}
