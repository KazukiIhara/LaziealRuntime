#include "LGF/VFX/Particle/ParticleWorld.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
	std::atomic<uint64_t> g_nextParticleWorldId{ 1u };

	bool IsFinite(const LGF::Vector3& value) {
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	bool IsFinite(const LGF::Vector4& value) {
		return std::isfinite(value.x) && std::isfinite(value.y) &&
			std::isfinite(value.z) && std::isfinite(value.w);
	}

	bool IsSizeValid(const LGF::Vector2& value) {
		return std::isfinite(value.x) && std::isfinite(value.y) && value.x >= 0.0f && value.y >= 0.0f;
	}

	bool IsRangeValid(float minimum, float maximum) {
		return std::isfinite(minimum) && std::isfinite(maximum) && minimum <= maximum;
	}

	bool IsEmitterDescValid(const LGF::VFX::ParticleEmitterDesc& desc) {
		return IsFinite(desc.position) && IsFinite(desc.acceleration) &&
			IsRangeValid(desc.minimumVelocity.x, desc.maximumVelocity.x) &&
			IsRangeValid(desc.minimumVelocity.y, desc.maximumVelocity.y) &&
			IsRangeValid(desc.minimumVelocity.z, desc.maximumVelocity.z) &&
			IsRangeValid(desc.minimumLifetime, desc.maximumLifetime) && desc.minimumLifetime > 0.0f &&
			IsRangeValid(desc.minimumRotation, desc.maximumRotation) &&
			IsRangeValid(desc.minimumAngularVelocity, desc.maximumAngularVelocity) &&
			IsFinite(desc.startColor) && IsFinite(desc.endColor) &&
			IsSizeValid(desc.startSize) && IsSizeValid(desc.endSize) &&
			std::isfinite(desc.emissionRate) && desc.emissionRate >= 0.0f;
	}

	void AdvanceGeneration(uint32_t& generation) {
		if (++generation == 0u) {
			++generation;
		}
	}
}

namespace LGF::VFX {

