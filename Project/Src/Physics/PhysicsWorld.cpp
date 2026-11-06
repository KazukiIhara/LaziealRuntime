#include "LGF/Physics/PhysicsWorld.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "../Core/HandleUtility.h"
#include "LGF/Math/Functions/VectorMath.h"
#include "PhysicsWorldUtility.h"

namespace {

	using namespace LGF;

	Physics::PhysicsWorldSettings AdjustSettings(const Physics::PhysicsWorldSettings& settings) {
		Physics::PhysicsWorldSettings result = settings;
		if (!Physics::Internal::IsFinite(result.gravity)) {
			result.gravity = { 0.0f, -9.8f, 0.0f };
		}
		if (!Collision::IsValid(result.worldBounds) ||
			!Physics::Internal::IsFinite(result.worldBounds.min) ||
			!Physics::Internal::IsFinite(result.worldBounds.max)) {
			result.worldBounds = {
				.min = { -100.0f, -100.0f, -100.0f },
				.max = { 100.0f, 100.0f, 100.0f },
			};
		}
		result.substepCount = std::max(result.substepCount, 1u);
		result.solverIterationCount = std::max(result.solverIterationCount, 1u);
		result.linearDamping = std::isfinite(result.linearDamping)
			? std::max(result.linearDamping, 0.0f)
			: 0.1f;
		result.penetrationSlop = std::isfinite(result.penetrationSlop)
			? std::max(result.penetrationSlop, 0.0f)
			: 0.005f;
		result.positionCorrectionRate = std::isfinite(result.positionCorrectionRate)
			? std::clamp(result.positionCorrectionRate, 0.0f, 1.0f)
			: 0.2f;
		result.maximumDepenetrationVelocity =
			std::isfinite(result.maximumDepenetrationVelocity)
			? std::max(result.maximumDepenetrationVelocity, 0.0f)
			: 5.0f;
		result.bounceVelocityThreshold = std::isfinite(result.bounceVelocityThreshold)
			? std::max(result.bounceVelocityThreshold, 0.0f)
			: 0.8f;
		if (!std::isfinite(result.maximumDeltaTime) ||
			result.maximumDeltaTime <= 0.0f) {
			result.maximumDeltaTime = 1.0f / 15.0f;
		}
		return result;
	}

	bool IsDynamicObjectShapeValid(const Physics::DynamicObjectShape& shape) {
		return std::visit([](const auto& value) {
			return Collision::IsValid(value);
			}, shape);
	}

	bool IsDynamicObjectMassValid(float mass) {
		return std::isfinite(mass) && mass >= std::numeric_limits<float>::min();
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

}

namespace LGF::Physics {

	bool StaticObjectHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	bool KinematicObjectHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	bool DynamicObjectHandle::IsValid() const {
		return index != InvalidIndex && generation != 0u;
	}

	PhysicsWorld::PhysicsWorld(const PhysicsWorldSettings& settings) :
		settings_(AdjustSettings(settings)) {
	}

