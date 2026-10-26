#include "LGF/Collision/ColliderWorld.h"

#include <algorithm>
#include <utility>

#include "../Core/HandleUtility.h"

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

	Contact ReverseContact(const Contact& contact) {
		Contact result = contact;
		result.normal *= -1.0f;
		return result;
	}

}

namespace LGF::Collision {

	bool ObjectHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	bool ColliderHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	void ColliderWorld::EventStorage::Clear() {
		enter.clear();
		stay.clear();
		exit.clear();
	}

	ColliderWorld::ColliderWorld() {
		RegisterTag("Default");
	}

	bool ColliderWorld::RegisterTag(const CollisionTag& tag) {
		if (tag.empty() || HasTag(tag)) {
			return false;
		}

		tags_.push_back(tag);
		tagGroups_.push_back({});
		return true;
	}

	void ColliderWorld::RegisterTags(const CollisionTagList& tags) {
		for (const CollisionTag& tag : tags) {
			RegisterTag(tag);
		}
	}

	bool ColliderWorld::HasTag(const CollisionTag& tag) const {
		return FindTagIndex(tag).has_value();
	}

	std::span<const CollisionTag> ColliderWorld::GetTags() const {
		return tags_;
	}

	ColliderHandle ColliderWorld::Add(
		const ColliderShape& shape,
		const ColliderOptions& options) {
		const std::optional<uint32_t> tagIndex = FindTagIndex(options.tag);
		std::vector<uint32_t> targetTagIndices;
		if (!tagIndex ||
			!ResolveTargetTagIndices(options.targetTags, targetTagIndices)) {
			return {};
		}

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
		slot.objectHandle = options.objectHandle;
		slot.targetTagIndices = std::move(targetTagIndices);
		slot.previousCollisions.clear();
		slot.currentCollisions.clear();
		slot.events.Clear();
		slot.isEnabled = options.enabled;
		slot.isOccupied = true;
		AddToTagGroup(index, *tagIndex);
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

		RemoveFromTagGroup(handle.index);
		slot->objectHandle = {};
		slot->targetTagIndices.clear();
		slot->previousCollisions.clear();
		slot->currentCollisions.clear();
		slot->events.Clear();
		slot->isOccupied = false;
		slot->generation = Internal::NextGeneration(slot->generation);
		freeIndices_.push_back(handle.index);
		--colliderCount_;
		return true;
	}

	void ColliderWorld::Clear() {
		for (TagGroup& tagGroup : tagGroups_) {
			tagGroup.colliderIndices.clear();
		}
		collisionPairs_.clear();
		freeIndices_.clear();
		freeIndices_.reserve(slots_.size());
		for (uint32_t index = 0u; index < slots_.size(); ++index) {
			Slot& slot = slots_[index];
			if (slot.isOccupied) {
				slot.generation = Internal::NextGeneration(slot.generation);
			}
			slot.objectHandle = {};
			slot.targetTagIndices.clear();
			slot.previousCollisions.clear();
			slot.currentCollisions.clear();
			slot.events.Clear();
			slot.tagIndex = ColliderHandle::InvalidIndex;
			slot.indexInTagGroup = ColliderHandle::InvalidIndex;
			slot.isOccupied = false;
			freeIndices_.push_back(index);
		}
		colliderCount_ = 0u;
	}

	bool ColliderWorld::IsValid(ColliderHandle handle) const {
		return FindSlot(handle) != nullptr;
	}

	std::size_t ColliderWorld::GetColliderCount() const {
		return colliderCount_;
	}

