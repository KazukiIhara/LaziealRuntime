#include "LGF/Animation/Easing.h"

#include <cmath>
#include <numbers>

namespace {

	constexpr float kBack1 = 1.70158f;
	constexpr float kBack2 = kBack1 * 1.525f;
	constexpr float kBack3 = kBack1 + 1.0f;
	constexpr float kElasticInOut =
		(2.0f * std::numbers::pi_v<float>) / 4.5f;
	constexpr float kElasticInOrOut =
		(2.0f * std::numbers::pi_v<float>) / 3.0f;
	constexpr float kBounceScale = 7.5625f;
	constexpr float kBounceDivider = 2.75f;

	float EaseOutBounceInternal(float t) {
		if (t < 1.0f / kBounceDivider) {
			return kBounceScale * t * t;
		}
		if (t < 2.0f / kBounceDivider) {
			t -= 1.5f / kBounceDivider;
			return kBounceScale * t * t + 0.75f;
		}
		if (t < 2.5f / kBounceDivider) {
			t -= 2.25f / kBounceDivider;
			return kBounceScale * t * t + 0.9375f;
		}

		t -= 2.625f / kBounceDivider;
		return kBounceScale * t * t + 0.984375f;
	}

}

namespace LGF::Easing {

	float Apply(EasingType type, float t) {
		switch (type) {
		case EasingType::Linear:
			return EaseLinear(t);
		case EasingType::EaseInSine:
			return EaseInSine(t);
		case EasingType::EaseOutSine:
			return EaseOutSine(t);
		case EasingType::EaseInOutSine:
			return EaseInOutSine(t);
		case EasingType::EaseInQuad:
			return EaseInQuad(t);
		case EasingType::EaseOutQuad:
			return EaseOutQuad(t);
		case EasingType::EaseInOutQuad:
			return EaseInOutQuad(t);
		case EasingType::EaseInCubic:
			return EaseInCubic(t);
		case EasingType::EaseOutCubic:
			return EaseOutCubic(t);
		case EasingType::EaseInOutCubic:
			return EaseInOutCubic(t);
		case EasingType::EaseInQuart:
			return EaseInQuart(t);
		case EasingType::EaseOutQuart:
			return EaseOutQuart(t);
		case EasingType::EaseInOutQuart:
			return EaseInOutQuart(t);
		case EasingType::EaseInQuint:
			return EaseInQuint(t);
		case EasingType::EaseOutQuint:
			return EaseOutQuint(t);
		case EasingType::EaseInOutQuint:
			return EaseInOutQuint(t);
		case EasingType::EaseInExpo:
			return EaseInExpo(t);
		case EasingType::EaseOutExpo:
			return EaseOutExpo(t);
		case EasingType::EaseInOutExpo:
			return EaseInOutExpo(t);
		case EasingType::EaseInCirc:
			return EaseInCirc(t);
		case EasingType::EaseOutCirc:
			return EaseOutCirc(t);
		case EasingType::EaseInOutCirc:
			return EaseInOutCirc(t);
		case EasingType::EaseInBack:
			return EaseInBack(t);
		case EasingType::EaseOutBack:
			return EaseOutBack(t);
		case EasingType::EaseInOutBack:
			return EaseInOutBack(t);
		case EasingType::EaseInElastic:
			return EaseInElastic(t);
		case EasingType::EaseOutElastic:
			return EaseOutElastic(t);
		case EasingType::EaseInOutElastic:
			return EaseInOutElastic(t);
		case EasingType::EaseInBounce:
			return EaseInBounce(t);
		case EasingType::EaseOutBounce:
			return EaseOutBounce(t);
		case EasingType::EaseInOutBounce:
			return EaseInOutBounce(t);
		default:
			return t;
		}
	}

	float EaseLinear(float t) {
		return t;
	}

	float EaseInSine(float t) {
		return 1.0f - std::cos(t * std::numbers::pi_v<float> * 0.5f);
	}

	float EaseOutSine(float t) {
		return std::sin(t * std::numbers::pi_v<float> * 0.5f);
	}

	float EaseInOutSine(float t) {
		return -(std::cos(std::numbers::pi_v<float> * t) - 1.0f) * 0.5f;
	}

	float EaseInQuad(float t) {
		return t * t;
	}

	float EaseOutQuad(float t) {
		return 1.0f - (1.0f - t) * (1.0f - t);
	}

