#include "LGF/Physics/ParticleWorld.h"

#include "../Collision/CollisionUtility.h"
#include "../Core/HandleUtility.h"
#include "LGF/Collision/Collision3D.h"
#include "LGF/Math/Functions/VectorMath.h"
#include "LGF/Math/MathConstants.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <type_traits>
#include <unordered_map>

namespace {

	using namespace LGF;

	bool IsFinite(const Vector3& value) {
		return std::isfinite(value.x) &&
			std::isfinite(value.y) &&
			std::isfinite(value.z);
	}

	LGF::Physics::CollisionParameter NormalizeCollisionParameter(
		const LGF::Physics::CollisionParameter& collisionParameter) {
		return {
			.restitution = std::isfinite(collisionParameter.restitution)
				? std::clamp(collisionParameter.restitution, 0.0f, 1.0f)
				: 0.1f,
			.friction = std::isfinite(collisionParameter.friction)
				? std::clamp(collisionParameter.friction, 0.0f, 1.0f)
				: 0.4f,
		};
	}

	LGF::Physics::ParticleWorldSettings NormalizeSettings(
		const LGF::Physics::ParticleWorldSettings& settings) {
		LGF::Physics::ParticleWorldSettings result = settings;
		if (!IsFinite(result.gravity)) {
			result.gravity = { 0.0f, -9.8f, 0.0f };
		}
		if (!Collision::IsValid(result.worldBounds) ||
			!IsFinite(result.worldBounds.min) ||
			!IsFinite(result.worldBounds.max)) {
			result.worldBounds = {
				.min = { -100.0f, -100.0f, -100.0f },
				.max = { 100.0f, 100.0f, 100.0f },
			};
		}
		result.substepCount = std::max(result.substepCount, 1u);
		result.solverIterationCount = std::max(result.solverIterationCount, 1u);
		result.linearDamping = std::isfinite(result.linearDamping)
			? std::max(result.linearDamping, 0.0f)
			: 0.1f;
		result.penetrationSlop = std::isfinite(result.penetrationSlop)
			? std::max(result.penetrationSlop, 0.0f)
			: 0.005f;
		result.positionCorrectionRate = std::isfinite(result.positionCorrectionRate)
			? std::clamp(result.positionCorrectionRate, 0.0f, 1.0f)
			: 0.2f;
		result.maximumDepenetrationVelocity =
			std::isfinite(result.maximumDepenetrationVelocity)
			? std::max(result.maximumDepenetrationVelocity, 0.0f)
			: 5.0f;
		result.bounceVelocityThreshold = std::isfinite(result.bounceVelocityThreshold)
			? std::max(result.bounceVelocityThreshold, 0.0f)
			: 0.8f;
		if (!std::isfinite(result.maximumDeltaTime) || result.maximumDeltaTime <= 0.0f) {
			result.maximumDeltaTime = 1.0f / 15.0f;
		}
		return result;
	}

	bool IsParticleDescValid(const LGF::Physics::ParticleDesc& desc) {
		return IsFinite(desc.position) &&
			IsFinite(desc.velocity) &&
			std::isfinite(desc.radius) &&
			desc.radius > 0.0f &&
			std::isfinite(desc.mass) &&
			desc.mass >= 0.0f;
	}

	bool IsColliderShapeValid(const LGF::Physics::StaticColliderShape& shape) {
		return std::visit([](const auto& colliderShape) {
			return Collision::IsValid(colliderShape);
		}, shape);
	}

	Vector3 Lerp(const Vector3& start, const Vector3& end, float t) {
		return start + (end - start) * t;
	}

	LGF::Physics::StaticColliderShape InterpolateColliderShape(
		const LGF::Physics::StaticColliderShape& previous,
		const LGF::Physics::StaticColliderShape& current,
		float t) {
		using LGF::Physics::StaticColliderShape;
		if (previous.index() != current.index()) {
			return current;
		}

		if (const auto* end = std::get_if<Collision::Sphere>(&current)) {
			const auto& start = std::get<Collision::Sphere>(previous);
			return StaticColliderShape{ Collision::Sphere{
				.center = Lerp(start.center, end->center, t),
				.radius = std::lerp(start.radius, end->radius, t),
			} };
		}
		if (const auto* end = std::get_if<Collision::SphericalCap>(&current)) {
			const auto& start = std::get<Collision::SphericalCap>(previous);
			Collision::SphericalCap result{
				.apex = Lerp(start.apex, end->apex, t),
				.axis = Lerp(start.axis, end->axis, t),
				.baseRadius = std::lerp(start.baseRadius, end->baseRadius, t),
				.height = std::lerp(start.height, end->height, t),
			};
			return Collision::IsValid(result) ? StaticColliderShape{ result } : current;
		}
		if (const auto* end = std::get_if<Collision::SphericalShellCap>(&current)) {
			const auto& start = std::get<Collision::SphericalShellCap>(previous);
			Collision::SphericalShellCap result{
				.outerApex = Lerp(start.outerApex, end->outerApex, t),
				.axis = Lerp(start.axis, end->axis, t),
				.outerRadius = std::lerp(start.outerRadius, end->outerRadius, t),
				.innerRadius = std::lerp(start.innerRadius, end->innerRadius, t),
				.height = std::lerp(start.height, end->height, t),
			};
			return Collision::IsValid(result) ? StaticColliderShape{ result } : current;
		}

		const auto& start = std::get<Collision::AABB>(previous);
		const auto& end = std::get<Collision::AABB>(current);
		return StaticColliderShape{ Collision::AABB{
			.min = Lerp(start.min, end.min, t),
			.max = Lerp(start.max, end.max, t),
		} };
	}

