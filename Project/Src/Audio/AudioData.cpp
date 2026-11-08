#include "AudioData.h"

#include <algorithm>
#include <limits>
#include <utility>

using namespace LGF;

AudioData::AudioData(
	IXAudio2& audioEngine,
	std::vector<uint8_t> waveFormat,
	std::vector<uint8_t> samples) :
	waveFormat_(std::move(waveFormat)),
	samples_(std::move(samples)) {
	if (waveFormat_.size() < sizeof(WAVEFORMATEX) ||
		samples_.empty() ||
		samples_.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
		return;
	}

	const WAVEFORMATEX* format =
		reinterpret_cast<const WAVEFORMATEX*>(waveFormat_.data());
	if (format->nSamplesPerSec == 0u || format->nBlockAlign == 0u) {
		return;
	}

	sampleRate_ = format->nSamplesPerSec;
	sampleCount_ = static_cast<uint32_t>(samples_.size() / format->nBlockAlign);
	if (sampleCount_ == 0u) {
		return;
	}

	if (FAILED(audioEngine.CreateSourceVoice(&sourceVoice_, format))) {
		sourceVoice_ = nullptr;
	}
}

AudioData::~AudioData() {
	Reset();
}

bool AudioData::IsValid() const {
	return sourceVoice_ != nullptr;
}

bool AudioData::Play() {
	if (sourceVoice_ == nullptr) {
		return false;
	}

	if (state_ == State::Paused) {
		if (FAILED(sourceVoice_->Start())) {
			return false;
		}
		state_ = State::Playing;
		return true;
	}

	if (IsPlaying()) {
		return true;
	}

	if (!SubmitBuffer() || FAILED(sourceVoice_->Start())) {
		return false;
	}
	state_ = State::Playing;
	return true;
}

bool AudioData::Pause() {
	if (!IsPlaying()) {
		return false;
	}
	if (FAILED(sourceVoice_->Stop())) {
		return false;
	}
	state_ = State::Paused;
	return true;
}

bool AudioData::Stop() {
	if (sourceVoice_ == nullptr) {
		return false;
	}

	const HRESULT stopResult = sourceVoice_->Stop();
	const HRESULT flushResult = sourceVoice_->FlushSourceBuffers();
	state_ = State::Stopped;
	submittedAtSample_ = 0u;
	return SUCCEEDED(stopResult) && SUCCEEDED(flushResult);
}

bool AudioData::IsPlaying() const {
	if (sourceVoice_ == nullptr || state_ != State::Playing) {
		return false;
	}

	XAUDIO2_VOICE_STATE voiceState{};
	sourceVoice_->GetState(&voiceState);
	return voiceState.BuffersQueued > 0u;
}

bool AudioData::IsPaused() const {
	if (sourceVoice_ == nullptr || state_ != State::Paused) {
		return false;
	}

	XAUDIO2_VOICE_STATE voiceState{};
	sourceVoice_->GetState(&voiceState);
	return voiceState.BuffersQueued > 0u;
}

bool AudioData::SetLoop(bool loop) {
	if (sourceVoice_ == nullptr) {
		return false;
	}
	if (loop_ == loop) {
		return true;
	}

	const bool wasPlaying = IsPlaying();
	const bool wasPaused = IsPaused();
	if ((wasPlaying || wasPaused) && !Stop()) {
		return false;
	}

	loop_ = loop;
	if (wasPlaying && !Play()) {
		return false;
	}
	if (wasPaused && (!Play() || !Pause())) {
		return false;
	}
	return true;
}

bool AudioData::IsLoop() const {
	return loop_;
}

bool AudioData::SetVolume(float volume) {
	if (sourceVoice_ == nullptr) {
		return false;
	}
	volume_ = std::clamp(volume, 0.0f, 1.0f);
	return SUCCEEDED(sourceVoice_->SetVolume(volume_));
}

float AudioData::GetVolume() const {
	return volume_;
}

double AudioData::GetDuration() const {
	if (sampleRate_ == 0u) {
		return 0.0;
	}
	return static_cast<double>(sampleCount_) / static_cast<double>(sampleRate_);
}

double AudioData::GetPosition() const {
	if (sourceVoice_ == nullptr ||
		sampleRate_ == 0u ||
		state_ == State::Stopped) {
		return 0.0;
	}

	uint64_t playedSamples = GetPlayedSampleCount();
	if (loop_ && sampleCount_ > 0u) {
		playedSamples %= sampleCount_;
	} else {
		playedSamples = std::min<uint64_t>(playedSamples, sampleCount_);
	}
	return static_cast<double>(playedSamples) / static_cast<double>(sampleRate_);
}

void AudioData::Reset() {
	if (sourceVoice_ != nullptr) {
		sourceVoice_->Stop();
		sourceVoice_->FlushSourceBuffers();
		sourceVoice_->DestroyVoice();
		sourceVoice_ = nullptr;
	}
	state_ = State::Stopped;
	submittedAtSample_ = 0u;
}

bool AudioData::SubmitBuffer() {
	if (sourceVoice_ == nullptr) {
		return false;
	}

	sourceVoice_->Stop();
	sourceVoice_->FlushSourceBuffers();

	XAUDIO2_VOICE_STATE voiceState{};
	sourceVoice_->GetState(&voiceState);
	submittedAtSample_ = voiceState.SamplesPlayed;

	const XAUDIO2_BUFFER buffer{
		.Flags = XAUDIO2_END_OF_STREAM,
		.AudioBytes = static_cast<uint32_t>(samples_.size()),
		.pAudioData = samples_.data(),
		.LoopCount = loop_ ? XAUDIO2_LOOP_INFINITE : 0u,
	};
	return SUCCEEDED(sourceVoice_->SubmitSourceBuffer(&buffer));
}

uint64_t AudioData::GetPlayedSampleCount() const {
	XAUDIO2_VOICE_STATE voiceState{};
	sourceVoice_->GetState(&voiceState);
	return voiceState.SamplesPlayed >= submittedAtSample_
		? voiceState.SamplesPlayed - submittedAtSample_
		: 0u;
}
