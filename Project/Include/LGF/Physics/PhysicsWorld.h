#pragma once

#include "LGF/Collision/Shapes3D.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

namespace LGF::Physics {

	/// <summary>登録された静的オブジェクトを識別するハンドル</summary>
	struct StaticObjectHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const StaticObjectHandle&) const = default;
	};

	/// <summary>登録された動的オブジェクトを識別するハンドル</summary>
	struct DynamicObjectHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const DynamicObjectHandle&) const = default;
	};

	/// <summary>
	/// 静的オブジェクトが持てる形状。
	/// 初期実装ではOBBのみを扱い、形状を増やす際はvariantへ型を追加する。
	/// </summary>
	using StaticObjectShape = std::variant<Collision::OBB>;

	/// <summary>
	/// 動的オブジェクトが持てる形状。
	/// 初期実装ではSphereのみを扱い、形状を増やす際はvariantへ型を追加する。
	/// </summary>
	using DynamicObjectShape = std::variant<Collision::Sphere>;

	/// <summary>静的オブジェクトの登録パラメータ</summary>
	struct StaticObjectDesc {
		StaticObjectShape shape{};
		bool enabled = true;
		uint64_t userData{};
	};

	/// <summary>動的オブジェクトの登録パラメータ</summary>
	struct DynamicObjectDesc {
		DynamicObjectShape shape{};
		Vector3 velocity{};
		bool enabled = true;
		uint64_t userData{};
	};

	/// <summary>登録されている静的オブジェクトの状態</summary>
	struct StaticObjectState {
		StaticObjectHandle handle{};
		StaticObjectShape shape{};
		bool enabled{};
		uint64_t userData{};
	};

	/// <summary>登録されている動的オブジェクトの状態</summary>
	struct DynamicObjectState {
		DynamicObjectHandle handle{};
		DynamicObjectShape shape{};
		Vector3 velocity{};
		bool enabled{};
		uint64_t userData{};
	};

	/// <summary>PhysicsWorldのシミュレーション設定</summary>
	struct PhysicsWorldSettings {
		Vector3 gravity{ 0.0f, -9.8f, 0.0f };
		Collision::AABB worldBounds{
			.min = { -100.0f, -100.0f, -100.0f },
			.max = { 100.0f, 100.0f, 100.0f },
		};
		uint32_t substepCount = 4u;
		uint32_t solverIterationCount = 4u;
		float maximumDeltaTime = 1.0f / 15.0f;
	};

	/// <summary>
	/// 静的オブジェクトと動的オブジェクトを管理する物理ワールド。
	/// 初期実装では、静的OBBと動的Sphereの重力落下とめり込み解消を扱う。
	/// </summary>
	class PhysicsWorld {
	public:
		explicit PhysicsWorld(const PhysicsWorldSettings& settings = {});

		StaticObjectHandle AddStaticObject(const StaticObjectDesc& desc);
		StaticObjectHandle AddStaticObject(
			const Collision::OBB& box,
			bool enabled = true,
			uint64_t userData = 0u);
		bool RemoveStaticObject(StaticObjectHandle handle);
		void ClearStaticObjects();

		DynamicObjectHandle AddDynamicObject(const DynamicObjectDesc& desc);
		DynamicObjectHandle AddDynamicObject(
			const Collision::Sphere& sphere,
			const Vector3& velocity = {},
			bool enabled = true,
			uint64_t userData = 0u);
		bool RemoveDynamicObject(DynamicObjectHandle handle);
		void ClearDynamicObjects();

		bool IsValid(StaticObjectHandle handle) const;
		bool IsValid(DynamicObjectHandle handle) const;
		std::size_t GetStaticObjectCount() const;
		std::size_t GetDynamicObjectCount() const;

		std::optional<StaticObjectState> GetState(StaticObjectHandle handle) const;
		std::optional<DynamicObjectState> GetState(DynamicObjectHandle handle) const;
		void GetStaticObjectStates(std::vector<StaticObjectState>& states) const;
		void GetDynamicObjectStates(std::vector<DynamicObjectState>& states) const;

		void SetSettings(const PhysicsWorldSettings& settings);
		const PhysicsWorldSettings& GetSettings() const;

		void Clear();
		/// <summary>指定時間だけ動的オブジェクトのシミュレーションを進める</summary>
		void Update(float deltaTime);

	private:
		struct StaticObjectSlot {
			StaticObjectShape shape{};
			uint64_t userData{};
			uint32_t generation = 1u;
			bool isEnabled = true;
			bool isOccupied{};
		};

		struct DynamicObjectSlot {
			DynamicObjectShape shape{};
			Vector3 velocity{};
			uint64_t userData{};
			uint32_t generation = 1u;
			bool isEnabled = true;
			bool isOccupied{};
		};


		StaticObjectSlot* FindStaticObject(StaticObjectHandle handle);
		const StaticObjectSlot* FindStaticObject(StaticObjectHandle handle) const;
		DynamicObjectSlot* FindDynamicObject(DynamicObjectHandle handle);
		const DynamicObjectSlot* FindDynamicObject(DynamicObjectHandle handle) const;

		void IntegrateDynamicObjects(float substepTime);
		void SolveStaticContacts();
		void CleanupDynamicObjects();
		void RemoveStaticObjectAt(uint32_t index);
		void RemoveDynamicObjectAt(uint32_t index);

		PhysicsWorldSettings settings_{};
		std::vector<StaticObjectSlot> staticObjects_;
		std::vector<uint32_t> freeStaticObjectIndices_;
		std::vector<DynamicObjectSlot> dynamicObjects_;
		std::vector<uint32_t> freeDynamicObjectIndices_;
		std::size_t staticObjectCount_{};
		std::size_t dynamicObjectCount_{};
	};

}
