#pragma once

namespace LGF::System {
	/// <summary>
	/// 現在のフレームと前のフレームの間隔を秒単位で取得
	/// </summary>
	double DeltaTime();

	bool Update();
}