	bool ParticleEmitterHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u && worldId != 0u;
	}

	ParticleWorld::ParticleWorld(const ParticleWorldSettings& settings) :
		settings_(settings),
		worldId_(g_nextParticleWorldId.fetch_add(1u)),
		random_(settings.randomSeed) {
		const auto& bounds = settings.worldBounds;
		if (settings.maximumParticleCount == 0u || settings.maximumEmitterCount == 0u ||
			!IsFinite(bounds.min) || !IsFinite(bounds.max) ||
			bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z ||
			!std::isfinite(settings.boundsPadding) || settings.boundsPadding < 0.0f ||
			!std::isfinite(settings.maximumDeltaTime) || settings.maximumDeltaTime <= 0.0f) {
			throw std::invalid_argument("Invalid ParticleWorldSettings.");
		}
		particles_.resize(settings.maximumParticleCount);
		activeParticleIndices_.reserve(settings.maximumParticleCount);
		freeParticleIndices_.reserve(settings.maximumParticleCount);
		emitters_.reserve(settings.maximumEmitterCount);
		freeEmitterIndices_.reserve(settings.maximumEmitterCount);
		Clear();
	}

	ParticleEmitterHandle ParticleWorld::CreateEmitter(const ParticleEmitterDesc& desc) {
		if (!IsEmitterDescValid(desc) || emitterCount_ >= settings_.maximumEmitterCount) {
			return {};
		}
		uint32_t index{};
		if (freeEmitterIndices_.empty()) {
			index = static_cast<uint32_t>(emitters_.size());
			emitters_.emplace_back();
		}
		else {
			index = freeEmitterIndices_.back();
			freeEmitterIndices_.pop_back();
		}
		EmitterSlot& emitter = emitters_[index];
		emitter.desc = desc;
		emitter.emissionRemainder = 0.0;
		emitter.pendingCount = 0u;
		emitter.isOccupied = true;
		++emitterCount_;
		return { index, emitter.generation, worldId_ };
	}

	bool ParticleWorld::RemoveEmitter(ParticleEmitterHandle handle) {
		EmitterSlot* emitter = FindEmitter(handle);
		if (!emitter) {
			return false;
		}
		emitter->isOccupied = false;
		emitter->pendingCount = 0u;
		emitter->desc.texture = {};
		AdvanceGeneration(emitter->generation);
		freeEmitterIndices_.push_back(handle.index);
		--emitterCount_;
		return true;
	}

	bool ParticleWorld::IsValid(ParticleEmitterHandle handle) const {
		return FindEmitter(handle) != nullptr;
	}

	bool ParticleWorld::SetEmitterDesc(ParticleEmitterHandle handle, const ParticleEmitterDesc& desc) {
		EmitterSlot* emitter = FindEmitter(handle);
		if (!emitter || !IsEmitterDescValid(desc)) {
			return false;
		}
		emitter->desc = desc;
		emitter->emissionRemainder = 0.0;
		return true;
	}

	bool ParticleWorld::SetEmitterPosition(ParticleEmitterHandle handle, const Vector3& position) {
		EmitterSlot* emitter = FindEmitter(handle);
		if (!emitter || !IsFinite(position)) {
			return false;
		}
		emitter->desc.position = position;
		return true;
	}

	bool ParticleWorld::SetEmitterEnabled(ParticleEmitterHandle handle, bool enabled) {
		EmitterSlot* emitter = FindEmitter(handle);
		if (!emitter) {
			return false;
		}
		emitter->desc.enabled = enabled;
		return true;
	}

	std::optional<ParticleEmitterDesc> ParticleWorld::GetEmitterDesc(ParticleEmitterHandle handle) const {
		const EmitterSlot* emitter = FindEmitter(handle);
		return emitter ? std::optional<ParticleEmitterDesc>{ emitter->desc } : std::nullopt;
	}

	bool ParticleWorld::Emit(ParticleEmitterHandle handle, uint32_t count) {
		EmitterSlot* emitter = FindEmitter(handle);
		if (!emitter || emitter->pendingCount > std::numeric_limits<uint64_t>::max() - count) {
			return false;
		}
		emitter->pendingCount += count;
		return true;
	}

	void ParticleWorld::Update(float deltaTime) {
		if (!std::isfinite(deltaTime) || deltaTime < 0.0f) {
			return;
		}
		const float frameTime = std::min(deltaTime, settings_.maximumDeltaTime);
		lastStepStatistics_ = {};
		for (EmitterSlot& emitter : emitters_) {
			if (!emitter.isOccupied) {
				continue;
			}
			GenerateParticles(emitter.desc, emitter.pendingCount);
			emitter.pendingCount = 0u;
			if (emitter.desc.enabled) {
				const double amount = emitter.emissionRemainder +
					static_cast<double>(emitter.desc.emissionRate) * frameTime;
				emitter.emissionRemainder = amount - std::floor(amount);
				// 巨大な入力でも整数変換をオーバーフローさせず、生成ループを容量で制限する。
				const uint64_t count = amount >= static_cast<double>(std::numeric_limits<uint32_t>::max())
					? std::numeric_limits<uint32_t>::max() : static_cast<uint64_t>(amount);
				GenerateParticles(emitter.desc, count);
			}
		}
		const auto& bounds = settings_.worldBounds;
		const double padding = settings_.boundsPadding;
		for (std::size_t i = 0u; i < activeParticleIndices_.size();) {
			ParticleSlot& particle = particles_[activeParticleIndices_[i]];
			particle.age += frameTime;
			if (particle.age >= particle.lifetime) {
				++lastStepStatistics_.expiredCount;
				RemoveParticleAt(i);
				continue;
			}
			particle.position += particle.velocity * frameTime + particle.acceleration * (0.5f * frameTime * frameTime);
			particle.velocity += particle.acceleration * frameTime;
			particle.rotation += particle.angularVelocity * frameTime;
			const Vector3& position = particle.position;
			if (!IsFinite(position) || !IsFinite(particle.velocity) || !std::isfinite(particle.rotation) ||
				(settings_.removeOutOfBounds &&
					(position.x < bounds.min.x - padding || position.x > bounds.max.x + padding ||
					position.y < bounds.min.y - padding || position.y > bounds.max.y + padding ||
					position.z < bounds.min.z - padding || position.z > bounds.max.z + padding))) {
				++lastStepStatistics_.outOfBoundsCount;
				RemoveParticleAt(i);
				continue;
			}
			++i;
		}
		lastStepStatistics_.particleCount = static_cast<uint32_t>(activeParticleIndices_.size());
		lastStepStatistics_.emitterCount = emitterCount_;
	}

	void ParticleWorld::Clear() {
		activeParticleIndices_.clear();
		freeParticleIndices_.clear();
		for (uint32_t i = settings_.maximumParticleCount; i > 0u; --i) {
			particles_[i - 1u].texture = {};
			freeParticleIndices_.push_back(i - 1u);
		}
		freeEmitterIndices_.clear();
		for (uint32_t i = 0u; i < emitters_.size(); ++i) {
			EmitterSlot& emitter = emitters_[i];
			emitter.isOccupied = false;
			emitter.pendingCount = 0u;
			emitter.desc.texture = {};
			AdvanceGeneration(emitter.generation);
			freeEmitterIndices_.push_back(i);
		}
		emitterCount_ = 0u;
		random_.seed(settings_.randomSeed);
		lastStepStatistics_ = {};
	}

	std::size_t ParticleWorld::GetParticleCount() const { return activeParticleIndices_.size(); }
	std::size_t ParticleWorld::GetEmitterCount() const { return emitterCount_; }
	const ParticleWorldSettings& ParticleWorld::GetSettings() const { return settings_; }
	const ParticleStepStatistics& ParticleWorld::GetLastStepStatistics() const { return lastStepStatistics_; }

	ParticleWorld::EmitterSlot* ParticleWorld::FindEmitter(ParticleEmitterHandle handle) {
		return const_cast<EmitterSlot*>(std::as_const(*this).FindEmitter(handle));
	}

	const ParticleWorld::EmitterSlot* ParticleWorld::FindEmitter(ParticleEmitterHandle handle) const {
		if (handle.worldId != worldId_ || handle.index >= emitters_.size()) {
			return nullptr;
		}
		const EmitterSlot& emitter = emitters_[handle.index];
		return emitter.isOccupied && emitter.generation == handle.generation ? &emitter : nullptr;
	}

	void ParticleWorld::GenerateParticles(const ParticleEmitterDesc& desc, uint64_t count) {
		const auto accepted = static_cast<uint32_t>(std::min<uint64_t>(count, freeParticleIndices_.size()));
		lastStepStatistics_.generatedCount += accepted;
		const uint64_t rejected = count - accepted;
		lastStepStatistics_.rejectedCount += std::min(rejected,
			std::numeric_limits<uint64_t>::max() - lastStepStatistics_.rejectedCount);
		for (uint32_t i = 0u; i < accepted; ++i) {
			const uint32_t index = freeParticleIndices_.back();
			freeParticleIndices_.pop_back();
			ParticleSlot& particle = particles_[index];
			particle.position = desc.position;
			particle.velocity = {
				Random(desc.minimumVelocity.x, desc.maximumVelocity.x),
				Random(desc.minimumVelocity.y, desc.maximumVelocity.y),
				Random(desc.minimumVelocity.z, desc.maximumVelocity.z),
			};
			particle.acceleration = desc.acceleration;
			particle.startColor = desc.startColor;
			particle.endColor = desc.endColor;
			particle.startSize = desc.startSize;
			particle.endSize = desc.endSize;
			particle.rotation = Random(desc.minimumRotation, desc.maximumRotation);
			particle.angularVelocity = Random(desc.minimumAngularVelocity, desc.maximumAngularVelocity);
			particle.age = 0.0f;
			particle.lifetime = Random(desc.minimumLifetime, desc.maximumLifetime);
			particle.texture = desc.texture;
			activeParticleIndices_.push_back(index);
		}
	}

	void ParticleWorld::RemoveParticleAt(std::size_t activeIndex) {
		const uint32_t index = activeParticleIndices_[activeIndex];
		particles_[index].texture = {};
		freeParticleIndices_.push_back(index);
		activeParticleIndices_[activeIndex] = activeParticleIndices_.back();
		activeParticleIndices_.pop_back();
	}

	float ParticleWorld::Random(float minimum, float maximum) {
		const double t = static_cast<double>(random_()) / std::mt19937::max();
		return static_cast<float>(minimum * (1.0 - t) + maximum * t);
	}

}
