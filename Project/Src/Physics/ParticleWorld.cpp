#include "LGF/Physics/ParticleWorld.h"

#include "LGF/Collision/Collision3D.h"
#include "LGF/Math/Functions/VectorMath.h"
#include "LGF/Math/MathConstants.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace {

	using namespace LGF;

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

	bool IsBoundaryShapeValid(const LGF::Physics::ParticleBoundaryShape& shape) {
		return std::visit([](const auto& value) {
			return LGF::Collision::IsValid(value);
		}, shape);
	}

	bool IsBoundaryOptionsValid(const LGF::Physics::ParticleBoundaryOptions& options) {
		return !options.cuttingPlane || LGF::Collision::IsValid(*options.cuttingPlane);
	}

	bool IsSurfaceRetained(
		const Vector3& point,
		const std::optional<LGF::Collision::Plane>& cuttingPlane) {
		if (!cuttingPlane) {
			return true;
		}

		const float distance =
			Math::Dot(cuttingPlane->normal, point) + cuttingPlane->distance;
		return distance <= Math::EPSILON;
	}

	Vector3 SafeNormal(const Vector3& value, const Vector3& fallback) {
		const float lengthSq = Math::LengthSq(value);
		if (lengthSq <= Math::EPSILON * Math::EPSILON) {
			return fallback;
		}
		return value / std::sqrt(lengthSq);
	}

	Vector3 Perpendicular(const Vector3& direction) {
		const Vector3 reference = std::abs(direction.y) < 0.9f
			? Vector3{ 0.0f, 1.0f, 0.0f }
			: Vector3{ 1.0f, 0.0f, 0.0f };
		return SafeNormal(Math::Cross(direction, reference), { 1.0f, 0.0f, 0.0f });
	}

	GridCell ToGridCell(const Vector3& position, float cellSize) {
		return {
			static_cast<int32_t>(std::floor(position.x / cellSize)),
			static_cast<int32_t>(std::floor(position.y / cellSize)),
			static_cast<int32_t>(std::floor(position.z / cellSize)),
		};
	}

	uint32_t NextGeneration(uint32_t generation) {
		++generation;
		return generation == 0u ? 1u : generation;
	}

}

namespace LGF::Physics {

	bool ParticleHandle::IsValid() const {
		return index != InvalidIndex;
	}

	bool ParticleBoundaryHandle::IsValid() const {
		return index != InvalidIndex;
	}

	ParticleWorld::ParticleWorld(const ParticleWorldSettings& settings)
		: settings_(NormalizeSettings(settings)) {}

