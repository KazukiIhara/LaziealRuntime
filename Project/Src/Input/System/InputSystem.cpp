#include "InputSystem.h"

#include <vector>

#include "Input/InputInternal.h"

using namespace LGF;

namespace {
	InputSystem* g_inputSystem = nullptr;

	void SetMouseButton(std::array<bool, static_cast<size_t>(MouseButtonCode::Count)>& buttons, MouseButtonCode code, bool isPressed) {
		buttons[static_cast<size_t>(code)] = isPressed;
	}
}

InputSystem::InputSystem() {
}

InputSystem::~InputSystem() = default;

bool InputSystem::Initialize(void* windowHandle) {
	g_inputSystem = this;

	hwnd_ = static_cast<HWND>(windowHandle);
	if (hwnd_ == nullptr) {
		return false;
	}
	UpdateCursorPos();
	previousCursorPos_ = cursorPos_;

	return RegisterRawInput(hwnd_);
}

bool InputSystem::Finalize() {
	g_inputSystem = nullptr;
	hwnd_ = nullptr;

	return true;
}

void InputSystem::BeginFrame() {
	UpdateCursorPos();
	UpdatePressedFrames();
}

void InputSystem::EndFrame() {
	previousKeys_ = currentKeys_;
	previousMouseButtons_ = currentMouseButtons_;
	previousCursorPos_ = cursorPos_;
	mouseDelta_ = {};
	mouseWheelDelta_ = 0.0f;
}

bool InputSystem::IsTriggered(KeyCode code) const {
	const size_t index = ToIndex(code);
	return currentKeys_[index] && !previousKeys_[index];
}

bool InputSystem::IsPressed(KeyCode code) const {
	return currentKeys_[ToIndex(code)];
}

bool InputSystem::IsReleased(KeyCode code) const {
	const size_t index = ToIndex(code);
	return !currentKeys_[index] && previousKeys_[index];
}

uint32_t InputSystem::GetPressedFrames(KeyCode code) const {
	return keyPressedFrames_[ToIndex(code)];
}

bool InputSystem::IsTriggered(MouseButtonCode code) const {
	const size_t index = ToIndex(code);
	return currentMouseButtons_[index] && !previousMouseButtons_[index];
}

bool InputSystem::IsPressed(MouseButtonCode code) const {
	return currentMouseButtons_[ToIndex(code)];
}

bool InputSystem::IsReleased(MouseButtonCode code) const {
	const size_t index = ToIndex(code);
	return !currentMouseButtons_[index] && previousMouseButtons_[index];
}

uint32_t InputSystem::GetPressedFrames(MouseButtonCode code) const {
	return mousePressedFrames_[ToIndex(code)];
}

Vector2 InputSystem::GetMouseDelta() const {
	return mouseDelta_;
}

float InputSystem::GetMouseWheelDelta() const {
	return mouseWheelDelta_;
}

Vector2 InputSystem::GetCursorPos() const {
	return cursorPos_;
}

Vector2 InputSystem::GetCursorDelta() const {
	return cursorPos_ - previousCursorPos_;
}

void InputSystem::HandleRawInput(void* rawInputHandle) {
	const HRAWINPUT handle = static_cast<HRAWINPUT>(rawInputHandle);
	UINT dataSize = 0;
	GetRawInputData(handle, RID_INPUT, nullptr, &dataSize, sizeof(RAWINPUTHEADER));

	if (dataSize == 0) {
		return;
	}

	std::vector<uint8_t> buffer(dataSize);
	const UINT readSize = GetRawInputData(handle, RID_INPUT, buffer.data(), &dataSize, sizeof(RAWINPUTHEADER));

	if (readSize != dataSize) {
		return;
	}

	const RAWINPUT* rawInput = reinterpret_cast<const RAWINPUT*>(buffer.data());

	if (rawInput->header.dwType == RIM_TYPEKEYBOARD) {
		HandleKeyboardInput(rawInput->data.keyboard);
	} else if (rawInput->header.dwType == RIM_TYPEMOUSE) {
		HandleMouseInput(rawInput->data.mouse);
	}
}

