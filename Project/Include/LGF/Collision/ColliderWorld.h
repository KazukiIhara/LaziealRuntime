#pragma once

#include "LGF/Collision/Collision3D.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace LGF::Collision {

	using ColliderShape = std::variant<Sphere, AABB, Capsule>;
	using CollisionTag = std::string;
	using CollisionTagList = std::vector<CollisionTag>;

	/// <summary>ユーザー側のオブジェクトを識別する世代付きハンドル</summary>
	struct ObjectHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const ObjectHandle&) const = default;
	};

	/// <summary>登録されたコライダーを識別する世代付きハンドル</summary>
	struct ColliderHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const ColliderHandle&) const = default;
	};

	/// <summary>コライダーの登録パラメータ</summary>
	struct ColliderOptions {
		CollisionTag tag = "Default";
		/// <summary>衝突を検出し、イベントを受け取る相手のタグ</summary>
		CollisionTagList targetTags;
		ObjectHandle objectHandle{};
		bool enabled = true;
	};

	/// <summary>登録されているコライダーの状態</summary>
	struct ColliderState {
		ColliderHandle handle{};
		CollisionTag tag;
		CollisionTagList targetTags;
		ObjectHandle objectHandle{};
		ColliderShape shape{};
		bool enabled{};
	};

	/// <summary>
	/// 自分と衝突した相手の情報。
	/// contact.normal は自分から collider へ向かう向きになる。
	/// </summary>
	struct ColliderCollision {
		ColliderHandle collider{};
		CollisionTag tag;
		ObjectHandle objectHandle{};
		Contact contact{};
	};

	/// <summary>直前の更新で発生したコライダー単位の衝突イベント</summary>
	struct ColliderEvents {
		std::span<const ColliderCollision> enter;
		std::span<const ColliderCollision> stay;
		std::span<const ColliderCollision> exit;
	};

	/// <summary>ワールド内のコライダーに対するレイキャスト結果</summary>
	struct WorldRaycastHit {
		ColliderHandle collider{};
		ObjectHandle objectHandle{};
		RaycastHit hit{};
	};

	/// <summary>コライダーの登録、衝突イベント、レイキャストを管理する</summary>
	class ColliderWorld {
	private:
		struct CollisionPair {
			uint32_t firstIndex{};
			uint32_t secondIndex{};
			/// <summary>各コライダーが相手のタグを対象にしているか</summary>
			bool notifyFirst{};
			bool notifySecond{};
		};

		struct EventStorage {
			std::vector<ColliderCollision> enter;
			std::vector<ColliderCollision> stay;
			std::vector<ColliderCollision> exit;

			void Clear();
		};

		struct Slot {
			ColliderShape shape{};
			ObjectHandle objectHandle{};
			std::vector<uint32_t> targetTagIndices;
			std::vector<ColliderCollision> previousCollisions;
			std::vector<ColliderCollision> currentCollisions;
			EventStorage events;
			uint32_t tagIndex = ColliderHandle::InvalidIndex;
			uint32_t indexInTagGroup = ColliderHandle::InvalidIndex;
			uint32_t generation = 1u;
			bool isEnabled = true;
			bool isOccupied{};
		};

		struct TagGroup {
			std::vector<uint32_t> colliderIndices;
		};

	public:
		ColliderWorld();

		/// <summary>コライダーで使用するタグをワールドに登録する</summary>
		bool RegisterTag(const CollisionTag& tag);
		void RegisterTags(const CollisionTagList& tags);
		bool HasTag(const CollisionTag& tag) const;
		std::span<const CollisionTag> GetTags() const;

		ColliderHandle Add(
			const ColliderShape& shape,
			const ColliderOptions& options = {});
		ColliderHandle Add(
			const Sphere& sphere,
			const ColliderOptions& options = {});
		ColliderHandle Add(
			const AABB& box,
			const ColliderOptions& options = {});
		ColliderHandle Add(
			const Capsule& capsule,
			const ColliderOptions& options = {});

		bool Remove(ColliderHandle handle);
		/// <summary>登録済みのタグを残して、すべてのコライダーを削除する</summary>
		void Clear();

		bool IsValid(ColliderHandle handle) const;
		std::size_t GetColliderCount() const;

		bool SetShape(ColliderHandle handle, const ColliderShape& shape);
		bool SetShape(ColliderHandle handle, const Sphere& sphere);
		bool SetShape(ColliderHandle handle, const AABB& box);
		bool SetShape(ColliderHandle handle, const Capsule& capsule);
		bool SetTag(ColliderHandle handle, const CollisionTag& tag);
		/// <summary>衝突イベントを受け取る相手のタグを設定する</summary>
		bool SetTargetTags(
			ColliderHandle handle,
			const CollisionTagList& targetTags);
		bool SetObjectHandle(
			ColliderHandle handle,
			ObjectHandle objectHandle);
		bool SetEnabled(ColliderHandle handle, bool enabled);

		std::optional<ColliderState> GetCollider(ColliderHandle handle) const;
		bool IsColliding(ColliderHandle handle) const;

		void Step();
		ColliderEvents GetEvents(ColliderHandle handle) const;

		/// <summary>
		/// 指定したタグのうち最も近いコライダーへレイキャストする。
		/// tags が空の場合は全タグを対象とする。
		/// </summary>
		std::optional<WorldRaycastHit> RaycastClosest(
			const Ray& ray,
			float maxDistance = std::numeric_limits<float>::infinity(),
			const CollisionTagList& tags = {}) const;

	private:

		Slot* FindSlot(ColliderHandle handle);
		const Slot* FindSlot(ColliderHandle handle) const;
		const ColliderCollision* FindCollision(
			const std::vector<ColliderCollision>& collisions,
			ColliderHandle collider) const;
		std::optional<uint32_t> FindTagIndex(const CollisionTag& tag) const;
		bool ResolveTargetTagIndices(
			const CollisionTagList& targetTags,
			std::vector<uint32_t>& targetTagIndices) const;
		void AddToTagGroup(uint32_t colliderIndex, uint32_t tagIndex);
		void RemoveFromTagGroup(uint32_t colliderIndex);
		void BuildCollisionPairs();
		void BuildEvents(Slot& slot);
		bool RaycastTagsContain(
			const CollisionTagList& tags,
			const CollisionTag& tag) const;

	private:

		std::vector<Slot> slots_;
		std::vector<uint32_t> freeIndices_;
		CollisionTagList tags_;
		std::vector<TagGroup> tagGroups_;
		std::vector<CollisionPair> collisionPairs_;
		std::size_t colliderCount_{};
	};

}
