#pragma once

#include "LGF/Collision/Shapes3D.h"
#include "LGF/Graphics/Font2D.h"
#include "LGF/Graphics/Font3D.h"
#include "LGF/Graphics/Rect2D.h"
#include "LGF/Graphics/Texture.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace LGF {
	class Camera3D;
	class UIPanel3D;

	/// <summary>UIWorldに登録された要素を識別する世代付きハンドル</summary>
	struct UIHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();
		uint32_t index = InvalidIndex;
		uint32_t generation{};
		bool IsValid() const;
		bool operator==(const UIHandle&) const = default;
	};

	/// <summary>従来のButton用コードとの互換性を保つための別名</summary>
	using UIButtonHandle = UIHandle;

	enum class UIElementType : uint8_t {
		Canvas,
		Panel,
		Label,
		Image,
		Button,
	};

	enum class UICanvasSpace : uint8_t {
		Screen,
		World,
	};

	enum class UIWorldOrientation : uint8_t {
		Fixed,
		Billboard,
		BillboardYAxis,
	};

	/// <summary>画面または親領域を基準にUI要素を配置する情報</summary>
	struct UITransform {
		Vector2 position{};
		Vector2 size{};
		/// <summary>画面または親領域に対する0.0から1.0の基準位置</summary>
		Vector2 anchor{};
		/// <summary>UI要素自身のサイズに対する0.0から1.0の基準位置</summary>
		Vector2 pivot{};
	};

	/// <summary>3D空間または親要素を基準にUI要素を配置する情報</summary>
	struct UITransform3D {
		Vector3 position{};
		Vector2 size{ 1.0f, 1.0f };
		Quaternion rotation{};
		/// <summary>子要素にも継承する拡縮。描画領域を表すsizeとは分けて扱う。</summary>
		Vector3 scale{ 1.0f, 1.0f, 1.0f };
	};

	using UIButtonTransform = UITransform;
	using UIButtonTransform3D = UITransform3D;

	struct UIElementDesc {
		UIHandle parent{};
		bool isVisible = true;
		bool isEnabled = true;
		/// <summary>falseなら親が非表示でも、この要素自身の表示設定だけで描画を決める。</summary>
		bool inheritVisibility = true;
	};

	struct UICanvasDesc {
		UICanvasSpace space = UICanvasSpace::Screen;
		UITransform transform{};
		UITransform3D transform3D{};
		UIWorldOrientation orientation = UIWorldOrientation::Fixed;
		bool isVisible = true;
		bool isEnabled = true;
	};

	struct UIPanelDesc {
		UIElementDesc element{};
		UITransform transform{};
		UITransform3D transform3D{};
		Vector4 color{ 0.08f, 0.10f, 0.16f, 0.92f };
	};

	struct UILabelDesc {
		UIElementDesc element{};
		std::string text;
		UITransform transform{};
		UITransform3D transform3D{};
		Font2D font;
		Font3D font3D;
		Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector2 textAnchor{ 0.5f, 0.5f };
		float worldUnitsPerPixel = 0.008f;
		float depthOffset = 0.01f;
	};

	struct UIImageDesc {
		UIElementDesc element{};
		UITransform transform{};
		UITransform3D transform3D{};
		Texture texture;
		Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector2 uvScale{ 1.0f, 1.0f };
		Vector2 uvOffset{};
		float depthOffset = 0.01f;
	};

	struct UIButtonState {
		bool isVisible{};
		bool isEnabled{};
		bool isHovered{};
		bool isSelected{};
		bool isTriggered{};
		bool isHeld{};
		bool isReleased{};
	};

	struct UIButtonEvents {
		bool triggered{};
		bool held{};
		bool released{};
	};

	struct UIButtonStyle {
		Vector4 normalColor{ 0.12f, 0.18f, 0.32f, 1.0f };
		Vector4 hoveredColor{ 0.05f, 0.42f, 1.0f, 1.0f };
		Vector4 selectedColor{ 0.95f, 0.42f, 0.04f, 1.0f };
		Vector4 heldColor{ 1.0f, 0.08f, 0.48f, 1.0f };
		Vector4 disabledColor{ 0.16f, 0.16f, 0.18f, 0.75f };
		Vector4 textColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 outlineColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector2 outlineScale{ 1.04f, 1.18f };
		float worldUnitsPerPixel = 0.008f;
		float textDepthOffset = 0.01f;
		float outlineDepthOffset = -0.01f;
	};

	struct UIButtonDesc {
		UIElementDesc element{};
		std::string text;
		UITransform transform{};
		UITransform3D transform3D{};
		Font2D font;
		Font3D font3D;
		UIButtonStyle style{};
		std::string shortcutCommand;
		bool isCursorEnabled = true;
		bool isNavigationEnabled{};
		bool isVisible = true;
		bool isEnabled = true;
	};

	struct UIButtonDesc3D {
		UIElementDesc element{};
		std::string text;
		UITransform3D transform{};
		Font3D font;
		UIButtonStyle style{};
		std::string shortcutCommand;
		bool isCursorEnabled = true;
		bool isNavigationEnabled{};
		bool isVisible = true;
		bool isEnabled = true;
	};

	struct UICursor3D {
		Collision::Ray ray{};
		float maxDistance = std::numeric_limits<float>::infinity();
	};

	UICursor3D MakeUICursor3D(const Camera3D& camera, const Vector2& screenPosition, float maxDistance = std::numeric_limits<float>::infinity());

	enum class UISelectionChange : uint8_t {
		None,
		Previous,
		Next,
	};

	struct UIWorldInput {
		Vector2 cursorPosition{};
		bool isCursorPressed{};
		UISelectionChange selectionChange{};
		bool isNavigationPressed{};
		bool isCursorEnabled = true;
		std::optional<UICursor3D> cursor3D;
	};

	/// <summary>Canvas以下のUI要素、親子関係、入力、描画をまとめて管理する。</summary>
	class UIWorld {
	private:
		struct ElementSlot {
			UIElementType type = UIElementType::Canvas;
			UICanvasSpace space = UICanvasSpace::Screen;
			UIWorldOrientation orientation = UIWorldOrientation::Fixed;
			UIHandle parent{};
			std::vector<UIHandle> children;
			std::string text;
			UITransform transform{};
			UITransform3D transform3D{};
			Rect2D resolvedRect{};
			Matrix4x4 resolvedNodeMatrix{};
			Matrix4x4 resolvedVisualMatrix{};
			Quaternion resolvedRotation{};
			Font2D font;
			Font3D font3D;
			Texture texture;
			Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
			Vector2 textAnchor{ 0.5f, 0.5f };
			Vector2 uvScale{ 1.0f, 1.0f };
			Vector2 uvOffset{};
			UIButtonStyle buttonStyle{};
			UIButtonState buttonState{};
			std::string shortcutCommand;
			float worldUnitsPerPixel = 0.008f;
			float depthOffset = 0.01f;
			uint32_t generation = 1u;
			bool isCursorEnabled = true;
			bool isNavigationEnabled{};
			bool localVisible = true;
			bool localEnabled = true;
			bool resolvedVisible = true;
			bool resolvedEnabled = true;
			bool inheritVisibility = true;
			bool isOccupied{};
		};

	public:
		UIHandle AddCanvas(const UICanvasDesc& desc = {});
		UIHandle AddPanel(const UIPanelDesc& desc = {});
		UIHandle AddLabel(const UILabelDesc& desc = {});
		UIHandle AddImage(const UIImageDesc& desc = {});
		UIButtonHandle AddButton(const UIButtonDesc& desc = {});
		UIButtonHandle AddButton3D(const UIButtonDesc3D& desc = {});
		bool Remove(UIHandle handle);
		void Clear();

		bool IsValid(UIHandle handle) const;
		UIElementType GetElementType(UIHandle handle) const;
		std::size_t GetElementCount() const;
		std::size_t GetButtonCount() const;

		bool SetParent(UIHandle child, UIHandle parent);
		UIHandle GetParent(UIHandle handle) const;
		bool SetInheritVisibility(UIHandle handle, bool inherit);
		bool GetInheritVisibility(UIHandle handle) const;
		bool SetText(UIHandle handle, std::string text);
		std::string GetText(UIHandle handle) const;
		bool SetTransform(UIHandle handle, const UITransform& transform);
		bool SetTransform3D(UIHandle handle, const UITransform3D& transform);
		bool SetVisible(UIHandle handle, bool visible);
		bool SetEnabled(UIHandle handle, bool enabled);
		bool SetCursorEnabled(UIHandle handle, bool enabled);
		bool SetNavigationEnabled(UIHandle handle, bool enabled);
		bool SetShortcutCommand(UIHandle handle, std::string_view command);
		void SetCamera(const Camera3D* camera);

		bool SetSelected(UIButtonHandle handle);
		UIButtonHandle GetSelected() const;
		std::optional<UIButtonState> GetButtonState(UIButtonHandle handle) const;
		UIButtonEvents GetEvents(UIButtonHandle handle) const;
		bool Trigger(UIButtonHandle handle) const;
		bool Press(UIButtonHandle handle) const;
		bool Release(UIButtonHandle handle) const;
		std::optional<Rect2D> GetButtonRect(UIButtonHandle handle) const;
		std::optional<Rect2D> GetScreenRect(UIHandle handle) const;
		std::optional<Matrix4x4> GetButtonWorldMatrix(UIButtonHandle handle) const;

		void Update(const UIWorldInput& input);
		void Update(const UIWorldInput& input, const Vector2& screenSize);
		/// <summary>登録されたScreen / World UIを、各要素に設定されたリソースで描画</summary>
		void Draw() const;
		void Draw(const Font2D& font, const UIButtonStyle& style = {}) const;
		bool Draw(UIButtonHandle handle, const Font2D& font, const UIButtonStyle& style = {}) const;
		void Draw3D(const UIPanel3D& panel, const Font3D& font, const UIButtonStyle& style = {}) const;
		bool Draw3D(UIButtonHandle handle, const UIPanel3D& panel, const Font3D& font, const UIButtonStyle& style = {}) const;
		void DrawHitboxes(const Vector4& color = { 1.0f, 0.0f, 0.0f, 0.25f }) const;
		void DrawHitboxes3D(const Vector4& color = { 1.0f, 0.0f, 0.0f, 1.0f }) const;

	private:
		UIHandle AddElement(UIElementType type, UICanvasSpace space, const UIElementDesc& desc);
		ElementSlot* FindSlot(UIHandle handle);
		const ElementSlot* FindSlot(UIHandle handle) const;
		bool CanHaveChildren(const ElementSlot& slot) const;
		bool IsButton(const ElementSlot& slot) const;
		void ResolveLayout(const Vector2& screenSize);
		void ResolveElement(UIHandle handle, const Rect2D* parentRect, const Matrix4x4* parentMatrix, bool parentVisible, bool parentEnabled, UIWorldOrientation orientation);
		void RemoveSingle(UIHandle handle);
		void DrawElement(const ElementSlot& slot) const;
		void DrawButton2D(const ElementSlot& slot, const Font2D& font, const UIButtonStyle& style) const;
		void DrawButton3D(const ElementSlot& slot, const UIPanel3D& panel, const Font3D& font, const UIButtonStyle& style) const;
		UIButtonHandle FindHovered2D(const Vector2& cursorPosition) const;
		UIButtonHandle FindHovered3D(const UICursor3D& cursor) const;
		void MoveSelection(UISelectionChange change);
		void InvalidateHandle(UIHandle handle);

	private:
		std::vector<ElementSlot> elements_;
		std::vector<UIHandle> elementOrder_;
		std::vector<UIHandle> resolvedOrder_;
		UIButtonHandle selectedButton_{};
		UIButtonHandle cursorCapturedButton_{};
		UIButtonHandle navigationCapturedButton_{};
		const Camera3D* camera_{};
		bool wasCursorPressed_{};
		bool wasNavigationPressed_{};
	};

}
