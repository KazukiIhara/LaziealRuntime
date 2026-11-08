#include "AudioSystem.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <xaudio2.h>

#include <limits>
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
	constexpr DWORD kAllStreams =
		static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS);
	constexpr DWORD kFirstAudioStream =
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

		if (FAILED(reader->SetStreamSelection(kAllStreams, FALSE)) ||
			FAILED(reader->SetStreamSelection(kFirstAudioStream, TRUE))) {
			return false;
		}

		ComPtr<IMFMediaType> requestedType;
		if (FAILED(MFCreateMediaType(&requestedType)) ||
			FAILED(requestedType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
			FAILED(requestedType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
			FAILED(reader->SetCurrentMediaType(
				kFirstAudioStream,
				nullptr,
				requestedType.Get()))) {
			return false;
		}

		ComPtr<IMFMediaType> outputType;
		if (FAILED(reader->GetCurrentMediaType(
			kFirstAudioStream,
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
				kFirstAudioStream,
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

AudioSystem::AudioSystem() {
}

AudioSystem::~AudioSystem() = default;

bool AudioSystem::Initialize(
	const std::filesystem::path& assetRoot,
	const AudioConfig& config) {
	assetRoot_ = std::filesystem::absolute(assetRoot).lexically_normal();
	isEnabled_ = config.enabled;
	g_audioSystem = this;

	if (!isEnabled_) {
		return true;
	}

	if (FAILED(MFStartup(MF_VERSION))) {
		g_audioSystem = nullptr;
		return false;
	}
	isMediaFoundationStarted_ = true;

	if (FAILED(XAudio2Create(&audioEngine_)) ||
		FAILED(audioEngine_->CreateMasteringVoice(&masteringVoice_)) ||
		FAILED(masteringVoice_->SetVolume(config.masterVolume))) {
		Finalize();
		return false;
	}
	return true;
}

bool AudioSystem::Finalize() {
	g_audioSystem = nullptr;

	for (const std::weak_ptr<AudioData>& weakData : audioData_) {
		if (const std::shared_ptr<AudioData> data = weakData.lock()) {
			data->Reset();
		}
	}
	audioData_.clear();

	if (masteringVoice_ != nullptr) {
		masteringVoice_->DestroyVoice();
		masteringVoice_ = nullptr;
	}
	audioEngine_.Reset();

	bool succeeded = true;
	if (isMediaFoundationStarted_) {
		succeeded = SUCCEEDED(MFShutdown());
		isMediaFoundationStarted_ = false;
	}
	isEnabled_ = false;
	return succeeded;
}

std::shared_ptr<AudioData> AudioSystem::Load(
	const std::filesystem::path& filePath) {
	if (!isEnabled_ || !audioEngine_ || filePath.empty()) {
		return {};
	}

	const std::filesystem::path resolvedPath = ResolvePath(filePath);
	DecodedAudio decodedAudio;
	if (!DecodeAudioFile(resolvedPath, decodedAudio)) {
		return {};
	}

	std::shared_ptr<AudioData> data =
		std::make_shared<AudioData>(
			*audioEngine_.Get(),
			std::move(decodedAudio.waveFormat),
			std::move(decodedAudio.samples));
	if (!data->IsValid()) {
		return {};
	}
	std::erase_if(audioData_, [](const auto& weakData) {
		return weakData.expired();
	});
	audioData_.emplace_back(data);
	return data;
}

std::filesystem::path AudioSystem::ResolvePath(
	const std::filesystem::path& filePath) const {
	if (filePath.empty()) {
		return {};
	}
	return filePath.is_absolute()
		? filePath.lexically_normal()
		: (assetRoot_ / filePath).lexically_normal();
}

std::shared_ptr<AudioData> LGF::AudioInternal::Load(
	const std::filesystem::path& filePath) {
	return g_audioSystem != nullptr ? g_audioSystem->Load(filePath) : nullptr;
}
