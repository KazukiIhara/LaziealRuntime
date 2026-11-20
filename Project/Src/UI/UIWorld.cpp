#include "LGF/UI/UIWorld.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <utility>

#include "LGF/Collision/Collision3D.h"
#include "LGF/Graphics/Camera3D.h"
#include "LGF/Graphics/Line3D.h"
#include "LGF/Graphics/Material.h"
#include "LGF/Graphics/Plane3D.h"
#include "LGF/Graphics/Sprite.h"
#include "LGF/Graphics/UIPanel3D.h"
#include "LGF/Input.h"
#include "LGF/Math/MathUtils.h"
#include "LGF/Window.h"

using namespace LGF;

namespace {

	Vector4 GetButtonColor(const UIButtonState& state, const UIButtonStyle& style) {
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

	Rect2D ResolveScreenRect(const UITransform& transform, const Rect2D& parentRect) {
		const Vector2 leftTop = parentRect.leftTop + parentRect.size * transform.anchor +
			transform.position - transform.size * transform.pivot;
		return { leftTop, transform.size };
	}

}

UICursor3D LGF::MakeUICursor3D(const Camera3D& camera, const Vector2& screenPosition, float maxDistance) {
	const Vector3 farPoint = camera.ScreenToWorld(screenPosition, 1.0f);
	return {
		.ray = {
			.origin = camera.translation,
			.direction = farPoint - camera.translation,
		},
		.maxDistance = maxDistance,
	};
}

bool UIHandle::IsValid() const {
	return index != InvalidIndex && generation != 0u;
}

UIHandle UIWorld::AddCanvas(const UICanvasDesc& desc) {
	const UIHandle handle = AddElement(UIElementType::Canvas, desc.space, {
		.isVisible = desc.isVisible,
		.isEnabled = desc.isEnabled,
	});
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return {};
	}
	slot->transform = desc.transform;
	slot->transform3D = desc.transform3D;
	slot->orientation = desc.orientation;
	return handle;
}

UIHandle UIWorld::AddPanel(const UIPanelDesc& desc) {
	UICanvasSpace space = UICanvasSpace::Screen;
	if (const ElementSlot* parent = FindSlot(desc.element.parent)) {
		space = parent->space;
	}
	const UIHandle handle = AddElement(UIElementType::Panel, space, desc.element);
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return {};
	}
	slot->transform = desc.transform;
	slot->transform3D = desc.transform3D;
	slot->color = desc.color;
	return handle;
}

UIHandle UIWorld::AddLabel(const UILabelDesc& desc) {
	UICanvasSpace space = UICanvasSpace::Screen;
	if (const ElementSlot* parent = FindSlot(desc.element.parent)) {
		space = parent->space;
	}
	const UIHandle handle = AddElement(UIElementType::Label, space, desc.element);
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return {};
	}
	slot->text = desc.text;
	slot->transform = desc.transform;
	slot->transform3D = desc.transform3D;
	slot->font = desc.font;
	slot->font3D = desc.font3D;
	slot->color = desc.color;
	slot->textAnchor = desc.textAnchor;
	slot->worldUnitsPerPixel = desc.worldUnitsPerPixel;
	slot->depthOffset = desc.depthOffset;
	return handle;
}

UIHandle UIWorld::AddImage(const UIImageDesc& desc) {
	UICanvasSpace space = UICanvasSpace::Screen;
	if (const ElementSlot* parent = FindSlot(desc.element.parent)) {
		space = parent->space;
	}
	const UIHandle handle = AddElement(UIElementType::Image, space, desc.element);
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return {};
	}
	slot->transform = desc.transform;
	slot->transform3D = desc.transform3D;
	slot->texture = desc.texture;
	slot->color = desc.color;
	slot->uvScale = desc.uvScale;
	slot->uvOffset = desc.uvOffset;
	slot->depthOffset = desc.depthOffset;
	return handle;
}

