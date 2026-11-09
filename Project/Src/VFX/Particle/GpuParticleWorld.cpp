#include "LGF/VFX/Particle/ParticleWorld.h"

#include "LGF/Graphics/Graphics3D.h"
#include "LGF/Graphics/Line3D.h"
#include "LGF/Math/MathUtils.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
	std::atomic<uint64_t> g_nextGpuParticleWorldId{ 1u };

	bool IsFinite(const LGF::Vector3& value) {
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	bool IsFinite(const LGF::Vector4& value) {
		return std::isfinite(value.x) && std::isfinite(value.y) &&
			std::isfinite(value.z) && std::isfinite(value.w);
	}

	bool IsSizeValid(const LGF::Vector2& value) {
		return std::isfinite(value.x) && std::isfinite(value.y) &&
			value.x >= 0.0f && value.y >= 0.0f;
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

	GpuParticleWorld::GpuParticleWorld(const ParticleWorldSettings& settings) :
		settings_(settings),
		// CPU版とGPU版のHandleが偶然一致しないよう、GPU版は上位bitを使用する。
		worldId_((uint64_t{ 1u } << 63u) | g_nextGpuParticleWorldId.fetch_add(1u)),
		random_(settings.randomSeed) {
		const auto& bounds = settings.worldBounds;
		if (settings.maximumParticleCount == 0u || settings.maximumParticleCount > 65535u ||
			settings.maximumEmitterCount == 0u || settings.maximumEmitterCount > 65535u ||
			!IsFinite(bounds.min) || !IsFinite(bounds.max) ||
			bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z ||
			!std::isfinite(settings.boundsPadding) || settings.boundsPadding < 0.0f ||
			!std::isfinite(settings.maximumDeltaTime) || settings.maximumDeltaTime <= 0.0f) {
			throw std::invalid_argument("Invalid GPU ParticleWorldSettings.");
		}

		emitters_.reserve(settings.maximumEmitterCount);
		freeEmitterIndices_.reserve(settings.maximumEmitterCount);
		graphicsHandle_ = Graphics3D::CreateGpuParticleWorld({
			.worldMinimum = bounds.min,
			.worldMaximum = bounds.max,
			.maximumParticleCount = settings.maximumParticleCount,
			.maximumSpawnBatchCount = settings.maximumEmitterCount,
			.boundsPadding = settings.boundsPadding,
			.removeOutOfBounds = settings.removeOutOfBounds,
		});
		if (!graphicsHandle_.IsValid()) {
			throw std::runtime_error("Failed to create GPU ParticleWorld resources.");
		}
	}

	GpuParticleWorld::~GpuParticleWorld() {
		if (graphicsHandle_.IsValid()) {
			Graphics3D::RemoveGpuParticleWorld(graphicsHandle_);
		}
	}

	ParticleEmitterHandle GpuParticleWorld::CreateEmitter(const ParticleEmitterDesc& desc) {
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

	bool GpuParticleWorld::RemoveEmitter(ParticleEmitterHandle handle) {
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

	bool GpuParticleWorld::IsValid(ParticleEmitterHandle handle) const {
		return FindEmitter(handle) != nullptr;
	}

	bool GpuParticleWorld::SetEmitterDesc(ParticleEmitterHandle handle, const ParticleEmitterDesc& desc) {
		EmitterSlot* emitter = FindEmitter(handle);
		if (!emitter || !IsEmitterDescValid(desc)) {
			return false;
		}
		emitter->desc = desc;
		emitter->emissionRemainder = 0.0;
		return true;
	}

	bool GpuParticleWorld::SetEmitterPosition(ParticleEmitterHandle handle, const Vector3& position) {
		EmitterSlot* emitter = FindEmitter(handle);
		if (!emitter || !IsFinite(position)) {
			return false;
		}
		emitter->desc.position = position;
		return true;
	}

	bool GpuParticleWorld::SetEmitterEnabled(ParticleEmitterHandle handle, bool enabled) {
		EmitterSlot* emitter = FindEmitter(handle);
		if (!emitter) {
			return false;
		}
		emitter->desc.enabled = enabled;
		return true;
	}

	std::optional<ParticleEmitterDesc> GpuParticleWorld::GetEmitterDesc(ParticleEmitterHandle handle) const {
		const EmitterSlot* emitter = FindEmitter(handle);
		return emitter ? std::optional<ParticleEmitterDesc>{ emitter->desc } : std::nullopt;
	}

	bool GpuParticleWorld::Emit(ParticleEmitterHandle handle, uint32_t count) {
		EmitterSlot* emitter = FindEmitter(handle);
		if (!emitter || emitter->pendingCount > std::numeric_limits<uint64_t>::max() - count) {
			return false;
		}
		emitter->pendingCount += count;
		return true;
	}

	void GpuParticleWorld::Update(float deltaTime) {
		if (!std::isfinite(deltaTime) || deltaTime < 0.0f) {
			return;
		}
		const float frameTime = std::min(deltaTime, settings_.maximumDeltaTime);
		std::vector<GpuParticleSpawnBatch3D> spawnBatches;
		spawnBatches.reserve(emitterCount_);

		for (EmitterSlot& emitter : emitters_) {
			if (!emitter.isOccupied) {
				continue;
			}
			uint64_t count = std::min<uint64_t>(
				emitter.pendingCount, settings_.maximumParticleCount);
			emitter.pendingCount = 0u;
			if (emitter.desc.enabled) {
				const double amount = emitter.emissionRemainder +
					static_cast<double>(emitter.desc.emissionRate) * frameTime;
				emitter.emissionRemainder = amount - std::floor(amount);
				const uint64_t continuousCount = amount >= static_cast<double>(settings_.maximumParticleCount)
					? settings_.maximumParticleCount : static_cast<uint64_t>(amount);
				count = std::min<uint64_t>(
					count + continuousCount, settings_.maximumParticleCount);
			}
			if (count == 0u) {
				continue;
			}

			// GPUが確保できる粒子数より大きな要求をDispatchしない。
			// 超過分は初版ではGPU Counterに含めず、今後64bit統計へ拡張する余地を残す。
			const ParticleEmitterDesc& desc = emitter.desc;
			spawnBatches.push_back({
				.position = desc.position,
				.minimumVelocity = desc.minimumVelocity,
				.maximumVelocity = desc.maximumVelocity,
				.acceleration = desc.acceleration,
				.minimumLifetime = desc.minimumLifetime,
				.maximumLifetime = desc.maximumLifetime,
				.startColor = desc.startColor,
				.endColor = desc.endColor,
				.startSize = desc.startSize,
				.endSize = desc.endSize,
				.minimumRotation = desc.minimumRotation,
				.maximumRotation = desc.maximumRotation,
				.minimumAngularVelocity = desc.minimumAngularVelocity,
				.maximumAngularVelocity = desc.maximumAngularVelocity,
				.count = static_cast<uint32_t>(std::min<uint64_t>(count, settings_.maximumParticleCount)),
				.randomSeed = random_(),
				.texture = desc.texture,
			});
		}

		Graphics3D::UpdateGpuParticleWorld(graphicsHandle_, frameTime, spawnBatches);
		const GpuParticleStatistics3D statistics = Graphics3D::GetGpuParticleStatistics(graphicsHandle_);
		latestStatistics_ = {
			.particleCount = statistics.particleCount,
			.emitterCount = emitterCount_,
			.generatedCount = statistics.generatedCount,
			.rejectedCount = statistics.rejectedCount,
			.expiredCount = statistics.expiredCount,
			.outOfBoundsCount = statistics.outOfBoundsCount,
			.latencyFrames = statistics.latencyFrames,
			.isAvailable = statistics.isAvailable,
		};
	}

	void GpuParticleWorld::Draw() const {
		Graphics3D::DrawGpuParticleWorld(graphicsHandle_);
	}

	void GpuParticleWorld::DrawDebug() const {
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
					Line3D{ corners[i], corners[i | axis] }.Draw(Math::Identity4x4(), Color::Cyan);
				}
			}
		}

		if (ImGui::GetCurrentContext() && ImGui::Begin("GPU Particle Worlds")) {
			ImGui::PushID(this);
			if (latestStatistics_.isAvailable) {
				ImGui::Text("%u particles / %u emitters (%u frames delayed)",
					latestStatistics_.particleCount, emitterCount_, latestStatistics_.latencyFrames);
				ImGui::Text("Generated: %llu / Rejected: %llu",
					static_cast<unsigned long long>(latestStatistics_.generatedCount),
					static_cast<unsigned long long>(latestStatistics_.rejectedCount));
				ImGui::Text("Expired: %u / Out of bounds: %u", latestStatistics_.expiredCount,
					latestStatistics_.outOfBoundsCount);
			}
			else {
				ImGui::TextUnformatted("Statistics readback is warming up...");
			}
			ImGui::PopID();
		}
		if (ImGui::GetCurrentContext()) {
			ImGui::End();
		}
	}

	void GpuParticleWorld::Clear() {
		for (uint32_t i = 0u; i < emitters_.size(); ++i) {
			EmitterSlot& emitter = emitters_[i];
			emitter.isOccupied = false;
			emitter.pendingCount = 0u;
			emitter.desc.texture = {};
			AdvanceGeneration(emitter.generation);
		}
		freeEmitterIndices_.clear();
		for (uint32_t i = static_cast<uint32_t>(emitters_.size()); i > 0u; --i) {
			freeEmitterIndices_.push_back(i - 1u);
		}
		emitterCount_ = 0u;
		random_.seed(settings_.randomSeed);
		latestStatistics_ = { .isAvailable = false };
		Graphics3D::ClearGpuParticleWorld(graphicsHandle_);
	}

	std::size_t GpuParticleWorld::GetParticleCount() const { return latestStatistics_.particleCount; }
	std::size_t GpuParticleWorld::GetEmitterCount() const { return emitterCount_; }
	const ParticleWorldSettings& GpuParticleWorld::GetSettings() const { return settings_; }
	const ParticleStepStatistics& GpuParticleWorld::GetLatestStatistics() const { return latestStatistics_; }

	GpuParticleWorld::EmitterSlot* GpuParticleWorld::FindEmitter(ParticleEmitterHandle handle) {
		return const_cast<EmitterSlot*>(std::as_const(*this).FindEmitter(handle));
	}

	const GpuParticleWorld::EmitterSlot* GpuParticleWorld::FindEmitter(ParticleEmitterHandle handle) const {
		if (handle.worldId != worldId_ || handle.index >= emitters_.size()) {
			return nullptr;
		}
		const EmitterSlot& emitter = emitters_[handle.index];
		return emitter.isOccupied && emitter.generation == handle.generation ? &emitter : nullptr;
	}

}
