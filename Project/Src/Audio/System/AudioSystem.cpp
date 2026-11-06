#include "AudioSystem.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <xaudio2.h>

#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Audio/AudioData.h"
#include "Audio/AudioInternal.h"
#include "Core/Config/RuntimeConfig.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "xaudio2.lib")

using namespace LGF;
using Microsoft::WRL::ComPtr;

namespace {

	AudioSystem* g_audioSystem = nullptr;
	constexpr DWORD AllStreams =
		static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS);
	constexpr DWORD FirstAudioStream =
		static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);

	struct DecodedAudio {
		std::vector<uint8_t> waveFormat;
		std::vector<uint8_t> samples;
	};

	bool AppendSample(IMFSample& sample, std::vector<uint8_t>& destination) {
		ComPtr<IMFMediaBuffer> buffer;
		if (FAILED(sample.ConvertToContiguousBuffer(&buffer))) {
			return false;
		}

		BYTE* source = nullptr;
		DWORD sourceSize = 0u;
		if (FAILED(buffer->Lock(&source, nullptr, &sourceSize))) {
			return false;
		}

		const bool hasCapacity = sourceSize <=
			static_cast<size_t>(std::numeric_limits<uint32_t>::max()) - destination.size();
		if (hasCapacity) {
			destination.insert(destination.end(), source, source + sourceSize);
		}
		buffer->Unlock();
		return hasCapacity;
	}

	bool DecodeAudioFile(
		const std::filesystem::path& filePath,
		DecodedAudio& decodedAudio) {
		ComPtr<IMFSourceReader> reader;
		if (FAILED(MFCreateSourceReaderFromURL(filePath.c_str(), nullptr, &reader))) {
			return false;
		}

		if (FAILED(reader->SetStreamSelection(AllStreams, FALSE)) ||
			FAILED(reader->SetStreamSelection(FirstAudioStream, TRUE))) {
			return false;
		}

		ComPtr<IMFMediaType> requestedType;
		if (FAILED(MFCreateMediaType(&requestedType)) ||
			FAILED(requestedType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
			FAILED(requestedType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
			FAILED(reader->SetCurrentMediaType(
				FirstAudioStream,
				nullptr,
				requestedType.Get()))) {
			return false;
		}

		ComPtr<IMFMediaType> outputType;
		if (FAILED(reader->GetCurrentMediaType(
			FirstAudioStream,
			&outputType))) {
			return false;
		}

		WAVEFORMATEX* waveFormat = nullptr;
		UINT32 waveFormatSize = 0u;
		if (FAILED(MFCreateWaveFormatExFromMFMediaType(
			outputType.Get(),
			&waveFormat,
			&waveFormatSize)) || waveFormat == nullptr) {
			return false;
		}
		decodedAudio.waveFormat.assign(
			reinterpret_cast<uint8_t*>(waveFormat),
			reinterpret_cast<uint8_t*>(waveFormat) + waveFormatSize);
		CoTaskMemFree(waveFormat);

		while (true) {
			DWORD flags = 0u;
			ComPtr<IMFSample> sample;
			if (FAILED(reader->ReadSample(
				FirstAudioStream,
				0u,
				nullptr,
				&flags,
				nullptr,
				&sample))) {
				return false;
			}

			if ((flags & MF_SOURCE_READERF_ERROR) != 0u) {
				return false;
			}
			if (sample && !AppendSample(*sample.Get(), decodedAudio.samples)) {
				return false;
			}
			if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0u) {
				break;
			}
		}

		return !decodedAudio.waveFormat.empty() && !decodedAudio.samples.empty();
	}

}

class AudioSystem::Impl {
public:
	std::filesystem::path assetRoot;
	ComPtr<IXAudio2> audioEngine;
	IXAudio2MasteringVoice* masteringVoice = nullptr;
	std::vector<std::weak_ptr<AudioDetail::AudioData>> audioData;
	std::unordered_map<std::wstring, std::shared_ptr<AudioDetail::AudioData>>
		loadedAudio;
	bool isMediaFoundationStarted = false;
	bool enabled = false;
};

AudioSystem::AudioSystem() :
	impl_(std::make_unique<Impl>()) {
}

AudioSystem::~AudioSystem() = default;

bool AudioSystem::Initialize(
	const std::filesystem::path& assetRoot,
	const AudioConfig& config) {
	impl_->assetRoot = std::filesystem::absolute(assetRoot).lexically_normal();
	impl_->enabled = config.enabled;
	g_audioSystem = this;

	if (!config.enabled) {
		return true;
	}

	if (FAILED(MFStartup(MF_VERSION))) {
		g_audioSystem = nullptr;
		return false;
	}
	impl_->isMediaFoundationStarted = true;

	if (FAILED(XAudio2Create(&impl_->audioEngine)) ||
		FAILED(impl_->audioEngine->CreateMasteringVoice(&impl_->masteringVoice)) ||
		FAILED(impl_->masteringVoice->SetVolume(config.masterVolume))) {
		Finalize();
		return false;
	}
	return true;
}

bool AudioSystem::Finalize() {
	g_audioSystem = nullptr;

	for (const std::weak_ptr<AudioDetail::AudioData>& weakData : impl_->audioData) {
		if (const std::shared_ptr<AudioDetail::AudioData> data = weakData.lock()) {
			data->Reset();
		}
	}
	impl_->audioData.clear();
	impl_->loadedAudio.clear();

	if (impl_->masteringVoice != nullptr) {
		impl_->masteringVoice->DestroyVoice();
		impl_->masteringVoice = nullptr;
	}
	impl_->audioEngine.Reset();

	bool succeeded = true;
	if (impl_->isMediaFoundationStarted) {
		succeeded = SUCCEEDED(MFShutdown());
		impl_->isMediaFoundationStarted = false;
	}
	impl_->enabled = false;
	return succeeded;
}