UIButtonHandle UIWorld::AddButton(const UIButtonDesc& desc) {
	UICanvasSpace space = UICanvasSpace::Screen;
	if (const ElementSlot* parent = FindSlot(desc.element.parent)) {
		space = parent->space;
	}
	UIElementDesc element = desc.element;
	element.isVisible = element.isVisible && desc.isVisible;
	element.isEnabled = element.isEnabled && desc.isEnabled;
	const UIHandle handle = AddElement(UIElementType::Button, space, element);
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return {};
	}
	slot->text = desc.text;
	slot->transform = desc.transform;
	slot->transform3D = desc.transform3D;
	slot->font = desc.font;
	slot->font3D = desc.font3D;
	slot->buttonStyle = desc.style;
	slot->shortcutCommand = desc.shortcutCommand;
	slot->isCursorEnabled = desc.isCursorEnabled;
	slot->isNavigationEnabled = desc.isNavigationEnabled;
	if (!selectedButton_.IsValid() && desc.isNavigationEnabled && element.isVisible && element.isEnabled) {
		selectedButton_ = handle;
	}
	return handle;
}

UIButtonHandle UIWorld::AddButton3D(const UIButtonDesc3D& desc) {
	UIElementDesc element = desc.element;
	element.isVisible = element.isVisible && desc.isVisible;
	element.isEnabled = element.isEnabled && desc.isEnabled;
	const UIHandle handle = AddElement(UIElementType::Button, UICanvasSpace::World, element);
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return {};
	}
	slot->text = desc.text;
	slot->transform3D = desc.transform;
	slot->font3D = desc.font;
	slot->buttonStyle = desc.style;
	slot->shortcutCommand = desc.shortcutCommand;
	slot->isCursorEnabled = desc.isCursorEnabled;
	slot->isNavigationEnabled = desc.isNavigationEnabled;
	if (!selectedButton_.IsValid() && desc.isNavigationEnabled && element.isVisible && element.isEnabled) {
		selectedButton_ = handle;
	}
	return handle;
}

UIHandle UIWorld::AddElement(UIElementType type, UICanvasSpace space, const UIElementDesc& desc) {
	if (desc.parent.IsValid()) {
		const ElementSlot* parent = FindSlot(desc.parent);
		if (!parent || !CanHaveChildren(*parent)) {
			return {};
		}
		space = parent->space;
	}

	std::size_t index = 0u;
	for (; index < elements_.size(); ++index) {
		if (!elements_[index].isOccupied) {
			break;
		}
	}
	if (index == elements_.size()) {
		elements_.push_back({});
	}

	ElementSlot& slot = elements_[index];
	const uint32_t generation = slot.generation;
	slot = {};
	slot.type = type;
	slot.space = space;
	slot.parent = desc.parent;
	slot.localVisible = desc.isVisible;
	slot.localEnabled = desc.isEnabled;
	slot.resolvedVisible = desc.isVisible;
	slot.resolvedEnabled = desc.isEnabled;
	slot.inheritVisibility = desc.inheritVisibility;
	slot.generation = generation;
	slot.isOccupied = true;

	const UIHandle handle{ static_cast<uint32_t>(index), slot.generation };
	elementOrder_.push_back(handle);
	if (ElementSlot* parent = FindSlot(desc.parent)) {
		parent->children.push_back(handle);
	}
	return handle;
}

bool UIWorld::Remove(UIHandle handle) {
	if (!IsValid(handle)) {
		return false;
	}

	std::vector<UIHandle> pending{ handle };
	std::vector<UIHandle> removalOrder;
	while (!pending.empty()) {
		const UIHandle current = pending.back();
		pending.pop_back();
		const ElementSlot* slot = FindSlot(current);
		if (!slot) {
			continue;
		}
		removalOrder.push_back(current);
		pending.insert(pending.end(), slot->children.begin(), slot->children.end());
	}

	for (auto iterator = removalOrder.rbegin(); iterator != removalOrder.rend(); ++iterator) {
		RemoveSingle(*iterator);
	}
	return true;
}