	Vector3 GetColliderReferencePosition(
		const LGF::Physics::StaticColliderShape& shape) {
		return std::visit([](const auto& collider) -> Vector3 {
			using T = std::decay_t<decltype(collider)>;
			if constexpr (std::is_same_v<T, Collision::Sphere>) {
				return collider.center;
			} else if constexpr (std::is_same_v<T, Collision::SphericalCap>) {
				return collider.apex;
			} else if constexpr (std::is_same_v<T, Collision::SphericalShellCap>) {
				return collider.outerApex;
			} else {
				return (collider.min + collider.max) * 0.5f;
			}
		}, shape);
	}

	Vector3 CalculateColliderLinearVelocity(
		const LGF::Physics::StaticColliderShape& previous,
		const LGF::Physics::StaticColliderShape& current,
		float frameTime) {
		if (previous.index() != current.index() || frameTime <= 0.0f) {
			return {};
		}
		return (
			GetColliderReferencePosition(current) -
			GetColliderReferencePosition(previous)) / frameTime;
	}

	struct GridCell {
		int32_t x = 0;
		int32_t y = 0;
		int32_t z = 0;

		bool operator==(const GridCell&) const = default;
	};

	struct GridCellHash {
		std::size_t operator()(const GridCell& cell) const {
			std::size_t result = std::hash<int32_t>{}(cell.x);
			result ^= std::hash<int32_t>{}(cell.y) +
				0x9e3779b9u + (result << 6u) + (result >> 2u);
			result ^= std::hash<int32_t>{}(cell.z) +
				0x9e3779b9u + (result << 6u) + (result >> 2u);
			return result;
		}
	};

	GridCell ToGridCell(const Vector3& position, float cellSize) {
		return {
			static_cast<int32_t>(std::floor(position.x / cellSize)),
			static_cast<int32_t>(std::floor(position.y / cellSize)),
			static_cast<int32_t>(std::floor(position.z / cellSize)),
		};
	}

	struct StaticContact {
		Vector3 normal{};
		float penetrationDepth = 0.0f;
	};

	std::optional<StaticContact> FindContact(
		const Collision::Sphere& particle,
		const Collision::Sphere& collider) {
		const Vector3 offset = particle.center - collider.center;
		const float minimumDistance = particle.radius + collider.radius;
		const float distanceSq = Math::LengthSq(offset);
		if (distanceSq > minimumDistance * minimumDistance) {
			return std::nullopt;
		}
		const float distance = std::sqrt(std::max(distanceSq, 0.0f));
		return StaticContact{
			.normal = Collision::Internal::SafeNormal(offset, { 0.0f, 1.0f, 0.0f }),
			.penetrationDepth = minimumDistance - distance,
		};
	}

	std::optional<StaticContact> FindContact(
		const Collision::Sphere& particle,
		const Collision::SphericalCap& collider) {
		if (!Collision::IsValid(particle) || !Collision::IsValid(collider)) {
			return std::nullopt;
		}

		const Vector3 axis = Math::Normalize(collider.axis);
		const float sphereRadius =
			(collider.baseRadius * collider.baseRadius +
				collider.height * collider.height) /
			(2.0f * collider.height);
		const Vector3 sphereCenter = collider.apex + axis * sphereRadius;
		const Vector3 centerOffset = particle.center - sphereCenter;
		const float centerDistance = Math::Length(centerOffset);
		const Vector3 surfaceDirection = Collision::Internal::SafeNormal(centerOffset, -axis);
		const Vector3 surfacePoint = sphereCenter + surfaceDirection * sphereRadius;
		const float surfaceHeight = Math::Dot(surfacePoint - collider.apex, axis);
		if (surfaceHeight < -Math::EPSILON ||
			surfaceHeight > collider.height + Math::EPSILON) {
			return std::nullopt;
		}

		const float penetrationDepth = centerDistance + particle.radius - sphereRadius;
		if (penetrationDepth < 0.0f) {
			return std::nullopt;
		}

		return StaticContact{
			.normal = -surfaceDirection,
			.penetrationDepth = penetrationDepth,
		};
	}

	std::optional<StaticContact> FindContact(
		const Collision::Sphere& particle,
		const Collision::SphericalShellCap& collider) {
		const std::optional<Collision::Contact> contact =
			Collision::Collide(particle, collider);
		if (!contact) {
			return std::nullopt;
		}

		const bool centerInsideMaterial = Collision::Contains(collider, particle.center);
		return StaticContact{
			.normal = centerInsideMaterial ? contact->normal : -contact->normal,
			.penetrationDepth = contact->penetrationDepth,
		};
	}