std::shared_ptr<AudioDetail::AudioData> AudioSystem::Create(
	const std::filesystem::path& filePath) {
	if (!impl_->enabled || !impl_->audioEngine || filePath.empty()) {
		return {};
	}

	const std::filesystem::path resolvedPath = ResolvePath(filePath);
	DecodedAudio decodedAudio;
	if (!DecodeAudioFile(resolvedPath, decodedAudio)) {
		return {};
	}

	auto data = std::make_shared<AudioDetail::AudioData>(
		*impl_->audioEngine.Get(),
		std::move(decodedAudio.waveFormat),
		std::move(decodedAudio.samples));
	if (!data->IsValid()) {
		return {};
	}
	std::erase_if(impl_->audioData, [](const auto& weakData) {
		return weakData.expired();
	});
	impl_->audioData.emplace_back(data);
	return data;
}

bool AudioSystem::Load(const std::filesystem::path& filePath) {
	std::shared_ptr<AudioDetail::AudioData> data = Create(filePath);
	if (!data) {
		return false;
	}
	impl_->loadedAudio.insert_or_assign(
		ResolvePath(filePath).native(),
		std::move(data));
	return true;
}

bool AudioSystem::Unload(const std::filesystem::path& filePath) {
	return impl_->loadedAudio.erase(ResolvePath(filePath).native()) > 0u;
}

void AudioSystem::UnloadAll() {
	impl_->loadedAudio.clear();
}

bool AudioSystem::IsLoaded(const std::filesystem::path& filePath) const {
	return Find(filePath) != nullptr;
}

bool AudioSystem::Play(const std::filesystem::path& filePath, bool loop) {
	const std::shared_ptr<AudioDetail::AudioData> data = Find(filePath);
	return data && data->SetLoop(loop) && data->Play();
}

bool AudioSystem::Pause(const std::filesystem::path& filePath) {
	const std::shared_ptr<AudioDetail::AudioData> data = Find(filePath);
	return data && data->Pause();
}

bool AudioSystem::Stop(const std::filesystem::path& filePath) {
	const std::shared_ptr<AudioDetail::AudioData> data = Find(filePath);
	return data && data->Stop();
}

bool AudioSystem::IsPlaying(const std::filesystem::path& filePath) const {
	const std::shared_ptr<AudioDetail::AudioData> data = Find(filePath);
	return data && data->IsPlaying();
}

bool AudioSystem::SetVolume(
	const std::filesystem::path& filePath,
	float volume) {
	const std::shared_ptr<AudioDetail::AudioData> data = Find(filePath);
	return data && data->SetVolume(volume);
}

std::shared_ptr<AudioDetail::AudioData> AudioSystem::Find(
	const std::filesystem::path& filePath) const {
	if (filePath.empty()) {
		return {};
	}
	const auto iterator = impl_->loadedAudio.find(ResolvePath(filePath).native());
	return iterator != impl_->loadedAudio.end() ? iterator->second : nullptr;
}

std::filesystem::path AudioSystem::ResolvePath(
	const std::filesystem::path& filePath) const {
	if (filePath.empty()) {
		return {};
	}
	return filePath.is_absolute()
		? filePath.lexically_normal()
		: (impl_->assetRoot / filePath).lexically_normal();
}

std::shared_ptr<AudioDetail::AudioData> LGF::AudioDetail::Create(
	const std::filesystem::path& filePath) {
	return g_audioSystem != nullptr ? g_audioSystem->Create(filePath) : nullptr;
}

bool LGF::AudioDetail::Load(const std::filesystem::path& filePath) {
	return g_audioSystem != nullptr && g_audioSystem->Load(filePath);
}

bool LGF::AudioDetail::Unload(const std::filesystem::path& filePath) {
	return g_audioSystem != nullptr && g_audioSystem->Unload(filePath);
}

void LGF::AudioDetail::UnloadAll() {
	if (g_audioSystem != nullptr) {
		g_audioSystem->UnloadAll();
	}
}

bool LGF::AudioDetail::IsLoaded(const std::filesystem::path& filePath) {
	return g_audioSystem != nullptr && g_audioSystem->IsLoaded(filePath);
}

bool LGF::AudioDetail::Play(
	const std::filesystem::path& filePath,
	bool loop) {
	return g_audioSystem != nullptr && g_audioSystem->Play(filePath, loop);
}

bool LGF::AudioDetail::Pause(const std::filesystem::path& filePath) {
	return g_audioSystem != nullptr && g_audioSystem->Pause(filePath);
}

bool LGF::AudioDetail::Stop(const std::filesystem::path& filePath) {
	return g_audioSystem != nullptr && g_audioSystem->Stop(filePath);
}

bool LGF::AudioDetail::IsPlaying(const std::filesystem::path& filePath) {
	return g_audioSystem != nullptr && g_audioSystem->IsPlaying(filePath);
}

bool LGF::AudioDetail::SetVolume(
	const std::filesystem::path& filePath,
	float volume) {
	return g_audioSystem != nullptr && g_audioSystem->SetVolume(filePath, volume);
}
