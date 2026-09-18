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
