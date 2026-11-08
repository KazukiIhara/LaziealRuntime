#pragma once

#include <wrl/client.h>
#include <xaudio2.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace LGF {

	struct AudioConfig;

	namespace AudioDetail {
		class AudioData;
	}

	class AudioSystem {
	public:
		AudioSystem();
		~AudioSystem();

		bool Initialize(const std::filesystem::path& assetRoot, const AudioConfig& config);
		bool Finalize();

		std::shared_ptr<AudioDetail::AudioData> Create(const std::filesystem::path& filePath);
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
		std::shared_ptr<AudioDetail::AudioData> Find(const std::filesystem::path& filePath) const;
		std::filesystem::path ResolvePath(const std::filesystem::path& filePath) const;

		std::filesystem::path assetRoot_;
		Microsoft::WRL::ComPtr<IXAudio2> audioEngine_;
		IXAudio2MasteringVoice* masteringVoice_ = nullptr;
		std::vector<std::weak_ptr<AudioDetail::AudioData>> audioData_;
		std::unordered_map<std::wstring, std::shared_ptr<AudioDetail::AudioData>> loadedAudio_;
		bool isMediaFoundationStarted_ = false;
		bool isEnabled_ = false;
	};

}
