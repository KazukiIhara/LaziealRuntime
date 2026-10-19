#pragma once

#include "LGF/Math/Functions/VectorMath.h"
#include "LGF/Math/MathConstants.h"

#include <cmath>

namespace LGF::Collision::Internal {

	inline Vector3 SafeNormal(const Vector3& value, const Vector3& fallback) {
		const float lengthSq = Math::LengthSq(value);
		if (lengthSq <= Math::EPSILON * Math::EPSILON) {
			return fallback;
		}
		return value / std::sqrt(lengthSq);
	}

}
