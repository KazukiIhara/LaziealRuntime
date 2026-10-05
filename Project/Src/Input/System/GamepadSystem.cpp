#include "GamepadSystem.h"

#include <algorithm>
#include <array>
#include <limits>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>

#include "Core/Config/RuntimeConfig.h"

using namespace LGF;

namespace {

	constexpr std::size_t kButtonCount = static_cast<std::size_t>(GamepadButtonCode::Count);
	constexpr std::size_t kSensorCount = 3u;
	constexpr double kNanosecondsToSeconds = 1.0 / 1'000'000'000.0;
	constexpr double kMaximumSensorInterval = 0.1;

	constexpr std::array<SDL_GamepadButton, kButtonCount> kSDLButtons{
		SDL_GAMEPAD_BUTTON_SOUTH,
		SDL_GAMEPAD_BUTTON_EAST,
		SDL_GAMEPAD_BUTTON_WEST,
		SDL_GAMEPAD_BUTTON_NORTH,
		SDL_GAMEPAD_BUTTON_BACK,
		SDL_GAMEPAD_BUTTON_GUIDE,
		SDL_GAMEPAD_BUTTON_START,
		SDL_GAMEPAD_BUTTON_LEFT_STICK,
		SDL_GAMEPAD_BUTTON_RIGHT_STICK,
		SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
		SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
		SDL_GAMEPAD_BUTTON_DPAD_UP,
		SDL_GAMEPAD_BUTTON_DPAD_DOWN,
		SDL_GAMEPAD_BUTTON_DPAD_LEFT,
		SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
		SDL_GAMEPAD_BUTTON_MISC1,
		SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
		SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
		SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
		SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
		SDL_GAMEPAD_BUTTON_TOUCHPAD,
		SDL_GAMEPAD_BUTTON_MISC2,
		SDL_GAMEPAD_BUTTON_MISC3,
		SDL_GAMEPAD_BUTTON_MISC4,
		SDL_GAMEPAD_BUTTON_MISC5,
		SDL_GAMEPAD_BUTTON_MISC6,
	};

	std::size_t ToIndex(GamepadButtonCode code) {
		return static_cast<std::size_t>(code);
	}

	std::size_t ToIndex(GamepadSensorType sensor) {
		return static_cast<std::size_t>(sensor);
	}

	float NormalizeStickAxis(Sint16 value) {
		if (value >= 0) {
			return static_cast<float>(value) /
				static_cast<float>(std::numeric_limits<Sint16>::max());
		}
		return static_cast<float>(value) /
			-static_cast<float>(std::numeric_limits<Sint16>::min());
	}

	float NormalizeTriggerAxis(Sint16 value) {
		return std::clamp(
			static_cast<float>(value) /
				static_cast<float>(std::numeric_limits<Sint16>::max()),
			0.0f,
			1.0f);
	}

	GamepadType ToGamepadType(SDL_GamepadType type) {
		switch (type) {
		case SDL_GAMEPAD_TYPE_STANDARD:
			return GamepadType::Standard;
		case SDL_GAMEPAD_TYPE_XBOX360:
			return GamepadType::Xbox360;
		case SDL_GAMEPAD_TYPE_XBOXONE:
			return GamepadType::XboxOne;
		case SDL_GAMEPAD_TYPE_PS3:
			return GamepadType::PlayStation3;
		case SDL_GAMEPAD_TYPE_PS4:
			return GamepadType::PlayStation4;
		case SDL_GAMEPAD_TYPE_PS5:
			return GamepadType::PlayStation5;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
			return GamepadType::SwitchPro;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
			return GamepadType::JoyConLeft;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
			return GamepadType::JoyConRight;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
			return GamepadType::JoyConPair;
		case SDL_GAMEPAD_TYPE_GAMECUBE:
			return GamepadType::GameCube;
		case SDL_GAMEPAD_TYPE_STEAM:
			return GamepadType::Steam;
		default:
			return GamepadType::Unknown;
		}
	}

