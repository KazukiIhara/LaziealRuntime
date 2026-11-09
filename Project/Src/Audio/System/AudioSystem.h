#pragma once

#include <wrl/client.h>
#include <xaudio2.h>

#include <filesystem>
#include <memory>
#include <vector>

namespace LGF {

	struct AudioConfig;
	class AudioData;

	/// <summary>
	/// 音声再生
	/// </summary>
	class AudioSystem {
	public:
		AudioSystem();
		~AudioSystem();

		bool Initialize(const std::filesystem::path& assetRoot, const AudioConfig& config);
		bool Finalize();

		std::shared_ptr<AudioData> Load(const std::filesystem::path& filePath);

	private:
		std::filesystem::path ResolvePath(const std::filesystem::path& filePath) const;

		std::filesystem::path assetRoot_;
		Microsoft::WRL::ComPtr<IXAudio2> audioEngine_;
		IXAudio2MasteringVoice* masteringVoice_ = nullptr;
		std::vector<std::weak_ptr<AudioData>> audioData_;
		bool isMediaFoundationStarted_ = false;
		bool isEnabled_ = false;
	};

}
