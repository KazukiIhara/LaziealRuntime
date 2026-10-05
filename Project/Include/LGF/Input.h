#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "LGF/Math/Vector2.h"
#include "LGF/Math/Vector3.h"

namespace LGF {

	enum class KeyCode : uint8_t {
		Space,
		Escape,
		Enter,
		Tab,
		Backspace,
		Left,
		Right,
		Up,
		Down,
		A,
		B,
		C,
		D,
		E,
		F,
		G,
		H,
		I,
		J,
		K,
		L,
		M,
		N,
		O,
		P,
		Q,
		R,
		S,
		T,
		U,
		V,
		W,
		X,
		Y,
		Z,
		Num0,
		Num1,
		Num2,
		Num3,
		Num4,
		Num5,
		Num6,
		Num7,
		Num8,
		Num9,
		Count
	};

	enum class MouseButtonCode : uint8_t {
		Left,
		Right,
		Middle,
		X1,
		X2,
		Count
	};

	enum class GamepadButtonCode : uint8_t {
		South,
		East,
		West,
		North,
		Back,
		Guide,
		Start,
		LeftStick,
		RightStick,
		LeftShoulder,
		RightShoulder,
		DPadUp,
		DPadDown,
		DPadLeft,
		DPadRight,
		Misc1,
		RightPaddle1,
		LeftPaddle1,
		RightPaddle2,
		LeftPaddle2,
		Touchpad,
		Misc2,
		Misc3,
		Misc4,
		Misc5,
		Misc6,
		Count
	};

	enum class GamepadType : uint8_t {
		Unknown,
		Standard,
		Xbox360,
		XboxOne,
		PlayStation3,
		PlayStation4,
		PlayStation5,
		SwitchPro,
		JoyConLeft,
		JoyConRight,
		JoyConPair,
		GameCube,
		Steam,
	};

	enum class GamepadSensorType : uint8_t {
		Default,
		Left,
		Right,
	};

	class KeyButton {
	public:
		constexpr explicit KeyButton(KeyCode code) :
			code_(code) {
		}

		bool Trigger() const;
		bool Press() const;
		bool Release() const;
		uint32_t PressedFrames() const;

	private:
		KeyCode code_;
	};

	class MouseButton {
	public:
		constexpr explicit MouseButton(MouseButtonCode code) :
			code_(code) {
		}

		bool Trigger() const;
		bool Press() const;
		bool Release() const;
		uint32_t PressedFrames() const;

	private:
		MouseButtonCode code_;
	};

	class GamepadButton {
	public:
		constexpr GamepadButton(uint32_t gamepadIndex, GamepadButtonCode code) :
			gamepadIndex_(gamepadIndex),
			code_(code) {
		}

		bool Trigger() const;
		bool Press() const;
		bool Release() const;
		uint32_t PressedFrames() const;

	private:
		uint32_t gamepadIndex_;
		GamepadButtonCode code_;
	};

	class Gamepad {
	public:
		static constexpr uint32_t MaxCount = 8u;

		constexpr explicit Gamepad(uint32_t index) :
			index_(index) {
		}

		bool IsConnected() const;
		GamepadType Type() const;
		std::string Name() const;
		Vector2 LeftStick() const;
		Vector2 RightStick() const;
		float LeftTrigger() const;
		float RightTrigger() const;
		GamepadButton Button(GamepadButtonCode code) const;

		bool HasGyroscope(GamepadSensorType sensor = GamepadSensorType::Default) const;
		bool HasAccelerometer(GamepadSensorType sensor = GamepadSensorType::Default) const;
		// 角速度をラジアン毎秒で取得する。
		Vector3 Gyroscope(GamepadSensorType sensor = GamepadSensorType::Default) const;
		// 重力を含む加速度をメートル毎秒毎秒で取得する。
		Vector3 Acceleration(GamepadSensorType sensor = GamepadSensorType::Default) const;
		// 現在のフレームでセンサー値を積分した回転量をラジアンで取得する。
		Vector3 RotationDelta(GamepadSensorType sensor = GamepadSensorType::Default) const;