	std::optional<StaticContact> FindContact(
		const Collision::Sphere& particle,
		const Collision::AABB& collider) {
		const Vector3 closest = Collision::ClosestPoint(particle.center, collider);
		const Vector3 offset = particle.center - closest;
		const float distanceSq = Math::LengthSq(offset);
		if (distanceSq > particle.radius * particle.radius) {
			return std::nullopt;
		}

		if (distanceSq > Math::EPSILON * Math::EPSILON) {
			const float distance = std::sqrt(distanceSq);
			return StaticContact{
				.normal = offset / distance,
				.penetrationDepth = particle.radius - distance,
			};
		}

		if (!Collision::Contains(collider, particle.center)) {
			return std::nullopt;
		}

		const std::array<float, 6> distances{
			particle.center.x - collider.min.x,
			collider.max.x - particle.center.x,
			particle.center.y - collider.min.y,
			collider.max.y - particle.center.y,
			particle.center.z - collider.min.z,
			collider.max.z - particle.center.z,
		};
		const std::array<Vector3, 6> normals{
			Vector3{ -1.0f, 0.0f, 0.0f },
			Vector3{ 1.0f, 0.0f, 0.0f },
			Vector3{ 0.0f, -1.0f, 0.0f },
			Vector3{ 0.0f, 1.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, -1.0f },
			Vector3{ 0.0f, 0.0f, 1.0f },
		};
		const auto nearest = std::min_element(distances.begin(), distances.end());
		const std::size_t index = static_cast<std::size_t>(nearest - distances.begin());
		return StaticContact{
			.normal = normals[index],
			.penetrationDepth = particle.radius + *nearest,
		};
	}

}

namespace LGF::Physics {

	bool ParticleHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	bool StaticColliderHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	ParticleWorld::ParticleWorld(const ParticleWorldSettings& settings) :
		settings_(NormalizeSettings(settings)) {
	}

	ParticleHandle ParticleWorld::Add(const ParticleDesc& desc) {
		if (!IsParticleDescValid(desc)) {
			return {};
		}

		uint32_t index = 0u;
		if (freeParticleIndices_.empty()) {
			index = static_cast<uint32_t>(particles_.size());
			particles_.emplace_back();
		} else {
			index = freeParticleIndices_.back();
			freeParticleIndices_.pop_back();
		}

		ParticleSlot& particle = particles_[index];
		const uint32_t generation = particle.generation;
		particle = ParticleSlot{
			.position = desc.position,
			.previousPosition = desc.position,
			.velocity = desc.velocity,
			.force = {},
			.inverseMass = desc.mass > 0.0f ? 1.0f / desc.mass : 0.0f,
			.radius = desc.radius,
			.collisionParameter = NormalizeCollisionParameter(desc.collisionParameter),
			.userData = desc.userData,
			.generation = generation,
			.enabled = desc.enabled,
			.occupied = true,
		};
		++particleCount_;
		return { index, generation };
	}

	bool ParticleWorld::Remove(ParticleHandle handle) {
		if (!FindParticle(handle)) {
			return false;
		}
		RemoveParticleAt(handle.index);
		return true;
	}

	void ParticleWorld::ClearParticles() {
		freeParticleIndices_.clear();
		freeParticleIndices_.reserve(particles_.size());
		for (uint32_t index = 0u; index < particles_.size(); ++index) {
			ParticleSlot& particle = particles_[index];
			if (particle.occupied) {
				particle.generation = Internal::NextGeneration(particle.generation);
			}
			particle.occupied = false;
			freeParticleIndices_.push_back(index);
		}
		particleCount_ = 0u;
	}

	bool ParticleWorld::IsValid(ParticleHandle handle) const {
		return FindParticle(handle) != nullptr;
	}

	std::size_t ParticleWorld::GetParticleCount() const {
		return particleCount_;
	}

	bool ParticleWorld::SetPosition(
		ParticleHandle handle,
		const Vector3& position,
		bool clearVelocity) {
		ParticleSlot* particle = FindParticle(handle);
		if (!particle || !IsFinite(position)) {
			return false;
		}
		particle->position = position;
		particle->previousPosition = position;
		if (clearVelocity) {
			particle->velocity = {};
		}
		return true;
	}

	bool ParticleWorld::SetVelocity(ParticleHandle handle, const Vector3& velocity) {
		ParticleSlot* particle = FindParticle(handle);
		if (!particle || !IsFinite(velocity)) {
			return false;
		}
		particle->velocity = velocity;
		return true;
	}

	bool ParticleWorld::SetMass(ParticleHandle handle, float mass) {
		ParticleSlot* particle = FindParticle(handle);
		if (!particle || !std::isfinite(mass) || mass < 0.0f) {
			return false;
		}
		particle->inverseMass = mass > 0.0f ? 1.0f / mass : 0.0f;
		if (particle->inverseMass == 0.0f) {
			particle->velocity = {};
		}
		return true;
	}

