#pragma once

#include "LGF/Animation/Easing.h"
#include "LGF/LGFMath.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace LGF {

	/// <summary>
	/// 2つの値をイージング付きで補間するシンプルアニメーション
	/// </summary>
	template <class T>
	class SimpleAnimation {
		static_assert(
			std::is_same_v<T, float> ||
			std::is_same_v<T, Vector2> ||
			std::is_same_v<T, Vector3> ||
			std::is_same_v<T, Vector4>,
			"SimpleAnimation supports float, Vector2, Vector3, and Vector4");

	public:
		/// <summary>
		/// ループ方法
		/// </summary>
		enum class LoopType {
			Restart,
			PingPong,
		};

		SimpleAnimation(
			const T& startValue = {},
			const T& endValue = {},
			EasingType easing = EasingType::Linear,
			bool loop = false,
			LoopType loopType = LoopType::Restart) :
			startValue_(startValue),
			endValue_(endValue),
			easing_(easing),
			loop_(loop),
			loopType_(loopType) {
		}

		/// <summary>
		/// 進行度から補間後の値を取得
		/// </summary>
		T GetValue(float t) const {
			const float normalizedTime = NormalizeTime(t);
			return Math::Lerp(
				startValue_,
				endValue_,
				Easing::Apply(easing_, normalizedTime));
		}

		const T& GetStartValue() const {
			return startValue_;
		}

		const T& GetEndValue() const {
			return endValue_;
		}

		EasingType GetEasing() const {
			return easing_;
		}

		bool IsLoop() const {
			return loop_;
		}

		LoopType GetLoopType() const {
			return loopType_;
		}

		void SetStartValue(const T& value) {
			startValue_ = value;
		}

		void SetEndValue(const T& value) {
			endValue_ = value;
		}

		void SetEasing(EasingType easing) {
			easing_ = easing;
		}

		void SetLoop(bool loop) {
			loop_ = loop;
		}

		void SetLoopType(LoopType loopType) {
			loopType_ = loopType;
		}

	private:
		float NormalizeTime(float t) const {
			if (!loop_) {
				return std::clamp(t, 0.0f, 1.0f);
			}

			if (loopType_ == LoopType::Restart) {
				return t - std::floor(t);
			}

			const float cycle = t - std::floor(t * 0.5f) * 2.0f;
			return cycle <= 1.0f ? cycle : 2.0f - cycle;
		}

		T startValue_{};
		T endValue_{};
		EasingType easing_ = EasingType::Linear;
		bool loop_ = false;
		LoopType loopType_ = LoopType::Restart;
	};

}
