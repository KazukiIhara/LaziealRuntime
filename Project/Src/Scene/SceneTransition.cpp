#include "LGF/Scene/SceneTransition.h"

#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

using namespace LGF;

namespace {

	using Json = nlohmann::json;

	constexpr std::pair<std::string_view, EasingType> kEasingTypes[] = {
		{ "Linear", EasingType::Linear },
		{ "EaseInSine", EasingType::EaseInSine },
		{ "EaseOutSine", EasingType::EaseOutSine },
		{ "EaseInOutSine", EasingType::EaseInOutSine },
		{ "EaseInQuad", EasingType::EaseInQuad },
		{ "EaseOutQuad", EasingType::EaseOutQuad },
		{ "EaseInOutQuad", EasingType::EaseInOutQuad },
		{ "EaseInCubic", EasingType::EaseInCubic },
		{ "EaseOutCubic", EasingType::EaseOutCubic },
		{ "EaseInOutCubic", EasingType::EaseInOutCubic },
		{ "EaseInQuart", EasingType::EaseInQuart },
		{ "EaseOutQuart", EasingType::EaseOutQuart },
		{ "EaseInOutQuart", EasingType::EaseInOutQuart },
		{ "EaseInQuint", EasingType::EaseInQuint },
		{ "EaseOutQuint", EasingType::EaseOutQuint },
		{ "EaseInOutQuint", EasingType::EaseInOutQuint },
		{ "EaseInExpo", EasingType::EaseInExpo },
		{ "EaseOutExpo", EasingType::EaseOutExpo },
		{ "EaseInOutExpo", EasingType::EaseInOutExpo },
		{ "EaseInCirc", EasingType::EaseInCirc },
		{ "EaseOutCirc", EasingType::EaseOutCirc },
		{ "EaseInOutCirc", EasingType::EaseInOutCirc },
		{ "EaseInBack", EasingType::EaseInBack },
		{ "EaseOutBack", EasingType::EaseOutBack },
		{ "EaseInOutBack", EasingType::EaseInOutBack },
		{ "EaseInElastic", EasingType::EaseInElastic },
		{ "EaseOutElastic", EasingType::EaseOutElastic },
		{ "EaseInOutElastic", EasingType::EaseInOutElastic },
		{ "EaseInBounce", EasingType::EaseInBounce },
		{ "EaseOutBounce", EasingType::EaseOutBounce },
		{ "EaseInOutBounce", EasingType::EaseInOutBounce },
	};

	EasingType ToEasingType(std::string_view name) {
		for (const auto& [easingName, type] : kEasingTypes) {
			if (easingName == name) {
				return type;
			}
		}
		throw std::runtime_error("Unknown Scene Transition easing: " + std::string(name));
	}

	Vector4 ReadVector4(const Json& json, std::string_view name) {
		if (!json.is_array() || json.size() != 4u) {
			throw std::runtime_error("Scene Transition value must have four components: " + std::string(name));
		}

		const Vector4 result{
			json[0].get<float>(),
			json[1].get<float>(),
			json[2].get<float>(),
			json[3].get<float>(),
		};
		if (!std::isfinite(result.x) || !std::isfinite(result.y) ||
			!std::isfinite(result.z) || !std::isfinite(result.w)) {
			throw std::runtime_error("Scene Transition value must be finite: " + std::string(name));
		}
		return result;
	}

}

SceneTransition LGF::LoadSceneTransition(const std::filesystem::path& filePath) {
	std::ifstream file(filePath);
	if (!file) {
		throw std::runtime_error("Failed to open Scene Transition Json: " + filePath.string());
	}

	Json root;
	file >> root;
	SceneTransition transition{};
	transition.fadeOutDuration = root.value("FadeOutDuration", transition.fadeOutDuration);
	transition.fadeInDuration = root.value("FadeInDuration", transition.fadeInDuration);
	transition.fadeOutEasing = ToEasingType(root.value("FadeOutEasing", "EaseInCubic"));
	transition.fadeInEasing = ToEasingType(root.value("FadeInEasing", "EaseOutCubic"));
	transition.pipeline = root.value("Pipeline", transition.pipeline);
	if (root.contains("Color")) {
		transition.color = ReadVector4(root["Color"], "Color");
	}
	if (root.contains("Parameters")) {
		if (!root["Parameters"].is_array() ||
			root["Parameters"].size() > kSceneTransitionParameterCount) {
			throw std::runtime_error("Scene Transition Parameters must contain at most four values.");
		}
		for (uint32_t index = 0u; index < root["Parameters"].size(); ++index) {
			transition.parameters[index] = ReadVector4(
				root["Parameters"][index],
				"Parameters[" + std::to_string(index) + "]");
		}
	}

	if (!std::isfinite(transition.fadeOutDuration) || transition.fadeOutDuration < 0.0f ||
		!std::isfinite(transition.fadeInDuration) || transition.fadeInDuration < 0.0f) {
		throw std::runtime_error("Scene Transition duration must be finite and non-negative.");
	}
	if (transition.pipeline.empty()) {
		throw std::runtime_error("Scene Transition Pipeline must not be empty.");
	}
	return transition;
}
