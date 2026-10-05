#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "LGF/Input.h"

namespace LGF {

	struct GamepadConfig;

	class GamepadSystem {
	public:
		GamepadSystem();
		~GamepadSystem();

		bool Initialize(const GamepadConfig& config);
		bool Finalize();

		void BeginFrame();
		void EndFrame();

		bool IsConnected(uint32_t gamepadIndex) const;
		GamepadType GetType(uint32_t gamepadIndex) const;
		std::string GetName(uint32_t gamepadIndex) const;
		Vector2 GetLeftStick(uint32_t gamepadIndex) const;
		Vector2 GetRightStick(uint32_t gamepadIndex) const;
		float GetLeftTrigger(uint32_t gamepadIndex) const;
		float GetRightTrigger(uint32_t gamepadIndex) const;

		bool IsTriggered(uint32_t gamepadIndex, GamepadButtonCode code) const;
		bool IsPressed(uint32_t gamepadIndex, GamepadButtonCode code) const;
		bool IsReleased(uint32_t gamepadIndex, GamepadButtonCode code) const;
		uint32_t GetPressedFrames(uint32_t gamepadIndex, GamepadButtonCode code) const;

		bool HasGyroscope(uint32_t gamepadIndex, GamepadSensorType sensor) const;
		bool HasAccelerometer(uint32_t gamepadIndex, GamepadSensorType sensor) const;
		Vector3 GetGyroscope(uint32_t gamepadIndex, GamepadSensorType sensor) const;
		Vector3 GetAcceleration(uint32_t gamepadIndex, GamepadSensorType sensor) const;
		Vector3 GetRotationDelta(uint32_t gamepadIndex, GamepadSensorType sensor) const;

	private:
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

}
