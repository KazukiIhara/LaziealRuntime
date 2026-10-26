#pragma once

#include "LGF/Collision/Shapes3D.h"

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

		// ハンドルが有効な値を持っているか調べる
		bool IsValid() const;
		// 2つのハンドルが同じオブジェクトを指しているか比較する
		bool operator==(const StaticObjectHandle&) const = default;
	};

	// 登録された動的オブジェクトを識別するハンドル
	struct DynamicObjectHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		// ハンドルが有効な値を持っているか調べる
		bool IsValid() const;
		// 2つのハンドルが同じオブジェクトを指しているか比較する
		bool operator==(const DynamicObjectHandle&) const = default;
	};

	// 静的オブジェクトが持てる形状。
	// 初期実装ではOBBのみを扱い、形状を増やす際はvariantへ型を追加する。
	using StaticObjectShape = std::variant<Collision::OBB>;

	// 動的オブジェクトが持てる形状。
	// 初期実装ではSphereのみを扱い、形状を増やす際はvariantへ型を追加する。
	using DynamicObjectShape = std::variant<Collision::Sphere>;

	// 静的オブジェクトの登録パラメータ
	struct StaticObjectDesc {
		StaticObjectShape shape{};
		bool enabled = true;
		uint64_t userData{};
	};

	// 動的オブジェクトの登録パラメータ
	struct DynamicObjectDesc {
		DynamicObjectShape shape{};
		Vector3 velocity{};
		bool enabled = true;
		uint64_t userData{};
	};

	// 登録されている静的オブジェクトの状態
	struct StaticObjectState {
		StaticObjectHandle handle{};
		StaticObjectShape shape{};
		bool enabled{};
		uint64_t userData{};
	};

	// 登録されている動的オブジェクトの状態
	struct DynamicObjectState {
		DynamicObjectHandle handle{};
		DynamicObjectShape shape{};
		Vector3 velocity{};
		bool enabled{};
		uint64_t userData{};
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
		float maximumDeltaTime = 1.0f / 15.0f;
	};

	// 静的オブジェクトと動的オブジェクトを管理する物理ワールド。
	class PhysicsWorld {
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

	public:
		// 指定された設定で物理ワールドを作成する
		explicit PhysicsWorld(const PhysicsWorldSettings& settings = {});

		// 静的オブジェクトをワールドへ追加する
		StaticObjectHandle AddStaticObject(const StaticObjectDesc& desc);
		// OBBを静的オブジェクトとしてワールドへ追加する
		StaticObjectHandle AddStaticObject(const Collision::OBB& box, bool enabled = true, uint64_t userData = 0u);
		// 指定された静的オブジェクトをワールドから削除する
		bool RemoveStaticObject(StaticObjectHandle handle);
		// すべての静的オブジェクトをワールドから削除する
		void ClearStaticObjects();
		// 静的オブジェクトのハンドルが現在も有効か調べる
		bool IsValid(StaticObjectHandle handle) const;
		std::size_t GetStaticObjectCount() const;
		std::optional<StaticObjectState> GetState(StaticObjectHandle handle) const;
		void GetStaticObjectStates(std::vector<StaticObjectState>& states) const;

		// 動的オブジェクトをワールドへ追加する
		DynamicObjectHandle AddDynamicObject(const DynamicObjectDesc& desc);
		// Sphereを動的オブジェクトとしてワールドへ追加する
		DynamicObjectHandle AddDynamicObject(const Collision::Sphere& sphere, const Vector3& velocity = {}, bool enabled = true, uint64_t userData = 0u);
		bool SetVelocity(DynamicObjectHandle handle, const Vector3& velocity);
		// 指定された動的オブジェクトをワールドから削除する
		bool RemoveDynamicObject(DynamicObjectHandle handle);
		// すべての動的オブジェクトをワールドから削除する
		void ClearDynamicObjects();
		// 動的オブジェクトのハンドルが現在も有効か調べる
		bool IsValid(DynamicObjectHandle handle) const;
		std::size_t GetDynamicObjectCount() const;
		std::optional<DynamicObjectState> GetState(DynamicObjectHandle handle) const;
		void GetDynamicObjectStates(std::vector<DynamicObjectState>& states) const;

		void SetSettings(const PhysicsWorldSettings& settings);
		const PhysicsWorldSettings& GetSettings() const;

		// ワールドに登録されたすべてのオブジェクトを削除する
		void Clear();
		// 指定時間だけ動的オブジェクトのシミュレーションを進める
		void Update(float deltaTime);

	private:
		// ハンドルに対応する静的オブジェクトの格納領域を探す
		StaticObjectSlot* FindStaticObject(StaticObjectHandle handle);
		// ハンドルに対応する読み取り専用の静的オブジェクト格納領域を探す
		const StaticObjectSlot* FindStaticObject(StaticObjectHandle handle) const;
		// 指定インデックスの静的オブジェクトを削除して格納領域を再利用可能にする
		void RemoveStaticObjectAt(uint32_t index);

		// ハンドルに対応する動的オブジェクトの格納領域を探す
		DynamicObjectSlot* FindDynamicObject(DynamicObjectHandle handle);
		// ハンドルに対応する読み取り専用の動的オブジェクト格納領域を探す
		const DynamicObjectSlot* FindDynamicObject(DynamicObjectHandle handle) const;
		// 指定インデックスの動的オブジェクトを削除して格納領域を再利用可能にする
		void RemoveDynamicObjectAt(uint32_t index);

		// 重力と速度から動的オブジェクトの位置を更新する
		void IntegrateDynamicObjects(float substepTime);
		// 動的オブジェクトと静的オブジェクトのめり込みを解消する
		void SolveStaticContacts();
		// ワールド範囲外へ出た動的オブジェクトを削除する
		void CleanupDynamicObjects();

	private:
		PhysicsWorldSettings settings_{};
		std::vector<StaticObjectSlot> staticObjects_;
		std::vector<uint32_t> freeStaticObjectIndices_;
		std::size_t staticObjectCount_{};

		std::vector<DynamicObjectSlot> dynamicObjects_;
		std::vector<uint32_t> freeDynamicObjectIndices_;
		std::size_t dynamicObjectCount_{};
	};

}