	bool ParticleWorld::SetRadius(ParticleHandle handle, float radius) {
		ParticleSlot* particle = FindParticle(handle);
		if (!particle || !std::isfinite(radius) || radius <= 0.0f) {
			return false;
		}
		particle->radius = radius;
		return true;
	}

	bool ParticleWorld::SetCollisionParameter(
		ParticleHandle handle,
		const CollisionParameter& collisionParameter) {
		ParticleSlot* particle = FindParticle(handle);
		if (!particle) {
			return false;
		}
		particle->collisionParameter = NormalizeCollisionParameter(collisionParameter);
		return true;
	}

	bool ParticleWorld::SetEnabled(ParticleHandle handle, bool enabled) {
		ParticleSlot* particle = FindParticle(handle);
		if (!particle) {
			return false;
		}
		particle->enabled = enabled;
		return true;
	}

	bool ParticleWorld::SetUserData(ParticleHandle handle, uint64_t userData) {
		ParticleSlot* particle = FindParticle(handle);
		if (!particle) {
			return false;
		}
		particle->userData = userData;
		return true;
	}

	bool ParticleWorld::AddForce(ParticleHandle handle, const Vector3& force) {
		ParticleSlot* particle = FindParticle(handle);
		if (!particle || !IsFinite(force)) {
			return false;
		}
		particle->force += force;
		return true;
	}

	bool ParticleWorld::AddImpulse(ParticleHandle handle, const Vector3& impulse) {
		ParticleSlot* particle = FindParticle(handle);
		if (!particle || !IsFinite(impulse) || particle->inverseMass <= 0.0f) {
			return false;
		}
		particle->velocity += impulse * particle->inverseMass;
		return true;
	}

	std::optional<ParticleState> ParticleWorld::GetState(ParticleHandle handle) const {
		const ParticleSlot* particle = FindParticle(handle);
		if (!particle) {
			return std::nullopt;
		}
		return ParticleState{
			.handle = handle,
			.position = particle->position,
			.velocity = particle->velocity,
			.radius = particle->radius,
			.mass = particle->inverseMass > 0.0f ? 1.0f / particle->inverseMass : 0.0f,
			.collisionParameter = particle->collisionParameter,
			.enabled = particle->enabled,
			.userData = particle->userData,
		};
	}

	void ParticleWorld::GetStates(std::vector<ParticleState>& states) const {
		states.clear();
		states.reserve(particleCount_);
		for (uint32_t index = 0u; index < particles_.size(); ++index) {
			const ParticleSlot& particle = particles_[index];
			if (!particle.occupied) {
				continue;
			}
			states.push_back(ParticleState{
				.handle = { index, particle.generation },
				.position = particle.position,
				.velocity = particle.velocity,
				.radius = particle.radius,
				.mass = particle.inverseMass > 0.0f ? 1.0f / particle.inverseMass : 0.0f,
				.collisionParameter = particle.collisionParameter,
				.enabled = particle.enabled,
				.userData = particle.userData,
			});
		}
	}

	StaticColliderHandle ParticleWorld::AddCollider(
		const StaticColliderShape& shape,
		const StaticColliderOptions& options) {
		if (!IsColliderShapeValid(shape)) {
			return {};
		}

		uint32_t index = 0u;
		if (freeColliderIndices_.empty()) {
			index = static_cast<uint32_t>(colliders_.size());
			colliders_.emplace_back();
		} else {
			index = freeColliderIndices_.back();
			freeColliderIndices_.pop_back();
		}

		ColliderSlot& collider = colliders_[index];
		const uint32_t generation = collider.generation;
		collider = ColliderSlot{
			.shape = shape,
			.previousShape = shape,
			.options = options,
			.generation = generation,
			.occupied = true,
		};
		collider.options.collisionParameter =
			NormalizeCollisionParameter(options.collisionParameter);
		++colliderCount_;
		return { index, generation };
	}

	StaticColliderHandle ParticleWorld::AddCollider(
		const Collision::Sphere& sphere,
		const StaticColliderOptions& options) {
		return AddCollider(StaticColliderShape{ sphere }, options);
	}

	StaticColliderHandle ParticleWorld::AddCollider(
		const Collision::SphericalCap& sphericalCap,
		const StaticColliderOptions& options) {
		return AddCollider(StaticColliderShape{ sphericalCap }, options);
	}

	StaticColliderHandle ParticleWorld::AddCollider(
		const Collision::SphericalShellCap& sphericalShellCap,
		const StaticColliderOptions& options) {
		return AddCollider(StaticColliderShape{ sphericalShellCap }, options);
	}

	StaticColliderHandle ParticleWorld::AddCollider(
		const Collision::AABB& box,
		const StaticColliderOptions& options) {
		return AddCollider(StaticColliderShape{ box }, options);
	}

	bool ParticleWorld::RemoveCollider(StaticColliderHandle handle) {
		ColliderSlot* collider = FindCollider(handle);
		if (!collider) {
			return false;
		}
		collider->occupied = false;
		collider->generation = Internal::NextGeneration(collider->generation);
		freeColliderIndices_.push_back(handle.index);
		--colliderCount_;
		return true;
	}

