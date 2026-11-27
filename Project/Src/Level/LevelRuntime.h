#pragma once

#include "LGF/Level.h"
#include "LGF/GameObject.h"
#include "LGF/UI/UILayout2D.h"

#include <optional>
#include <unordered_map>

namespace LGF::Detail {

	struct LevelRuntime {
		LevelData data;
		GameObjectWorld objects;
		std::optional<UILayoutInstance2D> ui;
		std::unordered_map<std::string, UIHandle> uiHandles;
	};

}
