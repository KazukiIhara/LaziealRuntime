#include "LGF/UI/UIWorld.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "LGF/Window.h"

using namespace LGF;

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
	slot.desc = desc;
	slot.state = {
		.isVisible = desc.isVisible,
		.isEnabled = desc.isEnabled,
	};
	slot.isOccupied = true;

	const UIButtonHandle handle{ static_cast<uint32_t>(index), slot.generation };
	buttonOrder_.push_back(handle);
	if (!selectedButton_.IsValid() && desc.isNavigationEnabled &&
		desc.isVisible && desc.isEnabled) {
		selectedButton_ = handle;
	}
	return handle;
}

bool UIWorld::RemoveButton(UIButtonHandle handle) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}

	slot->desc = {};
	slot->state = {};
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
		slot.desc = {};
		slot.state = {};
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

bool UIWorld::SetTransform(
	UIButtonHandle handle,
	const UIButtonTransform& transform) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->desc.transform = transform;
	return true;
}

bool UIWorld::SetVisible(UIButtonHandle handle, bool visible) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->desc.isVisible = visible;
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
	slot->desc.isEnabled = enabled;
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
	slot->desc.isCursorEnabled = enabled;
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
	slot->desc.isNavigationEnabled = enabled;
	if (!enabled && selectedButton_ == handle) {
		selectedButton_ = {};
	}
	if (!enabled && navigationCapturedButton_ == handle) {
		navigationCapturedButton_ = {};
	}
	return true;
}

bool UIWorld::SetDirectInput(
	UIButtonHandle handle,
	std::function<bool()> isPressed) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->desc.isDirectPressed = std::move(isPressed);
	return true;
}

bool UIWorld::SetCallbacks(
	UIButtonHandle handle,
	const UIButtonCallbacks& callbacks) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->desc.callbacks = callbacks;
	return true;
}

bool UIWorld::SetDrawCallback(
	UIButtonHandle handle,
	std::function<void(const UIButtonDrawContext&)> draw) {
	ButtonSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->desc.draw = std::move(draw);
	return true;
}

