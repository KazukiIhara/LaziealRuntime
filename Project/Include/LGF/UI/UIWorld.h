#pragma once

#include "LGF/Collision/Shapes3D.h"
#include "LGF/Graphics/Rect2D.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

namespace LGF {
	class Camera3D;

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

	/// <summary>Buttonの自由描画へ渡される情報</summary>
	struct UIButtonDrawContext {
		UIButtonHandle handle{};
		Rect2D rect{};
		UIButtonState state{};
	};

	/// <summary>3D空間へButtonを配置する情報</summary>
	struct UIButtonTransform3D {
		Vector3 position{};
		Vector2 size{ 1.0f, 1.0f };
		Quaternion rotation{};
	};

	/// <summary>3D Buttonの自由描画へ渡される情報</summary>
	struct UIButtonDrawContext3D {
		UIButtonHandle handle{};
		Matrix4x4 worldMatrix{};
		UIButtonState state{};
	};

	struct UIButtonCallbacks {
		std::function<void()> onTrigger;
		std::function<void()> onHold;
		std::function<void()> onRelease;
	};

	/// <summary>UIWorldへButtonを追加するための設定</summary>
	struct UIButtonDesc {
		UIButtonTransform transform{};
		UIButtonCallbacks callbacks{};
		std::function<void(const UIButtonDrawContext&)> draw;
		/// <summary>Button固有の入力が押されているかを返す。</summary>
		std::function<bool()> isDirectPressed;
		bool isCursorEnabled = true;
		bool isNavigationEnabled{};
		bool isVisible = true;
		bool isEnabled = true;
	};

	/// <summary>UIWorldへ3D Buttonを追加するための設定</summary>
	struct UIButtonDesc3D {
		UIButtonTransform3D transform{};
		UIButtonCallbacks callbacks{};
		std::function<void(const UIButtonDrawContext3D&)> draw;
		/// <summary>Button固有の入力が押されているかを返す。</summary>
		std::function<bool()> isDirectPressed;
		bool isCursorEnabled = true;
		bool isNavigationEnabled{};
		bool isVisible = true;
		bool isEnabled = true;
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

	/// <summary>Buttonの配置、選択、入力、描画をまとめて管理する。</summary>
	class UIWorld {
	private:
		enum class ButtonSpace : uint8_t {
			Screen,
			World,
		};

		struct ButtonSlot {
			ButtonSpace space{};
			UIButtonTransform transform{};
			UIButtonTransform3D transform3D{};
			UIButtonCallbacks callbacks{};
			std::function<void(const UIButtonDrawContext&)> draw;
			std::function<void(const UIButtonDrawContext3D&)> draw3D;
			std::function<bool()> isDirectPressed;
			UIButtonState state{};
			uint32_t generation = 1u;
			bool isCursorEnabled = true;
			bool isNavigationEnabled{};
			bool isVisible = true;
			bool isEnabled = true;
			bool isOccupied{};
		};

		enum class CallbackType : uint8_t {
			Trigger,
			Hold,
			Release,
		};

		struct PendingCallback {
			UIButtonHandle handle{};
			CallbackType type{};
		};

	public:
		UIButtonHandle AddButton(const UIButtonDesc& desc = {});
		UIButtonHandle AddButton3D(const UIButtonDesc3D& desc = {});
		bool RemoveButton(UIButtonHandle handle);
		void Clear();

		bool IsValid(UIButtonHandle handle) const;
		std::size_t GetButtonCount() const;

		bool SetTransform(UIButtonHandle handle, const UIButtonTransform& transform);
		bool SetTransform3D(UIButtonHandle handle, const UIButtonTransform3D& transform);
		bool SetVisible(UIButtonHandle handle, bool visible);
		bool SetEnabled(UIButtonHandle handle, bool enabled);
		bool SetCursorEnabled(UIButtonHandle handle, bool enabled);
		bool SetNavigationEnabled(UIButtonHandle handle, bool enabled);
		bool SetDirectInput(UIButtonHandle handle, std::function<bool()> isPressed);
		bool SetCallbacks(UIButtonHandle handle, const UIButtonCallbacks& callbacks);
		bool SetDrawCallback(
			UIButtonHandle handle,
			std::function<void(const UIButtonDrawContext&)> draw);
		bool SetDrawCallback3D(
			UIButtonHandle handle,
			std::function<void(const UIButtonDrawContext3D&)> draw);

		bool SetSelected(UIButtonHandle handle);
		UIButtonHandle GetSelected() const;
		std::optional<UIButtonState> GetButtonState(UIButtonHandle handle) const;
		std::optional<Rect2D> GetButtonRect(UIButtonHandle handle) const;
		std::optional<Matrix4x4> GetButtonWorldMatrix(UIButtonHandle handle) const;

		void Update(const UIWorldInput& input);
		void Draw() const;
		void Draw3D() const;
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
		void InvokeCallbacks(const std::vector<PendingCallback>& callbacks);
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
