#pragma once

#include <filesystem>
#include <memory>

namespace LGF {

	namespace AudioDetail {
		class AudioData;
	}

	/// <summary>
	/// WAV、MP3形式の音声を読み込み、再生を制御する。
	/// 相対パスはRuntimeSetting.iniのassetRootを基準に解決される。
	/// </summary>
	class Audio {
	public:
		Audio() = default;
		explicit Audio(const std::filesystem::path& filePath);

		/// <summary>
		/// 音声を読み込み、ファイルパスを指定する静的関数から操作できるようにする。
		/// 同じパスが既に存在する場合は、読み込み成功後に置き換える。
		/// </summary>
		static bool Load(const std::filesystem::path& filePath);
		static bool Unload(const std::filesystem::path& filePath);
		static void UnloadAll();
		static bool IsLoaded(const std::filesystem::path& filePath);
		static bool Play(
			const std::filesystem::path& filePath,
			bool loop = false);
		static bool Pause(const std::filesystem::path& filePath);
		static bool Stop(const std::filesystem::path& filePath);
		static bool IsPlaying(const std::filesystem::path& filePath);
		static bool SetVolume(
			const std::filesystem::path& filePath,
			float volume);

		bool Open(const std::filesystem::path& filePath);
		void Release();

		bool IsValid() const;
		explicit operator bool() const;

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
		std::shared_ptr<AudioDetail::AudioData> data_;
	};

}
