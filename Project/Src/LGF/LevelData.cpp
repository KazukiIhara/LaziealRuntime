#include "LGF/Level.h"

#include "Core/Framework/Framework.h"

namespace LGF::Detail {
	Framework& GetFramework();
}

LGF::LevelIoResult LGF::SetLevelData(const std::filesystem::path& fileName) {
	return Detail::GetFramework().SetLevelData(fileName);
}

void LGF::ClearLevelData() {
	Detail::GetFramework().ClearLevelData();
}

const LGF::LevelData* LGF::GetLevelData() {
	return Detail::GetFramework().GetLevelData();
}

LGF::UIWorld* LGF::GetLevelUIWorld() {
	return Detail::GetFramework().GetLevelUIWorld();
}

LGF::UIHandle LGF::FindLevelUIElement(std::string_view id) {
	return Detail::GetFramework().FindLevelUIElement(id);
}

LGF::GameObjectWorld* LGF::GetLevelGameObjectWorld() {
	return Detail::GetFramework().GetLevelGameObjectWorld();
}
