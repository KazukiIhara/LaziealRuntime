#include "LGF/System.h"

#include "Core/Framework/Framework.h"

namespace LGF::Detail {
	Framework& GetFramework() {
		static Framework framework;
		return framework;
	}
}

bool LGF::System::Update() {
	return Detail::GetFramework().Update();
}

double LGF::System::DeltaTime() {
	return Detail::GetFramework().GetDeltaTime();
}
