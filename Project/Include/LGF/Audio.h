#pragma once

#include <filesystem>
#include <memory>

namespace LGF {

	class AudioData;

	/// <summary>
	/// WAV、MP3形式の音声を読み込み、再生を制御する。
	/// 相対パスはRuntimeSetting.iniのassetRootを基準に解決される。
	/// </summary>
	class Audio {
	public:
		Audio() = default;
		explicit Audio(const std::filesystem::path& filePath);
		~Audio();

		bool IsValid() const;

		bool Play();
		bool Pause();
		bool Stop();
		bool IsPlaying() const;
		bool IsPaused() const;

		bool SetLoop(bool loop);
		bool IsLoop() const;
		bool SetVolume(float volume);
		float GetVolume() const;

		double GetDuration() const;
		double GetPosition() const;

	private:
		std::shared_ptr<AudioData> data_;
	};

}