	bool IsLeftJoyCon(GamepadType type) {
		return type == GamepadType::JoyConLeft;
	}

	bool IsRightJoyCon(GamepadType type) {
		return type == GamepadType::JoyConRight;
	}

}

class GamepadSystem::Impl {
public:
	struct SensorState {
		bool hasGyroscope = false;
		bool hasAccelerometer = false;
		Vector3 gyroscope{};
		Vector3 acceleration{};
		Vector3 rotationDelta{};
		Vector3 previousGyroscope{};
		uint64_t previousGyroscopeTimestamp = 0u;
	};

	struct Slot {
		SDL_Gamepad* handle = nullptr;
		SDL_JoystickID instanceId = 0u;
		GamepadType type = GamepadType::Unknown;
		std::string name{};
		std::array<bool, kButtonCount> currentButtons{};
		std::array<bool, kButtonCount> previousButtons{};
		std::array<uint32_t, kButtonCount> pressedFrames{};
		Vector2 leftStick{};
		Vector2 rightStick{};
		float leftTrigger = 0.0f;
		float rightTrigger = 0.0f;
		std::array<SensorState, kSensorCount> sensors{};
	};

	bool Initialize(const GamepadConfig& config) {
		isEnabled_ = config.enabled;
		if (!isEnabled_) {
			return true;
		}

		(void)SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1");
		(void)SDL_SetHint(
			SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS,
			config.combineJoyCons ? "1" : "0");
		(void)SDL_SetHint(
			SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS,
			config.verticalJoyCons ? "1" : "0");
		(void)SDL_SetHint(
			SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
			config.backgroundInput ? "1" : "0");

		if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
			isEnabled_ = false;
			return false;
		}
		isInitialized_ = true;

		int gamepadCount = 0;
		SDL_JoystickID* gamepadIds = SDL_GetGamepads(&gamepadCount);
		if (gamepadIds == nullptr && gamepadCount != 0) {
			Finalize();
			return false;
		}
		for (int index = 0; index < gamepadCount; ++index) {
			OpenGamepad(gamepadIds[index]);
		}
		SDL_free(gamepadIds);
		return true;
	}

	bool Finalize() {
		for (Slot& slot : slots_) {
			if (slot.handle != nullptr) {
				SDL_CloseGamepad(slot.handle);
			}
			slot = {};
		}

		if (isInitialized_) {
			SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
		}
		isInitialized_ = false;
		isEnabled_ = false;
		return true;
	}

	void BeginFrame() {
		if (!isInitialized_) {
			return;
		}

		for (Slot& slot : slots_) {
			for (SensorState& sensor : slot.sensors) {
				sensor.rotationDelta = {};
			}
		}

		SDL_PumpEvents();
		ProcessEvents();
		SDL_UpdateGamepads();

		for (Slot& slot : slots_) {
			if (slot.handle == nullptr || !SDL_GamepadConnected(slot.handle)) {
				continue;
			}
			UpdateState(slot);
		}
	}

	void EndFrame() {
		for (Slot& slot : slots_) {
			slot.previousButtons = slot.currentButtons;
		}
	}

	const Slot* GetSlot(uint32_t index) const {
		if (index >= slots_.size()) {
			return nullptr;
		}
		return &slots_[index];
	}

	Slot* GetSlotByInstanceId(SDL_JoystickID instanceId) {
		for (Slot& slot : slots_) {
			if (slot.handle != nullptr && slot.instanceId == instanceId) {
				return &slot;
			}
		}
		return nullptr;
	}

	const SensorState* ResolveSensor(
		const Slot& slot,
		GamepadSensorType sensorType,
		bool isGyroscope) const {
		const auto isAvailable = [isGyroscope](const SensorState& sensor) {
			return isGyroscope ? sensor.hasGyroscope : sensor.hasAccelerometer;
		};

		const SensorState& requestedSensor = slot.sensors[ToIndex(sensorType)];
		if (isAvailable(requestedSensor)) {
			return &requestedSensor;
		}

		const SensorState& defaultSensor =
			slot.sensors[ToIndex(GamepadSensorType::Default)];
		if (sensorType == GamepadSensorType::Left && IsLeftJoyCon(slot.type) &&
			isAvailable(defaultSensor)) {
			return &defaultSensor;
		}
		if (sensorType == GamepadSensorType::Right && IsRightJoyCon(slot.type) &&
			isAvailable(defaultSensor)) {
			return &defaultSensor;
		}

		if (sensorType != GamepadSensorType::Default) {
			return nullptr;
		}

		const SensorState& rightSensor = slot.sensors[ToIndex(GamepadSensorType::Right)];
		if (isAvailable(rightSensor)) {
			return &rightSensor;
		}
		const SensorState& leftSensor = slot.sensors[ToIndex(GamepadSensorType::Left)];
		return isAvailable(leftSensor) ? &leftSensor : nullptr;
	}

	std::array<Slot, Gamepad::MaxCount> slots_{};

