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
		uint32_t substepCount = 2u;
		uint32_t solverIterationCount = 4u;
		float linearDamping = 0.1f;
		float maximumDeltaTime = 1.0f / 15.0f;
	};

	using ParticleBoundaryShape = std::variant<
		Collision::Sphere,
		Collision::AABB,
		Collision::Cylinder>;

	struct ParticleBoundaryHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation = 0u;

		bool IsValid() const;
		bool operator==(const ParticleBoundaryHandle&) const = default;
	};

	struct ParticleBoundaryOptions {
		CollisionParameter collisionParameter{};
		/// <summary>
		/// プリミティブを切断する平面。
		/// 法線が向いている側の表面を削除し、切断面は塞がない。
		/// </summary>
		std::optional<Collision::Plane> cuttingPlane;
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

		ParticleBoundaryHandle AddBoundary(
			const ParticleBoundaryShape& shape,
			const ParticleBoundaryOptions& options = {});
		ParticleBoundaryHandle AddBoundary(
			const Collision::Sphere& sphere,
			const ParticleBoundaryOptions& options = {});
		ParticleBoundaryHandle AddBoundary(
			const Collision::AABB& box,
			const ParticleBoundaryOptions& options = {});
		ParticleBoundaryHandle AddBoundary(
			const Collision::Cylinder& cylinder,
			const ParticleBoundaryOptions& options = {});
		bool RemoveBoundary(ParticleBoundaryHandle handle);
		void ClearBoundaries();

		bool IsValid(ParticleBoundaryHandle handle) const;
		std::size_t GetBoundaryCount() const;
		bool SetBoundaryShape(
			ParticleBoundaryHandle handle,
			const ParticleBoundaryShape& shape);
		bool SetBoundaryOptions(
			ParticleBoundaryHandle handle,
			const ParticleBoundaryOptions& options);
		const ParticleBoundaryShape* GetBoundaryShape(
			ParticleBoundaryHandle handle) const;
		std::optional<ParticleBoundaryOptions> GetBoundaryOptions(
			ParticleBoundaryHandle handle) const;

		void SetSettings(const ParticleWorldSettings& settings);
		const ParticleWorldSettings& GetSettings() const;

		void Clear();
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

		struct BoundarySlot {
			ParticleBoundaryShape shape{};
			ParticleBoundaryOptions options{};
			uint32_t generation = 1u;
			bool occupied = false;
		};

		ParticleSlot* FindParticle(ParticleHandle handle);
		const ParticleSlot* FindParticle(ParticleHandle handle) const;
		BoundarySlot* FindBoundary(ParticleBoundaryHandle handle);
		const BoundarySlot* FindBoundary(ParticleBoundaryHandle handle) const;

		ParticleWorldSettings settings_{};
		std::vector<ParticleSlot> particles_;
		std::vector<uint32_t> freeParticleIndices_;
		std::vector<BoundarySlot> boundaries_;
		std::vector<uint32_t> freeBoundaryIndices_;
		std::size_t particleCount_ = 0u;
		std::size_t boundaryCount_ = 0u;
	};

}