	private:
		uint32_t index_;
	};

	/// <summary>
	/// 左右別々に接続されたJoy-Conを取得する。
	/// 同じ側が複数接続されている場合はplayerIndexで取得対象を指定する。
	/// </summary>
	namespace JoyCon {
		std::optional<Gamepad> Left(uint32_t playerIndex = 0u);
		std::optional<Gamepad> Right(uint32_t playerIndex = 0u);
	}

	namespace Key {
		inline constexpr KeyButton Space{ KeyCode::Space };
		inline constexpr KeyButton Escape{ KeyCode::Escape };
		inline constexpr KeyButton Enter{ KeyCode::Enter };
		inline constexpr KeyButton Tab{ KeyCode::Tab };
		inline constexpr KeyButton Backspace{ KeyCode::Backspace };
		inline constexpr KeyButton Left{ KeyCode::Left };
		inline constexpr KeyButton Right{ KeyCode::Right };
		inline constexpr KeyButton Up{ KeyCode::Up };
		inline constexpr KeyButton Down{ KeyCode::Down };

		inline constexpr KeyButton A{ KeyCode::A };
		inline constexpr KeyButton B{ KeyCode::B };
		inline constexpr KeyButton C{ KeyCode::C };
		inline constexpr KeyButton D{ KeyCode::D };
		inline constexpr KeyButton E{ KeyCode::E };
		inline constexpr KeyButton F{ KeyCode::F };
		inline constexpr KeyButton G{ KeyCode::G };
		inline constexpr KeyButton H{ KeyCode::H };
		inline constexpr KeyButton I{ KeyCode::I };
		inline constexpr KeyButton J{ KeyCode::J };
		inline constexpr KeyButton K{ KeyCode::K };
		inline constexpr KeyButton L{ KeyCode::L };
		inline constexpr KeyButton M{ KeyCode::M };
		inline constexpr KeyButton N{ KeyCode::N };
		inline constexpr KeyButton O{ KeyCode::O };
		inline constexpr KeyButton P{ KeyCode::P };
		inline constexpr KeyButton Q{ KeyCode::Q };
		inline constexpr KeyButton R{ KeyCode::R };
		inline constexpr KeyButton S{ KeyCode::S };
		inline constexpr KeyButton T{ KeyCode::T };
		inline constexpr KeyButton U{ KeyCode::U };
		inline constexpr KeyButton V{ KeyCode::V };
		inline constexpr KeyButton W{ KeyCode::W };
		inline constexpr KeyButton X{ KeyCode::X };
		inline constexpr KeyButton Y{ KeyCode::Y };
		inline constexpr KeyButton Z{ KeyCode::Z };

		inline constexpr KeyButton Num0{ KeyCode::Num0 };
		inline constexpr KeyButton Num1{ KeyCode::Num1 };
		inline constexpr KeyButton Num2{ KeyCode::Num2 };
		inline constexpr KeyButton Num3{ KeyCode::Num3 };
		inline constexpr KeyButton Num4{ KeyCode::Num4 };
		inline constexpr KeyButton Num5{ KeyCode::Num5 };
		inline constexpr KeyButton Num6{ KeyCode::Num6 };
		inline constexpr KeyButton Num7{ KeyCode::Num7 };
		inline constexpr KeyButton Num8{ KeyCode::Num8 };
		inline constexpr KeyButton Num9{ KeyCode::Num9 };
	}

	namespace Mouse {
		inline constexpr MouseButton Left{ MouseButtonCode::Left };
		inline constexpr MouseButton Right{ MouseButtonCode::Right };
		inline constexpr MouseButton Middle{ MouseButtonCode::Middle };
		inline constexpr MouseButton X1{ MouseButtonCode::X1 };
		inline constexpr MouseButton X2{ MouseButtonCode::X2 };

		Vector2 Delta();
		float WheelDelta();
	}

	namespace Cursor {
		Vector2 Pos();
		Vector2 Delta();
	}

}
