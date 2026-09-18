#pragma once

#include "LGF/Graphics/Camera3D.h"

namespace LGF {

	/// <summary>
	/// 原点を注視しながら周囲を移動するデバッグ用3Dカメラ
	/// </summary>
	class DebugCamera3D final : public Camera3D {
	public:
		/// <summary>
		/// 原点からの距離を指定してデバッグカメラを作成
		/// </summary>
		explicit DebugCamera3D(
			float distance = 5.0f,
			const Camera3D::Param& param = {});

		~DebugCamera3D() override;

		/// <summary>
		/// 十字キーの回転とQ/Eキーの距離変更を反映してカメラを更新
		/// </summary>
		void Update();

	public:
		float distance = 5.0f;
		float rotationSpeed = 60.0f * Math::DEG_TO_RAD;
		float distanceSpeed = 30.0f;

	private:
		void UpdateTranslation();
	};

}
