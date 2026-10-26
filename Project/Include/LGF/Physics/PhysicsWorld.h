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

		/// <summary>ハンドルが有効な値を持っているか調べる</summary>
		bool IsValid() const;
		/// <summary>2つのハンドルが同じオブジェクトを指しているか比較する</summary>
		bool operator==(const StaticObjectHandle&) const = default;
	};

	/// <summary>登録された動的オブジェクトを識別するハンドル</summary>
	struct DynamicObjectHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		/// <summary>ハンドルが有効な値を持っているか調べる</summary>
		bool IsValid() const;
		/// <summary>2つのハンドルが同じオブジェクトを指しているか比較する</summary>
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
		/// <summary>指定された設定で物理ワールドを作成する</summary>
		explicit PhysicsWorld(const PhysicsWorldSettings& settings = {});

		/// <summary>静的オブジェクトをワールドへ追加する</summary>
		StaticObjectHandle AddStaticObject(const StaticObjectDesc& desc);
		/// <summary>OBBを静的オブジェクトとしてワールドへ追加する</summary>
		StaticObjectHandle AddStaticObject(const Collision::OBB& box, bool enabled = true, uint64_t userData = 0u);
		/// <summary>指定された静的オブジェクトをワールドから削除する</summary>
		bool RemoveStaticObject(StaticObjectHandle handle);
		/// <summary>すべての静的オブジェクトをワールドから削除する</summary>
		void ClearStaticObjects();
		/// <summary>静的オブジェクトのハンドルが現在も有効か調べる</summary>
		bool IsValid(StaticObjectHandle handle) const;
		std::size_t GetStaticObjectCount() const;
		std::optional<StaticObjectState> GetState(StaticObjectHandle handle) const;
		void GetStaticObjectStates(std::vector<StaticObjectState>& states) const;

		/// <summary>動的オブジェクトをワールドへ追加する</summary>
		DynamicObjectHandle AddDynamicObject(const DynamicObjectDesc& desc);
		/// <summary>Sphereを動的オブジェクトとしてワールドへ追加する</summary>
		DynamicObjectHandle AddDynamicObject(const Collision::Sphere& sphere, const Vector3& velocity = {}, bool enabled = true, uint64_t userData = 0u);
		/// <summary>指定された動的オブジェクトをワールドから削除する</summary>
		bool RemoveDynamicObject(DynamicObjectHandle handle);
		/// <summary>すべての動的オブジェクトをワールドから削除する</summary>
		void ClearDynamicObjects();
		/// <summary>動的オブジェクトのハンドルが現在も有効か調べる</summary>
		bool IsValid(DynamicObjectHandle handle) const;
		std::size_t GetDynamicObjectCount() const;
		std::optional<DynamicObjectState> GetState(DynamicObjectHandle handle) const;
		void GetDynamicObjectStates(std::vector<DynamicObjectState>& states) const;

		void SetSettings(const PhysicsWorldSettings& settings);
		const PhysicsWorldSettings& GetSettings() const;

		/// <summary>ワールドに登録されたすべてのオブジェクトを削除する</summary>
		void Clear();
		/// <summary>指定時間だけ動的オブジェクトのシミュレーションを進める</summary>
		void Update(float deltaTime);

	private:
		/// <summary>ハンドルに対応する静的オブジェクトの格納領域を探す</summary>
		StaticObjectSlot* FindStaticObject(StaticObjectHandle handle);
		/// <summary>ハンドルに対応する読み取り専用の静的オブジェクト格納領域を探す</summary>
		const StaticObjectSlot* FindStaticObject(StaticObjectHandle handle) const;
		/// <summary>指定インデックスの静的オブジェクトを削除して格納領域を再利用可能にする</summary>
		void RemoveStaticObjectAt(uint32_t index);

		/// <summary>ハンドルに対応する動的オブジェクトの格納領域を探す</summary>
		DynamicObjectSlot* FindDynamicObject(DynamicObjectHandle handle);
		/// <summary>ハンドルに対応する読み取り専用の動的オブジェクト格納領域を探す</summary>
		const DynamicObjectSlot* FindDynamicObject(DynamicObjectHandle handle) const;
		/// <summary>指定インデックスの動的オブジェクトを削除して格納領域を再利用可能にする</summary>
		void RemoveDynamicObjectAt(uint32_t index);

		/// <summary>重力と速度から動的オブジェクトの位置を更新する</summary>
		void IntegrateDynamicObjects(float substepTime);
		/// <summary>動的オブジェクトと静的オブジェクトのめり込みを解消する</summary>
		void SolveStaticContacts();
		/// <summary>ワールド範囲外へ出た動的オブジェクトを削除する</summary>
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
