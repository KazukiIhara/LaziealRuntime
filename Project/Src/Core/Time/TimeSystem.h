#pragma once

#include <chrono>

namespace LGF {

	/// <summary>
	/// フレーム間の経過時間を管理するシステム
	/// </summary>
	class TimeSystem final {
	public:
		TimeSystem();
		~TimeSystem();

		bool Initialize();
		bool Finalize();

		void BeginFrame();

		double GetDeltaTime() const;

	private:
		using Clock = std::chrono::steady_clock;

		Clock::time_point previousFrameTime_{};
		double deltaTime_ = 0.0;
		bool isFirstFrame_ = true;
	};

}
