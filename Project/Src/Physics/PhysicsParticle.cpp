#include "LGF/Physics/PhysicsWorld.h"

#include "LGF/Graphics/GpuCompute.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

#include "../Core/HandleUtility.h"
#include "PhysicsWorldUtility.h"

namespace {
	bool IsPhysicsParticleDescValid(const LGF::Physics::PhysicsParticleDesc& desc) {
		return LGF::Physics::Internal::IsFinite(desc.position) &&
			LGF::Physics::Internal::IsFinite(desc.velocity) &&
			std::isfinite(desc.radius) && desc.radius > 0.0f &&
			std::isfinite(desc.mass) && desc.mass >= 0.0f;
	}
}

namespace LGF::Physics {

	bool PhysicsParticleHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	bool PhysicsParticleRenderGroupHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	PhysicsParticleHandle PhysicsWorld::AddParticle(const PhysicsParticleDesc& desc) {
		if (!gpuParticleWorldHandle_.IsValid() || !IsPhysicsParticleDescValid(desc) ||
			(desc.renderGroup.IsValid() && !FindParticleRenderGroup(desc.renderGroup)) ||
			desc.radius > settings_.maximumParticleRadius ||
			particleCount_ >= settings_.maximumParticleCount ||
			particleCommands_.size() + 3u > settings_.maximumParticleCommandCount) {
			return {};
		}

		uint32_t index{};
		if (freeParticleIndices_.empty()) {
			index = static_cast<uint32_t>(particles_.size());
			particles_.emplace_back();
		} else {
			index = freeParticleIndices_.back();
			freeParticleIndices_.pop_back();
		}

		PhysicsParticleSlot& particle = particles_[index];
		const uint32_t generation = particle.generation;
		particle = {
			.objectHandle = desc.objectHandle,
			.generation = generation,
			.isOccupied = true,
		};
		const PhysicsMaterial material = Internal::NormalizeMaterial(desc.material);
		particleCommands_.push_back({
			.type = GpuPhysicsParticleCommandType::Add,
			.index = index,
			.generation = generation,
			.boolValue = desc.enabled ? 1u : 0u,
			.vectorValue = desc.position,
			.scalarValue = desc.radius,
			.restitution = material.restitution,
			.friction = material.friction,
			.renderGroupIndex = desc.renderGroup.IsValid()
				? desc.renderGroup.index
				: PhysicsParticleRenderGroupHandle::InvalidIndex,
		});
		particleCommands_.push_back({
			.type = GpuPhysicsParticleCommandType::SetVelocity,
			.index = index,
			.generation = generation,
			.vectorValue = desc.velocity,
		});
		particleCommands_.push_back({
			.type = GpuPhysicsParticleCommandType::SetMass,
			.index = index,
			.generation = generation,
			.scalarValue = desc.mass,
		});
		++particleCount_;
		return { index, generation };
	}

	bool PhysicsWorld::RemoveParticle(PhysicsParticleHandle handle) {
		if (!FindParticle(handle) || !QueueParticleCommand({
			.type = GpuPhysicsParticleCommandType::Remove,
			.index = handle.index,
			.generation = handle.generation,
		})) {
			return false;
		}
		RemoveParticleAt(handle.index);
		return true;
	}

	void PhysicsWorld::ClearParticles() {
		particleCommands_.clear();
		freeParticleIndices_.clear();
		freeParticleIndices_.reserve(particles_.size());
		for (uint32_t index = 0u; index < particles_.size(); ++index) {
			PhysicsParticleSlot& particle = particles_[index];
			if (particle.isOccupied) {
				particle.generation = LGF::Internal::NextGeneration(particle.generation);
			}
			particle.isOccupied = false;
			freeParticleIndices_.push_back(index);
		}
		particleCount_ = 0u;
		lastParticleStepStatistics_ = {};
		if (gpuParticleWorldHandle_.IsValid()) {
			GpuCompute::ClearPhysicsParticleWorld(gpuParticleWorldHandle_);
		}
	}

	bool PhysicsWorld::IsValid(PhysicsParticleHandle handle) const {
		return FindParticle(handle) != nullptr;
	}

	std::size_t PhysicsWorld::GetParticleCount() const {
		return particleCount_;
	}

