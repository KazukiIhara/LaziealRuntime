#pragma once

#include <filesystem>
#include <memory>

namespace LGF {

	class AudioData;

	namespace AudioInternal {
		std::shared_ptr<AudioData> Load(const std::filesystem::path& filePath);
	}

}
