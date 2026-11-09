#include "LGF/VFX/Particle/ParticleWorld.h"

#include "LGF/Graphics/Billboard3D.h"
#include "LGF/Graphics/Line3D.h"
#include "LGF/Math/MathUtils.h"

#include <imgui/imgui.h>

namespace LGF::VFX {

	void ParticleWorld::Draw() const {
		for (const uint32_t index : activeParticleIndices_) {
			const ParticleSlot& particle = particles_[index];
			const float t = particle.age / particle.lifetime;
			const Vector4 color = particle.startColor * (1.0f - t) + particle.endColor * t;
			const Vector2 size = particle.startSize * (1.0f - t) + particle.endSize * t;
			Billboard3D{ particle.position, size, particle.rotation }.Draw(color, particle.texture);
		}
	}

	void ParticleWorld::DrawDebug() const {
		const Vector3& minimum = settings_.worldBounds.min;
		const Vector3& maximum = settings_.worldBounds.max;
		Vector3 corners[8];
		for (uint32_t i = 0u; i < 8u; ++i) {
			corners[i] = {
				(i & 1u) ? maximum.x : minimum.x,
				(i & 2u) ? maximum.y : minimum.y,
				(i & 4u) ? maximum.z : minimum.z,
			};
		}
		for (uint32_t i = 0u; i < 8u; ++i) {
			for (uint32_t axis = 1u; axis <= 4u; axis <<= 1u) {
				if ((i & axis) == 0u) {
					Line3D{ corners[i], corners[i | axis] }.Draw(Math::Identity4x4(), Color::Yellow);
				}
			}
		}
		if (ImGui::GetCurrentContext() && ImGui::Begin("Particle Worlds")) {
			ImGui::PushID(this);
			ImGui::Text("World %llu: %zu particles / %zu emitters",
				static_cast<unsigned long long>(worldId_), GetParticleCount(), GetEmitterCount());
			ImGui::Text("Generated: %llu / Rejected: %llu",
				static_cast<unsigned long long>(lastStepStatistics_.generatedCount),
				static_cast<unsigned long long>(lastStepStatistics_.rejectedCount));
			ImGui::Text("Expired: %u / Out of bounds: %u", lastStepStatistics_.expiredCount,
				lastStepStatistics_.outOfBoundsCount);
			ImGui::PopID();
		}
		if (ImGui::GetCurrentContext()) {
			ImGui::End();
		}
	}

}
