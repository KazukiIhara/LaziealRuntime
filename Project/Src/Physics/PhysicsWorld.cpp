#include "LGF/Physics/PhysicsWorld.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "../Core/HandleUtility.h"
#include "LGF/Collision/Collision3D.h"
#include "LGF/Math/Functions/VectorMath.h"

namespace {

	using namespace LGF;

	bool IsFinite(const Vector3& value) {
		return std::isfinite(value.x) &&
			std::isfinite(value.y) &&
			std::isfinite(value.z);
	}

	Physics::PhysicsWorldSettings NormalizeSettings(
		const Physics::PhysicsWorldSettings& settings) {
		Physics::PhysicsWorldSettings result = settings;
		if (!IsFinite(result.gravity)) {
			result.gravity = { 0.0f, -9.8f, 0.0f };
		}
		if (!Collision::IsValid(result.worldBounds) ||
			!IsFinite(result.worldBounds.min) ||
			!IsFinite(result.worldBounds.max)) {
			result.worldBounds = {
				.min = { -100.0f, -100.0f, -100.0f },
				.max = { 100.0f, 100.0f, 100.0f },
			};
		}
		result.substepCount = std::max(result.substepCount, 1u);
		result.solverIterationCount = std::max(
			result.solverIterationCount,
			1u);
		if (!std::isfinite(result.maximumDeltaTime) ||
			result.maximumDeltaTime <= 0.0f) {
			result.maximumDeltaTime = 1.0f / 15.0f;
		}
		return result;
	}

	bool IsShapeValid(const Physics::StaticObjectShape& shape) {
		return std::visit([](const auto& value) {
			return Collision::IsValid(value);
			}, shape);
	}

	bool IsShapeValid(const Physics::DynamicObjectShape& shape) {
		return std::visit([](const auto& value) {
			return Collision::IsValid(value);
			}, shape);
	}

	Vector3 GetPosition(const Physics::DynamicObjectShape& shape) {
		return std::visit([](const auto& value) {
			return value.center;
			}, shape);
	}

	void MoveShape(Physics::DynamicObjectShape& shape, const Vector3& movement) {
		std::visit([&](auto& value) {
			value.center += movement;
			}, shape);
	}

	struct StaticContact {
		Vector3 normal{};
		float penetrationDepth{};
	};

	std::optional<StaticContact> FindContact(
		const Physics::DynamicObjectShape& dynamicShape,
		const Physics::StaticObjectShape& staticShape) {
		return std::visit([](
			const auto& dynamicObject,
			const auto& staticObject) -> std::optional<StaticContact> {
			using DynamicShape = std::decay_t<decltype(dynamicObject)>;
			using StaticShape = std::decay_t<decltype(staticObject)>;
			if constexpr (
				std::is_same_v<DynamicShape, Collision::Sphere> &&
				std::is_same_v<StaticShape, Collision::OBB>) {
				const std::optional<Collision::Contact> contact =
					Collision::Collide(dynamicObject, staticObject);
				if (!contact) {
					return std::nullopt;
				}

				// 球の中心が箱の外側なら、接触法線とは反対へ押し戻す。
				// 中心まで箱へ入った場合は、Collision側が返す最寄り面の方向へ出す。
				const bool centerInside = Collision::Contains(
					staticObject,
					dynamicObject.center);
				return StaticContact{
					.normal = centerInside ? contact->normal : -contact->normal,
					.penetrationDepth = contact->penetrationDepth,
				};
			} else {
				// variantへ形状を追加した時点では、未対応の組み合わせを衝突なしとして扱う。
				return std::nullopt;
			}
			}, dynamicShape, staticShape);
	}

}

namespace LGF::Physics {

	bool StaticObjectHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	bool DynamicObjectHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	PhysicsWorld::PhysicsWorld(const PhysicsWorldSettings& settings) :
		settings_(NormalizeSettings(settings)) {
	}

