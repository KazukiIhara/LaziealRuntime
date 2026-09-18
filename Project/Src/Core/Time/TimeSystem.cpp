#include "TimeSystem.h"

#include <algorithm>

using namespace LGF;

namespace {

	constexpr double kMaximumDeltaTime = 0.1;

}

TimeSystem::TimeSystem() = default;

TimeSystem::~TimeSystem() = default;

bool TimeSystem::Initialize() {
	previousFrameTime_ = Clock::now();
	deltaTime_ = 0.0;
	isFirstFrame_ = true;
	return true;
}

bool TimeSystem::Finalize() {
	deltaTime_ = 0.0;
	isFirstFrame_ = true;
	return true;
}

void TimeSystem::BeginFrame() {
	const Clock::time_point currentFrameTime = Clock::now();
	if (isFirstFrame_) {
		previousFrameTime_ = currentFrameTime;
		deltaTime_ = 0.0;
		isFirstFrame_ = false;
		return;
	}

	const double elapsedSeconds =
		std::chrono::duration<double>(currentFrameTime - previousFrameTime_).count();
	previousFrameTime_ = currentFrameTime;
	deltaTime_ = std::clamp(elapsedSeconds, 0.0, kMaximumDeltaTime);
}

double TimeSystem::GetDeltaTime() const {
	return deltaTime_;
}
