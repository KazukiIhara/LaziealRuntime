#include "LGF/UI/UIWorld.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <utility>

#include "LGF/Collision/Collision3D.h"
#include "LGF/Graphics/Camera3D.h"
#include "LGF/Graphics/Font2D.h"
#include "LGF/Graphics/Font3D.h"
#include "LGF/Graphics/Line3D.h"
#include "LGF/Graphics/UIPanel3D.h"
#include "LGF/Input.h"
#include "LGF/Math/MathUtils.h"
#include "LGF/Window.h"

using namespace LGF;

namespace {

	Vector4 GetButtonColor(
		const UIButtonState& state,
		const UIButtonStyle& style) {
		if (!state.isEnabled) {
			return style.disabledColor;
		}
		if (state.isHeld) {
			return style.heldColor;
		}
		if (state.isHovered) {
			return style.hoveredColor;
		}
		if (state.isSelected) {
			return style.selectedColor;
		}
		return style.normalColor;
	}

}

UICursor3D LGF::MakeUICursor3D(
	const Camera3D& camera,
	const Vector2& screenPosition,
	float maxDistance) {
	const Vector3 farPoint = camera.ScreenToWorld(screenPosition, 1.0f);
	return {
		.ray = {
			.origin = camera.translation,
			.direction = farPoint - camera.translation,
		},
		.maxDistance = maxDistance,
	};
}

bool UIButtonHandle::IsValid() const {
	return index != InvalidIndex && generation != 0u;
}

UIButtonHandle UIWorld::AddButton(const UIButtonDesc& desc) {
	std::size_t index = 0u;
	for (; index < buttons_.size(); ++index) {
		if (!buttons_[index].isOccupied) {
			break;
		}
	}

	if (index == buttons_.size()) {
		buttons_.push_back({});
	}

	ButtonSlot& slot = buttons_[index];
	slot.space = ButtonSpace::Screen;
	slot.text = desc.text;
	slot.transform = desc.transform;
	slot.transform3D = {};
	slot.shortcutCommand = desc.shortcutCommand;
	slot.state = {
		.isVisible = desc.isVisible,
		.isEnabled = desc.isEnabled,
	};
	slot.isCursorEnabled = desc.isCursorEnabled;
	slot.isNavigationEnabled = desc.isNavigationEnabled;
	slot.isVisible = desc.isVisible;
	slot.isEnabled = desc.isEnabled;
	slot.isOccupied = true;

	const UIButtonHandle handle{ static_cast<uint32_t>(index), slot.generation };
	buttonOrder_.push_back(handle);
	if (!selectedButton_.IsValid() && desc.isNavigationEnabled &&
		desc.isVisible && desc.isEnabled) {
		selectedButton_ = handle;
	}
	return handle;
}

UIButtonHandle UIWorld::AddButton3D(const UIButtonDesc3D& desc) {
	std::size_t index = 0u;
	for (; index < buttons_.size(); ++index) {
		if (!buttons_[index].isOccupied) {
			break;
		}
	}

	if (index == buttons_.size()) {
		buttons_.push_back({});
	}

	ButtonSlot& slot = buttons_[index];
	slot.space = ButtonSpace::World;
	slot.text = desc.text;
	slot.transform = {};
	slot.transform3D = desc.transform;
	slot.shortcutCommand = desc.shortcutCommand;
	slot.state = {
		.isVisible = desc.isVisible,
		.isEnabled = desc.isEnabled,
	};
	slot.isCursorEnabled = desc.isCursorEnabled;
	slot.isNavigationEnabled = desc.isNavigationEnabled;
	slot.isVisible = desc.isVisible;
	slot.isEnabled = desc.isEnabled;
	slot.isOccupied = true;

	const UIButtonHandle handle{ static_cast<uint32_t>(index), slot.generation };
	buttonOrder_.push_back(handle);
	if (!selectedButton_.IsValid() && desc.isNavigationEnabled &&
		desc.isVisible && desc.isEnabled) {
		selectedButton_ = handle;
	}
	return handle;
}

