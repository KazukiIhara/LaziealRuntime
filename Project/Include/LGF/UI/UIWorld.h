#pragma once

#include "LGF/Graphics/Rect2D.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

namespace LGF {

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
	};

	/// <summary>Buttonの配置、選択、入力、描画をまとめて管理する。</summary>
	class UIWorld {
	private:
		struct ButtonSlot {
			UIButtonDesc desc{};
			UIButtonState state{};
			uint32_t generation = 1u;
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
		bool RemoveButton(UIButtonHandle handle);
		void Clear();

		bool IsValid(UIButtonHandle handle) const;
		std::size_t GetButtonCount() const;

		bool SetTransform(UIButtonHandle handle, const UIButtonTransform& transform);
		bool SetVisible(UIButtonHandle handle, bool visible);
		bool SetEnabled(UIButtonHandle handle, bool enabled);
		bool SetCursorEnabled(UIButtonHandle handle, bool enabled);
		bool SetNavigationEnabled(UIButtonHandle handle, bool enabled);
		bool SetDirectInput(UIButtonHandle handle, std::function<bool()> isPressed);
		bool SetCallbacks(UIButtonHandle handle, const UIButtonCallbacks& callbacks);
		bool SetDrawCallback(
			UIButtonHandle handle,
			std::function<void(const UIButtonDrawContext&)> draw);

		bool SetSelected(UIButtonHandle handle);
		UIButtonHandle GetSelected() const;
		std::optional<UIButtonState> GetButtonState(UIButtonHandle handle) const;
		std::optional<Rect2D> GetButtonRect(UIButtonHandle handle) const;

		void Update(const UIWorldInput& input);
		void Draw() const;
		/// <summary>現在のButtonの当たり判定を半透明の矩形で描画する。</summary>
		void DrawHitboxes(const Vector4& color = { 1.0f, 0.0f, 0.0f, 0.25f }) const;

	private:
		ButtonSlot* FindSlot(UIButtonHandle handle);
		const ButtonSlot* FindSlot(UIButtonHandle handle) const;
		Rect2D ResolveRect(const ButtonSlot& slot) const;
		UIButtonHandle FindHovered(const Vector2& cursorPosition) const;
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