	float EaseInOutQuad(float t) {
		return t < 0.5f
			? 2.0f * t * t
			: 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
	}

	float EaseInCubic(float t) {
		return t * t * t;
	}

	float EaseOutCubic(float t) {
		return 1.0f - std::pow(1.0f - t, 3.0f);
	}

	float EaseInOutCubic(float t) {
		return t < 0.5f
			? 4.0f * t * t * t
			: 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
	}

	float EaseInQuart(float t) {
		return t * t * t * t;
	}

	float EaseOutQuart(float t) {
		return 1.0f - std::pow(1.0f - t, 4.0f);
	}

	float EaseInOutQuart(float t) {
		return t < 0.5f
			? 8.0f * std::pow(t, 4.0f)
			: 1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) * 0.5f;
	}

	float EaseInQuint(float t) {
		return t * t * t * t * t;
	}

	float EaseOutQuint(float t) {
		return 1.0f - std::pow(1.0f - t, 5.0f);
	}

	float EaseInOutQuint(float t) {
		return t < 0.5f
			? 16.0f * std::pow(t, 5.0f)
			: 1.0f - std::pow(-2.0f * t + 2.0f, 5.0f) * 0.5f;
	}

	float EaseInExpo(float t) {
		return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);
	}

	float EaseOutExpo(float t) {
		return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
	}

	float EaseInOutExpo(float t) {
		if (t == 0.0f || t == 1.0f) {
			return t;
		}
		return t < 0.5f
			? std::pow(2.0f, 20.0f * t - 10.0f) * 0.5f
			: (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) * 0.5f;
	}

	float EaseInCirc(float t) {
		return 1.0f - std::sqrt(1.0f - t * t);
	}

	float EaseOutCirc(float t) {
		return std::sqrt(1.0f - std::pow(t - 1.0f, 2.0f));
	}

	float EaseInOutCirc(float t) {
		return t < 0.5f
			? (1.0f - std::sqrt(1.0f - 4.0f * t * t)) * 0.5f
			: (std::sqrt(1.0f - std::pow(-2.0f * t + 2.0f, 2.0f)) + 1.0f) * 0.5f;
	}

	float EaseInBack(float t) {
		return kBack3 * t * t * t - kBack1 * t * t;
	}

	float EaseOutBack(float t) {
		return 1.0f +
			kBack3 * std::pow(t - 1.0f, 3.0f) +
			kBack1 * std::pow(t - 1.0f, 2.0f);
	}

	float EaseInOutBack(float t) {
		return t < 0.5f
			? std::pow(2.0f * t, 2.0f) *
				((kBack2 + 1.0f) * 2.0f * t - kBack2) * 0.5f
			: (std::pow(2.0f * t - 2.0f, 2.0f) *
				((kBack2 + 1.0f) * (2.0f * t - 2.0f) + kBack2) + 2.0f) * 0.5f;
	}

	float EaseInElastic(float t) {
		if (t == 0.0f || t == 1.0f) {
			return t;
		}
		return -std::pow(2.0f, 10.0f * t - 10.0f) *
			std::sin((10.0f * t - 10.75f) * kElasticInOrOut);
	}

	float EaseOutElastic(float t) {
		if (t == 0.0f || t == 1.0f) {
			return t;
		}
		return std::pow(2.0f, -10.0f * t) *
			std::sin((10.0f * t - 0.75f) * kElasticInOrOut) + 1.0f;
	}

	float EaseInOutElastic(float t) {
		if (t == 0.0f || t == 1.0f) {
			return t;
		}
		return t < 0.5f
			? -0.5f * std::pow(2.0f, 20.0f * t - 10.0f) *
				std::sin((20.0f * t - 11.125f) * kElasticInOut)
			: 0.5f * std::pow(2.0f, -20.0f * t + 10.0f) *
				std::sin((20.0f * t - 11.125f) * kElasticInOut) + 1.0f;
	}

	float EaseOutBounce(float t) {
		return EaseOutBounceInternal(t);
	}

	float EaseInBounce(float t) {
		return 1.0f - EaseOutBounceInternal(1.0f - t);
	}

	float EaseInOutBounce(float t) {
		return t < 0.5f
			? (1.0f - EaseOutBounceInternal(1.0f - 2.0f * t)) * 0.5f
			: (1.0f + EaseOutBounceInternal(2.0f * t - 1.0f)) * 0.5f;
	}

}
