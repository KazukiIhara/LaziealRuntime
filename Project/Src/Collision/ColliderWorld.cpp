#include "LGF/Collision/ColliderWorld.h"

#include <algorithm>
#include <utility>

namespace {

	using namespace LGF::Collision;

	std::optional<Contact> CollideShapes(
		const ColliderShape& first,
		const ColliderShape& second) {
		return std::visit(
			[](const auto& firstShape, const auto& secondShape) {
				return Collide(firstShape, secondShape);
			},
			first,
			second);
	}

	std::optional<RaycastHit> RaycastShape(
		const Ray& ray,
		const ColliderShape& shape,
		float maxDistance) {
		return std::visit(
			[&](const auto& colliderShape) {
				return Raycast(ray, colliderShape, maxDistance);
			},
			shape);
	}

	uint32_t NextGeneration(uint32_t generation) {
		++generation;
		return generation == 0u ? 1u : generation;
	}

}

namespace LGF::Collision {

	bool ColliderHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	ColliderHandle ColliderWorld::Add(
		const ColliderShape& shape,
		const ColliderOptions& options) {
		uint32_t index = 0u;
		if (freeIndices_.empty()) {
			index = static_cast<uint32_t>(slots_.size());
			slots_.push_back({});
		} else {
			index = freeIndices_.back();
			freeIndices_.pop_back();
		}

		Slot& slot = slots_[index];
		slot.shape = shape;
		slot.options = options;
		slot.occupied = true;
		++colliderCount_;
		return { index, slot.generation };
	}

	ColliderHandle ColliderWorld::Add(
		const Sphere& sphere,
		const ColliderOptions& options) {
		return Add(ColliderShape{ sphere }, options);
	}

	ColliderHandle ColliderWorld::Add(
		const AABB& box,
		const ColliderOptions& options) {
		return Add(ColliderShape{ box }, options);
	}

	ColliderHandle ColliderWorld::Add(
		const Capsule& capsule,
		const ColliderOptions& options) {
		return Add(ColliderShape{ capsule }, options);
	}

	bool ColliderWorld::Remove(ColliderHandle handle) {
		Slot* slot = FindSlot(handle);
		if (!slot) {
			return false;
		}

		slot->occupied = false;
		slot->generation = NextGeneration(slot->generation);
		freeIndices_.push_back(handle.index);
		--colliderCount_;
		return true;
	}

	void ColliderWorld::Clear() {
		freeIndices_.clear();
		freeIndices_.reserve(slots_.size());
		for (uint32_t index = 0u; index < slots_.size(); ++index) {
			Slot& slot = slots_[index];
			if (slot.occupied) {
				slot.generation = NextGeneration(slot.generation);
			}
			slot.occupied = false;
			freeIndices_.push_back(index);
		}
		activePairs_.clear();
		events_.clear();
		colliderCount_ = 0u;
	}

	bool ColliderWorld::IsValid(ColliderHandle handle) const {
		return FindSlot(handle) != nullptr;
	}

	std::size_t ColliderWorld::GetColliderCount() const {
		return colliderCount_;
	}

	bool ColliderWorld::SetShape(ColliderHandle handle, const ColliderShape& shape) {
		Slot* slot = FindSlot(handle);
		if (!slot) {
			return false;
		}
		slot->shape = shape;
		return true;
	}

	bool ColliderWorld::SetShape(ColliderHandle handle, const Sphere& sphere) {
		return SetShape(handle, ColliderShape{ sphere });
	}

	bool ColliderWorld::SetShape(ColliderHandle handle, const AABB& box) {
		return SetShape(handle, ColliderShape{ box });
	}

	bool ColliderWorld::SetShape(ColliderHandle handle, const Capsule& capsule) {
		return SetShape(handle, ColliderShape{ capsule });
	}

	bool ColliderWorld::SetEnabled(ColliderHandle handle, bool enabled) {
		Slot* slot = FindSlot(handle);
		if (!slot) {
			return false;
		}
		slot->options.enabled = enabled;
		return true;
	}

	bool ColliderWorld::SetLayer(ColliderHandle handle, CollisionLayerMask layer) {
		Slot* slot = FindSlot(handle);
		if (!slot) {
			return false;
		}
		slot->options.layer = layer;
		return true;
	}

	bool ColliderWorld::SetMask(ColliderHandle handle, CollisionLayerMask mask) {
		Slot* slot = FindSlot(handle);
		if (!slot) {
			return false;
		}
		slot->options.mask = mask;
		return true;
	}

	bool ColliderWorld::SetTrigger(ColliderHandle handle, bool isTrigger) {
		Slot* slot = FindSlot(handle);
		if (!slot) {
			return false;
		}
		slot->options.isTrigger = isTrigger;
		return true;
	}

	bool ColliderWorld::SetUserData(ColliderHandle handle, uint64_t userData) {
		Slot* slot = FindSlot(handle);
		if (!slot) {
			return false;
		}
		slot->options.userData = userData;
		return true;
	}