void UIWorld::RemoveSingle(UIHandle handle) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return;
	}
	if (ElementSlot* parent = FindSlot(slot->parent)) {
		std::erase(parent->children, handle);
	}
	InvalidateHandle(handle);
	const uint32_t nextGeneration = slot->generation == std::numeric_limits<uint32_t>::max()
		? 1u
		: slot->generation + 1u;
	*slot = {};
	slot->generation = nextGeneration;
	std::erase(elementOrder_, handle);
	std::erase(resolvedOrder_, handle);
}

void UIWorld::Clear() {
	for (ElementSlot& slot : elements_) {
		if (!slot.isOccupied) {
			continue;
		}
		const uint32_t nextGeneration = slot.generation == std::numeric_limits<uint32_t>::max()
			? 1u
			: slot.generation + 1u;
		slot = {};
		slot.generation = nextGeneration;
	}
	elementOrder_.clear();
	resolvedOrder_.clear();
	selectedButton_ = {};
	cursorCapturedButton_ = {};
	navigationCapturedButton_ = {};
	wasCursorPressed_ = false;
	wasNavigationPressed_ = false;
}

bool UIWorld::IsValid(UIHandle handle) const {
	return FindSlot(handle) != nullptr;
}

UIElementType UIWorld::GetElementType(UIHandle handle) const {
	const ElementSlot* slot = FindSlot(handle);
	return slot ? slot->type : UIElementType::Canvas;
}

std::size_t UIWorld::GetElementCount() const {
	return elementOrder_.size();
}

std::size_t UIWorld::GetButtonCount() const {
	return static_cast<std::size_t>(std::ranges::count_if(elementOrder_, [this](UIHandle handle) {
		const ElementSlot* slot = FindSlot(handle);
		return slot && IsButton(*slot);
	}));
}

bool UIWorld::SetParent(UIHandle child, UIHandle parent) {
	ElementSlot* childSlot = FindSlot(child);
	if (!childSlot || childSlot->type == UIElementType::Canvas) {
		return false;
	}
	ElementSlot* parentSlot = nullptr;
	if (parent.IsValid()) {
		parentSlot = FindSlot(parent);
		if (!parentSlot || !CanHaveChildren(*parentSlot)) {
			return false;
		}
	}
	if (child == parent) {
		return false;
	}
	for (UIHandle ancestor = parent; ancestor.IsValid();) {
		if (ancestor == child) {
			return false;
		}
		const ElementSlot* ancestorSlot = FindSlot(ancestor);
		if (!ancestorSlot) {
			return false;
		}
		ancestor = ancestorSlot->parent;
	}

	if (ElementSlot* oldParent = FindSlot(childSlot->parent)) {
		std::erase(oldParent->children, child);
	}
	childSlot->parent = parent;
	if (parentSlot) {
		parentSlot->children.push_back(child);
		std::vector<UIHandle> pending{ child };
		while (!pending.empty()) {
			ElementSlot* descendant = FindSlot(pending.back());
			pending.pop_back();
			if (!descendant) {
				continue;
			}
			descendant->space = parentSlot->space;
			pending.insert(pending.end(), descendant->children.begin(), descendant->children.end());
		}
	}
	return true;
}

UIHandle UIWorld::GetParent(UIHandle handle) const {
	const ElementSlot* slot = FindSlot(handle);
	return slot ? slot->parent : UIHandle{};
}

bool UIWorld::SetInheritVisibility(UIHandle handle, bool inherit) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->inheritVisibility = inherit;
	return true;
}

bool UIWorld::GetInheritVisibility(UIHandle handle) const {
	const ElementSlot* slot = FindSlot(handle);
	return slot && slot->inheritVisibility;
}

