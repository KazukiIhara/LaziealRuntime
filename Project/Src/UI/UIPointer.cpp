#include "LGF/UI/UIPointer.h"

#include <algorithm>

#include "LGF/Graphics/Rect2D.h"
#include "LGF/Input.h"
#include "LGF/System.h"
#include "LGF/Window.h"

using namespace LGF;

UIPointer::UIPointer(const UIPointerDesc& desc) :
	desc_(desc) {
	if (desc_.isMouseEnabled) {
		position_ = Cursor::Pos();
	} else {
		ResetToWindowCenter();
	}
	ClampToWindow();
}

void UIPointer::Update() {
	const Vector2 previousPosition = position_;
	bool isGamepadMoved = false;

	if (desc_.isGamepadEnabled) {
		const Vector2 stick = Gamepad{ desc_.gamepadIndex }.LeftStick();
		if (stick.x != 0.0f || stick.y != 0.0f) {
			const float deltaTime = static_cast<float>(System::DeltaTime());
			position_ += Vector2{ stick.x, -stick.y } *
				(desc_.speed * deltaTime);
			source_ = UIPointerSource::Gamepad;
			isGamepadMoved = true;
		}
	}

	if (desc_.isMouseEnabled) {
		const Vector2 mouseDelta = Cursor::Delta();
		if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
			position_ = Cursor::Pos();
			source_ = UIPointerSource::Mouse;
		} else if (!isGamepadMoved && source_ == UIPointerSource::Mouse) {
			position_ = Cursor::Pos();
		}
	}

	ClampToWindow();
	delta_ = position_ - previousPosition;
}

const Vector2& UIPointer::GetPosition() const {
	return position_;
}

const Vector2& UIPointer::GetDelta() const {
	return delta_;
}

UIPointerSource UIPointer::GetSource() const {
	return source_;
}

bool UIPointer::IsGamepad() const {
	return source_ == UIPointerSource::Gamepad;
}

void UIPointer::SetPosition(const Vector2& position) {
	position_ = position;
	delta_ = {};
	ClampToWindow();
}

void UIPointer::ResetToWindowCenter() {
	const Window::Size windowSize = Window::GetSize();
	position_ = {
		static_cast<float>(windowSize.width) * 0.5f,
		static_cast<float>(windowSize.height) * 0.5f,
	};
	delta_ = {};
}

void UIPointer::Draw(const UIPointerStyle& style) const {
	const Vector4& color = IsGamepad()
		? style.gamepadColor
		: style.mouseColor;
	Rect2D{ position_ - style.size * 0.5f, style.size }.Draw(color);
}

void UIPointer::ClampToWindow() {
	if (!desc_.clampToWindow) {
		return;
	}

	const Window::Size windowSize = Window::GetSize();
	position_.x = std::clamp(
		position_.x,
		0.0f,
		static_cast<float>(windowSize.width));
	position_.y = std::clamp(
		position_.y,
		0.0f,
		static_cast<float>(windowSize.height));
}
