#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace LGF::Internal {

	inline void WriteJsonFile(const std::filesystem::path& filePath, std::string_view text) {
		if (filePath.filename().empty()) {
			throw std::runtime_error("A JSON file name is required.");
		}

		const std::filesystem::path targetPath = std::filesystem::absolute(filePath);
		std::filesystem::create_directories(targetPath.parent_path());
		static std::atomic_uint64_t nextTemporaryId{};
		std::filesystem::path temporaryPath = targetPath;
		temporaryPath += ".tmp." + std::to_string(GetCurrentProcessId()) + "." +
			std::to_string(nextTemporaryId.fetch_add(1u, std::memory_order_relaxed));

		try {
			std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
			file << text << '\n';
			file.close();
			if (!file) {
				throw std::runtime_error("Failed to write the temporary JSON file.");
			}
			if (!MoveFileExW(temporaryPath.c_str(), targetPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
				throw std::runtime_error("Failed to replace the JSON file. Windows error " + std::to_string(GetLastError()));
			}
		} catch (...) {
			std::error_code ignored;
			std::filesystem::remove(temporaryPath, ignored);
			throw;
		}
	}

}