bool UIWorld::SetText(UIHandle handle, std::string text) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot || (slot->type != UIElementType::Label && slot->type != UIElementType::Button)) {
		return false;
	}
	slot->text = std::move(text);
	return true;
}

std::string UIWorld::GetText(UIHandle handle) const {
	const ElementSlot* slot = FindSlot(handle);
	return slot ? slot->text : std::string{};
}

bool UIWorld::SetTransform(UIHandle handle, const UITransform& transform) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot || slot->space != UICanvasSpace::Screen) {
		return false;
	}
	slot->transform = transform;
	return true;
}

bool UIWorld::SetTransform3D(UIHandle handle, const UITransform3D& transform) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot || slot->space != UICanvasSpace::World) {
		return false;
	}
	slot->transform3D = transform;
	return true;
}

bool UIWorld::SetVisible(UIHandle handle, bool visible) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->localVisible = visible;
	if (!visible) {
		InvalidateHandle(handle);
	}
	return true;
}

bool UIWorld::SetEnabled(UIHandle handle, bool enabled) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return false;
	}
	slot->localEnabled = enabled;
	if (!enabled) {
		InvalidateHandle(handle);
	}
	return true;
}

bool UIWorld::SetCursorEnabled(UIHandle handle, bool enabled) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot || !IsButton(*slot)) {
		return false;
	}
	slot->isCursorEnabled = enabled;
	if (!enabled && cursorCapturedButton_ == handle) {
		cursorCapturedButton_ = {};
	}
	return true;
}

bool UIWorld::SetNavigationEnabled(UIHandle handle, bool enabled) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot || !IsButton(*slot)) {
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

bool UIWorld::SetShortcutCommand(UIHandle handle, std::string_view command) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot || !IsButton(*slot)) {
		return false;
	}
	slot->shortcutCommand = command;
	return true;
}

void UIWorld::SetCamera(const Camera3D* camera) {
	camera_ = camera;
}

bool UIWorld::SetSelected(UIButtonHandle handle) {
	const ElementSlot* slot = FindSlot(handle);
	if (!slot || !IsButton(*slot) || !slot->resolvedVisible || !slot->resolvedEnabled || !slot->isNavigationEnabled) {
		return false;
	}
	selectedButton_ = handle;
	return true;
}

UIButtonHandle UIWorld::GetSelected() const {
	return IsValid(selectedButton_) ? selectedButton_ : UIButtonHandle{};
}

std::optional<UIButtonState> UIWorld::GetButtonState(UIButtonHandle handle) const {
	const ElementSlot* slot = FindSlot(handle);
	return slot && IsButton(*slot) ? std::optional<UIButtonState>{ slot->buttonState } : std::nullopt;
}

UIButtonEvents UIWorld::GetEvents(UIButtonHandle handle) const {
	const auto state = GetButtonState(handle);
	return state ? UIButtonEvents{ state->isTriggered, state->isHeld, state->isReleased } : UIButtonEvents{};
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
	const ElementSlot* slot = FindSlot(handle);
	return slot && IsButton(*slot) && slot->space == UICanvasSpace::Screen
		? std::optional<Rect2D>{ slot->resolvedRect }
		: std::nullopt;
}

std::optional<Matrix4x4> UIWorld::GetButtonWorldMatrix(UIButtonHandle handle) const {
	const ElementSlot* slot = FindSlot(handle);
	return slot && IsButton(*slot) && slot->space == UICanvasSpace::World
		? std::optional<Matrix4x4>{ slot->resolvedVisualMatrix }
		: std::nullopt;
}