private:
	void ProcessEvents() {
		std::array<SDL_Event, 64u> events{};
		while (true) {
			const int eventCount = SDL_PeepEvents(
				events.data(),
				static_cast<int>(events.size()),
				SDL_GETEVENT,
				SDL_EVENT_GAMEPAD_AXIS_MOTION,
				SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED);
			if (eventCount <= 0) {
				break;
			}

			for (int index = 0; index < eventCount; ++index) {
				const SDL_Event& event = events[static_cast<std::size_t>(index)];
				switch (event.type) {
				case SDL_EVENT_GAMEPAD_ADDED:
					OpenGamepad(event.gdevice.which);
					break;
				case SDL_EVENT_GAMEPAD_REMOVED:
					CloseGamepad(event.gdevice.which);
					break;
				case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
					HandleSensorEvent(event.gsensor);
					break;
				default:
					break;
				}
			}
		}
	}

	void OpenGamepad(SDL_JoystickID instanceId) {
		if (GetSlotByInstanceId(instanceId) != nullptr) {
			return;
		}

		Slot* targetSlot = nullptr;
		for (Slot& slot : slots_) {
			if (slot.handle == nullptr) {
				targetSlot = &slot;
				break;
			}
		}
		if (targetSlot == nullptr) {
			return;
		}

		SDL_Gamepad* gamepad = SDL_OpenGamepad(instanceId);
		if (gamepad == nullptr) {
			return;
		}

		*targetSlot = {};
		targetSlot->handle = gamepad;
		targetSlot->instanceId = instanceId;
		targetSlot->type = ToGamepadType(SDL_GetGamepadType(gamepad));
		if (const char* name = SDL_GetGamepadName(gamepad)) {
			targetSlot->name = name;
		}

		EnableSensor(*targetSlot, SDL_SENSOR_GYRO, GamepadSensorType::Default, true);
		EnableSensor(*targetSlot, SDL_SENSOR_ACCEL, GamepadSensorType::Default, false);
		EnableSensor(*targetSlot, SDL_SENSOR_GYRO_L, GamepadSensorType::Left, true);
		EnableSensor(*targetSlot, SDL_SENSOR_ACCEL_L, GamepadSensorType::Left, false);
		EnableSensor(*targetSlot, SDL_SENSOR_GYRO_R, GamepadSensorType::Right, true);
		EnableSensor(*targetSlot, SDL_SENSOR_ACCEL_R, GamepadSensorType::Right, false);
		UpdateState(*targetSlot);
		targetSlot->previousButtons = targetSlot->currentButtons;
	}

	void CloseGamepad(SDL_JoystickID instanceId) {
		Slot* slot = GetSlotByInstanceId(instanceId);
		if (slot == nullptr) {
			return;
		}

		SDL_CloseGamepad(slot->handle);
		slot->handle = nullptr;
		slot->instanceId = 0u;
		slot->type = GamepadType::Unknown;
		slot->name.clear();
		slot->currentButtons.fill(false);
		slot->pressedFrames.fill(0u);
		slot->leftStick = {};
		slot->rightStick = {};
		slot->leftTrigger = 0.0f;
		slot->rightTrigger = 0.0f;
		slot->sensors = {};
	}

	void EnableSensor(
		Slot& slot,
		SDL_SensorType sensorType,
		GamepadSensorType gamepadSensorType,
		bool isGyroscope) {
		if (!SDL_GamepadHasSensor(slot.handle, sensorType)) {
			return;
		}
		if (!SDL_SetGamepadSensorEnabled(slot.handle, sensorType, true)) {
			return;
		}

		SensorState& sensor = slot.sensors[ToIndex(gamepadSensorType)];
		if (isGyroscope) {
			sensor.hasGyroscope = true;
		} else {
			sensor.hasAccelerometer = true;
		}
	}

	void UpdateState(Slot& slot) {
		for (std::size_t index = 0; index < kButtonCount; ++index) {
			slot.currentButtons[index] = SDL_GetGamepadButton(slot.handle, kSDLButtons[index]);
			if (slot.currentButtons[index]) {
				++slot.pressedFrames[index];
			} else {
				slot.pressedFrames[index] = 0u;
			}
		}

		slot.leftStick = {
			NormalizeStickAxis(SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_LEFTX)),
			-NormalizeStickAxis(SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_LEFTY)),
		};
		slot.rightStick = {
			NormalizeStickAxis(SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_RIGHTX)),
			-NormalizeStickAxis(SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_RIGHTY)),
		};
		slot.leftTrigger = NormalizeTriggerAxis(
			SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
		slot.rightTrigger = NormalizeTriggerAxis(
			SDL_GetGamepadAxis(slot.handle, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
	}

	void HandleSensorEvent(const SDL_GamepadSensorEvent& event) {
		Slot* slot = GetSlotByInstanceId(event.which);
		if (slot == nullptr) {
			return;
		}

		GamepadSensorType gamepadSensorType{};
		bool isGyroscope = false;
		switch (static_cast<SDL_SensorType>(event.sensor)) {
		case SDL_SENSOR_GYRO:
			gamepadSensorType = GamepadSensorType::Default;
			isGyroscope = true;
			break;
		case SDL_SENSOR_ACCEL:
			gamepadSensorType = GamepadSensorType::Default;
			break;
		case SDL_SENSOR_GYRO_L:
			gamepadSensorType = GamepadSensorType::Left;
			isGyroscope = true;
			break;
		case SDL_SENSOR_ACCEL_L:
			gamepadSensorType = GamepadSensorType::Left;
			break;
		case SDL_SENSOR_GYRO_R:
			gamepadSensorType = GamepadSensorType::Right;
			isGyroscope = true;
			break;
		case SDL_SENSOR_ACCEL_R:
			gamepadSensorType = GamepadSensorType::Right;
			break;
		default:
			return;
		}

		SensorState& sensor = slot->sensors[ToIndex(gamepadSensorType)];
		const Vector3 sensorValue{ event.data[0], event.data[1], event.data[2] };
		if (!isGyroscope) {
			sensor.acceleration = sensorValue;
			return;
		}

		const uint64_t timestamp = event.sensor_timestamp != 0u
			? event.sensor_timestamp
			: event.timestamp;
		if (sensor.previousGyroscopeTimestamp != 0u &&
			timestamp > sensor.previousGyroscopeTimestamp) {
			const double elapsedSeconds = static_cast<double>(
				timestamp - sensor.previousGyroscopeTimestamp) * kNanosecondsToSeconds;
			if (elapsedSeconds <= kMaximumSensorInterval) {
				const float halfElapsedSeconds = static_cast<float>(elapsedSeconds * 0.5);
				sensor.rotationDelta +=
					(sensor.previousGyroscope + sensorValue) * halfElapsedSeconds;
			}
		}
		sensor.previousGyroscope = sensorValue;
		sensor.previousGyroscopeTimestamp = timestamp;
		sensor.gyroscope = sensorValue;
	}

	bool isEnabled_ = false;
	bool isInitialized_ = false;
};

GamepadSystem::GamepadSystem() :
	impl_(std::make_unique<Impl>()) {
}

GamepadSystem::~GamepadSystem() = default;

bool GamepadSystem::Initialize(const GamepadConfig& config) {
	return impl_->Initialize(config);
}

bool GamepadSystem::Finalize() {
	return impl_->Finalize();
}

void GamepadSystem::BeginFrame() {
	impl_->BeginFrame();
}

void GamepadSystem::EndFrame() {
	impl_->EndFrame();
}

bool GamepadSystem::IsConnected(uint32_t gamepadIndex) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr && slot->handle != nullptr;
}

