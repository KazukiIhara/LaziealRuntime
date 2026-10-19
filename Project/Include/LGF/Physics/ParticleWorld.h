#pragma once

#include "LGF/Collision/Shapes3D.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

namespace LGF::Physics {

	struct ParticleHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation = 0u;

		bool IsValid() const;
		bool operator==(const ParticleHandle&) const = default;
	};

	struct CollisionParameter {
		float restitution = 0.1f;
		float friction = 0.4f;
	};

	struct ParticleDesc {
		Vector3 position{};
		Vector3 velocity{};
		float radius = 0.05f;
		float mass = 1.0f;
		CollisionParameter collisionParameter{};
		bool enabled = true;
		uint64_t userData = 0u;
	};

	struct ParticleState {
		ParticleHandle handle{};
		Vector3 position{};
		Vector3 velocity{};
		float radius = 0.0f;
		float mass = 0.0f;
		CollisionParameter collisionParameter{};
		bool enabled = false;
		uint64_t userData = 0u;
	};

	struct ParticleWorldSettings {
		Vector3 gravity{ 0.0f, -9.8f, 0.0f };
		Collision::AABB worldBounds{
			.min = { -100.0f, -100.0f, -100.0f },
			.max = { 100.0f, 100.0f, 100.0f },
		};
		uint32_t substepCount = 2u;
		uint32_t solverIterationCount = 4u;
		float linearDamping = 0.1f;
		float penetrationSlop = 0.005f;
		float positionCorrectionRate = 0.2f;
		float maximumDepenetrationVelocity = 5.0f;
		float bounceVelocityThreshold = 0.8f;
		float maximumDeltaTime = 1.0f / 15.0f;
	};

	// 直前のStepで計測した処理時間と衝突候補数。
	// 時間の単位はすべてミリ秒で、候補数は全サブステップの合計を表す。
	struct ParticleWorldStepStatistics {
		double totalMilliseconds = 0.0;
		double integrationMilliseconds = 0.0;
		double broadPhaseMilliseconds = 0.0;
		double positionSolveMilliseconds = 0.0;
		double velocitySolveMilliseconds = 0.0;
		double cleanupMilliseconds = 0.0;
		uint64_t candidatePairCount = 0u;
	};

	using StaticColliderShape = std::variant<
		Collision::Sphere,
		Collision::SphericalCap,
		Collision::SphericalShellCap,
		Collision::AABB>;

	struct StaticColliderHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation = 0u;

		bool IsValid() const;
		bool operator==(const StaticColliderHandle&) const = default;
	};

	struct StaticColliderOptions {
		CollisionParameter collisionParameter{};
		bool enabled = true;
		uint64_t userData = 0u;
	};

	class ParticleWorld {
	public:
		explicit ParticleWorld(const ParticleWorldSettings& settings = {});

		ParticleHandle Add(const ParticleDesc& desc);
		bool Remove(ParticleHandle handle);
		void ClearParticles();

		bool IsValid(ParticleHandle handle) const;
		std::size_t GetParticleCount() const;

		bool SetPosition(
			ParticleHandle handle,
			const Vector3& position,
			bool clearVelocity = true);
		bool SetVelocity(ParticleHandle handle, const Vector3& velocity);
		bool SetMass(ParticleHandle handle, float mass);
		bool SetRadius(ParticleHandle handle, float radius);
		bool SetCollisionParameter(
			ParticleHandle handle,
			const CollisionParameter& collisionParameter);
		bool SetEnabled(ParticleHandle handle, bool enabled);
		bool SetUserData(ParticleHandle handle, uint64_t userData);
		bool AddForce(ParticleHandle handle, const Vector3& force);
		bool AddImpulse(ParticleHandle handle, const Vector3& impulse);

		std::optional<ParticleState> GetState(ParticleHandle handle) const;
		void GetStates(std::vector<ParticleState>& states) const;

		StaticColliderHandle AddCollider(
			const StaticColliderShape& shape,
			const StaticColliderOptions& options = {});
		StaticColliderHandle AddCollider(
			const Collision::Sphere& sphere,
			const StaticColliderOptions& options = {});
		StaticColliderHandle AddCollider(
			const Collision::SphericalCap& sphericalCap,
			const StaticColliderOptions& options = {});
		StaticColliderHandle AddCollider(
			const Collision::SphericalShellCap& sphericalShellCap,
			const StaticColliderOptions& options = {});
		StaticColliderHandle AddCollider(
			const Collision::AABB& box,
			const StaticColliderOptions& options = {});
		bool RemoveCollider(StaticColliderHandle handle);
		void ClearColliders();

		bool IsValid(StaticColliderHandle handle) const;
		std::size_t GetColliderCount() const;
		bool SetColliderShape(
			StaticColliderHandle handle,
			const StaticColliderShape& shape);
		bool SetColliderOptions(
			StaticColliderHandle handle,
			const StaticColliderOptions& options);
		const StaticColliderShape* GetColliderShape(StaticColliderHandle handle) const;
		std::optional<StaticColliderOptions> GetColliderOptions(
			StaticColliderHandle handle) const;

		void SetSettings(const ParticleWorldSettings& settings);
		const ParticleWorldSettings& GetSettings() const;
		const ParticleWorldStepStatistics& GetLastStepStatistics() const;

		void Clear();
		// 指定時間だけシミュレーションを進め、衝突解決と範囲外粒子の削除を行う。
		void Step(float deltaTime);

	private:
		struct ParticleSlot {
			Vector3 position{};
			Vector3 previousPosition{};
			Vector3 velocity{};
			Vector3 force{};
			float inverseMass = 1.0f;
			float radius = 0.05f;
			CollisionParameter collisionParameter{};
			uint64_t userData = 0u;
			uint32_t generation = 1u;
			bool enabled = true;
			bool occupied = false;
		};

		struct ColliderSlot {
			StaticColliderShape shape{};
			StaticColliderShape previousShape{};
			StaticColliderOptions options{};
			uint32_t generation = 1u;
			bool occupied = false;
		};

		ParticleSlot* FindParticle(ParticleHandle handle);
		const ParticleSlot* FindParticle(ParticleHandle handle) const;
		ColliderSlot* FindCollider(StaticColliderHandle handle);
		const ColliderSlot* FindCollider(StaticColliderHandle handle) const;
		void RemoveParticleAt(uint32_t index);

		ParticleWorldSettings settings_{};
		std::vector<ParticleSlot> particles_;
		std::vector<uint32_t> freeParticleIndices_;
		std::vector<ColliderSlot> colliders_;
		std::vector<uint32_t> freeColliderIndices_;
		std::size_t particleCount_ = 0u;
		std::size_t colliderCount_ = 0u;
		ParticleWorldStepStatistics lastStepStatistics_{};
	};

}