	StaticObjectHandle PhysicsWorld::AddStaticObject(
		const StaticObjectDesc& desc) {
		if (!IsShapeValid(desc.shape)) {
			return {};
		}

		uint32_t index = 0u;
		if (freeStaticObjectIndices_.empty()) {
			index = static_cast<uint32_t>(staticObjects_.size());
			staticObjects_.emplace_back();
		} else {
			index = freeStaticObjectIndices_.back();
			freeStaticObjectIndices_.pop_back();
		}

		StaticObjectSlot& object = staticObjects_[index];
		const uint32_t generation = object.generation;
		object = StaticObjectSlot{
			.shape = desc.shape,
			.userData = desc.userData,
			.generation = generation,
			.isEnabled = desc.enabled,
			.isOccupied = true,
		};
		++staticObjectCount_;
		return { index, generation };
	}

	StaticObjectHandle PhysicsWorld::AddStaticObject(
		const Collision::OBB& box,
		bool enabled,
		uint64_t userData) {
		return AddStaticObject({
			.shape = StaticObjectShape{ box },
			.enabled = enabled,
			.userData = userData,
		});
	}

	bool PhysicsWorld::RemoveStaticObject(StaticObjectHandle handle) {
		if (!FindStaticObject(handle)) {
			return false;
		}
		RemoveStaticObjectAt(handle.index);
		return true;
	}

	void PhysicsWorld::ClearStaticObjects() {
		freeStaticObjectIndices_.clear();
		freeStaticObjectIndices_.reserve(staticObjects_.size());
		for (uint32_t index = 0u; index < staticObjects_.size(); ++index) {
			StaticObjectSlot& object = staticObjects_[index];
			if (object.isOccupied) {
				object.generation = Internal::NextGeneration(object.generation);
			}
			object.isOccupied = false;
			freeStaticObjectIndices_.push_back(index);
		}
		staticObjectCount_ = 0u;
	}

	bool PhysicsWorld::IsValid(StaticObjectHandle handle) const {
		return FindStaticObject(handle) != nullptr;
	}

	std::size_t PhysicsWorld::GetStaticObjectCount() const {
		return staticObjectCount_;
	}

	std::optional<StaticObjectState> PhysicsWorld::GetState(
		StaticObjectHandle handle) const {
		const StaticObjectSlot* object = FindStaticObject(handle);
		if (!object) {
			return std::nullopt;
		}
		return StaticObjectState{
			.handle = handle,
			.shape = object->shape,
			.enabled = object->isEnabled,
			.userData = object->userData,
		};
	}

	void PhysicsWorld::GetStaticObjectStates(
		std::vector<StaticObjectState>& states) const {
		states.clear();
		states.reserve(staticObjectCount_);
		for (uint32_t index = 0u; index < staticObjects_.size(); ++index) {
			const StaticObjectSlot& object = staticObjects_[index];
			if (!object.isOccupied) {
				continue;
			}
			states.push_back({
				.handle = { index, object.generation },
				.shape = object.shape,
				.enabled = object.isEnabled,
				.userData = object.userData,
			});
		}
	}

	DynamicObjectHandle PhysicsWorld::AddDynamicObject(
		const DynamicObjectDesc& desc) {
		if (!IsShapeValid(desc.shape) || !IsFinite(desc.velocity)) {
			return {};
		}

		uint32_t index = 0u;
		if (freeDynamicObjectIndices_.empty()) {
			index = static_cast<uint32_t>(dynamicObjects_.size());
			dynamicObjects_.emplace_back();
		} else {
			index = freeDynamicObjectIndices_.back();
			freeDynamicObjectIndices_.pop_back();
		}

		DynamicObjectSlot& object = dynamicObjects_[index];
		const uint32_t generation = object.generation;
		object = DynamicObjectSlot{
			.shape = desc.shape,
			.velocity = desc.velocity,
			.userData = desc.userData,
			.generation = generation,
			.isEnabled = desc.enabled,
			.isOccupied = true,
		};
		++dynamicObjectCount_;
		return { index, generation };
	}

