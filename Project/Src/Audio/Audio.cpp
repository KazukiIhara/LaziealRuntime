#include "LGF/Audio.h"

#include "Audio/AudioData.h"
#include "Audio/AudioInternal.h"

using namespace LGF;

Audio::Audio(const std::filesystem::path& filePath) :
	data_(AudioInternal::Load(filePath)) {
}

Audio::~Audio() = default;

bool Audio::IsValid() const {
	return data_ && data_->IsValid();
}

bool Audio::Play() {
	return data_ && data_->Play();
}

bool Audio::Pause() {
	return data_ && data_->Pause();
}

bool Audio::Stop() {
	return data_ && data_->Stop();
}

bool Audio::IsPlaying() const {
	return data_ && data_->IsPlaying();
}

bool Audio::IsPaused() const {
	return data_ && data_->IsPaused();
}

bool Audio::SetLoop(bool loop) {
	return data_ && data_->SetLoop(loop);
}

bool Audio::IsLoop() const {
	return data_ && data_->IsLoop();
}

bool Audio::SetVolume(float volume) {
	return data_ && data_->SetVolume(volume);
}

float Audio::GetVolume() const {
	return data_ ? data_->GetVolume() : 0.0f;
}

double Audio::GetDuration() const {
	return data_ ? data_->GetDuration() : 0.0;
}

double Audio::GetPosition() const {
	return data_ ? data_->GetPosition() : 0.0;
}
