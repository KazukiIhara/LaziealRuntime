#pragma once

#include "LGF/Input.h"

namespace LGF::Input::Internal {

	bool IsTriggered(KeyCode code);
	bool IsPressed(KeyCode code);
	bool IsReleased(KeyCode code);
	uint32_t GetPressedFrames(KeyCode code);

	bool IsTriggered(MouseButtonCode code);
	bool IsPressed(MouseButtonCode code);
	bool IsReleased(MouseButtonCode code);
	uint32_t GetPressedFrames(MouseButtonCode code);

	Vector2 GetMouseDelta();
	float GetMouseWheelDelta();
	Vector2 GetCursorPos();
	Vector2 GetCursorDelta();

}
