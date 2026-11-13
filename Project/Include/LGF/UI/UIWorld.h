#pragma once

#include "LGF/Collision/Shapes3D.h"
#include "LGF/Graphics/Rect2D.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace LGF {
	class Camera3D;
	class Font2D;
	class Font3D;
	class UIPanel3D;
	class UIWorld;

	/// <summary>UIWorldに登録されたButtonを識別する世代付きハンドル</summary>
	struct UIButtonHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const UIButtonHandle&) const = default;
	};

	/// <summary>画面または親領域を基準にButtonを配置する情報</summary>
	struct UIButtonTransform {
		Vector2 position{};
		Vector2 size{};
		/// <summary>画面サイズに対する0.0から1.0の基準位置</summary>
		Vector2 anchor{};
		/// <summary>Button自身のサイズに対する0.0から1.0の基準位置</summary>
		Vector2 pivot{};
	};

	/// <summary>3D空間へButtonを配置する情報</summary>
	struct UIButtonTransform3D {
		Vector3 position{};
		Vector2 size{ 1.0f, 1.0f };
		Quaternion rotation{};
	};

	/// <summary>現在のButtonの状態</summary>
	struct UIButtonState {
		bool isVisible{};
		bool isEnabled{};
		bool isHovered{};
		bool isSelected{};
		bool isTriggered{};
		bool isHeld{};
		bool isReleased{};
	};

	/// <summary>直前の更新で発生したButtonの入力イベント</summary>
	struct UIButtonEvents {
		bool triggered{};
		bool held{};
		bool released{};
	};

	/// <summary>Buttonの標準描画に使用する色と3D文字サイズ</summary>
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
	};

	/// <summary>UIWorldへButtonを追加するための設定</summary>
	struct UIButtonDesc {
		std::string text;
		UIButtonTransform transform{};
		/// <summary>Button固有の操作へ割り当てるInputCommand名</summary>
		std::string directInputCommand;
		bool isCursorEnabled = true;
		bool isNavigationEnabled{};
		bool isVisible = true;
		bool isEnabled = true;
	};

	/// <summary>UIWorldへ3D Buttonを追加するための設定</summary>
	struct UIButtonDesc3D {
		std::string text;
		UIButtonTransform3D transform{};
		/// <summary>Button固有の操作へ割り当てるInputCommand名</summary>
		std::string directInputCommand;
		bool isCursorEnabled = true;
		bool isNavigationEnabled{};
		bool isVisible = true;
		bool isEnabled = true;
	};

	/// <summary>UIWorld内のButtonを簡単に操作するための参照</summary>
	class UIButton {
	public:
		UIButton() = default;

		bool IsValid() const;
		explicit operator bool() const;
		UIButtonHandle GetHandle() const;

		std::string GetText() const;
		UIButtonState GetState() const;
		UIButtonEvents GetEvents() const;
		std::optional<Rect2D> GetRect() const;
		std::optional<Matrix4x4> GetWorldMatrix() const;

		bool Trigger() const;
		bool Press() const;
		bool Release() const;

		bool SetText(std::string text);
		bool SetTransform(const UIButtonTransform& transform);
		bool SetTransform3D(const UIButtonTransform3D& transform);
		bool SetVisible(bool visible);
		bool SetEnabled(bool enabled);
		bool SetCursorEnabled(bool enabled);
		bool SetNavigationEnabled(bool enabled);
		bool SetDirectInputCommand(std::string_view command);
		bool SetSelected();
		bool Remove();

		/// <summary>2D Buttonを標準の背景とテキストで描画</summary>
		bool Draw(const Font2D& font, const UIButtonStyle& style = {}) const;
		/// <summary>3D Buttonを標準のパネルとテキストで描画</summary>
		bool Draw(
			const UIPanel3D& panel,
			const Font3D& font,
			const UIButtonStyle& style = {}) const;

	private:
		friend class UIWorld;

		UIButton(UIWorld* world, UIButtonHandle handle);

		UIWorld* world_{};
		UIButtonHandle handle_{};
	};

	/// <summary>3D空間のButtonを指すカーソル</summary>
	struct UICursor3D {
		Collision::Ray ray{};
		float maxDistance = std::numeric_limits<float>::infinity();
	};

	/// <summary>Camera3Dとスクリーン座標から3D UI用カーソルを作成</summary>
	UICursor3D MakeUICursor3D(
		const Camera3D& camera,
		const Vector2& screenPosition,
		float maxDistance = std::numeric_limits<float>::infinity());

	/// <summary>外部で生成してUIWorldへ渡す入力状態</summary>
	struct UIWorldInput {
		Vector2 cursorPosition{};
		/// <summary>カーソルで決定する入力が現在押されているか</summary>
		bool isCursorPressed{};
		/// <summary>負の値で前、正の値で次のButtonを選択する。</summary>
		int32_t navigationOffset{};
		/// <summary>選択中のButtonを決定する入力が現在押されているか</summary>
		bool isNavigationPressed{};
		bool isCursorEnabled = true;
		/// <summary>指定時は2Dカーソルで何も指していない場合に3D Buttonを検索する。</summary>
		std::optional<UICursor3D> cursor3D;
	};

	/// <summary>Buttonの配置、選択、入力をまとめて管理する。</summary>
	class UIWorld {
	private:
		enum class ButtonSpace : uint8_t {
			Screen,
			World,
		};

		struct ButtonSlot {
			ButtonSpace space{};
			std::string text;
			UIButtonTransform transform{};
			UIButtonTransform3D transform3D{};
			std::string directInputCommand;
			UIButtonState state{};
			uint32_t generation = 1u;
			bool isCursorEnabled = true;
			bool isNavigationEnabled{};
			bool isVisible = true;
			bool isEnabled = true;
			bool isOccupied{};
		};

	public:
		UIWorld() = default;
		UIWorld(const UIWorld&) = delete;
		UIWorld& operator=(const UIWorld&) = delete;
		UIWorld(UIWorld&&) = delete;
		UIWorld& operator=(UIWorld&&) = delete;

		UIButton AddButton(const UIButtonDesc& desc = {});
		UIButton AddButton3D(const UIButtonDesc3D& desc = {});
		bool RemoveButton(UIButtonHandle handle);
		void Clear();

		bool IsValid(UIButtonHandle handle) const;
		std::size_t GetButtonCount() const;

		bool SetText(UIButtonHandle handle, std::string text);
		std::string GetText(UIButtonHandle handle) const;
		bool SetTransform(UIButtonHandle handle, const UIButtonTransform& transform);
		bool SetTransform3D(UIButtonHandle handle, const UIButtonTransform3D& transform);
		bool SetVisible(UIButtonHandle handle, bool visible);
		bool SetEnabled(UIButtonHandle handle, bool enabled);
		bool SetCursorEnabled(UIButtonHandle handle, bool enabled);
		bool SetNavigationEnabled(UIButtonHandle handle, bool enabled);
		bool SetDirectInputCommand(UIButtonHandle handle, std::string_view command);

		bool SetSelected(UIButtonHandle handle);
		UIButtonHandle GetSelected() const;
		std::optional<UIButtonState> GetButtonState(UIButtonHandle handle) const;
		UIButtonEvents GetEvents(UIButtonHandle handle) const;
		std::optional<Rect2D> GetButtonRect(UIButtonHandle handle) const;
		std::optional<Matrix4x4> GetButtonWorldMatrix(UIButtonHandle handle) const;

		void Update(const UIWorldInput& input);
		/// <summary>現在のButtonの当たり判定を半透明の矩形で描画する。</summary>
		void DrawHitboxes(const Vector4& color = { 1.0f, 0.0f, 0.0f, 0.25f }) const;
		/// <summary>現在の3D Buttonの当たり判定を線で描画する。</summary>
		void DrawHitboxes3D(const Vector4& color = { 1.0f, 0.0f, 0.0f, 1.0f }) const;

	private:
		ButtonSlot* FindSlot(UIButtonHandle handle);
		const ButtonSlot* FindSlot(UIButtonHandle handle) const;
		Rect2D ResolveRect(const ButtonSlot& slot) const;
		Matrix4x4 ResolveWorldMatrix(const ButtonSlot& slot) const;
		UIButtonHandle FindHovered2D(const Vector2& cursorPosition) const;
		UIButtonHandle FindHovered3D(const UICursor3D& cursor) const;
		void MoveSelection(int32_t offset);
		void InvalidateHandle(UIButtonHandle handle);

		std::vector<ButtonSlot> buttons_;
		std::vector<UIButtonHandle> buttonOrder_;
		UIButtonHandle selectedButton_{};
		UIButtonHandle cursorCapturedButton_{};
		UIButtonHandle navigationCapturedButton_{};
		bool wasCursorPressed_{};
		bool wasNavigationPressed_{};
	};

}