GamepadType GamepadSystem::GetType(uint32_t gamepadIndex) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr ? slot->type : GamepadType::Unknown;
}

std::string GamepadSystem::GetName(uint32_t gamepadIndex) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr ? slot->name : std::string{};
}

Vector2 GamepadSystem::GetLeftStick(uint32_t gamepadIndex) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr ? slot->leftStick : Vector2{};
}

Vector2 GamepadSystem::GetRightStick(uint32_t gamepadIndex) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr ? slot->rightStick : Vector2{};
}

float GamepadSystem::GetLeftTrigger(uint32_t gamepadIndex) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr ? slot->leftTrigger : 0.0f;
}

float GamepadSystem::GetRightTrigger(uint32_t gamepadIndex) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr ? slot->rightTrigger : 0.0f;
}

bool GamepadSystem::IsTriggered(uint32_t gamepadIndex, GamepadButtonCode code) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	if (slot == nullptr || ToIndex(code) >= kButtonCount) {
		return false;
	}
	const std::size_t index = ToIndex(code);
	return slot->currentButtons[index] && !slot->previousButtons[index];
}

bool GamepadSystem::IsPressed(uint32_t gamepadIndex, GamepadButtonCode code) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr && ToIndex(code) < kButtonCount &&
		slot->currentButtons[ToIndex(code)];
}