	void ParticleWorld::ClearColliders() {
		freeColliderIndices_.clear();
		freeColliderIndices_.reserve(colliders_.size());
		for (uint32_t index = 0u; index < colliders_.size(); ++index) {
			ColliderSlot& collider = colliders_[index];
			if (collider.occupied) {
				collider.generation = Internal::NextGeneration(collider.generation);
			}
			collider.occupied = false;
			freeColliderIndices_.push_back(index);
		}
		colliderCount_ = 0u;
	}

	bool ParticleWorld::IsValid(StaticColliderHandle handle) const {
		return FindCollider(handle) != nullptr;
	}

	std::size_t ParticleWorld::GetColliderCount() const {
		return colliderCount_;
	}

	bool ParticleWorld::SetColliderShape(
		StaticColliderHandle handle,
		const StaticColliderShape& shape) {
		ColliderSlot* collider = FindCollider(handle);
		if (!collider || !IsColliderShapeValid(shape)) {
			return false;
		}
		collider->shape = shape;
		return true;
	}

	bool ParticleWorld::SetColliderOptions(
		StaticColliderHandle handle,
		const StaticColliderOptions& options) {
		ColliderSlot* collider = FindCollider(handle);
		if (!collider) {
			return false;
		}
		collider->options = options;
		collider->options.collisionParameter =
			NormalizeCollisionParameter(options.collisionParameter);
		return true;
	}

	const StaticColliderShape* ParticleWorld::GetColliderShape(
		StaticColliderHandle handle) const {
		const ColliderSlot* collider = FindCollider(handle);
		return collider ? &collider->shape : nullptr;
	}

	std::optional<StaticColliderOptions> ParticleWorld::GetColliderOptions(
		StaticColliderHandle handle) const {
		const ColliderSlot* collider = FindCollider(handle);
		return collider
			? std::optional<StaticColliderOptions>(collider->options)
			: std::nullopt;
	}

	void ParticleWorld::SetSettings(const ParticleWorldSettings& settings) {
		settings_ = NormalizeSettings(settings);
	}

	const ParticleWorldSettings& ParticleWorld::GetSettings() const {
		return settings_;
	}

	const ParticleWorldStepStatistics& ParticleWorld::GetLastStepStatistics() const {
		return lastStepStatistics_;
	}

	void ParticleWorld::Clear() {
		ClearParticles();
		ClearColliders();
	}

	void ParticleWorld::UpdateColliderHistory() {
		for (ColliderSlot& collider : colliders_) {
			if (collider.occupied) {
				collider.previousShape = collider.shape;
			}
		}
	}

	void ParticleWorld::IntegrateParticles(float substepTime) {
		for (ParticleSlot& particle : particles_) {
			if (!particle.occupied || !particle.enabled) {
				continue;
			}

			particle.previousPosition = particle.position;
			if (particle.inverseMass <= 0.0f) {
				particle.velocity = {};
				continue;
			}

			const Vector3 acceleration =
				settings_.gravity + particle.force * particle.inverseMass;
			particle.velocity += acceleration * substepTime;
			particle.position += particle.velocity * substepTime;
		}
	}

	std::vector<ParticleWorld::ParticlePair> ParticleWorld::BuildParticlePairs() const {
		std::vector<ParticlePair> pairs;
		float maximumRadius = 0.0f;
		for (const ParticleSlot& particle : particles_) {
			if (particle.occupied && particle.enabled) {
				maximumRadius = std::max(maximumRadius, particle.radius);
			}
		}
		if (maximumRadius <= 0.0f) {
			return pairs;
		}

		// 粒子の直径をセル幅とし、同じセルと周囲26セルだけを候補にする
		const float cellSize = maximumRadius * 2.0f;
		pairs.reserve(particleCount_);
		std::unordered_map<GridCell, std::vector<uint32_t>, GridCellHash> grid;
		grid.reserve(particleCount_);
		for (uint32_t index = 0u; index < particles_.size(); ++index) {
			const ParticleSlot& particle = particles_[index];
			if (particle.occupied && particle.enabled) {
				grid[ToGridCell(particle.position, cellSize)].push_back(index);
			}
		}

		for (uint32_t firstIndex = 0u; firstIndex < particles_.size(); ++firstIndex) {
			const ParticleSlot& first = particles_[firstIndex];
			if (!first.occupied || !first.enabled) {
				continue;
			}

			const GridCell center = ToGridCell(first.position, cellSize);
			for (int32_t z = -1; z <= 1; ++z) {
				for (int32_t y = -1; y <= 1; ++y) {
					for (int32_t x = -1; x <= 1; ++x) {
						const GridCell neighbor{
							center.x + x,
							center.y + y,
							center.z + z,
						};
						const auto iterator = grid.find(neighbor);
						if (iterator == grid.end()) {
							continue;
						}
						for (const uint32_t secondIndex : iterator->second) {
							if (secondIndex > firstIndex) {
								pairs.push_back({ firstIndex, secondIndex });
							}
						}
					}
				}
			}
		}
		return pairs;
	}