	DynamicObjectHandle PhysicsWorld::AddDynamicObject(
		const Collision::Sphere& sphere,
		const Vector3& velocity,
		bool enabled,
		uint64_t userData) {
		return AddDynamicObject({
			.shape = DynamicObjectShape{ sphere },
			.velocity = velocity,
			.enabled = enabled,
			.userData = userData,
		});
	}

	bool PhysicsWorld::RemoveDynamicObject(DynamicObjectHandle handle) {
		if (!FindDynamicObject(handle)) {
			return false;
		}
		RemoveDynamicObjectAt(handle.index);
		return true;
	}

	void PhysicsWorld::ClearDynamicObjects() {
		freeDynamicObjectIndices_.clear();
		freeDynamicObjectIndices_.reserve(dynamicObjects_.size());
		for (uint32_t index = 0u; index < dynamicObjects_.size(); ++index) {
			DynamicObjectSlot& object = dynamicObjects_[index];
			if (object.isOccupied) {
				object.generation = Internal::NextGeneration(object.generation);
			}
			object.isOccupied = false;
			freeDynamicObjectIndices_.push_back(index);
		}
		dynamicObjectCount_ = 0u;
	}

	bool PhysicsWorld::IsValid(DynamicObjectHandle handle) const {
		return FindDynamicObject(handle) != nullptr;
	}

	std::size_t PhysicsWorld::GetDynamicObjectCount() const {
		return dynamicObjectCount_;
	}

	std::optional<DynamicObjectState> PhysicsWorld::GetState(
		DynamicObjectHandle handle) const {
		const DynamicObjectSlot* object = FindDynamicObject(handle);
		if (!object) {
			return std::nullopt;
		}
		return DynamicObjectState{
			.handle = handle,
			.shape = object->shape,
			.velocity = object->velocity,
			.enabled = object->isEnabled,
			.userData = object->userData,
		};
	}

	void PhysicsWorld::GetDynamicObjectStates(
		std::vector<DynamicObjectState>& states) const {
		states.clear();
		states.reserve(dynamicObjectCount_);
		for (uint32_t index = 0u; index < dynamicObjects_.size(); ++index) {
			const DynamicObjectSlot& object = dynamicObjects_[index];
			if (!object.isOccupied) {
				continue;
			}
			states.push_back({
				.handle = { index, object.generation },
				.shape = object.shape,
				.velocity = object.velocity,
				.enabled = object.isEnabled,
				.userData = object.userData,
			});
		}
	}

	void PhysicsWorld::SetSettings(const PhysicsWorldSettings& settings) {
		settings_ = NormalizeSettings(settings);
	}

	const PhysicsWorldSettings& PhysicsWorld::GetSettings() const {
		return settings_;
	}

	void PhysicsWorld::Clear() {
		ClearStaticObjects();
		ClearDynamicObjects();
	}

	void PhysicsWorld::Update(float deltaTime) {
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f ||
			dynamicObjectCount_ == 0u) {
			return;
		}

		// 大きすぎる経過時間を抑え、さらにサブステップへ分けて
		// 1回の移動量を小さくすることで床をすり抜けにくくする。
		const float frameTime = std::min(deltaTime, settings_.maximumDeltaTime);
		const float substepTime = frameTime /
			static_cast<float>(settings_.substepCount);
		for (uint32_t substep = 0u; substep < settings_.substepCount; ++substep) {
			IntegrateDynamicObjects(substepTime);
			for (uint32_t iteration = 0u;
				iteration < settings_.solverIterationCount;
				++iteration) {
				SolveStaticContacts();
			}
		}