	StaticObjectHandle PhysicsWorld::AddStaticObject(const StaticObjectDesc& desc) {
		if (!Internal::IsStaticObjectShapeValid(desc.shape)) {
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
		const StaticObjectOptions options{
			.material = Internal::NormalizeMaterial(desc.options.material),
			.objectHandle = desc.options.objectHandle,
			.enabled = desc.options.enabled,
		};
		object = StaticObjectSlot{
			.shape = desc.shape,
			.options = options,
			.generation = generation,
			.isOccupied = true,
		};
		++staticObjectCount_;
		return { index, generation };
	}

	StaticObjectHandle PhysicsWorld::AddStaticObject(const StaticObjectShape& shape, const StaticObjectOptions& options) {
		return AddStaticObject({
			.shape = shape,
			.options = options,
		});
	}

	StaticObjectHandle PhysicsWorld::AddStaticObject(const Collision::Sphere& sphere, const StaticObjectOptions& options) {
		return AddStaticObject(StaticObjectShape{ sphere }, options);
	}

	StaticObjectHandle PhysicsWorld::AddStaticObject(const Collision::SphericalCap& sphericalCap, const StaticObjectOptions& options) {
		return AddStaticObject(StaticObjectShape{ sphericalCap }, options);
	}

	StaticObjectHandle PhysicsWorld::AddStaticObject(const Collision::SphericalShellCap& sphericalShellCap, const StaticObjectOptions& options) {
		return AddStaticObject(StaticObjectShape{ sphericalShellCap }, options);
	}

	StaticObjectHandle PhysicsWorld::AddStaticObject(const Collision::AABB& box, const StaticObjectOptions& options) {
		return AddStaticObject(StaticObjectShape{ box }, options);
	}

	StaticObjectHandle PhysicsWorld::AddStaticObject(const Collision::OBB& box, const StaticObjectOptions& options) {
		return AddStaticObject(StaticObjectShape{ box }, options);
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
				object.generation = LGF::Internal::NextGeneration(object.generation);
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

	bool PhysicsWorld::SetStaticObjectShape(StaticObjectHandle handle, const StaticObjectShape& shape) {
		StaticObjectSlot* object = FindStaticObject(handle);
		if (!object || !Internal::IsStaticObjectShapeValid(shape)) {
			return false;
		}
		object->shape = shape;
		return true;
	}

	bool PhysicsWorld::SetStaticObjectOptions(StaticObjectHandle handle, const StaticObjectOptions& options) {
		StaticObjectSlot* object = FindStaticObject(handle);
		if (!object) {
			return false;
		}
		object->options = {
			.material = Internal::NormalizeMaterial(options.material),
			.objectHandle = options.objectHandle,
			.enabled = options.enabled,
		};
		return true;
	}

	bool PhysicsWorld::SetObjectHandle(StaticObjectHandle handle, ObjectHandle objectHandle) {
		StaticObjectSlot* object = FindStaticObject(handle);
		if (!object) {
			return false;
		}
		object->options.objectHandle = objectHandle;
		return true;
	}

	std::optional<StaticObjectState> PhysicsWorld::GetState(StaticObjectHandle handle) const {
		const StaticObjectSlot* object = FindStaticObject(handle);
		if (!object) {
			return std::nullopt;
		}
		return StaticObjectState{
			.handle = handle,
			.shape = object->shape,
			.options = object->options,
		};
	}

	void PhysicsWorld::GetStaticObjectStates(std::vector<StaticObjectState>& states) const {
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
				.options = object.options,
			});
		}
	}

	KinematicObjectHandle PhysicsWorld::AddKinematicObject(const KinematicObjectDesc& desc) {
		if (!Internal::IsStaticObjectShapeValid(desc.shape)) {
			return {};
		}

		uint32_t index = 0u;
		if (freeKinematicObjectIndices_.empty()) {
			index = static_cast<uint32_t>(kinematicObjects_.size());
			kinematicObjects_.emplace_back();
		} else {
			index = freeKinematicObjectIndices_.back();
			freeKinematicObjectIndices_.pop_back();
		}

		KinematicObjectSlot& object = kinematicObjects_[index];
		const uint32_t generation = object.generation;
		const KinematicObjectOptions options{
			.material = Internal::NormalizeMaterial(desc.options.material),
			.objectHandle = desc.options.objectHandle,
			.enabled = desc.options.enabled,
		};
		object = KinematicObjectSlot{
			.shape = desc.shape,
			.previousShape = desc.shape,
			.options = options,
			.generation = generation,
			.isOccupied = true,
		};
		++kinematicObjectCount_;
		return { index, generation };
	}

	KinematicObjectHandle PhysicsWorld::AddKinematicObject(const KinematicObjectShape& shape, const KinematicObjectOptions& options) {
		return AddKinematicObject({
			.shape = shape,
			.options = options,
		});
	}

	KinematicObjectHandle PhysicsWorld::AddKinematicObject(const Collision::Sphere& sphere, const KinematicObjectOptions& options) {
		return AddKinematicObject(KinematicObjectShape{ sphere }, options);
	}

	KinematicObjectHandle PhysicsWorld::AddKinematicObject(const Collision::SphericalCap& sphericalCap, const KinematicObjectOptions& options) {
		return AddKinematicObject(KinematicObjectShape{ sphericalCap }, options);
	}

	KinematicObjectHandle PhysicsWorld::AddKinematicObject(const Collision::SphericalShellCap& sphericalShellCap, const KinematicObjectOptions& options) {
		return AddKinematicObject(KinematicObjectShape{ sphericalShellCap }, options);
	}

	KinematicObjectHandle PhysicsWorld::AddKinematicObject(const Collision::AABB& box, const KinematicObjectOptions& options) {
		return AddKinematicObject(KinematicObjectShape{ box }, options);
	}

