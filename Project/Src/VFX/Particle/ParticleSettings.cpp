#include "LGF/VFX/Particle/ParticleWorld.h"

#include "LGF/Parameter.h"

#include <stdexcept>

namespace {
	LGF::ParameterDocument LoadDocument(const std::filesystem::path& filePath) {
		LGF::ParameterDocument document;
		const auto result = document.Load(filePath);
		if (!result) {
			throw std::runtime_error(result.message);
		}
		return document;
	}

	template<class T>
	void Read(const LGF::ParameterDocument& document, const char* name, T& value) {
		if (!document.Contains(name)) {
			return;
		}
		const auto loaded = document.Get<T>(name);
		if (!loaded) {
			throw std::invalid_argument(std::string("Invalid particle parameter type: ") + name);
		}
		value = *loaded;
	}

	void Read(const LGF::ParameterDocument& document, const char* name, uint32_t& value) {
		if (!document.Contains(name)) {
			return;
		}
		const auto loaded = document.Get<int32_t>(name);
		if (!loaded || *loaded < 0) {
			throw std::invalid_argument(std::string("Invalid particle integer parameter: ") + name);
		}
		value = static_cast<uint32_t>(*loaded);
	}
}

namespace LGF::VFX {

	ParticleWorldSettings LoadParticleWorldSettings(const std::filesystem::path& filePath) {
		const auto document = LoadDocument(filePath);
		ParticleWorldSettings settings;
		Read(document, "WorldBounds/Min", settings.worldBounds.min);
		Read(document, "WorldBounds/Max", settings.worldBounds.max);
		Read(document, "MaximumParticleCount", settings.maximumParticleCount);
		Read(document, "MaximumEmitterCount", settings.maximumEmitterCount);
		Read(document, "BoundsPadding", settings.boundsPadding);
		Read(document, "RemoveOutOfBounds", settings.removeOutOfBounds);
		Read(document, "MaximumDeltaTime", settings.maximumDeltaTime);
		Read(document, "RandomSeed", settings.randomSeed);
		return settings;
	}

	ParticleEmitterDesc LoadParticleEmitterDesc(const std::filesystem::path& filePath) {
		const auto document = LoadDocument(filePath);
		ParticleEmitterDesc desc;
		Read(document, "Position", desc.position);
		Read(document, "MinimumVelocity", desc.minimumVelocity);
		Read(document, "MaximumVelocity", desc.maximumVelocity);
		Read(document, "Acceleration", desc.acceleration);
		Read(document, "MinimumLifetime", desc.minimumLifetime);
		Read(document, "MaximumLifetime", desc.maximumLifetime);
		Read(document, "StartColor", desc.startColor);
		Read(document, "EndColor", desc.endColor);
		Read(document, "StartSize", desc.startSize);
		Read(document, "EndSize", desc.endSize);
		Read(document, "MinimumRotation", desc.minimumRotation);
		Read(document, "MaximumRotation", desc.maximumRotation);
		Read(document, "MinimumAngularVelocity", desc.minimumAngularVelocity);
		Read(document, "MaximumAngularVelocity", desc.maximumAngularVelocity);
		Read(document, "EmissionRate", desc.emissionRate);
		Read(document, "Enabled", desc.enabled);
		std::string texturePath;
		Read(document, "Texture", texturePath);
		if (!texturePath.empty()) {
			// テクスチャは既存のTextureと同じパス解決を使用する。
			desc.texture = Texture{ texturePath, TextureLoadDesc::Color() };
		}
		return desc;
	}

}
