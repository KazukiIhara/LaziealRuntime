#include "LGF/Physics/ParticleWorld.h"

#include "LGF/Collision/Collision3D.h"
#include "LGF/Math/Functions/VectorMath.h"
#include "LGF/Math/MathConstants.h"

#include <algorithm>
#include <array>
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

	Vector3 SafeNormal(const Vector3& value, const Vector3& fallback) {
		const float lengthSq = Math::LengthSq(value);
		if (lengthSq <= Math::EPSILON * Math::EPSILON) {
			return fallback;
		}
		return value / std::sqrt(lengthSq);
	}

	uint32_t NextGeneration(uint32_t generation) {
		++generation;
		return generation == 0u ? 1u : generation;
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
			.normal = SafeNormal(offset, { 0.0f, 1.0f, 0.0f }),
			.penetrationDepth = minimumDistance - distance,
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
				particle.generation = NextGeneration(particle.generation);
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
		collider->generation = NextGeneration(collider->generation);
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
				collider.generation = NextGeneration(collider.generation);
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

	void ParticleWorld::Clear() {
		ClearParticles();
		ClearColliders();
	}

	void ParticleWorld::Step(float deltaTime) {
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f || particleCount_ == 0u) {
			return;
		}

		const float frameTime = std::min(deltaTime, settings_.maximumDeltaTime);
		const float substepTime = frameTime / static_cast<float>(settings_.substepCount);
		const float damping = std::exp(-settings_.linearDamping * substepTime);

		struct ContactAccumulator {
			Vector3 normal{};
			float restitution = 0.0f;
			float friction = 0.0f;
			bool collided = false;
		};

		auto forEachParticlePair = [&](auto&& callback) {
			float maximumRadius = 0.0f;
			for (const ParticleSlot& particle : particles_) {
				if (particle.occupied && particle.enabled) {
					maximumRadius = std::max(maximumRadius, particle.radius);
				}
			}
			if (maximumRadius <= 0.0f) {
				return;
			}

			const float cellSize = maximumRadius * 2.0f;
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
									callback(firstIndex, secondIndex);
								}
							}
						}
					}
				}
			}
		};

		for (uint32_t substep = 0u; substep < settings_.substepCount; ++substep) {
			std::vector<ContactAccumulator> contacts(particles_.size());

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

			auto recordContact = [&](
				uint32_t particleIndex,
				const Vector3& normal,
				const CollisionParameter& particleParameter,
				const CollisionParameter& colliderParameter) {
				ContactAccumulator& contact = contacts[particleIndex];
				contact.normal += normal;
				contact.restitution = std::max(
					contact.restitution,
					std::max(particleParameter.restitution, colliderParameter.restitution));
				contact.friction = std::max(
					contact.friction,
					std::sqrt(particleParameter.friction * colliderParameter.friction));
				contact.collided = true;
			};

			for (uint32_t iteration = 0u;
				iteration < settings_.solverIterationCount;
				++iteration) {
				forEachParticlePair([&](uint32_t firstIndex, uint32_t secondIndex) {
					ParticleSlot& first = particles_[firstIndex];
					ParticleSlot& second = particles_[secondIndex];
					const float inverseMassSum = first.inverseMass + second.inverseMass;
					if (inverseMassSum <= 0.0f) {
						return;
					}

					const Vector3 delta = second.position - first.position;
					const float minimumDistance = first.radius + second.radius;
					const float distanceSq = Math::LengthSq(delta);
					if (distanceSq >= minimumDistance * minimumDistance) {
						return;
					}

					const Vector3 fallback = (firstIndex + secondIndex) % 3u == 0u
						? Vector3{ 1.0f, 0.0f, 0.0f }
						: ((firstIndex + secondIndex) % 3u == 1u
							? Vector3{ 0.0f, 1.0f, 0.0f }
							: Vector3{ 0.0f, 0.0f, 1.0f });
					const Vector3 normal = SafeNormal(delta, fallback);
					const float distance = std::sqrt(std::max(distanceSq, 0.0f));
					const Vector3 correction =
						normal * ((minimumDistance - distance) / inverseMassSum);
					first.position -= correction * first.inverseMass;
					second.position += correction * second.inverseMass;
				});

				for (uint32_t particleIndex = 0u;
					particleIndex < particles_.size();
					++particleIndex) {
					ParticleSlot& particle = particles_[particleIndex];
					if (!particle.occupied || !particle.enabled || particle.inverseMass <= 0.0f) {
						continue;
					}

					for (const ColliderSlot& collider : colliders_) {
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
							particle.position +=
								contact->normal * contact->penetrationDepth;
							recordContact(
								particleIndex,
								contact->normal,
								particle.collisionParameter,
								collider.options.collisionParameter);
						}, collider.shape);
					}
				}
			}

			for (uint32_t index = 0u; index < particles_.size(); ++index) {
				ParticleSlot& particle = particles_[index];
				if (!particle.occupied || !particle.enabled) {
					continue;
				}
				if (particle.inverseMass <= 0.0f) {
					particle.velocity = {};
					continue;
				}
				particle.velocity =
					(particle.position - particle.previousPosition) / substepTime;
				particle.velocity *= damping;

				const ContactAccumulator& contact = contacts[index];
				if (!contact.collided) {
					continue;
				}
				const Vector3 normal = SafeNormal(contact.normal, { 0.0f, 1.0f, 0.0f });
				const float normalSpeed = Math::Dot(particle.velocity, normal);
				if (normalSpeed < 0.0f) {
					const Vector3 normalVelocity = normal * normalSpeed;
					const Vector3 tangentVelocity = particle.velocity - normalVelocity;
					particle.velocity =
						tangentVelocity * (1.0f - contact.friction) -
						normalVelocity * contact.restitution;
				}
			}

			forEachParticlePair([&](uint32_t firstIndex, uint32_t secondIndex) {
				ParticleSlot& first = particles_[firstIndex];
				ParticleSlot& second = particles_[secondIndex];
				const float inverseMassSum = first.inverseMass + second.inverseMass;
				if (inverseMassSum <= 0.0f) {
					return;
				}

				const Vector3 delta = second.position - first.position;
				const float minimumDistance = first.radius + second.radius;
				if (Math::LengthSq(delta) > minimumDistance * minimumDistance + Math::EPSILON) {
					return;
				}
				const Vector3 normal = SafeNormal(delta, { 1.0f, 0.0f, 0.0f });
				const Vector3 relativeVelocity = second.velocity - first.velocity;
				const float normalSpeed = Math::Dot(relativeVelocity, normal);
				if (normalSpeed >= 0.0f) {
					return;
				}

				const float restitution = std::max(
					first.collisionParameter.restitution,
					second.collisionParameter.restitution);
				const float normalImpulseMagnitude =
					-(1.0f + restitution) * normalSpeed / inverseMassSum;
				const Vector3 normalImpulse = normal * normalImpulseMagnitude;
				first.velocity -= normalImpulse * first.inverseMass;
				second.velocity += normalImpulse * second.inverseMass;

				const Vector3 tangentVelocity = relativeVelocity - normal * normalSpeed;
				const float tangentSpeed = Math::Length(tangentVelocity);
				if (tangentSpeed <= Math::EPSILON) {
					return;
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
			});
		}

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
		particle.generation = NextGeneration(particle.generation);
		freeParticleIndices_.push_back(index);
		--particleCount_;
	}

}
