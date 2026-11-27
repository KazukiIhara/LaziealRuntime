#include "LevelRuntime.h"

#include "Core/Framework/Framework.h"
#include "LGF/WorldLayout.h"

#include <Windows.h>

#include <array>
#include <memory>
#include <utility>

using namespace LGF;

namespace {

	constexpr std::string_view kDefaultLevelDirectory = "Assets/LevelDatas";

	std::string GetPathText(const std::filesystem::path& path) {
		const auto text = path.generic_u8string();
		return std::string(reinterpret_cast<const char*>(text.data()), text.size());
	}

	Font2D LoadLevelFont() {
		const std::filesystem::path projectFont = "Assets/Fonts/NotoSansJP/NotoSansJP-wght.ttf";
		if (std::filesystem::is_regular_file(projectFont)) {
			return Font2D(projectFont, 22u);
		}
		std::array<wchar_t, MAX_PATH> windowsDirectory{};
		const UINT length = GetWindowsDirectoryW(windowsDirectory.data(), static_cast<UINT>(windowsDirectory.size()));
		if (length == 0u || length >= windowsDirectory.size()) {
			return {};
		}
		return Font2D(std::filesystem::path(windowsDirectory.data()) / "Fonts/segoeui.ttf", 22u);
	}

	std::filesystem::path ResolveReference(const std::filesystem::path& reference, const std::filesystem::path& directory) {
		return reference.empty() ? std::filesystem::path{} : (directory / reference).lexically_normal();
	}

}

LevelIoResult Framework::SetLevelData(const std::filesystem::path& fileName) {
	try {
		const std::filesystem::path filePath =
			std::filesystem::absolute(std::filesystem::path(kDefaultLevelDirectory) / fileName).lexically_normal();
		auto nextLevel = std::make_unique<Detail::LevelRuntime>();
		const LevelIoResult result = LoadLevelData(filePath, nextLevel->data);
		if (!result) {
			return result;
		}
		const std::filesystem::path directory = filePath.parent_path();
		LevelData& data = nextLevel->data;
		data.uiLayoutPath = ResolveReference(data.uiLayoutPath, directory);
		data.worldLayoutPath = ResolveReference(data.worldLayoutPath, directory);
		if (!data.worldLayoutPath.empty() && !std::filesystem::is_regular_file(data.worldLayoutPath)) {
			return { .message = "World layout file was not found: " + GetPathText(data.worldLayoutPath) };
		}
		if (!data.worldLayoutPath.empty()) {
			const WorldLayout layout = LoadWorldLayout(data.worldLayoutPath);
			nextLevel->objects = CreateGameObjectWorld(layout, data.worldLayoutPath.parent_path());
		}
		if (!data.uiLayoutPath.empty()) {
			const UILayout2D layout = LoadUILayout2D(data.uiLayoutPath);
			const std::filesystem::path uiDirectory = data.uiLayoutPath.parent_path();
			nextLevel->ui = CreateUIWorld2D(
				layout,
				LoadLevelFont(),
				[&uiDirectory](std::string_view reference) {
					const std::filesystem::path texturePath(std::u8string(reference.begin(), reference.end()));
					return Texture(ResolveReference(texturePath, uiDirectory), TextureLoadDesc::Color());
				});
			for (std::size_t index = 0u; index < layout.elements.size(); ++index) {
				nextLevel->uiHandles.emplace(layout.elements[index].id, nextLevel->ui->handles[index]);
			}
			nextLevel->ui->world.Update({ .isCursorEnabled = false });
		}
		level_ = std::move(nextLevel);
		return { .succeeded = true };
	} catch (const std::exception& error) {
		return { .message = GetPathText(fileName) + ": " + error.what() };
	}
}

void Framework::ClearLevelData() {
	level_.reset();
}

const LevelData* Framework::GetLevelData() const {
	return level_ ? &level_->data : nullptr;
}

UIWorld* Framework::GetLevelUIWorld() {
	return level_ && level_->ui ? &level_->ui->world : nullptr;
}

UIHandle Framework::FindLevelUIElement(std::string_view id) const {
	if (!level_ || !level_->ui) {
		return {};
	}
	const auto element = level_->uiHandles.find(std::string(id));
	return element != level_->uiHandles.end() && level_->ui->world.IsValid(element->second)
		? element->second
		: UIHandle{};
}

GameObjectWorld* Framework::GetLevelGameObjectWorld() {
	return level_ ? &level_->objects : nullptr;
}
