#pragma once

#include <xaudio2.h>

#include <cstdint>
#include <vector>

namespace LGF::AudioDetail {

	class AudioData {
	public:
		AudioData(
			IXAudio2& audioEngine,
			std::vector<uint8_t> waveFormat,
			std::vector<uint8_t> samples);
		~AudioData();

		AudioData(const AudioData&) = delete;
		AudioData& operator=(const AudioData&) = delete;

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

		void Reset();

	private:
		enum class State : uint8_t {
			Stopped,
			Playing,
			Paused,
		};

		bool SubmitBuffer();
		uint64_t GetPlayedSampleCount() const;

		IXAudio2SourceVoice* sourceVoice_ = nullptr;
		std::vector<uint8_t> waveFormat_;
		std::vector<uint8_t> samples_;
		uint64_t submittedAtSample_ = 0u;
		uint32_t sampleRate_ = 0u;
		uint32_t sampleCount_ = 0u;
		float volume_ = 1.0f;
		bool loop_ = false;
		State state_ = State::Stopped;
	};

}