void UIWorld::Update(const UIWorldInput& input) {
	ResolveLayout();
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

	for (const UIHandle handle : resolvedOrder_) {
		ElementSlot* slot = FindSlot(handle);
		if (!slot || !IsButton(*slot)) {
			continue;
		}
		UIButtonState& state = slot->buttonState;
		const bool wasHeld = state.isHeld;
		state.isVisible = slot->resolvedVisible;
		state.isEnabled = slot->resolvedEnabled;
		state.isHovered = handle == hoveredButton;
		state.isSelected = handle == selectedButton_;

		const bool isInteractable = state.isVisible && state.isEnabled;
		const bool isCursorHeld = input.isCursorEnabled && slot->isCursorEnabled &&
			input.isCursorPressed && handle == cursorCapturedButton_;
		const bool isNavigationHeld = slot->isNavigationEnabled &&
			input.isNavigationPressed && handle == navigationCapturedButton_;
		const bool isShortcutHeld = isInteractable && !slot->shortcutCommand.empty() && Input::Press(slot->shortcutCommand);
		state.isHeld = isInteractable && (isCursorHeld || isNavigationHeld || isShortcutHeld);
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

void UIWorld::ResolveLayout() {
	resolvedOrder_.clear();
	const Window::Size windowSize = Window::GetSize();
	const Rect2D screenRect{ {}, {
		static_cast<float>(windowSize.width),
		static_cast<float>(windowSize.height),
	} };
	const Matrix4x4 identity = Math::Identity4x4();
	for (const UIHandle handle : elementOrder_) {
		const ElementSlot* slot = FindSlot(handle);
		if (!slot || slot->parent.IsValid()) {
			continue;
		}
		ResolveElement(handle, &screenRect, &identity, true, true, UIWorldOrientation::Fixed);
	}
}

void UIWorld::ResolveElement(UIHandle handle, const Rect2D* parentRect, const Matrix4x4* parentMatrix, bool parentVisible, bool parentEnabled, UIWorldOrientation orientation) {
	ElementSlot* slot = FindSlot(handle);
	if (!slot) {
		return;
	}

	slot->resolvedVisible = slot->localVisible && (!slot->inheritVisibility || parentVisible);
	slot->resolvedEnabled = slot->localEnabled && parentEnabled;
	if (slot->type == UIElementType::Canvas) {
		orientation = slot->orientation;
	}
	slot->orientation = orientation;

	if (slot->space == UICanvasSpace::Screen) {
		if (slot->type == UIElementType::Canvas &&
			std::abs(slot->transform.size.x) <= Math::EPSILON &&
			std::abs(slot->transform.size.y) <= Math::EPSILON) {
			slot->resolvedRect = *parentRect;
		} else {
			slot->resolvedRect = ResolveScreenRect(slot->transform, *parentRect);
		}
		slot->resolvedNodeMatrix = Math::Identity4x4();
		slot->resolvedVisualMatrix = Math::Identity4x4();
		slot->resolvedRotation = {};
	} else {
		Quaternion rotation = slot->transform3D.rotation;
		if (slot->type == UIElementType::Canvas && camera_) {
			if (orientation == UIWorldOrientation::Billboard) {
				rotation = Math::MakeQuaternion({ camera_->pitch, camera_->yaw, 0.0f });
			} else if (orientation == UIWorldOrientation::BillboardYAxis) {
				rotation = Math::MakeQuaternion({ 0.0f, camera_->yaw, 0.0f });
			}
		}
		const Matrix4x4 localMatrix = Math::MakeAffineMatrix(slot->transform3D.scale, rotation, slot->transform3D.position);
		slot->resolvedNodeMatrix = localMatrix * *parentMatrix;
		slot->resolvedVisualMatrix = Math::MakeScaleMatrix({
			slot->transform3D.size.x,
			slot->transform3D.size.y,
			1.0f,
		}) * slot->resolvedNodeMatrix;
		if (const ElementSlot* parent = FindSlot(slot->parent)) {
			slot->resolvedRotation = rotation * parent->resolvedRotation;
		} else {
			slot->resolvedRotation = rotation;
		}
	}

	resolvedOrder_.push_back(handle);
	for (const UIHandle child : slot->children) {
		ResolveElement(
			child,
			&slot->resolvedRect,
			&slot->resolvedNodeMatrix,
			slot->resolvedVisible,
			slot->resolvedEnabled,
			orientation);
	}
}

void UIWorld::Draw() const {
	for (const UIHandle handle : resolvedOrder_) {
		const ElementSlot* slot = FindSlot(handle);
		if (slot && slot->resolvedVisible) {
			DrawElement(*slot);
		}
	}
}

void UIWorld::DrawElement(const ElementSlot& slot) const {
	if (slot.type == UIElementType::Canvas) {
		return;
	}
	if (slot.type == UIElementType::Button) {
		if (slot.space == UICanvasSpace::Screen) {
			if (slot.font.IsValid()) {
				DrawButton2D(slot, slot.font, slot.buttonStyle);
			} else {
				slot.resolvedRect.Draw(GetButtonColor(slot.buttonState, slot.buttonStyle));
			}
		} else {
			const UIPanel3D panel;
			if (slot.font3D.IsValid()) {
				DrawButton3D(slot, panel, slot.font3D, slot.buttonStyle);
			} else {
				panel.Draw(slot.resolvedVisualMatrix, GetButtonColor(slot.buttonState, slot.buttonStyle));
			}
		}
		return;
	}

	if (slot.space == UICanvasSpace::Screen) {
		if (slot.type == UIElementType::Panel) {
			slot.resolvedRect.Draw(slot.color);
		} else if (slot.type == UIElementType::Label && slot.font.IsValid()) {
			slot.font.Draw(
				slot.text,
				slot.resolvedRect.leftTop + slot.resolvedRect.size * slot.textAnchor,
				{ .color = slot.color, .anchor = slot.textAnchor });
		} else if (slot.type == UIElementType::Image && slot.texture.IsValid()) {
			Sprite sprite{ slot.texture, slot.resolvedRect.leftTop, slot.resolvedRect.size };
			sprite.uvScale = slot.uvScale;
			sprite.uvOffset = slot.uvOffset;
			sprite.Draw(slot.color);
		}
		return;
	}

	if (slot.type == UIElementType::Panel) {
		UIPanel3D{}.Draw(slot.resolvedVisualMatrix, slot.color);
	} else if (slot.type == UIElementType::Label && slot.font3D.IsValid()) {
		slot.font3D.Draw(
			slot.text,
			Math::Transform({ 0.0f, 0.0f, -slot.depthOffset }, slot.resolvedNodeMatrix),
			{
				.color = slot.color,
				.anchor = slot.textAnchor,
				.worldUnitsPerPixel = slot.worldUnitsPerPixel,
				.mode = Font3DMode::World,
				.rotation = slot.resolvedRotation,
			});
	} else if (slot.type == UIElementType::Image && slot.texture.IsValid()) {
		const Matrix4x4 imageMatrix = Math::MakeTranslateMatrix({ 0.0f, 0.0f, -slot.depthOffset }) * slot.resolvedVisualMatrix;
		Plane3D{}.Draw(imageMatrix, Material3D{
			.color = slot.color,
			.baseColorTexture = slot.texture,
			.unlit = true,
			.uvScale = slot.uvScale,
			.uvOffset = slot.uvOffset,
		});
	}
}

void UIWorld::Draw(const Font2D& font, const UIButtonStyle& style) const {
	for (const UIHandle handle : resolvedOrder_) {
		Draw(handle, font, style);
	}
}

bool UIWorld::Draw(UIButtonHandle handle, const Font2D& font, const UIButtonStyle& style) const {
	const ElementSlot* slot = FindSlot(handle);
	if (!slot || !IsButton(*slot) || slot->space != UICanvasSpace::Screen) {
		return false;
	}
	if (slot->resolvedVisible) {
		DrawButton2D(*slot, font, style);
	}
	return true;
}

void UIWorld::DrawButton2D(const ElementSlot& slot, const Font2D& font, const UIButtonStyle& style) const {
	slot.resolvedRect.Draw(GetButtonColor(slot.buttonState, style));
	font.Draw(
		slot.text,
		slot.resolvedRect.leftTop + slot.resolvedRect.size * 0.5f,
		{ .color = style.textColor, .anchor = { 0.5f, 0.5f } });
}

void UIWorld::Draw3D(const UIPanel3D& panel, const Font3D& font, const UIButtonStyle& style) const {
	for (const UIHandle handle : resolvedOrder_) {
		Draw3D(handle, panel, font, style);
	}
}

bool UIWorld::Draw3D(UIButtonHandle handle, const UIPanel3D& panel, const Font3D& font, const UIButtonStyle& style) const {
	const ElementSlot* slot = FindSlot(handle);
	if (!slot || !IsButton(*slot) || slot->space != UICanvasSpace::World) {
		return false;
	}
	if (slot->resolvedVisible) {
		DrawButton3D(*slot, panel, font, style);
	}
	return true;
}

void UIWorld::DrawButton3D(const ElementSlot& slot, const UIPanel3D& panel, const Font3D& font, const UIButtonStyle& style) const {
	if (slot.buttonState.isSelected || slot.buttonState.isHovered || slot.buttonState.isHeld) {
		const Matrix4x4 outlineMatrix = Math::MakeAffineMatrix(
			{ style.outlineScale.x, style.outlineScale.y, 1.0f },
			{},
			{ 0.0f, 0.0f, -style.outlineDepthOffset }) * slot.resolvedVisualMatrix;
		panel.Draw(outlineMatrix, style.outlineColor);
	}
	panel.Draw(slot.resolvedVisualMatrix, GetButtonColor(slot.buttonState, style));
	font.Draw(
		slot.text,
		Math::Transform({ 0.0f, 0.0f, -style.textDepthOffset }, slot.resolvedNodeMatrix),
		{
			.color = style.textColor,
			.anchor = { 0.5f, 0.5f },
			.worldUnitsPerPixel = style.worldUnitsPerPixel,
			.mode = Font3DMode::World,
			.rotation = slot.resolvedRotation,
		});
}

void UIWorld::DrawHitboxes(const Vector4& color) const {
	for (const UIHandle handle : resolvedOrder_) {
		const ElementSlot* slot = FindSlot(handle);
		if (!slot || slot->space != UICanvasSpace::Screen || !slot->resolvedVisible || slot->type == UIElementType::Canvas) {
			continue;
		}
		slot->resolvedRect.Draw(color);
	}
}

void UIWorld::DrawHitboxes3D(const Vector4& color) const {
	constexpr std::array<Vector3, 4> localCorners{
		Vector3{ -0.5f, -0.5f, 0.0f },
		Vector3{ 0.5f, -0.5f, 0.0f },
		Vector3{ 0.5f, 0.5f, 0.0f },
		Vector3{ -0.5f, 0.5f, 0.0f },
	};
	for (const UIHandle handle : resolvedOrder_) {
		const ElementSlot* slot = FindSlot(handle);
		if (!slot || slot->space != UICanvasSpace::World || !slot->resolvedVisible ||
			slot->type == UIElementType::Canvas || slot->type == UIElementType::Label) {
			continue;
		}
		std::array<Vector3, 4> corners{};
		for (std::size_t index = 0u; index < corners.size(); ++index) {
			corners[index] = Math::Transform(localCorners[index], slot->resolvedVisualMatrix);
		}
		for (std::size_t index = 0u; index < corners.size(); ++index) {
			Line3D(corners[index], corners[(index + 1u) % corners.size()]).Draw(Math::Identity4x4(), color);
		}
	}
}

UIWorld::ElementSlot* UIWorld::FindSlot(UIHandle handle) {
	if (!handle.IsValid() || static_cast<std::size_t>(handle.index) >= elements_.size()) {
		return nullptr;
	}
	ElementSlot& slot = elements_[handle.index];
	return slot.isOccupied && slot.generation == handle.generation ? &slot : nullptr;
}

const UIWorld::ElementSlot* UIWorld::FindSlot(UIHandle handle) const {
	if (!handle.IsValid() || static_cast<std::size_t>(handle.index) >= elements_.size()) {
		return nullptr;
	}
	const ElementSlot& slot = elements_[handle.index];
	return slot.isOccupied && slot.generation == handle.generation ? &slot : nullptr;
}

bool UIWorld::CanHaveChildren(const ElementSlot& slot) const {
	return slot.type == UIElementType::Canvas || slot.type == UIElementType::Panel;
}

bool UIWorld::IsButton(const ElementSlot& slot) const {
	return slot.type == UIElementType::Button;
}

UIButtonHandle UIWorld::FindHovered2D(const Vector2& cursorPosition) const {
	for (auto iterator = resolvedOrder_.rbegin(); iterator != resolvedOrder_.rend(); ++iterator) {
		const ElementSlot* slot = FindSlot(*iterator);
		if (!slot || !IsButton(*slot) || slot->space != UICanvasSpace::Screen ||
			!slot->resolvedVisible || !slot->resolvedEnabled || !slot->isCursorEnabled) {
			continue;
		}
		const Vector2 rightBottom = slot->resolvedRect.leftTop + slot->resolvedRect.size;
		if (cursorPosition.x >= slot->resolvedRect.leftTop.x && cursorPosition.y >= slot->resolvedRect.leftTop.y &&
			cursorPosition.x <= rightBottom.x && cursorPosition.y <= rightBottom.y) {
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
	for (const UIHandle handle : resolvedOrder_) {
		const ElementSlot* slot = FindSlot(handle);
		if (!slot || !IsButton(*slot) || slot->space != UICanvasSpace::World ||
			!slot->resolvedVisible || !slot->resolvedEnabled || !slot->isCursorEnabled) {
			continue;
		}
		if (std::abs(slot->transform3D.size.x) <= Math::EPSILON || std::abs(slot->transform3D.size.y) <= Math::EPSILON) {
			continue;
		}
		const Vector3 normal = Math::Normalize(Math::TransformNormal({ 0.0f, 0.0f, 1.0f }, slot->resolvedNodeMatrix));
		const Vector3 center = Math::Transform({}, slot->resolvedNodeMatrix);
		const Collision::Plane plane{ .normal = normal, .distance = -Math::Dot(normal, center) };
		const std::optional<Collision::RaycastHit> hit = Collision::Raycast(cursor.ray, plane, closestDistance);
		if (!hit) {
			continue;
		}
		const Vector3 localPoint = Math::Transform(hit->point, Math::Inverse(slot->resolvedVisualMatrix));
		if (std::abs(localPoint.x) <= 0.5f && std::abs(localPoint.y) <= 0.5f) {
			closestButton = handle;
			closestDistance = hit->distance;
		}
	}
	return closestButton;
}

void UIWorld::MoveSelection(UISelectionChange change) {
	const bool moveNext = change == UISelectionChange::Next;
	std::vector<UIButtonHandle> candidates;
	for (const UIHandle handle : resolvedOrder_) {
		const ElementSlot* slot = FindSlot(handle);
		if (slot && IsButton(*slot) && slot->resolvedVisible && slot->resolvedEnabled && slot->isNavigationEnabled) {
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
	const std::size_t currentIndex = static_cast<std::size_t>(std::distance(candidates.begin(), selectedIterator));
	selectedButton_ = moveNext
		? candidates[(currentIndex + 1u) % candidates.size()]
		: candidates[(currentIndex + candidates.size() - 1u) % candidates.size()];
}

void UIWorld::InvalidateHandle(UIHandle handle) {
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