bool UIWorld::Remove(UIButtonHandle handle) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}

	slot->text.clear();
	slot->transform = {};
	slot->transform3D = {};
	slot->shortcutCommand.clear();
	slot->state = {};
	slot->isCursorEnabled = true;
	slot->isNavigationEnabled = false;
	slot->isVisible = true;
	slot->isEnabled = true;
	slot->isOccupied = false;
	++slot->generation;
	if (slot->generation == 0u) {
		slot->generation = 1u;
	}

	std::erase(buttonOrder_, handle);
	InvalidateHandle(handle);
	return true;
}

void UIWorld::Clear() {
	for (ButtonSlot& slot : buttons_) {
		if (!slot.isOccupied) {
			continue;
		}
		slot.text.clear();
		slot.transform = {};
		slot.transform3D = {};
		slot.shortcutCommand.clear();
		slot.state = {};
		slot.isCursorEnabled = true;
		slot.isNavigationEnabled = false;
		slot.isVisible = true;
		slot.isEnabled = true;
		slot.isOccupied = false;
		++slot.generation;
		if (slot.generation == 0u) {
			slot.generation = 1u;
		}
	}

	buttonOrder_.clear();
	selectedButton_ = {};
	cursorCapturedButton_ = {};
	navigationCapturedButton_ = {};
	wasCursorPressed_ = false;
	wasNavigationPressed_ = false;
}

bool UIWorld::IsValid(UIButtonHandle handle) const {
	return FindSlot(handle) != nullptr;
}

std::size_t UIWorld::GetButtonCount() const {
	return buttonOrder_.size();
}

bool UIWorld::SetText(UIButtonHandle handle, std::string text) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->text = std::move(text);
	return true;
}

std::string UIWorld::GetText(UIButtonHandle handle) const {
	const ButtonSlot* slot = FindSlot(handle);
	return slot ? slot->text : std::string{};
}

bool UIWorld::SetTransform(
	UIButtonHandle handle,
	const UIButtonTransform& transform) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot || slot->space != ButtonSpace::Screen) {
		return false;
	}
	slot->transform = transform;
	return true;
}

bool UIWorld::SetTransform3D(
	UIButtonHandle handle,
	const UIButtonTransform3D& transform) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot || slot->space != ButtonSpace::World) {
		return false;
	}
	slot->transform3D = transform;
	return true;
}

bool UIWorld::SetVisible(UIButtonHandle handle, bool visible) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->isVisible = visible;
	slot->state.isVisible = visible;
	if (!visible) {
		InvalidateHandle(handle);
	}
	return true;
}

bool UIWorld::SetEnabled(UIButtonHandle handle, bool enabled) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->isEnabled = enabled;
	slot->state.isEnabled = enabled;
	if (!enabled) {
		InvalidateHandle(handle);
	}
	return true;
}

bool UIWorld::SetCursorEnabled(UIButtonHandle handle, bool enabled) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->isCursorEnabled = enabled;
	if (!enabled && cursorCapturedButton_ == handle) {
		cursorCapturedButton_ = {};
	}
	return true;
}

bool UIWorld::SetNavigationEnabled(UIButtonHandle handle, bool enabled) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->isNavigationEnabled = enabled;
	if (!enabled && selectedButton_ == handle) {
		selectedButton_ = {};
	}
	if (!enabled && navigationCapturedButton_ == handle) {
		navigationCapturedButton_ = {};
	}
	return true;
}

bool UIWorld::SetShortcutCommand(
	UIButtonHandle handle,
	std::string_view command) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->shortcutCommand = command;
	return true;
}

bool UIWorld::SetSelected(UIButtonHandle handle) {
	const ButtonSlot* slot = FindSlot(handle);
	if (!slot || !slot->isVisible || !slot->isEnabled ||
		!slot->isNavigationEnabled) {
		return false;
	}
	selectedButton_ = handle;
	return true;
}

UIButtonHandle UIWorld::GetSelected() const {
	return IsValid(selectedButton_) ? selectedButton_ : UIButtonHandle{};
}

std::optional<UIButtonState> UIWorld::GetButtonState(UIButtonHandle handle) const {
	const ButtonSlot* slot = FindSlot(handle);
	return slot ? std::optional<UIButtonState>{ slot->state } : std::nullopt;
}

UIButtonEvents UIWorld::GetEvents(UIButtonHandle handle) const {
	const ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return {};
	}
	return {
		.triggered = slot->state.isTriggered,
		.held = slot->state.isHeld,
		.released = slot->state.isReleased,
	};
}

