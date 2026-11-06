#pragma once

#include <filesystem>
#include <memory>

namespace LGF::AudioDetail {

	class AudioData;

	std::shared_ptr<AudioData> Create(const std::filesystem::path& filePath);
	bool Load(const std::filesystem::path& filePath);
	bool Unload(const std::filesystem::path& filePath);
	void UnloadAll();
	bool IsLoaded(const std::filesystem::path& filePath);
	bool Play(const std::filesystem::path& filePath, bool loop);
	bool Pause(const std::filesystem::path& filePath);
	bool Stop(const std::filesystem::path& filePath);
	bool IsPlaying(const std::filesystem::path& filePath);
	bool SetVolume(const std::filesystem::path& filePath, float volume);

}