	void ParticleWorld::SolvePositions(
		const std::vector<ParticlePair>& particlePairs,
		const std::vector<StaticColliderShape>& colliderShapes,
		const std::vector<Vector3>& colliderVelocities,
		float maximumCorrectionPerIteration,
		std::vector<ContactAccumulator>& contacts,
		std::vector<Vector3>& positionCorrections) {
		const auto recordContact = [&](
			uint32_t particleIndex,
			const Vector3& normal,
			const Vector3& surfaceVelocity,
			const CollisionParameter& particleParameter,
			const CollisionParameter& colliderParameter) {
			ContactAccumulator& contact = contacts[particleIndex];
			contact.normal += normal;
			contact.surfaceVelocity += surfaceVelocity;
			contact.restitution = std::max(
				contact.restitution,
				std::max(particleParameter.restitution, colliderParameter.restitution));
			contact.friction = std::max(
				contact.friction,
				std::sqrt(particleParameter.friction * colliderParameter.friction));
			++contact.contactCount;
			contact.collided = true;
		};

		for (uint32_t iteration = 0u;
			iteration < settings_.solverIterationCount;
			++iteration) {
			for (const ParticlePair& pair : particlePairs) {
				const uint32_t firstIndex = pair.firstIndex;
				const uint32_t secondIndex = pair.secondIndex;
				ParticleSlot& first = particles_[firstIndex];
				ParticleSlot& second = particles_[secondIndex];
				const float inverseMassSum = first.inverseMass + second.inverseMass;
				if (inverseMassSum <= 0.0f) {
					continue;
				}

				const Vector3 delta = second.position - first.position;
				const float minimumDistance = first.radius + second.radius;
				const float distanceSq = Math::LengthSq(delta);
				if (distanceSq >= minimumDistance * minimumDistance) {
					continue;
				}

				// 中心が一致した場合は粒子番号から決めた固定方向を法線に使う
				const Vector3 fallback = (firstIndex + secondIndex) % 3u == 0u
					? Vector3{ 1.0f, 0.0f, 0.0f }
					: ((firstIndex + secondIndex) % 3u == 1u
						? Vector3{ 0.0f, 1.0f, 0.0f }
						: Vector3{ 0.0f, 0.0f, 1.0f });
				const Vector3 normal = Collision::Internal::SafeNormal(delta, fallback);
				const float distance = std::sqrt(std::max(distanceSq, 0.0f));
				const float effectivePenetration = std::max(
					minimumDistance - distance - settings_.penetrationSlop,
					0.0f);
				const float correctionDepth = std::min(
					effectivePenetration * settings_.positionCorrectionRate,
					maximumCorrectionPerIteration);
				const Vector3 correction = normal * (correctionDepth / inverseMassSum);
				const Vector3 firstCorrection = -correction * first.inverseMass;
				const Vector3 secondCorrection = correction * second.inverseMass;
				first.position += firstCorrection;
				second.position += secondCorrection;
				positionCorrections[firstIndex] += firstCorrection;
				positionCorrections[secondIndex] += secondCorrection;
			}

			for (uint32_t particleIndex = 0u;
				particleIndex < particles_.size();
				++particleIndex) {
				ParticleSlot& particle = particles_[particleIndex];
				if (!particle.occupied || !particle.enabled || particle.inverseMass <= 0.0f) {
					continue;
				}

				for (std::size_t colliderIndex = 0u;
					colliderIndex < colliders_.size();
					++colliderIndex) {
					const ColliderSlot& collider = colliders_[colliderIndex];
					if (!collider.occupied || !collider.options.enabled) {
						continue;
					}

					const Collision::Sphere particleSphere{
						.center = particle.position,
						.radius = particle.radius,
					};
					std::visit([&](const auto& shape) {
						const std::optional<StaticContact> contact =
							FindContact(particleSphere, shape);
						if (!contact || contact->penetrationDepth <= 0.0f) {
							return;
						}

						const float effectivePenetration = std::max(
							contact->penetrationDepth - settings_.penetrationSlop,
							0.0f);
						const float correctionDepth = std::min(
							effectivePenetration * settings_.positionCorrectionRate,
							maximumCorrectionPerIteration);
						const Vector3 correction = contact->normal * correctionDepth;
						particle.position += correction;
						positionCorrections[particleIndex] += correction;
						recordContact(
							particleIndex,
							contact->normal,
							colliderVelocities[colliderIndex],
							particle.collisionParameter,
							collider.options.collisionParameter);
					}, colliderShapes[colliderIndex]);
				}
			}
		}
	}