bool UIWorld::Trigger(UIButtonHandle handle) const {
	return GetEvents(handle).triggered;
}

bool UIWorld::Press(UIButtonHandle handle) const {
	return GetEvents(handle).held;
}

bool UIWorld::Release(UIButtonHandle handle) const {
	return GetEvents(handle).released;
}

std::optional<Rect2D> UIWorld::GetButtonRect(UIButtonHandle handle) const {
	const ButtonSlot* slot = FindSlot(handle);
	return slot && slot->space == ButtonSpace::Screen
		? std::optional<Rect2D>{ ResolveRect(*slot) }
		: std::nullopt;
}

std::optional<Matrix4x4> UIWorld::GetButtonWorldMatrix(UIButtonHandle handle) const {
	const ButtonSlot* slot = FindSlot(handle);
	return slot && slot->space == ButtonSpace::World
		? std::optional<Matrix4x4>{ ResolveWorldMatrix(*slot) }
		: std::nullopt;
}

void UIWorld::Update(const UIWorldInput& input) {
	if (!SetSelected(selectedButton_)) {
		selectedButton_ = {};
	}
	if (input.selectionChange != UISelectionChange::None) {
		MoveSelection(input.selectionChange);
	}

	UIButtonHandle hoveredButton{};
	if (input.isCursorEnabled) {
		hoveredButton = FindHovered2D(input.cursorPosition);
		if (!hoveredButton.IsValid() && input.cursor3D) {
			hoveredButton = FindHovered3D(*input.cursor3D);
		}
	}

	if (input.isCursorPressed && !wasCursorPressed_) {
		cursorCapturedButton_ = hoveredButton;
	}
	if (input.isNavigationPressed && !wasNavigationPressed_) {
		navigationCapturedButton_ = selectedButton_;
	}

	for (const UIButtonHandle handle : buttonOrder_) {
		ButtonSlot* slot = FindSlot(handle);
		if (!slot) {
			continue;
		}

		UIButtonState& state = slot->state;
		const bool wasHeld = state.isHeld;
		state.isVisible = slot->isVisible;
		state.isEnabled = slot->isEnabled;
		state.isHovered = handle == hoveredButton;
		state.isSelected = handle == selectedButton_;

		const bool isInteractable = state.isVisible && state.isEnabled;
		const bool isCursorHeld = input.isCursorEnabled &&
			slot->isCursorEnabled && input.isCursorPressed &&
			handle == cursorCapturedButton_;
		const bool isNavigationHeld = slot->isNavigationEnabled &&
			input.isNavigationPressed &&
			handle == navigationCapturedButton_;
		const bool isShortcutHeld = isInteractable &&
			!slot->shortcutCommand.empty() &&
			Input::Press(slot->shortcutCommand);

		state.isHeld = isInteractable &&
			(isCursorHeld || isNavigationHeld || isShortcutHeld);
		state.isTriggered = state.isHeld && !wasHeld;
		state.isReleased = !state.isHeld && wasHeld;

	}

	wasCursorPressed_ = input.isCursorPressed;
	wasNavigationPressed_ = input.isNavigationPressed;
	if (!input.isCursorPressed) {
		cursorCapturedButton_ = {};
	}
	if (!input.isNavigationPressed) {
		navigationCapturedButton_ = {};
	}
}

void UIWorld::Draw(const Font2D& font, const UIButtonStyle& style) const {
	for (const UIButtonHandle handle : buttonOrder_) {
		Draw(handle, font, style);
	}
}

bool UIWorld::Draw(
	UIButtonHandle handle,
	const Font2D& font,
	const UIButtonStyle& style) const {
	const ButtonSlot* slot = FindSlot(handle);
	if (!slot || slot->space != ButtonSpace::Screen) {
		return false;
	}
	if (!slot->state.isVisible) {
		return true;
	}

	const Rect2D rect = ResolveRect(*slot);
	rect.Draw(GetButtonColor(slot->state, style));
	font.Draw(
		slot->text,
		rect.leftTop + rect.size * 0.5f,
		{
			.color = style.textColor,
			.anchor = { 0.5f, 0.5f },
		});
	return true;
}