	const ColliderShape* ColliderWorld::GetShape(ColliderHandle handle) const {
		const Slot* slot = FindSlot(handle);
		return slot ? &slot->shape : nullptr;
	}

	std::optional<ColliderOptions> ColliderWorld::GetOptions(ColliderHandle handle) const {
		const Slot* slot = FindSlot(handle);
		return slot ? std::optional<ColliderOptions>(slot->options) : std::nullopt;
	}

	bool ColliderWorld::IsColliding(ColliderHandle handle) const {
		if (!IsValid(handle)) {
			return false;
		}
		return std::ranges::any_of(activePairs_, [&](const auto& pair) {
			return pair.first.first == handle || pair.first.second == handle;
		});
	}

	void ColliderWorld::Step() {
		events_.clear();
		std::map<PairKey, PairState> currentPairs;

		for (uint32_t firstIndex = 0u; firstIndex < slots_.size(); ++firstIndex) {
			const Slot& firstSlot = slots_[firstIndex];
			if (!firstSlot.occupied || !firstSlot.options.enabled) {
				continue;
			}

			for (uint32_t secondIndex = firstIndex + 1u;
				secondIndex < slots_.size(); ++secondIndex) {
				const Slot& secondSlot = slots_[secondIndex];
				if (!secondSlot.occupied || !secondSlot.options.enabled) {
					continue;
				}
				if ((firstSlot.options.mask & secondSlot.options.layer) == 0u ||
					(secondSlot.options.mask & firstSlot.options.layer) == 0u) {
					continue;
				}

				const std::optional<Contact> contact =
					CollideShapes(firstSlot.shape, secondSlot.shape);
				if (!contact) {
					continue;
				}

				const PairKey key{
					.first = { firstIndex, firstSlot.generation },
					.second = { secondIndex, secondSlot.generation },
				};
				const PairState state{
					.contact = *contact,
					.firstUserData = firstSlot.options.userData,
					.secondUserData = secondSlot.options.userData,
					.isTrigger = firstSlot.options.isTrigger || secondSlot.options.isTrigger,
				};
				const CollisionPhase phase = activePairs_.contains(key)
					? CollisionPhase::Stay
					: CollisionPhase::Enter;
				currentPairs.emplace(key, state);
				events_.push_back({
					.first = key.first,
					.second = key.second,
					.firstUserData = state.firstUserData,
					.secondUserData = state.secondUserData,
					.phase = phase,
					.contact = state.contact,
					.isTrigger = state.isTrigger,
				});
			}
		}

		for (const auto& [key, state] : activePairs_) {
			if (currentPairs.contains(key)) {
				continue;
			}
			events_.push_back({
				.first = key.first,
				.second = key.second,
				.firstUserData = state.firstUserData,
				.secondUserData = state.secondUserData,
				.phase = CollisionPhase::Exit,
				.contact = state.contact,
				.isTrigger = state.isTrigger,
			});
		}

		activePairs_ = std::move(currentPairs);
	}

	std::span<const CollisionEvent> ColliderWorld::GetEvents() const {
		return { events_.data(), events_.size() };
	}

	std::optional<WorldRaycastHit> ColliderWorld::RaycastClosest(
		const Ray& ray,
		float maxDistance,
		CollisionLayerMask mask) const {
		std::optional<WorldRaycastHit> closestHit;
		float closestDistance = maxDistance;
		for (uint32_t index = 0u; index < slots_.size(); ++index) {
			const Slot& slot = slots_[index];
			if (!slot.occupied || !slot.options.enabled ||
				(slot.options.layer & mask) == 0u) {
				continue;
			}

			const std::optional<RaycastHit> hit =
				RaycastShape(ray, slot.shape, closestDistance);
			if (!hit || hit->distance > closestDistance) {
				continue;
			}

			closestDistance = hit->distance;
			closestHit = WorldRaycastHit{
				.collider = { index, slot.generation },
				.userData = slot.options.userData,
				.hit = *hit,
			};
		}
		return closestHit;
	}

	bool ColliderWorld::PairKey::operator<(const PairKey& other) const {
		if (first.index != other.first.index) {
			return first.index < other.first.index;
		}
		if (first.generation != other.first.generation) {
			return first.generation < other.first.generation;
		}
		if (second.index != other.second.index) {
			return second.index < other.second.index;
		}
		return second.generation < other.second.generation;
	}

	ColliderWorld::Slot* ColliderWorld::FindSlot(ColliderHandle handle) {
		if (!handle.IsValid() || handle.index >= slots_.size()) {
			return nullptr;
		}
		Slot& slot = slots_[handle.index];
		return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
	}

	const ColliderWorld::Slot* ColliderWorld::FindSlot(ColliderHandle handle) const {
		if (!handle.IsValid() || handle.index >= slots_.size()) {
			return nullptr;
		}
		const Slot& slot = slots_[handle.index];
		return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
	}

}
