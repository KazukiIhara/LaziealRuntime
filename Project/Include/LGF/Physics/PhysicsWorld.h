#pragma once

#include "LGF/Collision/Shapes3D.h"
#include "LGF/ObjectHandle.h"
#include "LGF/Graphics/GpuPhysicsParticle.h"
#include "LGF/Graphics/Model.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

namespace LGF::Physics {

	// 登録された静的オブジェクトを識別するハンドル
	struct StaticObjectHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const StaticObjectHandle&) const = default;
	};

	// 登録されたキネマティックオブジェクトを識別するハンドル
	struct KinematicObjectHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const KinematicObjectHandle&) const = default;
	};

	// 登録された動的オブジェクトを識別するハンドル
	struct DynamicObjectHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const DynamicObjectHandle&) const = default;
	};

	// 登録された物理粒子を識別するハンドル
	struct PhysicsParticleHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const PhysicsParticleHandle&) const = default;
	};

	// 同じ見た目でGPU描画する物理粒子を識別するハンドル
	struct PhysicsParticleRenderGroupHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const PhysicsParticleRenderGroupHandle&) const = default;
	};

	struct PhysicsParticleSphereVisual {};

	struct PhysicsParticleModelVisual {
		Model model{};
		Vector3 scale{ 1.0f, 1.0f, 1.0f };
	};

	using PhysicsParticleVisual = std::variant<
		PhysicsParticleSphereVisual,
		PhysicsParticleModelVisual>;

	// 同じPrimitiveまたはModelで描画する物理粒子グループの設定
	struct PhysicsParticleRenderGroupDesc {
		PhysicsParticleVisual visual{};
		Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	// 衝突時に使用する反発と摩擦の係数
	struct PhysicsMaterial {
		float restitution = 0.1f;
		float friction = 0.4f;
	};

	// 静的オブジェクトが持てる形状
	using StaticObjectShape = std::variant<
		Collision::Sphere,
		Collision::SphericalCap,
		Collision::SphericalShellCap,
		Collision::AABB,
		Collision::OBB>;

	// キネマティックオブジェクトが持てる形状
	using KinematicObjectShape = StaticObjectShape;

	// 動的オブジェクトが持てる形状
	using DynamicObjectShape = std::variant<Collision::Sphere>;

	// 静的オブジェクトの登録オプション
	struct StaticObjectOptions {
		PhysicsMaterial material{};
		ObjectHandle objectHandle{};
		PhysicsParticleRenderGroupHandle renderGroup{};
		bool enabled = true;
	};

	// 静的オブジェクトの登録パラメータ
	struct StaticObjectDesc {
		StaticObjectShape shape{};
		StaticObjectOptions options{};
	};

	// キネマティックオブジェクトの登録オプション
	struct KinematicObjectOptions {
		PhysicsMaterial material{};
		ObjectHandle objectHandle{};
		bool enabled = true;
	};

	// キネマティックオブジェクトの登録パラメータ
	struct KinematicObjectDesc {
		KinematicObjectShape shape{};
		KinematicObjectOptions options{};
	};

	// 動的オブジェクトの登録パラメータ
	struct DynamicObjectDesc {
		DynamicObjectShape shape{};
		Vector3 velocity{};
		float mass = 1.0f;
		PhysicsMaterial material{};
		ObjectHandle objectHandle{};
		bool enabled = true;
	};

	// 物理粒子の登録パラメータ
	struct PhysicsParticleDesc {
		Vector3 position{};
		Vector3 velocity{};
		float radius = 0.05f;
		float mass = 1.0f;
		PhysicsMaterial material{};
		ObjectHandle objectHandle{};
		bool enabled = true;
	};

	// 登録されている静的オブジェクトの状態
	struct StaticObjectState {
		StaticObjectHandle handle{};
		StaticObjectShape shape{};
		StaticObjectOptions options{};
	};

	// 登録されているキネマティックオブジェクトの状態
	struct KinematicObjectState {
		KinematicObjectHandle handle{};
		KinematicObjectShape shape{};
		Vector3 linearVelocity{};
		Vector3 angularVelocity{};
		KinematicObjectOptions options{};
	};

	// 登録されている動的オブジェクトの状態
	struct DynamicObjectState {
		DynamicObjectHandle handle{};
		DynamicObjectShape shape{};
		Vector3 velocity{};
		float mass{};
		PhysicsMaterial material{};
		ObjectHandle objectHandle{};
		bool enabled{};
	};

	// 登録されている物理粒子の状態
	struct PhysicsParticleState {
		PhysicsParticleHandle handle{};
		Vector3 position{};
		Vector3 velocity{};
		float radius{};
		float mass{};
		PhysicsMaterial material{};
		ObjectHandle objectHandle{};
		bool enabled{};
	};

	// GPUから非同期で回収した物理粒子統計
	struct PhysicsParticleStepStatistics {
		uint64_t particleCount{};
		uint64_t candidatePairCount{};
		uint64_t retiredParticleCount{};
		uint32_t latencyFrames{};
		bool isAvailable{};
	};

	// PhysicsWorldのシミュレーション設定
	struct PhysicsWorldSettings {
		Vector3 gravity{ 0.0f, -9.8f, 0.0f };
		Collision::AABB worldBounds{
			.min = { -100.0f, -100.0f, -100.0f },
			.max = { 100.0f, 100.0f, 100.0f },
		};
		uint32_t substepCount = 4u;
		uint32_t solverIterationCount = 4u;
		float linearDamping = 0.1f;
		float penetrationSlop = 0.005f;
		float positionCorrectionRate = 0.2f;
		float maximumDepenetrationVelocity = 5.0f;
		float bounceVelocityThreshold = 0.8f;
		float maximumDeltaTime = 1.0f / 15.0f;
		uint32_t maximumParticleCount = 65535u;
		uint32_t maximumColliderCount = 4096u;
		uint32_t maximumParticleCommandCount = 262144u;
		float maximumParticleRadius = 1.0f;
	};

	// 静的オブジェクト、キネマティックオブジェクト、動的オブジェクト、物理粒子を管理する物理ワールド
	class PhysicsWorld {
	private:
		struct StaticObjectSlot {
			StaticObjectShape shape{};
			StaticObjectOptions options{};
			uint32_t generation = 1u;
			bool isOccupied{};
		};

		struct KinematicObjectSlot {
			KinematicObjectShape shape{};
			KinematicObjectShape previousShape{};
			Vector3 linearVelocity{};
			Vector3 angularVelocity{};
			KinematicObjectOptions options{};
			uint32_t generation = 1u;
			bool isOccupied{};
		};

		struct DynamicObjectSlot {
			DynamicObjectShape shape{};
			Vector3 velocity{};
			Vector3 force{};
			float inverseMass = 1.0f;
			PhysicsMaterial material{};
			ObjectHandle objectHandle{};
			uint32_t generation = 1u;
			bool isEnabled = true;
			bool isOccupied{};
		};

		struct PhysicsParticleSlot {
			ObjectHandle objectHandle{};
			uint32_t generation = 1u;
			bool isOccupied{};
		};

		struct PhysicsParticleRenderGroupSlot {
			PhysicsParticleRenderGroupDesc desc{};
			uint32_t generation = 1u;
			bool isOccupied{};
		};

		struct CollisionSurface {
			StaticObjectShape shape{};
			Vector3 referencePosition{};
			Vector3 linearVelocity{};
			Vector3 angularVelocity{};
			PhysicsMaterial material{};
		};

	public:
		explicit PhysicsWorld(const PhysicsWorldSettings& settings = {});
		~PhysicsWorld();
		PhysicsWorld(const PhysicsWorld&) = delete;
		PhysicsWorld& operator=(const PhysicsWorld&) = delete;
		PhysicsWorld(PhysicsWorld&&) = delete;
		PhysicsWorld& operator=(PhysicsWorld&&) = delete;

		StaticObjectHandle AddStaticObject(const StaticObjectDesc& desc);
		StaticObjectHandle AddStaticObject(const StaticObjectShape& shape, const StaticObjectOptions& options = {});
		StaticObjectHandle AddStaticObject(const Collision::Sphere& sphere, const StaticObjectOptions& options = {});
		StaticObjectHandle AddStaticObject(const Collision::SphericalCap& sphericalCap, const StaticObjectOptions& options = {});
		StaticObjectHandle AddStaticObject(const Collision::SphericalShellCap& sphericalShellCap, const StaticObjectOptions& options = {});
		StaticObjectHandle AddStaticObject(const Collision::AABB& box, const StaticObjectOptions& options = {});
		StaticObjectHandle AddStaticObject(const Collision::OBB& box, const StaticObjectOptions& options = {});
		bool RemoveStaticObject(StaticObjectHandle handle);
		void ClearStaticObjects();
		bool IsValid(StaticObjectHandle handle) const;
		std::size_t GetStaticObjectCount() const;
		bool SetStaticObjectShape(StaticObjectHandle handle, const StaticObjectShape& shape);
		bool SetStaticObjectOptions(StaticObjectHandle handle, const StaticObjectOptions& options);
		bool SetObjectHandle(StaticObjectHandle handle, ObjectHandle objectHandle);
		std::optional<StaticObjectState> GetState(StaticObjectHandle handle) const;
		void GetStaticObjectStates(std::vector<StaticObjectState>& states) const;

		KinematicObjectHandle AddKinematicObject(const KinematicObjectDesc& desc);
		KinematicObjectHandle AddKinematicObject(const KinematicObjectShape& shape, const KinematicObjectOptions& options = {});
		KinematicObjectHandle AddKinematicObject(const Collision::Sphere& sphere, const KinematicObjectOptions& options = {});
		KinematicObjectHandle AddKinematicObject(const Collision::SphericalCap& sphericalCap, const KinematicObjectOptions& options = {});
		KinematicObjectHandle AddKinematicObject(const Collision::SphericalShellCap& sphericalShellCap, const KinematicObjectOptions& options = {});
		KinematicObjectHandle AddKinematicObject(const Collision::AABB& box, const KinematicObjectOptions& options = {});
		KinematicObjectHandle AddKinematicObject(const Collision::OBB& box, const KinematicObjectOptions& options = {});
		bool RemoveKinematicObject(KinematicObjectHandle handle);
		void ClearKinematicObjects();
		bool IsValid(KinematicObjectHandle handle) const;
		std::size_t GetKinematicObjectCount() const;
		bool SetKinematicObjectShape(KinematicObjectHandle handle, const KinematicObjectShape& shape);
		bool SetKinematicObjectOptions(KinematicObjectHandle handle, const KinematicObjectOptions& options);
		bool SetObjectHandle(KinematicObjectHandle handle, ObjectHandle objectHandle);
		std::optional<KinematicObjectState> GetState(KinematicObjectHandle handle) const;
		void GetKinematicObjectStates(std::vector<KinematicObjectState>& states) const;

		DynamicObjectHandle AddDynamicObject(const DynamicObjectDesc& desc);
		DynamicObjectHandle AddDynamicObject(const Collision::Sphere& sphere, const Vector3& velocity = {}, bool enabled = true, ObjectHandle objectHandle = {});
		bool SetVelocity(DynamicObjectHandle handle, const Vector3& velocity);
		bool SetMass(DynamicObjectHandle handle, float mass);
		bool AddForce(DynamicObjectHandle handle, const Vector3& force);
		bool AddImpulse(DynamicObjectHandle handle, const Vector3& impulse);
		bool SetObjectHandle(DynamicObjectHandle handle, ObjectHandle objectHandle);
		bool RemoveDynamicObject(DynamicObjectHandle handle);
		void ClearDynamicObjects();
		bool IsValid(DynamicObjectHandle handle) const;
		std::size_t GetDynamicObjectCount() const;
		std::optional<DynamicObjectState> GetState(DynamicObjectHandle handle) const;
		void GetDynamicObjectStates(std::vector<DynamicObjectState>& states) const;

		PhysicsParticleHandle AddParticle(const PhysicsParticleDesc& desc);
		bool RemoveParticle(PhysicsParticleHandle handle);
		void ClearParticles();
		bool IsValid(PhysicsParticleHandle handle) const;
		std::size_t GetParticleCount() const;
		PhysicsParticleRenderGroupHandle CreateParticleRenderGroup(
			const PhysicsParticleRenderGroupDesc& desc = {});
		bool RemoveParticleRenderGroup(PhysicsParticleRenderGroupHandle handle);
		bool SetParticleRenderGroupDesc(
			PhysicsParticleRenderGroupHandle handle,
			const PhysicsParticleRenderGroupDesc& desc);
		bool DrawParticleRenderGroups() const;

		// 粒子への変更命令はキューへ追加され、次のUpdate開始時に登録順で反映される
		bool SetParticlePosition(PhysicsParticleHandle handle, const Vector3& position, bool clearVelocity = true);
		bool SetParticleVelocity(PhysicsParticleHandle handle, const Vector3& velocity);
		bool SetParticleMass(PhysicsParticleHandle handle, float mass);
		bool SetParticleRadius(PhysicsParticleHandle handle, float radius);
		bool SetParticleMaterial(PhysicsParticleHandle handle, const PhysicsMaterial& material);
		bool SetObjectHandle(PhysicsParticleHandle handle, ObjectHandle objectHandle);
		bool SetParticleEnabled(PhysicsParticleHandle handle, bool enabled);
		bool SetParticleRenderGroup(
			PhysicsParticleHandle handle,
			PhysicsParticleRenderGroupHandle renderGroup);
		bool AddParticleForce(PhysicsParticleHandle handle, const Vector3& force);
		bool AddParticleImpulse(PhysicsParticleHandle handle, const Vector3& impulse);
		bool RequestParticleStates();
		bool GetLatestParticleStates(
			std::vector<PhysicsParticleState>& states,
			uint32_t& latencyFrames) const;
		std::optional<PhysicsParticleState> GetLatestParticleState(
			PhysicsParticleHandle handle,
			uint32_t& latencyFrames) const;
		const PhysicsParticleStepStatistics& GetLastParticleStepStatistics() const;

		void SetSettings(const PhysicsWorldSettings& settings);
		const PhysicsWorldSettings& GetSettings() const;

		void Clear();
		void Update(float deltaTime);

	private:
		StaticObjectSlot* FindStaticObject(StaticObjectHandle handle);
		const StaticObjectSlot* FindStaticObject(StaticObjectHandle handle) const;
		void RemoveStaticObjectAt(uint32_t index);

		KinematicObjectSlot* FindKinematicObject(KinematicObjectHandle handle);
		const KinematicObjectSlot* FindKinematicObject(KinematicObjectHandle handle) const;
		void RemoveKinematicObjectAt(uint32_t index);
		void UpdateKinematicObjectVelocities(float frameTime);
		void UpdateKinematicObjectHistory();
		std::vector<CollisionSurface> BuildCollisionSurfaces(float interpolation) const;

		DynamicObjectSlot* FindDynamicObject(DynamicObjectHandle handle);
		const DynamicObjectSlot* FindDynamicObject(DynamicObjectHandle handle) const;
		void RemoveDynamicObjectAt(uint32_t index);
		void IntegrateDynamicObjects(float substepTime);
		void SolveObjectContacts(const std::vector<CollisionSurface>& collisionSurfaces);
		void SolveDynamicObjectContacts();
		void CleanupDynamicObjects();

		PhysicsParticleSlot* FindParticle(PhysicsParticleHandle handle);
		const PhysicsParticleSlot* FindParticle(PhysicsParticleHandle handle) const;
		void RemoveParticleAt(uint32_t index);
		bool QueueParticleCommand(const GpuPhysicsParticleCommand& command);
		void ProcessRetiredParticles();
		PhysicsParticleRenderGroupSlot* FindParticleRenderGroup(
			PhysicsParticleRenderGroupHandle handle);
		const PhysicsParticleRenderGroupSlot* FindParticleRenderGroup(
			PhysicsParticleRenderGroupHandle handle) const;
		GpuPhysicsCollider ConvertCollisionSurface(const CollisionSurface& surface) const;
		void UpdateParticles(float deltaTime);

	private:
		PhysicsWorldSettings settings_{};

		std::vector<StaticObjectSlot> staticObjects_;
		std::vector<uint32_t> freeStaticObjectIndices_;
		std::size_t staticObjectCount_{};

		std::vector<KinematicObjectSlot> kinematicObjects_;
		std::vector<uint32_t> freeKinematicObjectIndices_;
		std::size_t kinematicObjectCount_{};

		std::vector<DynamicObjectSlot> dynamicObjects_;
		std::vector<uint32_t> freeDynamicObjectIndices_;
		std::size_t dynamicObjectCount_{};

		std::vector<PhysicsParticleSlot> particles_;
		std::vector<uint32_t> freeParticleIndices_;
		std::vector<GpuPhysicsParticleCommand> particleCommands_;
		std::vector<PhysicsParticleRenderGroupSlot> particleRenderGroups_;
		GpuPhysicsParticleWorldHandle gpuParticleWorldHandle_{};
		std::size_t particleCount_{};
		PhysicsParticleStepStatistics lastParticleStepStatistics_{};
	};

}
