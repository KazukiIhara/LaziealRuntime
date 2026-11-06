#pragma once

#include <filesystem>
#include <memory>

namespace LGF {

	struct AudioConfig;

	namespace AudioDetail {
		class AudioData;
	}

	class AudioSystem {
	public:
		AudioSystem();
		~AudioSystem();

		bool Initialize(
			const std::filesystem::path& assetRoot,
			const AudioConfig& config);
		bool Finalize();

		std::shared_ptr<AudioDetail::AudioData> Create(
			const std::filesystem::path& filePath);
		bool Load(const std::filesystem::path& filePath);
		bool Unload(const std::filesystem::path& filePath);
		void UnloadAll();
		bool IsLoaded(const std::filesystem::path& filePath) const;
		bool Play(const std::filesystem::path& filePath, bool loop);
		bool Pause(const std::filesystem::path& filePath);
		bool Stop(const std::filesystem::path& filePath);
		bool IsPlaying(const std::filesystem::path& filePath) const;
		bool SetVolume(const std::filesystem::path& filePath, float volume);

	private:
		std::shared_ptr<AudioDetail::AudioData> Find(
			const std::filesystem::path& filePath) const;
		std::filesystem::path ResolvePath(
			const std::filesystem::path& filePath) const;

		class Impl;
		std::unique_ptr<Impl> impl_;
	};

}