bool InputSystem::RegisterRawInput(HWND hwnd) {
	RAWINPUTDEVICE devices[2]{};

	devices[0].usUsagePage = 0x01;
	devices[0].usUsage = 0x06;
	devices[0].dwFlags = 0;
	devices[0].hwndTarget = hwnd;

	devices[1].usUsagePage = 0x01;
	devices[1].usUsage = 0x02;
	devices[1].dwFlags = 0;
	devices[1].hwndTarget = hwnd;

	return RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE)) != FALSE;
}

void InputSystem::UpdateCursorPos() {
	if (hwnd_ == nullptr) {
		cursorPos_ = {};
		return;
	}

	POINT point{};
	if (!::GetCursorPos(&point)) {
		return;
	}

	ScreenToClient(hwnd_, &point);

	cursorPos_ = {
		static_cast<float>(point.x),
		static_cast<float>(point.y)
	};
}

void InputSystem::UpdatePressedFrames() {
	for (size_t i = 0; i < currentKeys_.size(); ++i) {
		if (currentKeys_[i]) {
			++keyPressedFrames_[i];
		} else {
			keyPressedFrames_[i] = 0;
		}
	}

	for (size_t i = 0; i < currentMouseButtons_.size(); ++i) {
		if (currentMouseButtons_[i]) {
			++mousePressedFrames_[i];
		} else {
			mousePressedFrames_[i] = 0;
		}
	}
}

void InputSystem::HandleKeyboardInput(const RAWKEYBOARD& keyboard) {
	if (keyboard.VKey == 0xFF) {
		return;
	}

	const KeyCode keyCode = ToKeyCode(keyboard.VKey);
	if (keyCode == KeyCode::Count) {
		return;
	}

	currentKeys_[ToIndex(keyCode)] = (keyboard.Flags & RI_KEY_BREAK) == 0;
}

void InputSystem::HandleMouseInput(const RAWMOUSE& mouse) {
	if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
		mouseDelta_.x += static_cast<float>(mouse.lLastX);
		mouseDelta_.y += static_cast<float>(mouse.lLastY);
	}

	const USHORT buttonFlags = mouse.usButtonFlags;

	if ((buttonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0) {
		SetMouseButton(currentMouseButtons_, MouseButtonCode::Left, true);
	}
	if ((buttonFlags & RI_MOUSE_LEFT_BUTTON_UP) != 0) {
		SetMouseButton(currentMouseButtons_, MouseButtonCode::Left, false);
	}
	if ((buttonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) != 0) {
		SetMouseButton(currentMouseButtons_, MouseButtonCode::Right, true);
	}
	if ((buttonFlags & RI_MOUSE_RIGHT_BUTTON_UP) != 0) {
		SetMouseButton(currentMouseButtons_, MouseButtonCode::Right, false);
	}
	if ((buttonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) != 0) {
		SetMouseButton(currentMouseButtons_, MouseButtonCode::Middle, true);
	}
	if ((buttonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) != 0) {
		SetMouseButton(currentMouseButtons_, MouseButtonCode::Middle, false);
	}
	if ((buttonFlags & RI_MOUSE_BUTTON_4_DOWN) != 0) {
		SetMouseButton(currentMouseButtons_, MouseButtonCode::X1, true);
	}
	if ((buttonFlags & RI_MOUSE_BUTTON_4_UP) != 0) {
		SetMouseButton(currentMouseButtons_, MouseButtonCode::X1, false);
	}
	if ((buttonFlags & RI_MOUSE_BUTTON_5_DOWN) != 0) {
		SetMouseButton(currentMouseButtons_, MouseButtonCode::X2, true);
	}
	if ((buttonFlags & RI_MOUSE_BUTTON_5_UP) != 0) {
		SetMouseButton(currentMouseButtons_, MouseButtonCode::X2, false);
	}
	if ((buttonFlags & RI_MOUSE_WHEEL) != 0) {
		mouseWheelDelta_ += static_cast<float>(static_cast<SHORT>(mouse.usButtonData)) / static_cast<float>(WHEEL_DELTA);
	}
}