	KinematicObjectHandle PhysicsWorld::AddKinematicObject(const Collision::OBB& box, const KinematicObjectOptions& options) {
		return AddKinematicObject(KinematicObjectShape{ box }, options);
	}

	bool PhysicsWorld::RemoveKinematicObject(KinematicObjectHandle handle) {
		if (!FindKinematicObject(handle)) {
			return false;
		}
		RemoveKinematicObjectAt(handle.index);
		return true;
	}

	void PhysicsWorld::ClearKinematicObjects() {
		freeKinematicObjectIndices_.clear();
		freeKinematicObjectIndices_.reserve(kinematicObjects_.size());
		for (uint32_t index = 0u; index < kinematicObjects_.size(); ++index) {
			KinematicObjectSlot& object = kinematicObjects_[index];
			if (object.isOccupied) {
				object.generation = LGF::Internal::NextGeneration(object.generation);
			}
			object.isOccupied = false;
			freeKinematicObjectIndices_.push_back(index);
		}
		kinematicObjectCount_ = 0u;
	}

	bool PhysicsWorld::IsValid(KinematicObjectHandle handle) const {
		return FindKinematicObject(handle) != nullptr;
	}

	std::size_t PhysicsWorld::GetKinematicObjectCount() const {
		return kinematicObjectCount_;
	}

	bool PhysicsWorld::SetKinematicObjectShape(KinematicObjectHandle handle, const KinematicObjectShape& shape) {
		KinematicObjectSlot* object = FindKinematicObject(handle);
		if (!object || !Internal::IsStaticObjectShapeValid(shape)) {
			return false;
		}
		object->shape = shape;
		return true;
	}

	bool PhysicsWorld::SetKinematicObjectOptions(KinematicObjectHandle handle, const KinematicObjectOptions& options) {
		KinematicObjectSlot* object = FindKinematicObject(handle);
		if (!object) {
			return false;
		}
		object->options = {
			.material = Internal::NormalizeMaterial(options.material),
			.objectHandle = options.objectHandle,
			.enabled = options.enabled,
		};
		return true;
	}

	bool PhysicsWorld::SetObjectHandle(KinematicObjectHandle handle, ObjectHandle objectHandle) {
		KinematicObjectSlot* object = FindKinematicObject(handle);
		if (!object) {
			return false;
		}
		object->options.objectHandle = objectHandle;
		return true;
	}

	std::optional<KinematicObjectState> PhysicsWorld::GetState(KinematicObjectHandle handle) const {
		const KinematicObjectSlot* object = FindKinematicObject(handle);
		if (!object) {
			return std::nullopt;
		}
		return KinematicObjectState{
			.handle = handle,
			.shape = object->shape,
			.linearVelocity = object->linearVelocity,
			.angularVelocity = object->angularVelocity,
			.options = object->options,
		};
	}

	void PhysicsWorld::GetKinematicObjectStates(std::vector<KinematicObjectState>& states) const {
		states.clear();
		states.reserve(kinematicObjectCount_);
		for (uint32_t index = 0u; index < kinematicObjects_.size(); ++index) {
			const KinematicObjectSlot& object = kinematicObjects_[index];
			if (!object.isOccupied) {
				continue;
			}
			states.push_back({
				.handle = { index, object.generation },
				.shape = object.shape,
				.linearVelocity = object.linearVelocity,
				.angularVelocity = object.angularVelocity,
				.options = object.options,
			});
		}
	}

	DynamicObjectHandle PhysicsWorld::AddDynamicObject(const DynamicObjectDesc& desc) {
		if (!IsDynamicObjectShapeValid(desc.shape) ||
			!Internal::IsFinite(desc.velocity) ||
			!IsDynamicObjectMassValid(desc.mass)) {
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
			.inverseMass = 1.0f / desc.mass,
			.material = Internal::NormalizeMaterial(desc.material),
			.objectHandle = desc.objectHandle,
			.generation = generation,
			.isEnabled = desc.enabled,
			.isOccupied = true,
		};
		++dynamicObjectCount_;
		return { index, generation };
	}

	DynamicObjectHandle PhysicsWorld::AddDynamicObject(const Collision::Sphere& sphere, const Vector3& velocity, bool enabled, ObjectHandle objectHandle) {
		return AddDynamicObject({
			.shape = DynamicObjectShape{ sphere },
			.velocity = velocity,
			.objectHandle = objectHandle,
			.enabled = enabled,
		});
	}

