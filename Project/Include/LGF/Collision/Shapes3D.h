#pragma once

#include "LGF/Math/Quaternion.h"
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
	/// 円形の境界を持つ、閉じていない球面。
	/// apex は球面の頂点、axis は頂点から円形境界へ向かう軸を表す。
	/// baseRadius は円形境界の半径、height は頂点から境界までの軸方向の距離。
	/// axis は単位ベクトルでなくてもよい。
	/// </summary>
	struct SphericalCap {
		Vector3 apex{};
		Vector3 axis{ 0.0f, 1.0f, 0.0f };
		float baseRadius = 1.0f;
		float height = 0.5f;
	};

	/// <summary>
	/// 同心の内外球面と、開口部の環状面で囲まれた厚みのある球殻。
	/// outerApex は外側球面の頂点、axis は頂点から開口部へ向かう軸を表す。
	/// height は outerApex から開口平面までの軸方向の距離。
	/// axis は単位ベクトルでなくてもよい。
	/// </summary>
	struct SphericalShellCap {
		Vector3 outerApex{};
		Vector3 axis{ 0.0f, 1.0f, 0.0f };
		float outerRadius = 2.0f;
		float innerRadius = 1.8f;
		float height = 1.0f;
	};

	/// <summary>
	/// 軸平行境界ボックス。各成分で min &lt;= max となるように指定する。
	/// </summary>
	struct AABB {
		Vector3 min{ -0.5f, -0.5f, -0.5f };
		Vector3 max{ 0.5f, 0.5f, 0.5f };
	};

	/// <summary>
	/// 中心、各ローカル軸方向の半径、回転で表す有向境界ボックス。
	/// rotationは単位クォータニオンでなくてもよい。
	/// </summary>
	struct OBB {
		Vector3 center{};
		Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };
		Quaternion rotation{};
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
	/// startとendを上下の円の中心として表す円柱。
	/// startとendは異なる位置にし、radiusは0以上で指定する。
	/// </summary>
	struct Cylinder {
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