bool GamepadSystem::IsReleased(uint32_t gamepadIndex, GamepadButtonCode code) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	if (slot == nullptr || ToIndex(code) >= kButtonCount) {
		return false;
	}
	const std::size_t index = ToIndex(code);
	return !slot->currentButtons[index] && slot->previousButtons[index];
}

uint32_t GamepadSystem::GetPressedFrames(uint32_t gamepadIndex, GamepadButtonCode code) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr && ToIndex(code) < kButtonCount
		? slot->pressedFrames[ToIndex(code)]
		: 0u;
}

bool GamepadSystem::HasGyroscope(uint32_t gamepadIndex, GamepadSensorType sensor) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr && impl_->ResolveSensor(*slot, sensor, true) != nullptr;
}

bool GamepadSystem::HasAccelerometer(uint32_t gamepadIndex, GamepadSensorType sensor) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	return slot != nullptr && impl_->ResolveSensor(*slot, sensor, false) != nullptr;
}

Vector3 GamepadSystem::GetGyroscope(uint32_t gamepadIndex, GamepadSensorType sensor) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	if (slot == nullptr) {
		return {};
	}
	const Impl::SensorState* state = impl_->ResolveSensor(*slot, sensor, true);
	return state != nullptr ? state->gyroscope : Vector3{};
}

Vector3 GamepadSystem::GetAcceleration(uint32_t gamepadIndex, GamepadSensorType sensor) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	if (slot == nullptr) {
		return {};
	}
	const Impl::SensorState* state = impl_->ResolveSensor(*slot, sensor, false);
	return state != nullptr ? state->acceleration : Vector3{};
}

Vector3 GamepadSystem::GetRotationDelta(uint32_t gamepadIndex, GamepadSensorType sensor) const {
	const Impl::Slot* slot = impl_->GetSlot(gamepadIndex);
	if (slot == nullptr) {
		return {};
	}
	const Impl::SensorState* state = impl_->ResolveSensor(*slot, sensor, true);
	return state != nullptr ? state->rotationDelta : Vector3{};
}
