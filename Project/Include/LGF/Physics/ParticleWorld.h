#pragma once

#include "LGF/Collision/Shapes3D.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

namespace LGF::Physics {

	/// <summary>登録された粒子を識別するハンドル</summary>
	struct ParticleHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const ParticleHandle&) const = default;
	};

	/// <summary>反発と摩擦の係数</summary>
	struct CollisionParameter {
		float restitution = 0.1f;
		float friction = 0.4f;
	};

	/// <summary>粒子の登録パラメータ</summary>
	struct ParticleDesc {
		Vector3 position{};
		Vector3 velocity{};
		float radius = 0.05f;
		float mass = 1.0f;
		CollisionParameter collisionParameter{};
		bool enabled = true;
		uint64_t userData{};
	};

	/// <summary>登録されている粒子の状態</summary>
	struct ParticleState {
		ParticleHandle handle{};
		Vector3 position{};
		Vector3 velocity{};
		float radius{};
		float mass{};
		CollisionParameter collisionParameter{};
		bool enabled{};
		uint64_t userData{};
	};

	/// <summary>物理ワールドのシミュレーション設定</summary>
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

	/// <summary>
	/// 直前のStepで計測した処理時間と衝突候補数
	/// 時間の単位はミリ秒、候補数は全サブステップの合計
	/// </summary>
	struct ParticleWorldStepStatistics {
		double totalMilliseconds{};
		double integrationMilliseconds{};
		double broadPhaseMilliseconds{};
		double positionSolveMilliseconds{};
		double velocitySolveMilliseconds{};
		double cleanupMilliseconds{};
		uint64_t candidatePairCount{};
	};

	using StaticColliderShape = std::variant<
		Collision::Sphere,
		Collision::SphericalCap,
		Collision::SphericalShellCap,
		Collision::AABB>;

	/// <summary>登録された静的コライダーを識別するハンドル</summary>
	struct StaticColliderHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const StaticColliderHandle&) const = default;
	};

	/// <summary>静的コライダーの登録パラメータ</summary>
	struct StaticColliderOptions {
		CollisionParameter collisionParameter{};
		bool enabled = true;
		uint64_t userData{};
	};

	/// <summary>
	/// 球形粒子の移動と衝突解決を管理する物理ワールド
	/// </summary>
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
		/// <summary>衝突解決を行い、指定時間だけシミュレーションを進める</summary>
		void Step(float deltaTime);

	private:
		struct ParticlePair {
			uint32_t firstIndex{};
			uint32_t secondIndex{};
		};

		struct ContactAccumulator {
			Vector3 normal{};
			Vector3 surfaceVelocity{};
			float restitution{};
			float friction{};
			uint32_t contactCount{};
			bool hasCollision{};
		};

		struct ParticleSlot {
			Vector3 position{};
			Vector3 previousPosition{};
			Vector3 velocity{};
			Vector3 force{};
			float inverseMass = 1.0f;
			float radius = 0.05f;
			CollisionParameter collisionParameter{};
			uint64_t userData{};
			uint32_t generation = 1u;
			bool isEnabled = true;
			bool isOccupied{};
		};

		struct ColliderSlot {
			StaticColliderShape shape{};
			StaticColliderShape previousShape{};
			StaticColliderOptions options{};
			uint32_t generation = 1u;
			bool isOccupied{};
		};

		ParticleSlot* FindParticle(ParticleHandle handle);
		const ParticleSlot* FindParticle(ParticleHandle handle) const;
		ColliderSlot* FindCollider(StaticColliderHandle handle);
		const ColliderSlot* FindCollider(StaticColliderHandle handle) const;
		void UpdateColliderHistory();
		void IntegrateParticles(float substepTime);
		std::vector<ParticlePair> BuildParticlePairs() const;
		void SolvePositions(
			const std::vector<ParticlePair>& particlePairs,
			const std::vector<StaticColliderShape>& colliderShapes,
			const std::vector<Vector3>& colliderVelocities,
			float maximumCorrectionPerIteration,
			std::vector<ContactAccumulator>& contacts,
			std::vector<Vector3>& positionCorrections);
		void SolveVelocities(
			const std::vector<ParticlePair>& particlePairs,
			const std::vector<ContactAccumulator>& contacts,
			const std::vector<Vector3>& positionCorrections,
			float substepTime,
			float damping);
		void CleanupParticles();
		void RemoveParticleAt(uint32_t index);

		ParticleWorldSettings settings_{};
		std::vector<ParticleSlot> particles_;
		std::vector<uint32_t> freeParticleIndices_;
		std::vector<ColliderSlot> colliders_;
		std::vector<uint32_t> freeColliderIndices_;
		std::size_t particleCount_{};
		std::size_t colliderCount_{};
		ParticleWorldStepStatistics lastStepStatistics_{};
	};

}
