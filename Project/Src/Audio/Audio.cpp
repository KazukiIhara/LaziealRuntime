#include "LGF/Audio.h"

#include "Audio/AudioData.h"
#include "Audio/AudioInternal.h"

using namespace LGF;

Audio::Audio(const std::filesystem::path& filePath) {
	Open(filePath);
}

bool Audio::Load(const std::filesystem::path& filePath) {
	return AudioDetail::Load(filePath);
}

bool Audio::Unload(const std::filesystem::path& filePath) {
	return AudioDetail::Unload(filePath);
}

void Audio::UnloadAll() {
	AudioDetail::UnloadAll();
}

bool Audio::IsLoaded(const std::filesystem::path& filePath) {
	return AudioDetail::IsLoaded(filePath);
}

bool Audio::Play(
	const std::filesystem::path& filePath,
	bool loop) {
	return AudioDetail::Play(filePath, loop);
}

bool Audio::Pause(const std::filesystem::path& filePath) {
	return AudioDetail::Pause(filePath);
}

bool Audio::Stop(const std::filesystem::path& filePath) {
	return AudioDetail::Stop(filePath);
}

bool Audio::IsPlaying(const std::filesystem::path& filePath) {
	return AudioDetail::IsPlaying(filePath);
}

bool Audio::SetVolume(
	const std::filesystem::path& filePath,
	float volume) {
	return AudioDetail::SetVolume(filePath, volume);
}

bool Audio::Open(const std::filesystem::path& filePath) {
	Release();
	data_ = AudioDetail::Create(filePath);
	return IsValid();
}

void Audio::Release() {
	data_.reset();
}

bool Audio::IsValid() const {
	return data_ && data_->IsValid();
}

Audio::operator bool() const {
	return IsValid();
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