	PhysicsParticleRenderGroupHandle PhysicsWorld::CreateParticleRenderGroup(
		const PhysicsParticleRenderGroupDesc& desc) {
		if (!Internal::IsFinite(Vector3{ desc.color.x, desc.color.y, desc.color.z }) ||
			!std::isfinite(desc.color.w) ||
			!std::visit([](const auto& visual) {
				using T = std::decay_t<decltype(visual)>;
				if constexpr (std::is_same_v<T, PhysicsParticleModelVisual>) {
					return visual.model.IsValid() && Internal::IsFinite(visual.scale) &&
						visual.scale.x > 0.0f && visual.scale.y > 0.0f && visual.scale.z > 0.0f;
				}
				return true;
			}, desc.visual)) {
			return {};
		}
		const uint32_t index = static_cast<uint32_t>(particleRenderGroups_.size());
		particleRenderGroups_.push_back({
			.desc = desc,
			.generation = 1u,
			.isOccupied = true,
		});
		return { index, 1u };
	}

	bool PhysicsWorld::RemoveParticleRenderGroup(PhysicsParticleRenderGroupHandle handle) {
		PhysicsParticleRenderGroupSlot* group = FindParticleRenderGroup(handle);
		if (!group) {
			return false;
		}
		group->isOccupied = false;
		group->generation = LGF::Internal::NextGeneration(group->generation);
		return true;
	}

	bool PhysicsWorld::SetParticleRenderGroupDesc(
		PhysicsParticleRenderGroupHandle handle,
		const PhysicsParticleRenderGroupDesc& desc) {
		PhysicsParticleRenderGroupSlot* group = FindParticleRenderGroup(handle);
		if (!group ||
			!Internal::IsFinite(Vector3{ desc.color.x, desc.color.y, desc.color.z }) ||
			!std::isfinite(desc.color.w) ||
			!std::visit([](const auto& visual) {
				using T = std::decay_t<decltype(visual)>;
				if constexpr (std::is_same_v<T, PhysicsParticleModelVisual>) {
					return visual.model.IsValid() && Internal::IsFinite(visual.scale) &&
						visual.scale.x > 0.0f && visual.scale.y > 0.0f && visual.scale.z > 0.0f;
				}
				return true;
			}, desc.visual)) {
			return false;
		}
		group->desc = desc;
		return true;
	}

	bool PhysicsWorld::DrawParticleRenderGroups() const {
		bool result = true;
		for (uint32_t index = 0u; index < particleRenderGroups_.size(); ++index) {
			const PhysicsParticleRenderGroupSlot& group = particleRenderGroups_[index];
			if (group.isOccupied) {
				result = std::visit([&](const auto& visual) {
					using T = std::decay_t<decltype(visual)>;
					if constexpr (std::is_same_v<T, PhysicsParticleModelVisual>) {
						return GpuCompute::DrawPhysicsParticleModel(
							gpuParticleWorldHandle_, index, visual.model, visual.scale, group.desc.color);
					}
					return GpuCompute::DrawPhysicsParticleSpheres(
						gpuParticleWorldHandle_, index, group.desc.color);
				}, group.desc.visual) && result;
			}
		}
		return result;
	}

	bool PhysicsWorld::SetParticlePosition(PhysicsParticleHandle handle, const Vector3& position, bool clearVelocity) {
		return Internal::IsFinite(position) && QueueParticleCommand({
			.type = GpuPhysicsParticleCommandType::SetPosition,
			.index = handle.index,
			.generation = handle.generation,
			.boolValue = clearVelocity ? 1u : 0u,
			.vectorValue = position,
		});
	}

	bool PhysicsWorld::SetParticleVelocity(PhysicsParticleHandle handle, const Vector3& velocity) {
		return Internal::IsFinite(velocity) && QueueParticleCommand({
			.type = GpuPhysicsParticleCommandType::SetVelocity,
			.index = handle.index,
			.generation = handle.generation,
			.vectorValue = velocity,
		});
	}

	bool PhysicsWorld::SetParticleMass(PhysicsParticleHandle handle, float mass) {
		return std::isfinite(mass) && mass >= 0.0f && QueueParticleCommand({
			.type = GpuPhysicsParticleCommandType::SetMass,
			.index = handle.index,
			.generation = handle.generation,
			.scalarValue = mass,
		});
	}