		CleanupDynamicObjects();
	}

	PhysicsWorld::StaticObjectSlot* PhysicsWorld::FindStaticObject(
		StaticObjectHandle handle) {
		if (!handle.IsValid() || handle.index >= staticObjects_.size()) {
			return nullptr;
		}
		StaticObjectSlot& object = staticObjects_[handle.index];
		return object.isOccupied && object.generation == handle.generation
			? &object
			: nullptr;
	}

	const PhysicsWorld::StaticObjectSlot* PhysicsWorld::FindStaticObject(
		StaticObjectHandle handle) const {
		if (!handle.IsValid() || handle.index >= staticObjects_.size()) {
			return nullptr;
		}
		const StaticObjectSlot& object = staticObjects_[handle.index];
		return object.isOccupied && object.generation == handle.generation
			? &object
			: nullptr;
	}

	void PhysicsWorld::RemoveStaticObjectAt(uint32_t index) {
		StaticObjectSlot& object = staticObjects_[index];
		if (!object.isOccupied) {
			return;
		}
		object.isOccupied = false;
		object.generation = Internal::NextGeneration(object.generation);
		freeStaticObjectIndices_.push_back(index);
		--staticObjectCount_;
	}

	PhysicsWorld::DynamicObjectSlot* PhysicsWorld::FindDynamicObject(
		DynamicObjectHandle handle) {
		if (!handle.IsValid() || handle.index >= dynamicObjects_.size()) {
			return nullptr;
		}
		DynamicObjectSlot& object = dynamicObjects_[handle.index];
		return object.isOccupied && object.generation == handle.generation
			? &object
			: nullptr;
	}

	const PhysicsWorld::DynamicObjectSlot* PhysicsWorld::FindDynamicObject(
		DynamicObjectHandle handle) const {
		if (!handle.IsValid() || handle.index >= dynamicObjects_.size()) {
			return nullptr;
		}
		const DynamicObjectSlot& object = dynamicObjects_[handle.index];
		return object.isOccupied && object.generation == handle.generation
			? &object
			: nullptr;
	}

	void PhysicsWorld::RemoveDynamicObjectAt(uint32_t index) {
		DynamicObjectSlot& object = dynamicObjects_[index];
		if (!object.isOccupied) {
			return;
		}
		object.isOccupied = false;
		object.generation = Internal::NextGeneration(object.generation);
		freeDynamicObjectIndices_.push_back(index);
		--dynamicObjectCount_;
	}

	void PhysicsWorld::IntegrateDynamicObjects(float substepTime) {
		for (DynamicObjectSlot& object : dynamicObjects_) {
			if (!object.isOccupied || !object.isEnabled) {
				continue;
			}

			// 速度を更新してから位置へ反映する半陰的オイラー法を使う。
			object.velocity += settings_.gravity * substepTime;
			MoveShape(object.shape, object.velocity * substepTime);
		}
	}

	void PhysicsWorld::SolveStaticContacts() {
		for (DynamicObjectSlot& dynamicObject : dynamicObjects_) {
			if (!dynamicObject.isOccupied || !dynamicObject.isEnabled) {
				continue;
			}

			for (const StaticObjectSlot& staticObject : staticObjects_) {
				if (!staticObject.isOccupied || !staticObject.isEnabled) {
					continue;
				}

				const std::optional<StaticContact> contact = FindContact(
					dynamicObject.shape,
					staticObject.shape);
				if (!contact || contact->penetrationDepth <= 0.0f) {
					continue;
				}

				// 貫通量を一度で戻し、SphereをOBBの表面へ移動させる。
				MoveShape(
					dynamicObject.shape,
					contact->normal * contact->penetrationDepth);

				// 床へ向かう速度成分だけを取り除く。
				// 接線方向の速度は残すため、斜面ではそのまま滑る。
				const float normalSpeed = Math::Dot(
					dynamicObject.velocity,
					contact->normal);
				if (normalSpeed < 0.0f) {
					dynamicObject.velocity -= contact->normal * normalSpeed;
				}
			}
		}
	}

	void PhysicsWorld::CleanupDynamicObjects() {
		for (uint32_t index = 0u; index < dynamicObjects_.size(); ++index) {
			const DynamicObjectSlot& object = dynamicObjects_[index];
			if (!object.isOccupied) {
				continue;
			}
			if (!Collision::Contains(settings_.worldBounds, GetPosition(object.shape))) {
				RemoveDynamicObjectAt(index);
			}
		}
	}

}
