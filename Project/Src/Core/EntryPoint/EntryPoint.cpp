#include <Windows.h>

#include "LGF/Runtime.h"

/// <summary>
/// エントリーポイント
/// </summary>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	return LGF::Run(ConfigureRuntime(), Main);
}