	void ParticleWorld::SolveVelocities(
		const std::vector<ParticlePair>& particlePairs,
		const std::vector<ContactAccumulator>& contacts,
		const std::vector<Vector3>& positionCorrections,
		float substepTime,
		float damping) {
		for (uint32_t index = 0u; index < particles_.size(); ++index) {
			ParticleSlot& particle = particles_[index];
			if (!particle.occupied || !particle.enabled) {
				continue;
			}
			if (particle.inverseMass <= 0.0f) {
				particle.velocity = {};
				continue;
			}

			// 位置補正を除いた移動量から速度を再構築する
			particle.velocity =
				(particle.position - particle.previousPosition -
					positionCorrections[index]) / substepTime;
			particle.velocity *= damping;

			const ContactAccumulator& contact = contacts[index];
			if (!contact.collided) {
				continue;
			}

			const Vector3 normal = Collision::Internal::SafeNormal(
				contact.normal,
				{ 0.0f, 1.0f, 0.0f });
			const Vector3 surfaceVelocity =
				contact.surfaceVelocity / static_cast<float>(contact.contactCount);
			const Vector3 relativeVelocity = particle.velocity - surfaceVelocity;
			const float normalSpeed = Math::Dot(relativeVelocity, normal);
			if (normalSpeed >= 0.0f) {
				continue;
			}

			const Vector3 normalVelocity = normal * normalSpeed;
			const Vector3 tangentVelocity = relativeVelocity - normalVelocity;
			const float restitution = -normalSpeed > settings_.bounceVelocityThreshold
				? contact.restitution
				: 0.0f;
			particle.velocity = surfaceVelocity +
				tangentVelocity * (1.0f - contact.friction) -
				normalVelocity * restitution;
		}

		for (const ParticlePair& pair : particlePairs) {
			const uint32_t firstIndex = pair.firstIndex;
			const uint32_t secondIndex = pair.secondIndex;
			ParticleSlot& first = particles_[firstIndex];
			ParticleSlot& second = particles_[secondIndex];
			const float inverseMassSum = first.inverseMass + second.inverseMass;
			if (inverseMassSum <= 0.0f) {
				continue;
			}

			const Vector3 delta = second.position - first.position;
			const float minimumDistance = first.radius + second.radius;
			if (Math::LengthSq(delta) > minimumDistance * minimumDistance + Math::EPSILON) {
				continue;
			}

			const Vector3 normal = Collision::Internal::SafeNormal(
				delta,
				{ 1.0f, 0.0f, 0.0f });
			const Vector3 relativeVelocity = second.velocity - first.velocity;
			const float normalSpeed = Math::Dot(relativeVelocity, normal);
			if (normalSpeed >= 0.0f) {
				continue;
			}

			const float restitution = -normalSpeed > settings_.bounceVelocityThreshold
				? std::max(
					first.collisionParameter.restitution,
					second.collisionParameter.restitution)
				: 0.0f;
			const float normalImpulseMagnitude =
				-(1.0f + restitution) * normalSpeed / inverseMassSum;
			const Vector3 normalImpulse = normal * normalImpulseMagnitude;
			first.velocity -= normalImpulse * first.inverseMass;
			second.velocity += normalImpulse * second.inverseMass;

			const Vector3 tangentVelocity = relativeVelocity - normal * normalSpeed;
			const float tangentSpeed = Math::Length(tangentVelocity);
			if (tangentSpeed <= Math::EPSILON) {
				continue;
			}

			const float friction = std::sqrt(
				first.collisionParameter.friction *
				second.collisionParameter.friction);
			const float tangentImpulseMagnitude = std::min(
				tangentSpeed / inverseMassSum,
				friction * normalImpulseMagnitude);
			const Vector3 tangentImpulse =
				-tangentVelocity / tangentSpeed * tangentImpulseMagnitude;
			first.velocity -= tangentImpulse * first.inverseMass;
			second.velocity += tangentImpulse * second.inverseMass;
		}
	}

	void ParticleWorld::CleanupParticles() {
		for (uint32_t index = 0u; index < particles_.size(); ++index) {
			ParticleSlot& particle = particles_[index];
			if (!particle.occupied) {
				continue;
			}

			particle.force = {};
			if (!Collision::Contains(settings_.worldBounds, particle.position)) {
				RemoveParticleAt(index);
			}
		}
	}

	void ParticleWorld::Step(float deltaTime) {
		using StepClock = std::chrono::steady_clock;
		const auto toMilliseconds = [](StepClock::duration duration) {
			return std::chrono::duration<double, std::milli>(duration).count();
		};

		lastStepStatistics_ = {};
		// 0以下や非数の経過時間では、安全のためシミュレーションを進めない。
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
			return;
		}
		const StepClock::time_point stepStart = StepClock::now();
		if (particleCount_ == 0u) {
			// 粒子がない間も形状履歴を更新し、再開時の急移動を防ぐ
			UpdateColliderHistory();
			lastStepStatistics_.totalMilliseconds =
				toMilliseconds(StepClock::now() - stepStart);
			return;
		}

		// 極端に大きなdeltaTimeは1回の移動量とめり込みを増やすため、上限を設ける。
		// さらに時間をサブステップへ分割し、衝突を見落としにくくする。
		const float frameTime = std::min(deltaTime, settings_.maximumDeltaTime);
		const float substepTime = frameTime / static_cast<float>(settings_.substepCount);
		// 指数減衰にすると、サブステップ数が変わっても減衰の強さが変わりにくい。
		const float damping = std::exp(-settings_.linearDamping * substepTime);