KeyCode InputSystem::ToKeyCode(USHORT virtualKey) const {
	switch (virtualKey) {
		case VK_SPACE:
			return KeyCode::Space;
		case VK_ESCAPE:
			return KeyCode::Escape;
		case VK_RETURN:
			return KeyCode::Enter;
		case VK_TAB:
			return KeyCode::Tab;
		case VK_BACK:
			return KeyCode::Backspace;
		case VK_LEFT:
			return KeyCode::Left;
		case VK_RIGHT:
			return KeyCode::Right;
		case VK_UP:
			return KeyCode::Up;
		case VK_DOWN:
			return KeyCode::Down;
		case 'A':
			return KeyCode::A;
		case 'B':
			return KeyCode::B;
		case 'C':
			return KeyCode::C;
		case 'D':
			return KeyCode::D;
		case 'E':
			return KeyCode::E;
		case 'F':
			return KeyCode::F;
		case 'G':
			return KeyCode::G;
		case 'H':
			return KeyCode::H;
		case 'I':
			return KeyCode::I;
		case 'J':
			return KeyCode::J;
		case 'K':
			return KeyCode::K;
		case 'L':
			return KeyCode::L;
		case 'M':
			return KeyCode::M;
		case 'N':
			return KeyCode::N;
		case 'O':
			return KeyCode::O;
		case 'P':
			return KeyCode::P;
		case 'Q':
			return KeyCode::Q;
		case 'R':
			return KeyCode::R;
		case 'S':
			return KeyCode::S;
		case 'T':
			return KeyCode::T;
		case 'U':
			return KeyCode::U;
		case 'V':
			return KeyCode::V;
		case 'W':
			return KeyCode::W;
		case 'X':
			return KeyCode::X;
		case 'Y':
			return KeyCode::Y;
		case 'Z':
			return KeyCode::Z;
		case '0':
			return KeyCode::Num0;
		case '1':
			return KeyCode::Num1;
		case '2':
			return KeyCode::Num2;
		case '3':
			return KeyCode::Num3;
		case '4':
			return KeyCode::Num4;
		case '5':
			return KeyCode::Num5;
		case '6':
			return KeyCode::Num6;
		case '7':
			return KeyCode::Num7;
		case '8':
			return KeyCode::Num8;
		case '9':
			return KeyCode::Num9;
		default:
			return KeyCode::Count;
	}
}

size_t InputSystem::ToIndex(KeyCode code) const {
	return static_cast<size_t>(code);
}

size_t InputSystem::ToIndex(MouseButtonCode code) const {
	return static_cast<size_t>(code);
}

bool LGF::Input::Internal::IsTriggered(KeyCode code) {
	return g_inputSystem != nullptr && g_inputSystem->IsTriggered(code);
}

bool LGF::Input::Internal::IsPressed(KeyCode code) {
	return g_inputSystem != nullptr && g_inputSystem->IsPressed(code);
}

bool LGF::Input::Internal::IsReleased(KeyCode code) {
	return g_inputSystem != nullptr && g_inputSystem->IsReleased(code);
}

uint32_t LGF::Input::Internal::GetPressedFrames(KeyCode code) {
	if (g_inputSystem == nullptr) {
		return 0;
	}

	return g_inputSystem->GetPressedFrames(code);
}

bool LGF::Input::Internal::IsTriggered(MouseButtonCode code) {
	return g_inputSystem != nullptr && g_inputSystem->IsTriggered(code);
}

bool LGF::Input::Internal::IsPressed(MouseButtonCode code) {
	return g_inputSystem != nullptr && g_inputSystem->IsPressed(code);
}

bool LGF::Input::Internal::IsReleased(MouseButtonCode code) {
	return g_inputSystem != nullptr && g_inputSystem->IsReleased(code);
}

uint32_t LGF::Input::Internal::GetPressedFrames(MouseButtonCode code) {
	if (g_inputSystem == nullptr) {
		return 0;
	}

	return g_inputSystem->GetPressedFrames(code);
}

Vector2 LGF::Input::Internal::GetMouseDelta() {
	if (g_inputSystem == nullptr) {
		return {};
	}

	return g_inputSystem->GetMouseDelta();
}

float LGF::Input::Internal::GetMouseWheelDelta() {
	if (g_inputSystem == nullptr) {
		return 0.0f;
	}

	return g_inputSystem->GetMouseWheelDelta();
}

Vector2 LGF::Input::Internal::GetCursorPos() {
	if (g_inputSystem == nullptr) {
		return {};
	}

	return g_inputSystem->GetCursorPos();
}

Vector2 LGF::Input::Internal::GetCursorDelta() {
	if (g_inputSystem == nullptr) {
		return {};
	}

	return g_inputSystem->GetCursorDelta();
}