	bool ColliderWorld::SetShape(
		ColliderHandle handle,
		const ColliderShape& shape) {
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

	bool ColliderWorld::SetTag(ColliderHandle handle, const CollisionTag& tag) {
		Slot* slot = FindSlot(handle);
		const std::optional<uint32_t> tagIndex = FindTagIndex(tag);
		if (!slot || !tagIndex) {
			return false;
		}
		if (slot->tagIndex == *tagIndex) {
			return true;
		}

		RemoveFromTagGroup(handle.index);
		AddToTagGroup(handle.index, *tagIndex);
		return true;
	}

	bool ColliderWorld::SetTargetTags(
		ColliderHandle handle,
		const CollisionTagList& targetTags) {
		Slot* slot = FindSlot(handle);
		if (!slot) {
			return false;
		}

		std::vector<uint32_t> targetTagIndices;
		if (!ResolveTargetTagIndices(targetTags, targetTagIndices)) {
			return false;
		}
		slot->targetTagIndices = std::move(targetTagIndices);
		return true;
	}

	bool ColliderWorld::SetObjectHandle(
		ColliderHandle handle,
		ObjectHandle objectHandle) {
		Slot* slot = FindSlot(handle);
		if (!slot) {
			return false;
		}
		slot->objectHandle = objectHandle;
		return true;
	}

	bool ColliderWorld::SetEnabled(ColliderHandle handle, bool enabled) {
		Slot* slot = FindSlot(handle);
		if (!slot) {
			return false;
		}
		slot->isEnabled = enabled;
		return true;
	}

	std::optional<ColliderState> ColliderWorld::GetCollider(
		ColliderHandle handle) const {
		const Slot* slot = FindSlot(handle);
		if (!slot) {
			return std::nullopt;
		}
		CollisionTagList targetTags;
		targetTags.reserve(slot->targetTagIndices.size());
		for (const uint32_t tagIndex : slot->targetTagIndices) {
			targetTags.push_back(tags_[tagIndex]);
		}

		return ColliderState{
			.handle = handle,
			.tag = tags_[slot->tagIndex],
			.targetTags = std::move(targetTags),
			.objectHandle = slot->objectHandle,
			.shape = slot->shape,
			.enabled = slot->isEnabled,
		};
	}

	bool ColliderWorld::IsColliding(ColliderHandle handle) const {
		const Slot* slot = FindSlot(handle);
		return slot && !slot->currentCollisions.empty();
	}

	void ColliderWorld::Step() {
		for (Slot& slot : slots_) {
			if (!slot.isOccupied) {
				continue;
			}
			slot.previousCollisions.swap(slot.currentCollisions);
			slot.currentCollisions.clear();
			slot.events.Clear();
		}

		BuildCollisionPairs();
		for (const CollisionPair& pair : collisionPairs_) {
			Slot& firstSlot = slots_[pair.firstIndex];
			Slot& secondSlot = slots_[pair.secondIndex];
			const std::optional<Contact> contact =
				CollideShapes(firstSlot.shape, secondSlot.shape);
			if (!contact) {
				continue;
			}

			const ColliderHandle firstHandle{
				pair.firstIndex,
				firstSlot.generation,
			};
			const ColliderHandle secondHandle{
				pair.secondIndex,
				secondSlot.generation,
			};
			if (pair.notifyFirst) {
				firstSlot.currentCollisions.push_back({
					.collider = secondHandle,
					.tag = tags_[secondSlot.tagIndex],
					.objectHandle = secondSlot.objectHandle,
					.contact = *contact,
				});
			}
			if (pair.notifySecond) {
				secondSlot.currentCollisions.push_back({
					.collider = firstHandle,
					.tag = tags_[firstSlot.tagIndex],
					.objectHandle = firstSlot.objectHandle,
					.contact = ReverseContact(*contact),
				});
			}
		}

		for (Slot& slot : slots_) {
			if (slot.isOccupied) {
				BuildEvents(slot);
			}
		}
	}

	ColliderEvents ColliderWorld::GetEvents(ColliderHandle handle) const {
		const Slot* slot = FindSlot(handle);
		return slot
			? ColliderEvents{
				.enter = slot->events.enter,
				.stay = slot->events.stay,
				.exit = slot->events.exit,
			}
			: ColliderEvents{};
	}

	std::optional<WorldRaycastHit> ColliderWorld::RaycastClosest(
		const Ray& ray,
		float maxDistance,
		const CollisionTagList& tags) const {
		std::optional<WorldRaycastHit> closestHit;
		float closestDistance = maxDistance;
		for (uint32_t index = 0u; index < slots_.size(); ++index) {
			const Slot& slot = slots_[index];
			if (!slot.isOccupied || !slot.isEnabled ||
				!RaycastTagsContain(tags, tags_[slot.tagIndex])) {
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
				.objectHandle = slot.objectHandle,
				.hit = *hit,
			};
		}
		return closestHit;
	}

	ColliderWorld::Slot* ColliderWorld::FindSlot(ColliderHandle handle) {
		if (!handle.IsValid() || handle.index >= slots_.size()) {
			return nullptr;
		}
		Slot& slot = slots_[handle.index];
		return slot.isOccupied && slot.generation == handle.generation ? &slot : nullptr;
	}

	const ColliderWorld::Slot* ColliderWorld::FindSlot(
		ColliderHandle handle) const {
		if (!handle.IsValid() || handle.index >= slots_.size()) {
			return nullptr;
		}
		const Slot& slot = slots_[handle.index];
		return slot.isOccupied && slot.generation == handle.generation ? &slot : nullptr;
	}

	const ColliderCollision* ColliderWorld::FindCollision(
		const std::vector<ColliderCollision>& collisions,
		ColliderHandle collider) const {
		const auto found = std::ranges::find_if(
			collisions,
			[&](const ColliderCollision& collision) {
				return collision.collider == collider;
			});
		return found != collisions.end() ? &*found : nullptr;
	}

	std::optional<uint32_t> ColliderWorld::FindTagIndex(
		const CollisionTag& tag) const {
		const auto found = std::ranges::find(tags_, tag);
		return found != tags_.end()
			? std::optional<uint32_t>(static_cast<uint32_t>(found - tags_.begin()))
			: std::nullopt;
	}

	bool ColliderWorld::ResolveTargetTagIndices(
		const CollisionTagList& targetTags,
		std::vector<uint32_t>& targetTagIndices) const {
		targetTagIndices.clear();
		targetTagIndices.reserve(targetTags.size());
		for (const CollisionTag& targetTag : targetTags) {
			const std::optional<uint32_t> tagIndex = FindTagIndex(targetTag);
			if (!tagIndex) {
				return false;
			}
			if (std::ranges::find(targetTagIndices, *tagIndex) ==
				targetTagIndices.end()) {
				targetTagIndices.push_back(*tagIndex);
			}
		}
		return true;
	}

	void ColliderWorld::AddToTagGroup(
		uint32_t colliderIndex,
		uint32_t tagIndex) {
		Slot& slot = slots_[colliderIndex];
		TagGroup& tagGroup = tagGroups_[tagIndex];
		slot.tagIndex = tagIndex;
		slot.indexInTagGroup =
			static_cast<uint32_t>(tagGroup.colliderIndices.size());
		tagGroup.colliderIndices.push_back(colliderIndex);
	}

	void ColliderWorld::RemoveFromTagGroup(uint32_t colliderIndex) {
		Slot& slot = slots_[colliderIndex];
		if (slot.tagIndex == ColliderHandle::InvalidIndex) {
			return;
		}

		std::vector<uint32_t>& colliderIndices =
			tagGroups_[slot.tagIndex].colliderIndices;
		const uint32_t movedColliderIndex = colliderIndices.back();
		colliderIndices[slot.indexInTagGroup] = movedColliderIndex;
		colliderIndices.pop_back();
		slots_[movedColliderIndex].indexInTagGroup = slot.indexInTagGroup;
		slot.tagIndex = ColliderHandle::InvalidIndex;
		slot.indexInTagGroup = ColliderHandle::InvalidIndex;
	}

	void ColliderWorld::BuildCollisionPairs() {
		collisionPairs_.clear();
		for (uint32_t selfIndex = 0u; selfIndex < slots_.size(); ++selfIndex) {
			const Slot& selfSlot = slots_[selfIndex];
			if (!selfSlot.isOccupied || !selfSlot.isEnabled) {
				continue;
			}

			for (const uint32_t targetTagIndex : selfSlot.targetTagIndices) {
				for (const uint32_t otherIndex :
					tagGroups_[targetTagIndex].colliderIndices) {
					if (selfIndex == otherIndex) {
						continue;
					}
					const Slot& otherSlot = slots_[otherIndex];
					if (!otherSlot.isOccupied || !otherSlot.isEnabled) {
						continue;
					}

					if (selfIndex < otherIndex) {
						collisionPairs_.push_back({
							.firstIndex = selfIndex,
							.secondIndex = otherIndex,
							.notifyFirst = true,
						});
					} else {
						collisionPairs_.push_back({
							.firstIndex = otherIndex,
							.secondIndex = selfIndex,
							.notifySecond = true,
						});
					}
				}
			}
		}

		std::ranges::sort(
			collisionPairs_,
			[](const CollisionPair& lhs, const CollisionPair& rhs) {
				return lhs.firstIndex != rhs.firstIndex
					? lhs.firstIndex < rhs.firstIndex
					: lhs.secondIndex < rhs.secondIndex;
			});

		std::size_t pairCount = 0u;
		for (std::size_t index = 0u; index < collisionPairs_.size(); ++index) {
			const CollisionPair candidate = collisionPairs_[index];
			if (pairCount > 0u) {
				CollisionPair& previous = collisionPairs_[pairCount - 1u];
				if (previous.firstIndex == candidate.firstIndex &&
					previous.secondIndex == candidate.secondIndex) {
					previous.notifyFirst |= candidate.notifyFirst;
					previous.notifySecond |= candidate.notifySecond;
					continue;
				}
			}
			collisionPairs_[pairCount] = candidate;
			++pairCount;
		}
		collisionPairs_.resize(pairCount);
	}

	void ColliderWorld::BuildEvents(Slot& slot) {
		for (const ColliderCollision& collision : slot.currentCollisions) {
			if (FindCollision(slot.previousCollisions, collision.collider)) {
				slot.events.stay.push_back(collision);
			} else {
				slot.events.enter.push_back(collision);
			}
		}

		for (const ColliderCollision& collision : slot.previousCollisions) {
			if (!FindCollision(slot.currentCollisions, collision.collider)) {
				slot.events.exit.push_back(collision);
			}
		}
	}

	bool ColliderWorld::RaycastTagsContain(
		const CollisionTagList& tags,
		const CollisionTag& tag) const {
		return tags.empty() || std::ranges::find(tags, tag) != tags.end();
	}

}