	bool PhysicsWorld::SetParticleRadius(PhysicsParticleHandle handle, float radius) {
		return std::isfinite(radius) && radius > 0.0f && radius <= settings_.maximumParticleRadius &&
			QueueParticleCommand({
				.type = GpuPhysicsParticleCommandType::SetRadius,
				.index = handle.index,
				.generation = handle.generation,
				.scalarValue = radius,
			});
	}

	bool PhysicsWorld::SetParticleMaterial(PhysicsParticleHandle handle, const PhysicsMaterial& material) {
		const PhysicsMaterial normalized = Internal::NormalizeMaterial(material);
		return QueueParticleCommand({
			.type = GpuPhysicsParticleCommandType::SetMaterial,
			.index = handle.index,
			.generation = handle.generation,
			.restitution = normalized.restitution,
			.friction = normalized.friction,
		});
	}

	bool PhysicsWorld::SetObjectHandle(PhysicsParticleHandle handle, ObjectHandle objectHandle) {
		PhysicsParticleSlot* particle = FindParticle(handle);
		if (!particle) {
			return false;
		}
		particle->objectHandle = objectHandle;
		return true;
	}

	bool PhysicsWorld::SetParticleEnabled(PhysicsParticleHandle handle, bool enabled) {
		return QueueParticleCommand({
			.type = GpuPhysicsParticleCommandType::SetEnabled,
			.index = handle.index,
			.generation = handle.generation,
			.boolValue = enabled ? 1u : 0u,
		});
	}

	bool PhysicsWorld::SetParticleRenderGroup(
		PhysicsParticleHandle handle,
		PhysicsParticleRenderGroupHandle renderGroup) {
		if (renderGroup.IsValid() && !FindParticleRenderGroup(renderGroup)) {
			return false;
		}
		return QueueParticleCommand({
			.type = GpuPhysicsParticleCommandType::SetRenderGroup,
			.index = handle.index,
			.generation = handle.generation,
			.renderGroupIndex = renderGroup.IsValid()
				? renderGroup.index
				: PhysicsParticleRenderGroupHandle::InvalidIndex,
		});
	}

	bool PhysicsWorld::AddParticleForce(PhysicsParticleHandle handle, const Vector3& force) {
		return Internal::IsFinite(force) && QueueParticleCommand({
			.type = GpuPhysicsParticleCommandType::AddForce,
			.index = handle.index,
			.generation = handle.generation,
			.vectorValue = force,
		});
	}

	bool PhysicsWorld::AddParticleImpulse(PhysicsParticleHandle handle, const Vector3& impulse) {
		return Internal::IsFinite(impulse) && QueueParticleCommand({
			.type = GpuPhysicsParticleCommandType::AddImpulse,
			.index = handle.index,
			.generation = handle.generation,
			.vectorValue = impulse,
		});
	}

	bool PhysicsWorld::RequestParticleStates() {
		return GpuCompute::RequestPhysicsParticleStates(gpuParticleWorldHandle_);
	}

	bool PhysicsWorld::GetLatestParticleStates(
		std::vector<PhysicsParticleState>& states,
		uint32_t& latencyFrames) const {
		std::vector<GpuPhysicsParticleState> gpuStates;
		if (!GpuCompute::GetLatestPhysicsParticleStates(
			gpuParticleWorldHandle_, gpuStates, latencyFrames)) {
			states.clear();
			return false;
		}
		states.clear();
		states.reserve(particleCount_);
		const uint32_t stateCount = static_cast<uint32_t>(std::min(gpuStates.size(), particles_.size()));
		for (uint32_t index = 0u; index < stateCount; ++index) {
			const GpuPhysicsParticleState& gpuState = gpuStates[index];
			const PhysicsParticleSlot& slot = particles_[index];
			if (gpuState.alive == 0u || !slot.isOccupied || slot.generation != gpuState.generation) {
				continue;
			}
			states.push_back({
				.handle = { index, gpuState.generation },
				.position = gpuState.position,
				.velocity = gpuState.velocity,
				.radius = gpuState.radius,
				.mass = gpuState.inverseMass > 0.0f ? 1.0f / gpuState.inverseMass : 0.0f,
				.material = {
					.restitution = gpuState.restitution,
					.friction = gpuState.friction,
				},
				.objectHandle = slot.objectHandle,
				.enabled = gpuState.enabled != 0u,
			});
		}
		return true;
	}