bool UIWorld::SetSelected(UIButtonHandle handle) {
	const ButtonSlot* slot = FindSlot(handle);
	if (!slot || !slot->desc.isVisible || !slot->desc.isEnabled ||
		!slot->desc.isNavigationEnabled) {
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

std::optional<Rect2D> UIWorld::GetButtonRect(UIButtonHandle handle) const {
	const ButtonSlot* slot = FindSlot(handle);
	return slot ? std::optional<Rect2D>{ ResolveRect(*slot) } : std::nullopt;
}

void UIWorld::Update(const UIWorldInput& input) {
	if (!SetSelected(selectedButton_)) {
		selectedButton_ = {};
	}
	if (input.navigationOffset != 0) {
		MoveSelection(input.navigationOffset);
	}

	const UIButtonHandle hoveredButton = input.isCursorEnabled
		? FindHovered(input.cursorPosition)
		: UIButtonHandle{};

	if (input.isCursorPressed && !wasCursorPressed_) {
		cursorCapturedButton_ = hoveredButton;
	}
	if (input.isNavigationPressed && !wasNavigationPressed_) {
		navigationCapturedButton_ = selectedButton_;
	}

	std::vector<PendingCallback> pendingCallbacks;
	for (const UIButtonHandle handle : buttonOrder_) {
		ButtonSlot* slot = FindSlot(handle);
		if (!slot) {
			continue;
		}

		UIButtonState& state = slot->state;
		const bool wasHeld = state.isHeld;
		state.isVisible = slot->desc.isVisible;
		state.isEnabled = slot->desc.isEnabled;
		state.isHovered = handle == hoveredButton;
		state.isSelected = handle == selectedButton_;

		const bool isInteractable = state.isVisible && state.isEnabled;
		const bool isCursorHeld = input.isCursorEnabled &&
			slot->desc.isCursorEnabled && input.isCursorPressed &&
			handle == cursorCapturedButton_;
		const bool isNavigationHeld = slot->desc.isNavigationEnabled &&
			input.isNavigationPressed &&
			handle == navigationCapturedButton_;
		const bool isDirectHeld = isInteractable &&
			slot->desc.isDirectPressed &&
			slot->desc.isDirectPressed();

		state.isHeld = isInteractable &&
			(isCursorHeld || isNavigationHeld || isDirectHeld);
		state.isTriggered = state.isHeld && !wasHeld;
		state.isReleased = !state.isHeld && wasHeld;

		if (state.isTriggered) {
			pendingCallbacks.push_back({ handle, CallbackType::Trigger });
		}
		if (state.isHeld) {
			pendingCallbacks.push_back({ handle, CallbackType::Hold });
		}
		if (state.isReleased) {
			pendingCallbacks.push_back({ handle, CallbackType::Release });
		}
	}

	wasCursorPressed_ = input.isCursorPressed;
	wasNavigationPressed_ = input.isNavigationPressed;
	if (!input.isCursorPressed) {
		cursorCapturedButton_ = {};
	}
	if (!input.isNavigationPressed) {
		navigationCapturedButton_ = {};
	}

	InvokeCallbacks(pendingCallbacks);
}

void UIWorld::Draw() const {
	const std::vector<UIButtonHandle> drawOrder = buttonOrder_;
	for (const UIButtonHandle handle : drawOrder) {
		const ButtonSlot* slot = FindSlot(handle);
		if (!slot || !slot->desc.isVisible || !slot->desc.draw) {
			continue;
		}

		const auto draw = slot->desc.draw;
		draw({
			.handle = handle,
			.rect = ResolveRect(*slot),
			.state = slot->state,
		});
	}
}

void UIWorld::DrawHitboxes(const Vector4& color) const {
	for (const UIButtonHandle handle : buttonOrder_) {
		const ButtonSlot* slot = FindSlot(handle);
		if (!slot || !slot->desc.isVisible) {
			continue;
		}
		ResolveRect(*slot).Draw(color);
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
	const UIButtonTransform& transform = slot.desc.transform;
	const Vector2 windowSizeVector{
		static_cast<float>(windowSize.width),
		static_cast<float>(windowSize.height),
	};
	const Vector2 leftTop = windowSizeVector * transform.anchor +
		transform.position - transform.size * transform.pivot;
	return Rect2D{ leftTop, transform.size };
}

UIButtonHandle UIWorld::FindHovered(const Vector2& cursorPosition) const {
	for (auto iterator = buttonOrder_.rbegin();
		iterator != buttonOrder_.rend(); ++iterator) {
		const ButtonSlot* slot = FindSlot(*iterator);
		if (!slot || !slot->desc.isVisible || !slot->desc.isEnabled ||
			!slot->desc.isCursorEnabled) {
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

void UIWorld::MoveSelection(int32_t offset) {
	std::vector<UIButtonHandle> candidates;
	for (const UIButtonHandle handle : buttonOrder_) {
		const ButtonSlot* slot = FindSlot(handle);
		if (slot && slot->desc.isVisible && slot->desc.isEnabled &&
			slot->desc.isNavigationEnabled) {
			candidates.push_back(handle);
		}
	}
	if (candidates.empty()) {
		selectedButton_ = {};
		return;
	}

	const auto selectedIterator = std::ranges::find(candidates, selectedButton_);
	if (selectedIterator == candidates.end()) {
		selectedButton_ = offset > 0 ? candidates.front() : candidates.back();
		return;
	}

	const std::size_t currentIndex = static_cast<std::size_t>(
		std::distance(candidates.begin(), selectedIterator));
	selectedButton_ = offset > 0
		? candidates[(currentIndex + 1u) % candidates.size()]
		: candidates[(currentIndex + candidates.size() - 1u) % candidates.size()];
}

void UIWorld::InvokeCallbacks(const std::vector<PendingCallback>& callbacks) {
	for (const PendingCallback& pending : callbacks) {
		const ButtonSlot* slot = FindSlot(pending.handle);
		if (!slot) {
			continue;
		}

		std::function<void()> callback;
		switch (pending.type) {
			case CallbackType::Trigger:
				callback = slot->desc.callbacks.onTrigger;
				break;
			case CallbackType::Hold:
				callback = slot->desc.callbacks.onHold;
				break;
			case CallbackType::Release:
				callback = slot->desc.callbacks.onRelease;
				break;
		}
		if (callback) {
			callback();
		}
	}
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