		for (uint32_t substep = 0u; substep < settings_.substepCount; ++substep) {
			// 動くコライダーをフレーム開始位置から現在位置まで少しずつ進める。
			// いきなり最終位置へ移動させず、各サブステップで衝突を確認する。
			const float colliderInterpolation =
				static_cast<float>(substep + 1u) /
				static_cast<float>(settings_.substepCount);
			std::vector<ContactAccumulator> contacts(particles_.size());
			// めり込み解消のためだけに動かした距離を記録する。
			// 後で速度を求める際に差し引き、押し出しを運動速度へ変換させない。
			std::vector<Vector3> positionCorrections(particles_.size());
			std::vector<StaticColliderShape> interpolatedColliderShapes;
			std::vector<Vector3> colliderLinearVelocities;
			interpolatedColliderShapes.reserve(colliders_.size());
			colliderLinearVelocities.reserve(colliders_.size());
			for (const ColliderSlot& collider : colliders_) {
				interpolatedColliderShapes.push_back(InterpolateColliderShape(
					collider.previousShape,
					collider.shape,
					colliderInterpolation));
				colliderLinearVelocities.push_back(CalculateColliderLinearVelocity(
					collider.previousShape,
					collider.shape,
					frameTime));
			}
			// 深いめり込みを1回で直すと位置が急変するため、
			// 1反復で許可する補正距離へ変換して上限をかける。
			const float maximumCorrectionPerIteration =
				settings_.maximumDepenetrationVelocity * substepTime /
				static_cast<float>(settings_.solverIterationCount);

			// 重力と外力から速度を更新し、その速度で衝突前の予測位置へ進める。
			// 速度を先に更新するため、積分方法は半陰的オイラー法に相当する。
			const StepClock::time_point integrationStart = StepClock::now();
			IntegrateParticles(substepTime);
			lastStepStatistics_.integrationMilliseconds +=
				toMilliseconds(StepClock::now() - integrationStart);

			// 予測位置を使って候補ペアを一度だけ作り、位置補正と速度解決で共有する。
			const StepClock::time_point broadPhaseStart = StepClock::now();
			const std::vector<ParticlePair> particlePairs = BuildParticlePairs();
			lastStepStatistics_.broadPhaseMilliseconds +=
				toMilliseconds(StepClock::now() - broadPhaseStart);
			lastStepStatistics_.candidatePairCount += particlePairs.size();

			const StepClock::time_point positionSolveStart = StepClock::now();
			SolvePositions(
				particlePairs,
				interpolatedColliderShapes,
				colliderLinearVelocities,
				maximumCorrectionPerIteration,
				contacts,
				positionCorrections);
			lastStepStatistics_.positionSolveMilliseconds +=
				toMilliseconds(StepClock::now() - positionSolveStart);

			const StepClock::time_point velocitySolveStart = StepClock::now();
			SolveVelocities(
				particlePairs,
				contacts,
				positionCorrections,
				substepTime,
				damping);
			lastStepStatistics_.velocitySolveMilliseconds +=
				toMilliseconds(StepClock::now() - velocitySolveStart);
		}

		const StepClock::time_point cleanupStart = StepClock::now();
		CleanupParticles();
		// 今回の形状を次フレームの開始形状として保存する
		UpdateColliderHistory();
		lastStepStatistics_.cleanupMilliseconds =
			toMilliseconds(StepClock::now() - cleanupStart);
		lastStepStatistics_.totalMilliseconds =
			toMilliseconds(StepClock::now() - stepStart);
	}

	ParticleWorld::ParticleSlot* ParticleWorld::FindParticle(ParticleHandle handle) {
		if (!handle.IsValid() || handle.index >= particles_.size()) {
			return nullptr;
		}
		ParticleSlot& particle = particles_[handle.index];
		return particle.occupied && particle.generation == handle.generation
			? &particle
			: nullptr;
	}

	const ParticleWorld::ParticleSlot* ParticleWorld::FindParticle(
		ParticleHandle handle) const {
		if (!handle.IsValid() || handle.index >= particles_.size()) {
			return nullptr;
		}
		const ParticleSlot& particle = particles_[handle.index];
		return particle.occupied && particle.generation == handle.generation
			? &particle
			: nullptr;
	}

	ParticleWorld::ColliderSlot* ParticleWorld::FindCollider(
		StaticColliderHandle handle) {
		if (!handle.IsValid() || handle.index >= colliders_.size()) {
			return nullptr;
		}
		ColliderSlot& collider = colliders_[handle.index];
		return collider.occupied && collider.generation == handle.generation
			? &collider
			: nullptr;
	}

	const ParticleWorld::ColliderSlot* ParticleWorld::FindCollider(
		StaticColliderHandle handle) const {
		if (!handle.IsValid() || handle.index >= colliders_.size()) {
			return nullptr;
		}
		const ColliderSlot& collider = colliders_[handle.index];
		return collider.occupied && collider.generation == handle.generation
			? &collider
			: nullptr;
	}

	void ParticleWorld::RemoveParticleAt(uint32_t index) {
		ParticleSlot& particle = particles_[index];
		if (!particle.occupied) {
			return;
		}
		particle.occupied = false;
		particle.generation = Internal::NextGeneration(particle.generation);
		freeParticleIndices_.push_back(index);
		--particleCount_;
	}

}
