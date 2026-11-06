#include "LGF/Physics/PhysicsWorld.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

#include "../Core/HandleUtility.h"
#include "LGF/Math/Functions/VectorMath.h"
#include "PhysicsWorldUtility.h"

namespace {

	using namespace LGF;

	bool IsPhysicsParticleDescValid(const LGF::Physics::PhysicsParticleDesc& desc) {
		return LGF::Physics::Internal::IsFinite(desc.position) &&
			LGF::Physics::Internal::IsFinite(desc.velocity) &&
			std::isfinite(desc.radius) &&
			desc.radius > 0.0f &&
			std::isfinite(desc.mass) &&
			desc.mass >= 0.0f;
	}

	struct GridCell {
		int32_t x = 0;
		int32_t y = 0;
		int32_t z = 0;

		bool operator==(const GridCell&) const = default;
	};

	struct GridCellHash {
		std::size_t operator()(const GridCell& cell) const {
			std::size_t hash = std::hash<int32_t>{}(cell.x);
			hash ^= std::hash<int32_t>{}(cell.y) +
				0x9e3779b9u + (hash << 6u) + (hash >> 2u);
			hash ^= std::hash<int32_t>{}(cell.z) +
				0x9e3779b9u + (hash << 6u) + (hash >> 2u);
			return hash;
		}
	};

	GridCell ToGridCell(const Vector3& position, float cellSize) {
		return {
			static_cast<int32_t>(std::floor(position.x / cellSize)),
			static_cast<int32_t>(std::floor(position.y / cellSize)),
			static_cast<int32_t>(std::floor(position.z / cellSize)),
		};
	}

}

namespace LGF::Physics {