	bool PhysicsWorld::SetVelocity(DynamicObjectHandle handle, const Vector3& velocity) {
		DynamicObjectSlot* object = FindDynamicObject(handle);
		if (!object || !Internal::IsFinite(velocity)) {
			return false;
		}
		object->velocity = velocity;
		return true;
	}

	bool PhysicsWorld::SetMass(DynamicObjectHandle handle, float mass) {
		DynamicObjectSlot* object = FindDynamicObject(handle);
		if (!object || !IsDynamicObjectMassValid(mass)) {
			return false;
		}
		object->inverseMass = 1.0f / mass;
		return true;
	}

	bool PhysicsWorld::SetObjectHandle(DynamicObjectHandle handle, ObjectHandle objectHandle) {
		DynamicObjectSlot* object = FindDynamicObject(handle);
		if (!object) {
			return false;
		}
		object->objectHandle = objectHandle;
		return true;
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
				object.generation = LGF::Internal::NextGeneration(object.generation);
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

	std::optional<DynamicObjectState> PhysicsWorld::GetState(DynamicObjectHandle handle) const {
		const DynamicObjectSlot* object = FindDynamicObject(handle);
		if (!object) {
			return std::nullopt;
		}
		return DynamicObjectState{
			.handle = handle,
			.shape = object->shape,
			.velocity = object->velocity,
			.mass = 1.0f / object->inverseMass,
			.material = object->material,
			.objectHandle = object->objectHandle,
			.enabled = object->isEnabled,
		};
	}

	void PhysicsWorld::GetDynamicObjectStates(std::vector<DynamicObjectState>& states) const {
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
				.mass = 1.0f / object.inverseMass,
				.material = object.material,
				.objectHandle = object.objectHandle,
				.enabled = object.isEnabled,
			});
		}
	}

	void PhysicsWorld::SetSettings(const PhysicsWorldSettings& settings) {
		settings_ = AdjustSettings(settings);
	}

	const PhysicsWorldSettings& PhysicsWorld::GetSettings() const {
		return settings_;
	}

	void PhysicsWorld::Clear() {
		ClearStaticObjects();
		ClearKinematicObjects();
		ClearDynamicObjects();
		ClearParticles();
	}

	void PhysicsWorld::Update(float deltaTime) {
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
			return;
		}

		const float frameTime = std::min(deltaTime, settings_.maximumDeltaTime);
		UpdateKinematicObjectVelocities(frameTime);
		UpdateParticles(frameTime);

		if (dynamicObjectCount_ > 0u) {
			const float substepTime = frameTime / static_cast<float>(settings_.substepCount);
			for (uint32_t substep = 0u; substep < settings_.substepCount; ++substep) {
				const float interpolation = static_cast<float>(substep + 1u) / static_cast<float>(settings_.substepCount);
				const std::vector<CollisionSurface> collisionSurfaces = BuildCollisionSurfaces(interpolation);
				IntegrateDynamicObjects(substepTime);
				for (uint32_t iteration = 0u; iteration < settings_.solverIterationCount; ++iteration) {
					SolveObjectContacts(collisionSurfaces);
					SolveDynamicObjectContacts();
				}
			}
			CleanupDynamicObjects();
		}

		UpdateKinematicObjectHistory();
	}

	PhysicsWorld::StaticObjectSlot* PhysicsWorld::FindStaticObject(StaticObjectHandle handle) {
		if (!handle.IsValid() || handle.index >= staticObjects_.size()) {
			return nullptr;
		}
		StaticObjectSlot& object = staticObjects_[handle.index];
		return object.isOccupied && object.generation == handle.generation
			? &object
			: nullptr;
	}

	const PhysicsWorld::StaticObjectSlot* PhysicsWorld::FindStaticObject(StaticObjectHandle handle) const {
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
		object.generation = LGF::Internal::NextGeneration(object.generation);
		freeStaticObjectIndices_.push_back(index);
		--staticObjectCount_;
	}

	PhysicsWorld::KinematicObjectSlot* PhysicsWorld::FindKinematicObject(KinematicObjectHandle handle) {
		if (!handle.IsValid() || handle.index >= kinematicObjects_.size()) {
			return nullptr;
		}
		KinematicObjectSlot& object = kinematicObjects_[handle.index];
		return object.isOccupied && object.generation == handle.generation
			? &object
			: nullptr;
	}

	const PhysicsWorld::KinematicObjectSlot* PhysicsWorld::FindKinematicObject(KinematicObjectHandle handle) const {
		if (!handle.IsValid() || handle.index >= kinematicObjects_.size()) {
			return nullptr;
		}
		const KinematicObjectSlot& object = kinematicObjects_[handle.index];
		return object.isOccupied && object.generation == handle.generation
			? &object
			: nullptr;
	}

	void PhysicsWorld::RemoveKinematicObjectAt(uint32_t index) {
		KinematicObjectSlot& object = kinematicObjects_[index];
		if (!object.isOccupied) {
			return;
		}
		object.isOccupied = false;
		object.generation = LGF::Internal::NextGeneration(object.generation);
		freeKinematicObjectIndices_.push_back(index);
		--kinematicObjectCount_;
	}

	void PhysicsWorld::UpdateKinematicObjectVelocities(float frameTime) {
		for (KinematicObjectSlot& object : kinematicObjects_) {
			if (!object.isOccupied || !object.options.enabled) {
				object.linearVelocity = {};
				object.angularVelocity = {};
				continue;
			}
			object.linearVelocity = Internal::CalculateStaticObjectLinearVelocity(object.previousShape, object.shape, frameTime);
			object.angularVelocity = Internal::CalculateStaticObjectAngularVelocity(object.previousShape, object.shape, frameTime);
		}
	}

	void PhysicsWorld::UpdateKinematicObjectHistory() {
		for (KinematicObjectSlot& object : kinematicObjects_) {
			if (object.isOccupied) {
				object.previousShape = object.shape;
			}
		}
	}

	std::vector<PhysicsWorld::CollisionSurface> PhysicsWorld::BuildCollisionSurfaces(float interpolation) const {
		std::vector<CollisionSurface> result;
		result.reserve(staticObjectCount_ + kinematicObjectCount_);

		for (const StaticObjectSlot& object : staticObjects_) {
			if (!object.isOccupied || !object.options.enabled) {
				continue;
			}
			result.push_back({
				.shape = object.shape,
				.referencePosition = Internal::GetStaticObjectReferencePosition(object.shape),
				.material = object.options.material,
			});
		}
		for (const KinematicObjectSlot& object : kinematicObjects_) {
			if (!object.isOccupied || !object.options.enabled) {
				continue;
			}
			const KinematicObjectShape shape = Internal::InterpolateStaticObjectShape(object.previousShape, object.shape, interpolation);
			result.push_back({
				.shape = shape,
				.referencePosition = Internal::GetStaticObjectReferencePosition(shape),
				.linearVelocity = object.linearVelocity,
				.angularVelocity = object.angularVelocity,
				.material = object.options.material,
			});
		}
		return result;
	}

	PhysicsWorld::DynamicObjectSlot* PhysicsWorld::FindDynamicObject(DynamicObjectHandle handle) {
		if (!handle.IsValid() || handle.index >= dynamicObjects_.size()) {
			return nullptr;
		}
		DynamicObjectSlot& object = dynamicObjects_[handle.index];
		return object.isOccupied && object.generation == handle.generation
			? &object
			: nullptr;
	}

	const PhysicsWorld::DynamicObjectSlot* PhysicsWorld::FindDynamicObject(DynamicObjectHandle handle) const {
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
		object.generation = LGF::Internal::NextGeneration(object.generation);
		freeDynamicObjectIndices_.push_back(index);
		--dynamicObjectCount_;
	}

	void PhysicsWorld::IntegrateDynamicObjects(float substepTime) {
		for (DynamicObjectSlot& object : dynamicObjects_) {
			if (!object.isOccupied || !object.isEnabled) {
				continue;
			}
			object.velocity += settings_.gravity * substepTime;
			MoveShape(object.shape, object.velocity * substepTime);
		}
	}

	void PhysicsWorld::SolveObjectContacts(const std::vector<CollisionSurface>& collisionSurfaces) {
		for (DynamicObjectSlot& dynamicObject : dynamicObjects_) {
			if (!dynamicObject.isOccupied || !dynamicObject.isEnabled) {
				continue;
			}

			const auto& sphere = std::get<Collision::Sphere>(dynamicObject.shape);
			for (const CollisionSurface& collisionSurface : collisionSurfaces) {
				const std::optional<Internal::StaticContact> contact = Internal::FindContact(sphere, collisionSurface.shape);
				if (!contact || contact->penetrationDepth <= 0.0f) {
					continue;
				}

				MoveShape(dynamicObject.shape, contact->normal * contact->penetrationDepth);
				const Vector3 surfaceVelocity = collisionSurface.linearVelocity + Math::Cross(collisionSurface.angularVelocity, contact->point - collisionSurface.referencePosition);
				const Vector3 relativeVelocity = dynamicObject.velocity - surfaceVelocity;
				const float normalSpeed = Math::Dot(relativeVelocity, contact->normal);
				if (normalSpeed < 0.0f) {
					const float restitution = -normalSpeed > settings_.bounceVelocityThreshold ? std::max(dynamicObject.material.restitution, collisionSurface.material.restitution) : 0.0f;
					const float normalImpulseMagnitude = -(1.0f + restitution) * normalSpeed;
					Vector3 resolvedRelativeVelocity = relativeVelocity + contact->normal * normalImpulseMagnitude;
					const Vector3 tangentVelocity = relativeVelocity - contact->normal * normalSpeed;
					const float tangentSpeed = Math::Length(tangentVelocity);
					if (tangentSpeed > Math::EPSILON) {
						const float friction = std::sqrt(dynamicObject.material.friction * collisionSurface.material.friction);
						const float frictionImpulseMagnitude = std::min(tangentSpeed, friction * normalImpulseMagnitude);
						resolvedRelativeVelocity -= tangentVelocity / tangentSpeed * frictionImpulseMagnitude;
					}
					dynamicObject.velocity = surfaceVelocity + resolvedRelativeVelocity;
				}
			}
		}
	}

	void PhysicsWorld::SolveDynamicObjectContacts() {
		for (uint32_t firstIndex = 0u; firstIndex < dynamicObjects_.size(); ++firstIndex) {
			DynamicObjectSlot& firstObject = dynamicObjects_[firstIndex];
			if (!firstObject.isOccupied || !firstObject.isEnabled) {
				continue;
			}

			for (uint32_t secondIndex = firstIndex + 1u; secondIndex < dynamicObjects_.size(); ++secondIndex) {
				DynamicObjectSlot& secondObject = dynamicObjects_[secondIndex];
				if (!secondObject.isOccupied || !secondObject.isEnabled) {
					continue;
				}

				const auto& firstSphere = std::get<Collision::Sphere>(firstObject.shape);
				const auto& secondSphere = std::get<Collision::Sphere>(secondObject.shape);
				const std::optional<Internal::StaticContact> contact = Internal::FindContact(firstSphere, secondSphere);
				if (!contact || contact->penetrationDepth <= 0.0f) {
					continue;
				}

				const float inverseMassSum = firstObject.inverseMass + secondObject.inverseMass;
				const Vector3 correction = contact->normal * (contact->penetrationDepth / inverseMassSum);
				MoveShape(firstObject.shape, correction * firstObject.inverseMass);
				MoveShape(secondObject.shape, -correction * secondObject.inverseMass);

				const Vector3 relativeVelocity = firstObject.velocity - secondObject.velocity;
				const float normalSpeed = Math::Dot(relativeVelocity, contact->normal);
				if (normalSpeed >= 0.0f) {
					continue;
				}

				const float restitution = -normalSpeed > settings_.bounceVelocityThreshold ? std::max(firstObject.material.restitution, secondObject.material.restitution) : 0.0f;
				const float normalImpulseMagnitude = -(1.0f + restitution) * normalSpeed / inverseMassSum;
				const Vector3 normalImpulse = contact->normal * normalImpulseMagnitude;
				firstObject.velocity += normalImpulse * firstObject.inverseMass;
				secondObject.velocity -= normalImpulse * secondObject.inverseMass;

				const Vector3 tangentVelocity = relativeVelocity - contact->normal * normalSpeed;
				const float tangentSpeed = Math::Length(tangentVelocity);
				if (tangentSpeed <= Math::EPSILON) {
					continue;
				}

				const float friction = std::sqrt(firstObject.material.friction * secondObject.material.friction);
				const float frictionImpulseMagnitude = std::min(tangentSpeed / inverseMassSum, friction * normalImpulseMagnitude);
				const Vector3 frictionImpulse = tangentVelocity / tangentSpeed * frictionImpulseMagnitude;
				firstObject.velocity -= frictionImpulse * firstObject.inverseMass;
				secondObject.velocity += frictionImpulse * secondObject.inverseMass;
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