void UIWorld::Draw3D(
	const UIPanel3D& panel,
	const Font3D& font,
	const UIButtonStyle& style) const {
	for (const UIButtonHandle handle : buttonOrder_) {
		Draw3D(handle, panel, font, style);
	}
}

bool UIWorld::Draw3D(
	UIButtonHandle handle,
	const UIPanel3D& panel,
	const Font3D& font,
	const UIButtonStyle& style) const {
	const ButtonSlot* slot = FindSlot(handle);
	if (!slot || slot->space != ButtonSpace::World) {
		return false;
	}
	if (!slot->state.isVisible) {
		return true;
	}

	const Matrix4x4 worldMatrix = ResolveWorldMatrix(*slot);
	if (slot->state.isSelected || slot->state.isHovered || slot->state.isHeld) {
		const Matrix4x4 outlineMatrix = Math::MakeAffineMatrix(
			{ style.outlineScale.x, style.outlineScale.y, 1.0f },
			{},
			{ 0.0f, 0.0f, 0.01f }) * worldMatrix;
		panel.Draw(outlineMatrix, style.outlineColor);
	}
	panel.Draw(worldMatrix, GetButtonColor(slot->state, style));
	font.Draw(
		slot->text,
		Math::Transform({ 0.0f, 0.0f, -0.01f }, worldMatrix),
		{
			.color = style.textColor,
			.anchor = { 0.5f, 0.5f },
			.worldUnitsPerPixel = style.worldUnitsPerPixel,
			.mode = Font3DMode::World,
		});
	return true;
}

void UIWorld::DrawHitboxes(const Vector4& color) const {
	for (const UIButtonHandle handle : buttonOrder_) {
		const ButtonSlot* slot = FindSlot(handle);
		if (!slot || slot->space != ButtonSpace::Screen || !slot->isVisible) {
			continue;
		}
		ResolveRect(*slot).Draw(color);
	}
}

void UIWorld::DrawHitboxes3D(const Vector4& color) const {
	constexpr std::array<Vector3, 4> localCorners{
		Vector3{ -0.5f, -0.5f, 0.0f },
		Vector3{ 0.5f, -0.5f, 0.0f },
		Vector3{ 0.5f, 0.5f, 0.0f },
		Vector3{ -0.5f, 0.5f, 0.0f },
	};

	for (const UIButtonHandle handle : buttonOrder_) {
		const ButtonSlot* slot = FindSlot(handle);
		if (!slot || slot->space != ButtonSpace::World || !slot->isVisible) {
			continue;
		}

		const Matrix4x4 worldMatrix = ResolveWorldMatrix(*slot);
		std::array<Vector3, 4> corners{};
		for (std::size_t index = 0u; index < corners.size(); ++index) {
			corners[index] = Math::Transform(localCorners[index], worldMatrix);
		}
		for (std::size_t index = 0u; index < corners.size(); ++index) {
			Line3D(corners[index], corners[(index + 1u) % corners.size()])
				.Draw(Math::Identity4x4(), color);
		}
	}
}

UIWorld::ButtonSlot* UIWorld::FindSlot(UIButtonHandle handle) {
	if (!handle.IsValid() ||
		static_cast<std::size_t>(handle.index) >= buttons_.size()) {
		return nullptr;
	}
	ButtonSlot& slot = buttons_[handle.index];
	return slot.isOccupied && slot.generation == handle.generation
		? &slot
		: nullptr;
}

const UIWorld::ButtonSlot* UIWorld::FindSlot(UIButtonHandle handle) const {
	if (!handle.IsValid() ||
		static_cast<std::size_t>(handle.index) >= buttons_.size()) {
		return nullptr;
	}
	const ButtonSlot& slot = buttons_[handle.index];
	return slot.isOccupied && slot.generation == handle.generation
		? &slot
		: nullptr;
}

Rect2D UIWorld::ResolveRect(const ButtonSlot& slot) const {
	const Window::Size windowSize = Window::GetSize();
	const UIButtonTransform& transform = slot.transform;
	const Vector2 windowSizeVector{
		static_cast<float>(windowSize.width),
		static_cast<float>(windowSize.height),
	};
	const Vector2 leftTop = windowSizeVector * transform.anchor +
		transform.position - transform.size * transform.pivot;
	return Rect2D{ leftTop, transform.size };
}