	std::optional<PhysicsParticleState> PhysicsWorld::GetLatestParticleState(
		PhysicsParticleHandle handle,
		uint32_t& latencyFrames) const {
		if (!FindParticle(handle)) {
			latencyFrames = 0u;
			return std::nullopt;
		}
		GpuPhysicsParticleState state{};
		if (!GpuCompute::GetLatestPhysicsParticleState(
			gpuParticleWorldHandle_, handle.index, state, latencyFrames)) {
			return std::nullopt;
		}
		if (state.alive == 0u || state.generation != handle.generation) {
			return std::nullopt;
		}
		const PhysicsParticleSlot& slot = particles_[handle.index];
		return PhysicsParticleState{
			.handle = handle,
			.position = state.position,
			.velocity = state.velocity,
			.radius = state.radius,
			.mass = state.inverseMass > 0.0f ? 1.0f / state.inverseMass : 0.0f,
			.material = {
				.restitution = state.restitution,
				.friction = state.friction,
			},
			.objectHandle = slot.objectHandle,
			.enabled = state.enabled != 0u,
		};
	}

	const PhysicsParticleStepStatistics& PhysicsWorld::GetLastParticleStepStatistics() const {
		return lastParticleStepStatistics_;
	}

	bool PhysicsWorld::QueueParticleCommand(const GpuPhysicsParticleCommand& command) {
		if (!FindParticle({ command.index, command.generation }) ||
			particleCommands_.size() >= settings_.maximumParticleCommandCount) {
			return false;
		}
		particleCommands_.push_back(command);
		return true;
	}

	void PhysicsWorld::ProcessRetiredParticles() {
		std::vector<GpuRetiredPhysicsParticle> retiredParticles;
		if (!GpuCompute::ConsumeRetiredPhysicsParticles(gpuParticleWorldHandle_, retiredParticles)) {
			return;
		}
		for (const GpuRetiredPhysicsParticle& retired : retiredParticles) {
			if (retired.index >= particles_.size()) {
				continue;
			}
			PhysicsParticleSlot& particle = particles_[retired.index];
			if (particle.isOccupied && particle.generation == retired.generation) {
				RemoveParticleAt(retired.index);
			}
		}
	}

	GpuPhysicsCollider PhysicsWorld::ConvertCollisionSurface(const CollisionSurface& surface) const {
		GpuPhysicsCollider result{
			.referencePosition = surface.referencePosition,
			.restitution = surface.material.restitution,
			.linearVelocity = surface.linearVelocity,
			.friction = surface.material.friction,
			.angularVelocity = surface.angularVelocity,
		};
		std::visit([&](const auto& shape) {
			using T = std::decay_t<decltype(shape)>;
			if constexpr (std::is_same_v<T, Collision::Sphere>) {
				result.type = GpuPhysicsColliderType::Sphere;
				result.data0 = { shape.center.x, shape.center.y, shape.center.z, shape.radius };
			} else if constexpr (std::is_same_v<T, Collision::SphericalCap>) {
				result.type = GpuPhysicsColliderType::SphericalCap;
				result.data0 = { shape.apex.x, shape.apex.y, shape.apex.z, shape.baseRadius };
				result.data1 = { shape.axis.x, shape.axis.y, shape.axis.z, shape.height };
			} else if constexpr (std::is_same_v<T, Collision::SphericalShellCap>) {
				result.type = GpuPhysicsColliderType::SphericalShellCap;
				result.data0 = { shape.outerApex.x, shape.outerApex.y, shape.outerApex.z, shape.outerRadius };
				result.data1 = { shape.axis.x, shape.axis.y, shape.axis.z, shape.innerRadius };
				result.data2.x = shape.height;
			} else if constexpr (std::is_same_v<T, Collision::AABB>) {
				result.type = GpuPhysicsColliderType::AABB;
				result.data0 = { shape.min.x, shape.min.y, shape.min.z, 0.0f };
				result.data1 = { shape.max.x, shape.max.y, shape.max.z, 0.0f };
			} else if constexpr (std::is_same_v<T, Collision::OBB>) {
				result.type = GpuPhysicsColliderType::OBB;
				result.data0 = { shape.center.x, shape.center.y, shape.center.z, 0.0f };
				result.data1 = { shape.halfExtents.x, shape.halfExtents.y, shape.halfExtents.z, 0.0f };
				result.data2 = { shape.rotation.x, shape.rotation.y, shape.rotation.z, shape.rotation.w };
			}
		}, surface.shape);
		return result;
	}

