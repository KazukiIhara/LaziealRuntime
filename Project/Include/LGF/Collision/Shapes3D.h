#pragma once

#include "LGF/Math/Vector3.h"

namespace LGF::Collision {

	/// <summary>
	/// 球。radius は 0 以上で指定する。
	/// </summary>
	struct Sphere {
		Vector3 center{};
		float radius = 0.5f;
	};

	/// <summary>
	/// 軸平行境界ボックス。各成分で min &lt;= max となるように指定する。
	/// </summary>
	struct AABB {
		Vector3 min{ -0.5f, -0.5f, -0.5f };
		Vector3 max{ 0.5f, 0.5f, 0.5f };
	};

	/// <summary>
	/// start と end を結ぶ線分と、その周囲の半径で表すカプセル。
	/// </summary>
	struct Capsule {
		Vector3 start{ 0.0f, -0.5f, 0.0f };
		Vector3 end{ 0.0f, 0.5f, 0.0f };
		float radius = 0.5f;
	};

	/// <summary>
	/// レイ。direction は単位ベクトルでなくてもよい。
	/// </summary>
	struct Ray {
		Vector3 origin{};
		Vector3 direction{ 0.0f, 0.0f, 1.0f };
	};

	/// <summary>
	/// dot(normal, point) + distance = 0 で表す平面。
	/// normal は単位ベクトルでなくてもよい。
	/// </summary>
	struct Plane {
		Vector3 normal{ 0.0f, 1.0f, 0.0f };
		float distance = 0.0f;
	};

}