	bool PhysicsParticleHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	PhysicsParticleHandle PhysicsWorld::AddParticle(const PhysicsParticleDesc& desc) {
		if (!IsPhysicsParticleDescValid(desc)) {
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

		PhysicsParticleSlot& particle = particles_[index];
		const uint32_t generation = particle.generation;
		particle = PhysicsParticleSlot{
			.position = desc.position,
			.previousPosition = desc.position,
			.velocity = desc.velocity,
			.force = {},
			.inverseMass = desc.mass > 0.0f ? 1.0f / desc.mass : 0.0f,
			.radius = desc.radius,
			.material = Internal::NormalizeMaterial(desc.material),
			.objectHandle = desc.objectHandle,
			.generation = generation,
			.isEnabled = desc.enabled,
			.isOccupied = true,
		};
		++particleCount_;
		return { index, generation };
	}

	bool PhysicsWorld::RemoveParticle(PhysicsParticleHandle handle) {
		if (!FindParticle(handle)) {
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
	}

	bool PhysicsWorld::IsValid(PhysicsParticleHandle handle) const {
		return FindParticle(handle) != nullptr;
	}

	std::size_t PhysicsWorld::GetParticleCount() const {
		return particleCount_;
	}

	bool PhysicsWorld::SetParticlePosition(PhysicsParticleHandle handle, const Vector3& position, bool clearVelocity) {
		if (!Internal::IsFinite(position)) {
			return false;
		}
		return QueueParticleCommand({
			.type = PhysicsParticleCommandType::SetPosition,
			.handle = handle,
			.vectorValue = position,
			.boolValue = clearVelocity,
		});
	}

	bool PhysicsWorld::SetParticleVelocity(PhysicsParticleHandle handle, const Vector3& velocity) {
		if (!Internal::IsFinite(velocity)) {
			return false;
		}
		return QueueParticleCommand({
			.type = PhysicsParticleCommandType::SetVelocity,
			.handle = handle,
			.vectorValue = velocity,
		});
	}

	bool PhysicsWorld::SetParticleMass(PhysicsParticleHandle handle, float mass) {
		if (!std::isfinite(mass) || mass < 0.0f) {
			return false;
		}
		return QueueParticleCommand({
			.type = PhysicsParticleCommandType::SetMass,
			.handle = handle,
			.scalarValue = mass,
		});
	}

	bool PhysicsWorld::SetParticleRadius(PhysicsParticleHandle handle, float radius) {
		if (!std::isfinite(radius) || radius <= 0.0f) {
			return false;
		}
		return QueueParticleCommand({
			.type = PhysicsParticleCommandType::SetRadius,
			.handle = handle,
			.scalarValue = radius,
		});
	}

	bool PhysicsWorld::SetParticleMaterial(PhysicsParticleHandle handle, const PhysicsMaterial& material) {
		return QueueParticleCommand({
			.type = PhysicsParticleCommandType::SetMaterial,
			.handle = handle,
			.materialValue = Internal::NormalizeMaterial(material),
		});
	}

	bool PhysicsWorld::SetObjectHandle(PhysicsParticleHandle handle, ObjectHandle objectHandle) {
		return QueueParticleCommand({
			.type = PhysicsParticleCommandType::SetObjectHandle,
			.handle = handle,
			.objectHandleValue = objectHandle,
		});
	}

	bool PhysicsWorld::SetParticleEnabled(PhysicsParticleHandle handle, bool enabled) {
		return QueueParticleCommand({
			.type = PhysicsParticleCommandType::SetEnabled,
			.handle = handle,
			.boolValue = enabled,
		});
	}

	bool PhysicsWorld::AddParticleForce(PhysicsParticleHandle handle, const Vector3& force) {
		if (!Internal::IsFinite(force)) {
			return false;
		}
		return QueueParticleCommand({
			.type = PhysicsParticleCommandType::AddForce,
			.handle = handle,
			.vectorValue = force,
		});
	}

	bool PhysicsWorld::AddParticleImpulse(PhysicsParticleHandle handle, const Vector3& impulse) {
		if (!Internal::IsFinite(impulse)) {
			return false;
		}
		return QueueParticleCommand({
			.type = PhysicsParticleCommandType::AddImpulse,
			.handle = handle,
			.vectorValue = impulse,
		});
	}

	std::optional<PhysicsParticleState> PhysicsWorld::GetParticleState(PhysicsParticleHandle handle) const {
		const PhysicsParticleSlot* particle = FindParticle(handle);
		if (!particle) {
			return std::nullopt;
		}
		return PhysicsParticleState{
			.handle = handle,
			.position = particle->position,
			.velocity = particle->velocity,
			.radius = particle->radius,
			.mass = particle->inverseMass > 0.0f ? 1.0f / particle->inverseMass : 0.0f,
			.material = particle->material,
			.objectHandle = particle->objectHandle,
			.enabled = particle->isEnabled,
		};
	}

	void PhysicsWorld::GetParticleStates(std::vector<PhysicsParticleState>& states) const {
		states.clear();
		states.reserve(particleCount_);
		for (uint32_t index = 0u; index < particles_.size(); ++index) {
			const PhysicsParticleSlot& particle = particles_[index];
			if (!particle.isOccupied) {
				continue;
			}
			states.push_back(PhysicsParticleState{
				.handle = { index, particle.generation },
				.position = particle.position,
				.velocity = particle.velocity,
				.radius = particle.radius,
				.mass = particle.inverseMass > 0.0f ? 1.0f / particle.inverseMass : 0.0f,
				.material = particle.material,
				.objectHandle = particle.objectHandle,
				.enabled = particle.isEnabled,
				});
		}
	}

	const PhysicsParticleStepStatistics& PhysicsWorld::GetLastParticleStepStatistics() const {
		return lastParticleStepStatistics_;
	}

	bool PhysicsWorld::QueueParticleCommand(const PhysicsParticleCommand& command) {
		if (!FindParticle(command.handle)) {
			return false;
		}
		particleCommands_.push_back(command);
		return true;
	}

	void PhysicsWorld::ApplyParticleCommands() {
		for (const PhysicsParticleCommand& command : particleCommands_) {
			PhysicsParticleSlot* particle = FindParticle(command.handle);
			if (!particle) {
				continue;
			}

			switch (command.type) {
			case PhysicsParticleCommandType::SetPosition:
				particle->position = command.vectorValue;
				particle->previousPosition = command.vectorValue;
				if (command.boolValue) {
					particle->velocity = {};
				}
				break;
			case PhysicsParticleCommandType::SetVelocity:
				particle->velocity = command.vectorValue;
				break;
			case PhysicsParticleCommandType::SetMass:
				particle->inverseMass = command.scalarValue > 0.0f
					? 1.0f / command.scalarValue
					: 0.0f;
				if (particle->inverseMass == 0.0f) {
					particle->velocity = {};
				}
				break;
			case PhysicsParticleCommandType::SetRadius:
				particle->radius = command.scalarValue;
				break;
			case PhysicsParticleCommandType::SetMaterial:
				particle->material = command.materialValue;
				break;
			case PhysicsParticleCommandType::SetObjectHandle:
				particle->objectHandle = command.objectHandleValue;
				break;
			case PhysicsParticleCommandType::SetEnabled:
				particle->isEnabled = command.boolValue;
				break;
			case PhysicsParticleCommandType::AddForce:
				particle->force += command.vectorValue;
				break;
			case PhysicsParticleCommandType::AddImpulse:
				if (particle->inverseMass > 0.0f) {
					particle->velocity += command.vectorValue * particle->inverseMass;
				}
				break;
			}
		}
		particleCommands_.clear();
	}

	void PhysicsWorld::IntegrateParticles(float substepTime) {
		for (PhysicsParticleSlot& particle : particles_) {
			if (!particle.isOccupied || !particle.isEnabled) {
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

	std::vector<PhysicsWorld::PhysicsParticlePair> PhysicsWorld::BuildParticlePairs() const {
		std::vector<PhysicsParticlePair> pairs;
		float maximumRadius = 0.0f;
		for (const PhysicsParticleSlot& particle : particles_) {
			if (particle.isOccupied && particle.isEnabled) {
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
			const PhysicsParticleSlot& particle = particles_[index];
			if (particle.isOccupied && particle.isEnabled) {
				grid[ToGridCell(particle.position, cellSize)].push_back(index);
			}
		}

		for (uint32_t firstIndex = 0u; firstIndex < particles_.size(); ++firstIndex) {
			const PhysicsParticleSlot& first = particles_[firstIndex];
			if (!first.isOccupied || !first.isEnabled) {
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

	void PhysicsWorld::SolveParticlePositions(
		const std::vector<PhysicsParticlePair>& particlePairs,
		const std::vector<CollisionSurface>& collisionSurfaces,
		float maximumCorrectionPerIteration,
		std::vector<PhysicsParticleContactAccumulator>& contacts,
		std::vector<Vector3>& positionCorrections) {

		const auto recordContact = [&](uint32_t particleIndex, const Vector3& normal, const Vector3& surfaceVelocity, const PhysicsMaterial& particleParameter, const PhysicsMaterial& colliderParameter) {
				PhysicsParticleContactAccumulator& contact = contacts[particleIndex];
				contact.normal += normal;
				contact.surfaceVelocity += surfaceVelocity;
				contact.restitution = std::max(contact.restitution, std::max(particleParameter.restitution, colliderParameter.restitution));
				contact.friction = std::max(contact.friction, std::sqrt(particleParameter.friction * colliderParameter.friction));
				++contact.contactCount;
				contact.hasCollision = true;
			};

		for (uint32_t iteration = 0u;
			iteration < settings_.solverIterationCount;
			++iteration) {
			for (const PhysicsParticlePair& pair : particlePairs) {
				const uint32_t firstIndex = pair.firstIndex;
				const uint32_t secondIndex = pair.secondIndex;
				PhysicsParticleSlot& first = particles_[firstIndex];
				PhysicsParticleSlot& second = particles_[secondIndex];
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
				const float effectivePenetration = std::max(minimumDistance - distance - settings_.penetrationSlop, 0.0f);
				const float correctionDepth = std::min(effectivePenetration * settings_.positionCorrectionRate, maximumCorrectionPerIteration);
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
				PhysicsParticleSlot& particle = particles_[particleIndex];
				if (!particle.isOccupied || !particle.isEnabled || particle.inverseMass <= 0.0f) {
					continue;
				}

				for (const CollisionSurface& collisionSurface : collisionSurfaces) {
					const Collision::Sphere particleSphere{
						.center = particle.position,
						.radius = particle.radius,
					};
					std::visit([&](const auto& shape) {
						const std::optional<Internal::StaticContact> contact = Internal::FindContact(particleSphere, shape);
						if (!contact || contact->penetrationDepth <= 0.0f) {
							return;
						}

						const float effectivePenetration = std::max(contact->penetrationDepth - settings_.penetrationSlop, 0.0f);
						const float correctionDepth = std::min(effectivePenetration * settings_.positionCorrectionRate, maximumCorrectionPerIteration);
						const Vector3 correction = contact->normal * correctionDepth;
						particle.position += correction;
						positionCorrections[particleIndex] += correction;
						const Vector3 surfaceVelocity = collisionSurface.linearVelocity + Math::Cross(collisionSurface.angularVelocity, contact->point - collisionSurface.referencePosition);
						recordContact(particleIndex, contact->normal, surfaceVelocity, particle.material, collisionSurface.material);
						}, collisionSurface.shape);

				}
			}
		}
	}

	void PhysicsWorld::SolveParticleVelocities(
		const std::vector<PhysicsParticlePair>& particlePairs,
		const std::vector<PhysicsParticleContactAccumulator>& contacts,
		const std::vector<Vector3>& positionCorrections,
		float substepTime,
		float damping) {

		for (uint32_t index = 0u; index < particles_.size(); ++index) {
			PhysicsParticleSlot& particle = particles_[index];
			if (!particle.isOccupied || !particle.isEnabled) {
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

			const PhysicsParticleContactAccumulator& contact = contacts[index];
			if (!contact.hasCollision) {
				continue;
			}

			const Vector3 normal = Collision::Internal::SafeNormal(contact.normal, { 0.0f, 1.0f, 0.0f });
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

		for (const PhysicsParticlePair& pair : particlePairs) {
			const uint32_t firstIndex = pair.firstIndex;
			const uint32_t secondIndex = pair.secondIndex;
			PhysicsParticleSlot& first = particles_[firstIndex];
			PhysicsParticleSlot& second = particles_[secondIndex];
			const float inverseMassSum = first.inverseMass + second.inverseMass;
			if (inverseMassSum <= 0.0f) {
				continue;
			}

			const Vector3 delta = second.position - first.position;
			const float minimumDistance = first.radius + second.radius;
			if (Math::LengthSq(delta) > minimumDistance * minimumDistance + Math::EPSILON) {
				continue;
			}

			const Vector3 normal = Collision::Internal::SafeNormal(delta, { 1.0f, 0.0f, 0.0f });
			const Vector3 relativeVelocity = second.velocity - first.velocity;
			const float normalSpeed = Math::Dot(relativeVelocity, normal);
			if (normalSpeed >= 0.0f) {
				continue;
			}

			const float restitution = -normalSpeed > settings_.bounceVelocityThreshold
				? std::max(first.material.restitution, second.material.restitution)
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

			const float friction = std::sqrt(first.material.friction * second.material.friction);
			const float tangentImpulseMagnitude = std::min(tangentSpeed / inverseMassSum, friction * normalImpulseMagnitude);
			const Vector3 tangentImpulse =
				-tangentVelocity / tangentSpeed * tangentImpulseMagnitude;
			first.velocity -= tangentImpulse * first.inverseMass;
			second.velocity += tangentImpulse * second.inverseMass;
		}
	}

	void PhysicsWorld::CleanupParticles() {
		for (uint32_t index = 0u; index < particles_.size(); ++index) {
			PhysicsParticleSlot& particle = particles_[index];
			if (!particle.isOccupied) {
				continue;
			}

			particle.force = {};
			if (!Collision::Contains(settings_.worldBounds, particle.position)) {
				RemoveParticleAt(index);
			}
		}
	}

	void PhysicsWorld::UpdateParticles(float deltaTime) {
		using StepClock = std::chrono::steady_clock;
		const auto toMilliseconds = [](StepClock::duration duration) {
			return std::chrono::duration<double, std::milli>(duration).count();
			};

		lastParticleStepStatistics_ = {};
		// 0以下や非数の経過時間では、安全のためシミュレーションを進めない。
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
			return;
		}
		ApplyParticleCommands();
		const StepClock::time_point stepStart = StepClock::now();
		if (particleCount_ == 0u) {
			lastParticleStepStatistics_.totalMilliseconds =
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
			std::vector<PhysicsParticleContactAccumulator> contacts(particles_.size());
			// めり込み解消のためだけに動かした距離を記録する。
			// 後で速度を求める際に差し引き、押し出しを運動速度へ変換させない。
			std::vector<Vector3> positionCorrections(particles_.size());
			const std::vector<CollisionSurface> collisionSurfaces = BuildCollisionSurfaces(colliderInterpolation);
			// 深いめり込みを1回で直すと位置が急変するため、
			// 1反復で許可する補正距離へ変換して上限をかける。
			const float maximumCorrectionPerIteration =
				settings_.maximumDepenetrationVelocity * substepTime /
				static_cast<float>(settings_.solverIterationCount);

			// 重力と外力から速度を更新し、その速度で衝突前の予測位置へ進める。
			// 速度を先に更新するため、積分方法は半陰的オイラー法に相当する。
			const StepClock::time_point integrationStart = StepClock::now();
			IntegrateParticles(substepTime);
			lastParticleStepStatistics_.integrationMilliseconds +=
				toMilliseconds(StepClock::now() - integrationStart);

			// 予測位置を使って候補ペアを一度だけ作り、位置補正と速度解決で共有する。
			const StepClock::time_point broadPhaseStart = StepClock::now();
			const std::vector<PhysicsParticlePair> particlePairs = BuildParticlePairs();
			lastParticleStepStatistics_.broadPhaseMilliseconds +=
				toMilliseconds(StepClock::now() - broadPhaseStart);
			lastParticleStepStatistics_.candidatePairCount += particlePairs.size();

			const StepClock::time_point positionSolveStart = StepClock::now();
			SolveParticlePositions(particlePairs, collisionSurfaces, maximumCorrectionPerIteration, contacts, positionCorrections);
			lastParticleStepStatistics_.positionSolveMilliseconds +=
				toMilliseconds(StepClock::now() - positionSolveStart);

			const StepClock::time_point velocitySolveStart = StepClock::now();
			SolveParticleVelocities(particlePairs, contacts, positionCorrections, substepTime, damping);
			lastParticleStepStatistics_.velocitySolveMilliseconds +=
				toMilliseconds(StepClock::now() - velocitySolveStart);
		}

		const StepClock::time_point cleanupStart = StepClock::now();
		CleanupParticles();
		lastParticleStepStatistics_.cleanupMilliseconds =
			toMilliseconds(StepClock::now() - cleanupStart);
		lastParticleStepStatistics_.totalMilliseconds =
			toMilliseconds(StepClock::now() - stepStart);
	}

	PhysicsWorld::PhysicsParticleSlot* PhysicsWorld::FindParticle(PhysicsParticleHandle handle) {
		if (!handle.IsValid() || handle.index >= particles_.size()) {
			return nullptr;
		}
		PhysicsParticleSlot& particle = particles_[handle.index];
		return particle.isOccupied && particle.generation == handle.generation
			? &particle
			: nullptr;
	}

	const PhysicsWorld::PhysicsParticleSlot* PhysicsWorld::FindParticle(PhysicsParticleHandle handle) const {
		if (!handle.IsValid() || handle.index >= particles_.size()) {
			return nullptr;
		}
		const PhysicsParticleSlot& particle = particles_[handle.index];
		return particle.isOccupied && particle.generation == handle.generation
			? &particle
			: nullptr;
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


