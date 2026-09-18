#pragma once

#include <memory>

namespace LGF {

	struct RuntimeConfig;
	class GraphicsSystem;
	class InputSystem;
	class TimeSystem;
	class WindowSystem;

	/// <summary>
	/// Runtime層のWindow、Input、Timeと描画フレームワークを接続する。
	/// </summary>
	class Framework {
	public:
		Framework();
		~Framework();

		bool Initialize(const RuntimeConfig& config);
		bool Update();
		bool Finalize();

		double GetDeltaTime() const;

	private:
		void BeginFrame();
		void EndFrame();

		std::unique_ptr<WindowSystem> windowSystem_;
		std::unique_ptr<GraphicsSystem> graphicsSystem_;
		std::unique_ptr<InputSystem> inputSystem_;
		std::unique_ptr<TimeSystem> timeSystem_;
		bool isFrameStarted_ = false;
	};

}
