#pragma once

#include "LGF/Collision/Collision3D.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace LGF::Collision {

	using CollisionLayerMask = uint32_t;
	inline constexpr CollisionLayerMask AllLayers =
		std::numeric_limits<CollisionLayerMask>::max();

	using ColliderShape = std::variant<Sphere, AABB, Capsule>;

	struct ColliderHandle final {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation = 0u;

		bool IsValid() const;
		bool operator==(const ColliderHandle&) const = default;
	};

	struct ColliderOptions final {
		CollisionLayerMask layer = 1u;
		CollisionLayerMask mask = AllLayers;
		bool isTrigger = false;
		bool enabled = true;
		uint64_t userData = 0u;
	};

	enum class CollisionPhase : uint8_t {
		Enter,
		Stay,
		Exit,
	};

	struct CollisionEvent final {
		ColliderHandle first{};
		ColliderHandle second{};
		uint64_t firstUserData = 0u;
		uint64_t secondUserData = 0u;
		CollisionPhase phase = CollisionPhase::Enter;
		Contact contact{};
		bool isTrigger = false;
	};

	struct WorldRaycastHit final {
		ColliderHandle collider{};
		uint64_t userData = 0u;
		RaycastHit hit{};
	};

	class ColliderWorld final {
	public:
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
		void Clear();

		bool IsValid(ColliderHandle handle) const;
		std::size_t GetColliderCount() const;

		bool SetShape(ColliderHandle handle, const ColliderShape& shape);
		bool SetShape(ColliderHandle handle, const Sphere& sphere);
		bool SetShape(ColliderHandle handle, const AABB& box);
		bool SetShape(ColliderHandle handle, const Capsule& capsule);
		bool SetEnabled(ColliderHandle handle, bool enabled);
		bool SetLayer(ColliderHandle handle, CollisionLayerMask layer);
		bool SetMask(ColliderHandle handle, CollisionLayerMask mask);
		bool SetTrigger(ColliderHandle handle, bool isTrigger);
		bool SetUserData(ColliderHandle handle, uint64_t userData);

		const ColliderShape* GetShape(ColliderHandle handle) const;
		std::optional<ColliderOptions> GetOptions(ColliderHandle handle) const;
		bool IsColliding(ColliderHandle handle) const;

		void Step();
		std::span<const CollisionEvent> GetEvents() const;

		std::optional<WorldRaycastHit> RaycastClosest(
			const Ray& ray,
			float maxDistance = std::numeric_limits<float>::infinity(),
			CollisionLayerMask mask = AllLayers) const;

	private:
		struct Slot final {
			ColliderShape shape{};
			ColliderOptions options{};
			uint32_t generation = 1u;
			bool occupied = false;
		};

		struct PairKey final {
			ColliderHandle first{};
			ColliderHandle second{};

			bool operator<(const PairKey& other) const;
		};

		struct PairState final {
			Contact contact{};
			uint64_t firstUserData = 0u;
			uint64_t secondUserData = 0u;
			bool isTrigger = false;
		};

		Slot* FindSlot(ColliderHandle handle);
		const Slot* FindSlot(ColliderHandle handle) const;

		std::vector<Slot> slots_;
		std::vector<uint32_t> freeIndices_;
		std::map<PairKey, PairState> activePairs_;
		std::vector<CollisionEvent> events_;
		std::size_t colliderCount_ = 0u;
	};

}
