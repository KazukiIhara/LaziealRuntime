#pragma once

#include "LGF/LGFMath.h"

#include <cstdint>

namespace LGF {

	enum class UIPointerSource : uint8_t {
		Mouse,
		Gamepad,
	};

	/// <summary>マウスとゲームパッドを統合する仮想ポインター設定</summary>
	struct UIPointerDesc {
		uint32_t gamepadIndex{};
		float speed = 720.0f;
		bool isMouseEnabled = true;
		bool isGamepadEnabled = true;
		bool clampToWindow = true;
	};

	/// <summary>仮想ポインターの標準描画設定</summary>
	struct UIPointerStyle {
		Vector2 size{ 14.0f, 14.0f };
		Vector4 mouseColor{ 0.0f, 1.0f, 1.0f, 1.0f };
		Vector4 gamepadColor{ 1.0f, 0.08f, 0.58f, 1.0f };
	};

	/// <summary>マウスとゲームパッドからスクリーン座標を生成する。</summary>
	class UIPointer {
	public:
		explicit UIPointer(const UIPointerDesc& desc = {});

		void Update();

		const Vector2& GetPosition() const;
		const Vector2& GetDelta() const;
		UIPointerSource GetSource() const;
		bool IsGamepad() const;

		void SetPosition(const Vector2& position);
		void ResetToWindowCenter();
		void Draw(const UIPointerStyle& style = {}) const;

	private:
		void ClampToWindow();

		UIPointerDesc desc_{};
		Vector2 position_{};
		Vector2 delta_{};
		UIPointerSource source_ = UIPointerSource::Mouse;
	};

}