	void PhysicsWorld::UpdateParticles(float deltaTime) {
		lastParticleStepStatistics_ = {};
		if (!gpuParticleWorldHandle_.IsValid() || !std::isfinite(deltaTime) || deltaTime <= 0.0f) {
			return;
		}
		ProcessRetiredParticles();

		std::vector<GpuPhysicsCollider> colliders;
		colliders.reserve((staticObjectCount_ + kinematicObjectCount_) * settings_.substepCount);
		uint32_t colliderCountPerSubstep{};
		for (uint32_t substep = 0u; substep < settings_.substepCount; ++substep) {
			const float interpolation = static_cast<float>(substep + 1u) /
				static_cast<float>(settings_.substepCount);
			const std::vector<CollisionSurface> surfaces = BuildCollisionSurfaces(interpolation);
			if (substep == 0u) {
				colliderCountPerSubstep = static_cast<uint32_t>(surfaces.size());
				if (colliderCountPerSubstep > settings_.maximumColliderCount) {
					return;
				}
			}
			for (const CollisionSurface& surface : surfaces) {
				colliders.push_back(ConvertCollisionSurface(surface));
			}
		}

		if (GpuCompute::UpdatePhysicsParticleWorld(
			gpuParticleWorldHandle_, deltaTime, particleCommands_, colliders, colliderCountPerSubstep)) {
			particleCommands_.clear();
		}

		const GpuPhysicsParticleStatistics statistics =
			GpuCompute::GetPhysicsParticleStatistics(gpuParticleWorldHandle_);
		lastParticleStepStatistics_ = {
			.particleCount = statistics.particleCount,
			.candidatePairCount = statistics.candidatePairCount,
			.retiredParticleCount = statistics.retiredParticleCount,
			.latencyFrames = statistics.latencyFrames,
			.isAvailable = statistics.isAvailable,
		};
	}

	PhysicsWorld::PhysicsParticleRenderGroupSlot* PhysicsWorld::FindParticleRenderGroup(
		PhysicsParticleRenderGroupHandle handle) {
		return const_cast<PhysicsParticleRenderGroupSlot*>(
			std::as_const(*this).FindParticleRenderGroup(handle));
	}

	const PhysicsWorld::PhysicsParticleRenderGroupSlot* PhysicsWorld::FindParticleRenderGroup(
		PhysicsParticleRenderGroupHandle handle) const {
		if (!handle.IsValid() || handle.index >= particleRenderGroups_.size()) {
			return nullptr;
		}
		const PhysicsParticleRenderGroupSlot& group = particleRenderGroups_[handle.index];
		return group.isOccupied && group.generation == handle.generation ? &group : nullptr;
	}

	PhysicsWorld::PhysicsParticleSlot* PhysicsWorld::FindParticle(PhysicsParticleHandle handle) {
		return const_cast<PhysicsParticleSlot*>(std::as_const(*this).FindParticle(handle));
	}

	const PhysicsWorld::PhysicsParticleSlot* PhysicsWorld::FindParticle(PhysicsParticleHandle handle) const {
		if (!handle.IsValid() || handle.index >= particles_.size()) {
			return nullptr;
		}
		const PhysicsParticleSlot& particle = particles_[handle.index];
		return particle.isOccupied && particle.generation == handle.generation ? &particle : nullptr;
	}

	void PhysicsWorld::RemoveParticleAt(uint32_t index) {
		PhysicsParticleSlot& particle = particles_[index];
		if (!particle.isOccupied) {
			return;
		}
		particle.isOccupied = false;
		particle.generation = LGF::Internal::NextGeneration(particle.generation);
		freeParticleIndices_.push_back(index);
		--particleCount_;
	}

}