	ParticleHandle ParticleWorld::Add(const ParticleDesc& desc) {
		if (
			!IsFinite(desc.position) ||
			!IsFinite(desc.velocity) ||
			!std::isfinite(desc.radius) ||
			!std::isfinite(desc.mass) ||
			desc.radius <= 0.0f ||
			desc.mass < 0.0f) {
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
		return { index, particle.generation };
	}

	bool ParticleWorld::Remove(ParticleHandle handle) {
		ParticleSlot* particle = FindParticle(handle);
		if (!particle) {
			return false;
		}

		particle->occupied = false;
		particle->generation = NextGeneration(particle->generation);
		freeParticleIndices_.push_back(handle.index);
		--particleCount_;
		return true;
	}

	void ParticleWorld::ClearParticles() {
		freeParticleIndices_.clear();
		for (uint32_t index = 0u; index < particles_.size(); ++index) {
			ParticleSlot& particle = particles_[index];
			particle.occupied = false;
			particle.generation = NextGeneration(particle.generation);
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
		if (!particle || !IsFinite(impulse)) {
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

	ParticleBoundaryHandle ParticleWorld::AddBoundary(
		const ParticleBoundaryShape& shape,
		const ParticleBoundaryOptions& options) {
		if (!IsBoundaryShapeValid(shape) || !IsBoundaryOptionsValid(options)) {
			return {};
		}

		uint32_t index = 0u;
		if (freeBoundaryIndices_.empty()) {
			index = static_cast<uint32_t>(boundaries_.size());
			boundaries_.emplace_back();
		} else {
			index = freeBoundaryIndices_.back();
			freeBoundaryIndices_.pop_back();
		}

		BoundarySlot& boundary = boundaries_[index];
		const uint32_t generation = boundary.generation;
		boundary = BoundarySlot{
			.shape = shape,
			.options = options,
			.generation = generation,
			.occupied = true,
		};
		boundary.options.collisionParameter =
			NormalizeCollisionParameter(boundary.options.collisionParameter);
		++boundaryCount_;
		return { index, boundary.generation };
	}

	ParticleBoundaryHandle ParticleWorld::AddBoundary(
		const Collision::Sphere& sphere,
		const ParticleBoundaryOptions& options) {
		return AddBoundary(ParticleBoundaryShape{ sphere }, options);
	}

	ParticleBoundaryHandle ParticleWorld::AddBoundary(
		const Collision::AABB& box,
		const ParticleBoundaryOptions& options) {
		return AddBoundary(ParticleBoundaryShape{ box }, options);
	}

	ParticleBoundaryHandle ParticleWorld::AddBoundary(
		const Collision::Cylinder& cylinder,
		const ParticleBoundaryOptions& options) {
		return AddBoundary(ParticleBoundaryShape{ cylinder }, options);
	}

	bool ParticleWorld::RemoveBoundary(ParticleBoundaryHandle handle) {
		BoundarySlot* boundary = FindBoundary(handle);
		if (!boundary) {
			return false;
		}
		boundary->occupied = false;
		boundary->generation = NextGeneration(boundary->generation);
		freeBoundaryIndices_.push_back(handle.index);
		--boundaryCount_;
		return true;
	}

	void ParticleWorld::ClearBoundaries() {
		freeBoundaryIndices_.clear();
		for (uint32_t index = 0u; index < boundaries_.size(); ++index) {
			BoundarySlot& boundary = boundaries_[index];
			boundary.occupied = false;
			boundary.generation = NextGeneration(boundary.generation);
			freeBoundaryIndices_.push_back(index);
		}
		boundaryCount_ = 0u;
	}

	bool ParticleWorld::IsValid(ParticleBoundaryHandle handle) const {
		return FindBoundary(handle) != nullptr;
	}

	std::size_t ParticleWorld::GetBoundaryCount() const {
		return boundaryCount_;
	}

	bool ParticleWorld::SetBoundaryShape(
		ParticleBoundaryHandle handle,
		const ParticleBoundaryShape& shape) {
		BoundarySlot* boundary = FindBoundary(handle);
		if (!boundary || !IsBoundaryShapeValid(shape)) {
			return false;
		}
		boundary->shape = shape;
		return true;
	}

	bool ParticleWorld::SetBoundaryOptions(
		ParticleBoundaryHandle handle,
		const ParticleBoundaryOptions& options) {
		BoundarySlot* boundary = FindBoundary(handle);
		if (!boundary || !IsBoundaryOptionsValid(options)) {
			return false;
		}
		boundary->options = options;
		boundary->options.collisionParameter =
			NormalizeCollisionParameter(options.collisionParameter);
		return true;
	}

	const ParticleBoundaryShape* ParticleWorld::GetBoundaryShape(
		ParticleBoundaryHandle handle) const {
		const BoundarySlot* boundary = FindBoundary(handle);
		return boundary ? &boundary->shape : nullptr;
	}

	std::optional<ParticleBoundaryOptions> ParticleWorld::GetBoundaryOptions(
		ParticleBoundaryHandle handle) const {
		const BoundarySlot* boundary = FindBoundary(handle);
		return boundary
			? std::optional<ParticleBoundaryOptions>(boundary->options)
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
		ClearBoundaries();
	}

	void ParticleWorld::Step(float deltaTime) {
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f || particleCount_ == 0u) {
			return;
		}

		const float frameTime = std::min(deltaTime, settings_.maximumDeltaTime);
		const float substepTime = frameTime / static_cast<float>(settings_.substepCount);
		const float damping = std::exp(-settings_.linearDamping * substepTime);

		struct BoundaryContact {
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
			std::vector<BoundaryContact> boundaryContacts(particles_.size());

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

			auto recordBoundaryContact = [&](
				uint32_t particleIndex,
				const Vector3& normal,
				const CollisionParameter& particleCollisionParameter,
				const CollisionParameter& boundaryCollisionParameter) {
				BoundaryContact& contact = boundaryContacts[particleIndex];
				contact.normal += normal;
				contact.restitution = std::max(
					contact.restitution,
					std::max(
						particleCollisionParameter.restitution,
						boundaryCollisionParameter.restitution));
				contact.friction = std::max(
					contact.friction,
					std::sqrt(
						particleCollisionParameter.friction *
						boundaryCollisionParameter.friction));
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

					for (const BoundarySlot& boundary : boundaries_) {
						if (!boundary.occupied || !boundary.options.enabled) {
							continue;
						}

						std::visit([&](const auto& shape) {
							using Shape = std::decay_t<decltype(shape)>;
							if constexpr (std::is_same_v<Shape, Collision::Sphere>) {
								const float allowedRadius = shape.radius - particle.radius;
								if (allowedRadius < 0.0f) {
									return;
								}
								const Vector3 offset = particle.position - shape.center;
								const float distanceSq = Math::LengthSq(offset);
								if (distanceSq <= allowedRadius * allowedRadius) {
									return;
								}
								const Vector3 outward = SafeNormal(
									offset,
									{ 0.0f, 1.0f, 0.0f });
								const Vector3 surfacePoint =
									shape.center + outward * shape.radius;
								if (!IsSurfaceRetained(
									surfacePoint,
									boundary.options.cuttingPlane)) {
									return;
								}
								particle.position =
									shape.center + outward * allowedRadius;
								recordBoundaryContact(
									particleIndex,
									-outward,
									particle.collisionParameter,
									boundary.options.collisionParameter);
							} else if constexpr (std::is_same_v<Shape, Collision::AABB>) {
								const Vector3 minimum = shape.min + Vector3{
									particle.radius,
									particle.radius,
									particle.radius,
								};
								const Vector3 maximum = shape.max - Vector3{
									particle.radius,
									particle.radius,
									particle.radius,
								};
								if (
									minimum.x > maximum.x ||
									minimum.y > maximum.y ||
									minimum.z > maximum.z) {
									return;
								}

								const auto solveFace = [&] (
									float& coordinate,
									float limit,
									bool isMinimum,
									const Vector3& normal,
									const Vector3& surfacePoint) {
									const bool outside = isMinimum
										? coordinate < limit
										: coordinate > limit;
									if (
										!outside ||
										!IsSurfaceRetained(
											surfacePoint,
											boundary.options.cuttingPlane)) {
										return;
									}
									coordinate = limit;
									recordBoundaryContact(
										particleIndex,
										normal,
										particle.collisionParameter,
										boundary.options.collisionParameter);
								};

								solveFace(
									particle.position.x,
									minimum.x,
									true,
									{ 1.0f, 0.0f, 0.0f },
									{ shape.min.x,
										std::clamp(particle.position.y, shape.min.y, shape.max.y),
										std::clamp(particle.position.z, shape.min.z, shape.max.z) });
								solveFace(
									particle.position.x,
									maximum.x,
									false,
									{ -1.0f, 0.0f, 0.0f },
									{ shape.max.x,
										std::clamp(particle.position.y, shape.min.y, shape.max.y),
										std::clamp(particle.position.z, shape.min.z, shape.max.z) });
								solveFace(
									particle.position.y,
									minimum.y,
									true,
									{ 0.0f, 1.0f, 0.0f },
									{ std::clamp(particle.position.x, shape.min.x, shape.max.x),
										shape.min.y,
										std::clamp(particle.position.z, shape.min.z, shape.max.z) });
								solveFace(
									particle.position.y,
									maximum.y,
									false,
									{ 0.0f, -1.0f, 0.0f },
									{ std::clamp(particle.position.x, shape.min.x, shape.max.x),
										shape.max.y,
										std::clamp(particle.position.z, shape.min.z, shape.max.z) });
								solveFace(
									particle.position.z,
									minimum.z,
									true,
									{ 0.0f, 0.0f, 1.0f },
									{ std::clamp(particle.position.x, shape.min.x, shape.max.x),
										std::clamp(particle.position.y, shape.min.y, shape.max.y),
										shape.min.z });
								solveFace(
									particle.position.z,
									maximum.z,
									false,
									{ 0.0f, 0.0f, -1.0f },
									{ std::clamp(particle.position.x, shape.min.x, shape.max.x),
										std::clamp(particle.position.y, shape.min.y, shape.max.y),
										shape.max.z });
							} else if constexpr (std::is_same_v<Shape, Collision::Cylinder>) {
								const Vector3 axis = shape.end - shape.start;
								const float axisLength = Math::Length(axis);
								const float allowedRadius = shape.radius - particle.radius;
								const float minimumHeight = particle.radius;
								const float maximumHeight = axisLength - particle.radius;
								if (
									axisLength <= Math::EPSILON ||
									allowedRadius < 0.0f ||
									minimumHeight > maximumHeight) {
									return;
								}

								const Vector3 axisDirection = axis / axisLength;
								Vector3 offset = particle.position - shape.start;
								float height = Math::Dot(offset, axisDirection);
								Vector3 radial = offset - axisDirection * height;
								float radialLength = Math::Length(radial);
								const Vector3 radialDirection = SafeNormal(
									radial,
									Perpendicular(axisDirection));

								if (radialLength > allowedRadius) {
									const Vector3 surfacePoint =
										shape.start +
										axisDirection * std::clamp(height, 0.0f, axisLength) +
										radialDirection * shape.radius;
									if (IsSurfaceRetained(
										surfacePoint,
										boundary.options.cuttingPlane)) {
										particle.position -=
											radialDirection * (radialLength - allowedRadius);
										recordBoundaryContact(
											particleIndex,
											-radialDirection,
											particle.collisionParameter,
											boundary.options.collisionParameter);
									}
								}

								offset = particle.position - shape.start;
								height = Math::Dot(offset, axisDirection);
								radial = offset - axisDirection * height;
								radialLength = Math::Length(radial);
								const Vector3 clampedRadial = radialLength > shape.radius
									? SafeNormal(radial, radialDirection) * shape.radius
									: radial;

								if (height < minimumHeight) {
									const Vector3 surfacePoint = shape.start + clampedRadial;
									if (IsSurfaceRetained(
										surfacePoint,
										boundary.options.cuttingPlane)) {
										particle.position +=
											axisDirection * (minimumHeight - height);
										recordBoundaryContact(
											particleIndex,
											axisDirection,
											particle.collisionParameter,
											boundary.options.collisionParameter);
									}
								} else if (height > maximumHeight) {
									const Vector3 surfacePoint = shape.end + clampedRadial;
									if (IsSurfaceRetained(
										surfacePoint,
										boundary.options.cuttingPlane)) {
										particle.position -=
											axisDirection * (height - maximumHeight);
										recordBoundaryContact(
											particleIndex,
											-axisDirection,
											particle.collisionParameter,
											boundary.options.collisionParameter);
									}
								}
							}
						}, boundary.shape);
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

				const BoundaryContact& contact = boundaryContacts[index];
				if (!contact.collided) {
					continue;
				}
				const Vector3 normal = SafeNormal(
					contact.normal,
					{ 0.0f, 1.0f, 0.0f });
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

				const float restitution =
					std::max(
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
				const float friction =
					std::sqrt(
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

		for (ParticleSlot& particle : particles_) {
			if (particle.occupied) {
				particle.force = {};
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

	ParticleWorld::BoundarySlot* ParticleWorld::FindBoundary(
		ParticleBoundaryHandle handle) {
		if (!handle.IsValid() || handle.index >= boundaries_.size()) {
			return nullptr;
		}
		BoundarySlot& boundary = boundaries_[handle.index];
		return boundary.occupied && boundary.generation == handle.generation
			? &boundary
			: nullptr;
	}

	const ParticleWorld::BoundarySlot* ParticleWorld::FindBoundary(
		ParticleBoundaryHandle handle) const {
		if (!handle.IsValid() || handle.index >= boundaries_.size()) {
			return nullptr;
		}
		const BoundarySlot& boundary = boundaries_[handle.index];
		return boundary.occupied && boundary.generation == handle.generation
			? &boundary
			: nullptr;
	}

}