Matrix4x4 UIWorld::ResolveWorldMatrix(const ButtonSlot& slot) const {
	return Math::MakeAffineMatrix(
		{ slot.transform3D.size.x, slot.transform3D.size.y, 1.0f },
		slot.transform3D.rotation,
		slot.transform3D.position);
}

UIButtonHandle UIWorld::FindHovered2D(const Vector2& cursorPosition) const {
	for (auto iterator = buttonOrder_.rbegin();
		iterator != buttonOrder_.rend(); ++iterator) {
		const ButtonSlot* slot = FindSlot(*iterator);
		if (!slot || slot->space != ButtonSpace::Screen ||
			!slot->isVisible || !slot->isEnabled || !slot->isCursorEnabled) {
			continue;
		}

		const Rect2D rect = ResolveRect(*slot);
		const Vector2 rightBottom = rect.leftTop + rect.size;
		if (cursorPosition.x >= rect.leftTop.x &&
			cursorPosition.y >= rect.leftTop.y &&
			cursorPosition.x <= rightBottom.x &&
			cursorPosition.y <= rightBottom.y) {
			return *iterator;
		}
	}
	return {};
}

UIButtonHandle UIWorld::FindHovered3D(const UICursor3D& cursor) const {
	if (!Collision::IsValid(cursor.ray) || cursor.maxDistance < 0.0f) {
		return {};
	}

	UIButtonHandle closestButton{};
	float closestDistance = cursor.maxDistance;
	for (const UIButtonHandle handle : buttonOrder_) {
		const ButtonSlot* slot = FindSlot(handle);
		if (!slot || slot->space != ButtonSpace::World ||
			!slot->isVisible || !slot->isEnabled || !slot->isCursorEnabled) {
			continue;
		}
		if (std::abs(slot->transform3D.size.x) <= Math::EPSILON ||
			std::abs(slot->transform3D.size.y) <= Math::EPSILON) {
			continue;
		}

		const Matrix4x4 worldMatrix = ResolveWorldMatrix(*slot);
		const Vector3 normal = Math::TransformNormal(
			{ 0.0f, 0.0f, 1.0f },
			worldMatrix);
		const Collision::Plane plane{
			.normal = normal,
			.distance = -Math::Dot(normal, slot->transform3D.position),
		};
		const std::optional<Collision::RaycastHit> hit = Collision::Raycast(
			cursor.ray,
			plane,
			closestDistance);
		if (!hit) {
			continue;
		}

		const Vector3 localPoint = Math::Transform(
			hit->point,
			Math::Inverse(worldMatrix));
		if (std::abs(localPoint.x) <= 0.5f &&
			std::abs(localPoint.y) <= 0.5f) {
			closestButton = handle;
			closestDistance = hit->distance;
		}
	}
	return closestButton;
}

void UIWorld::MoveSelection(UISelectionChange change) {
	const bool moveNext = change == UISelectionChange::Next;
	std::vector<UIButtonHandle> candidates;
	for (const UIButtonHandle handle : buttonOrder_) {
		const ButtonSlot* slot = FindSlot(handle);
		if (slot && slot->isVisible && slot->isEnabled &&
			slot->isNavigationEnabled) {
			candidates.push_back(handle);
		}
	}
	if (candidates.empty()) {
		selectedButton_ = {};
		return;
	}

	const auto selectedIterator = std::ranges::find(candidates, selectedButton_);
	if (selectedIterator == candidates.end()) {
		selectedButton_ = moveNext ? candidates.front() : candidates.back();
		return;
	}

	const std::size_t currentIndex = static_cast<std::size_t>(
		std::distance(candidates.begin(), selectedIterator));
	selectedButton_ = moveNext
		? candidates[(currentIndex + 1u) % candidates.size()]
		: candidates[(currentIndex + candidates.size() - 1u) % candidates.size()];
}

void UIWorld::InvalidateHandle(UIButtonHandle handle) {
	if (selectedButton_ == handle) {
		selectedButton_ = {};
	}
	if (cursorCapturedButton_ == handle) {
		cursorCapturedButton_ = {};
	}
	if (navigationCapturedButton_ == handle) {
		navigationCapturedButton_ = {};
	}
}
