#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <type_traits>

#include "../Collision/CollisionUtility.h"
#include "LGF/Collision/Collision3D.h"
#include "LGF/Math/Functions/QuaternionMath.h"
#include "LGF/Math/Functions/VectorMath.h"
#include "LGF/Math/MathConstants.h"
#include "LGF/Physics/PhysicsWorld.h"

namespace LGF::Physics::Internal {

	inline bool IsFinite(const Vector3& value) {
		return std::isfinite(value.x) &&
			std::isfinite(value.y) &&
			std::isfinite(value.z);
	}

	inline PhysicsMaterial NormalizeMaterial(const PhysicsMaterial& material) {
		return {
			.restitution = std::isfinite(material.restitution)
				? std::clamp(material.restitution, 0.0f, 1.0f)
				: 0.1f,
			.friction = std::isfinite(material.friction)
				? std::clamp(material.friction, 0.0f, 1.0f)
				: 0.4f,
		};
	}

	inline bool IsStaticObjectShapeValid(const StaticObjectShape& shape) {
		return std::visit([](const auto& value) {
			return Collision::IsValid(value);
			}, shape);
	}

	inline Vector3 Lerp(const Vector3& start, const Vector3& end, float t) {
		return start + (end - start) * t;
	}

	inline StaticObjectShape InterpolateStaticObjectShape(const StaticObjectShape& previous, const StaticObjectShape& current, float t) {
		if (previous.index() != current.index()) {
			return current;
		}

		if (const auto* currentSphere = std::get_if<Collision::Sphere>(&current)) {
			const auto& previousSphere = std::get<Collision::Sphere>(previous);
			return Collision::Sphere{
				.center = Lerp(previousSphere.center, currentSphere->center, t),
				.radius = std::lerp(previousSphere.radius, currentSphere->radius, t),
			};
		}
		if (const auto* currentCap = std::get_if<Collision::SphericalCap>(&current)) {
			const auto& previousCap = std::get<Collision::SphericalCap>(previous);
			Collision::SphericalCap result{
				.apex = Lerp(previousCap.apex, currentCap->apex, t),
				.axis = Lerp(previousCap.axis, currentCap->axis, t),
				.baseRadius = std::lerp(previousCap.baseRadius, currentCap->baseRadius, t),
				.height = std::lerp(previousCap.height, currentCap->height, t),
			};
			return Collision::IsValid(result) ? StaticObjectShape{ result } : current;
		}
		if (const auto* currentShell = std::get_if<Collision::SphericalShellCap>(&current)) {
			const auto& previousShell = std::get<Collision::SphericalShellCap>(previous);
			Collision::SphericalShellCap result{
				.outerApex = Lerp(previousShell.outerApex, currentShell->outerApex, t),
				.axis = Lerp(previousShell.axis, currentShell->axis, t),
				.outerRadius = std::lerp(previousShell.outerRadius, currentShell->outerRadius, t),
				.innerRadius = std::lerp(previousShell.innerRadius, currentShell->innerRadius, t),
				.height = std::lerp(previousShell.height, currentShell->height, t),
			};
			return Collision::IsValid(result) ? StaticObjectShape{ result } : current;
		}
		if (const auto* currentBox = std::get_if<Collision::AABB>(&current)) {
			const auto& previousBox = std::get<Collision::AABB>(previous);
			return Collision::AABB{
				.min = Lerp(previousBox.min, currentBox->min, t),
				.max = Lerp(previousBox.max, currentBox->max, t),
			};
		}
		if (const auto* currentBox = std::get_if<Collision::OBB>(&current)) {
			const auto& previousBox = std::get<Collision::OBB>(previous);
			return Collision::OBB{
				.center = Lerp(previousBox.center, currentBox->center, t),
				.halfExtents = Lerp(previousBox.halfExtents, currentBox->halfExtents, t),
				.rotation = Math::Slerp(previousBox.rotation, currentBox->rotation, t),
			};
		}

		return current;
	}

	inline Vector3 GetStaticObjectReferencePosition(const StaticObjectShape& shape) {
		return std::visit([](const auto& value) -> Vector3 {
			using T = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<T, Collision::Sphere>) {
				return value.center;
			} else if constexpr (std::is_same_v<T, Collision::SphericalCap>) {
				return value.apex;
			} else if constexpr (std::is_same_v<T, Collision::SphericalShellCap>) {
				return value.outerApex;
			} else if constexpr (std::is_same_v<T, Collision::AABB>) {
				return (value.min + value.max) * 0.5f;
			} else {
				return value.center;
			}
			}, shape);
	}

	inline Vector3 CalculateStaticObjectLinearVelocity(const StaticObjectShape& previous, const StaticObjectShape& current, float frameTime) {
		if (previous.index() != current.index() || frameTime <= 0.0f) {
			return {};
		}
		return (
			GetStaticObjectReferencePosition(current) -
			GetStaticObjectReferencePosition(previous)) / frameTime;
	}

	inline Vector3 CalculateStaticObjectAngularVelocity(const StaticObjectShape& previous, const StaticObjectShape& current, float frameTime) {
		if (previous.index() != current.index() || frameTime <= 0.0f) {
			return {};
		}

		const auto* previousBox = std::get_if<Collision::OBB>(&previous);
		const auto* currentBox = std::get_if<Collision::OBB>(&current);
		if (!previousBox || !currentBox) {
			return {};
		}

		Quaternion previousRotation = Math::Normalize(previousBox->rotation);
		Quaternion currentRotation = Math::Normalize(currentBox->rotation);
		if (Math::Dot(previousRotation, currentRotation) < 0.0f) {
			currentRotation = -currentRotation;
		}
		Quaternion deltaRotation = Math::Normalize(currentRotation * Math::Conjugate(previousRotation));
		if (deltaRotation.w < 0.0f) {
			deltaRotation = -deltaRotation;
		}

		const float halfAngle = std::acos(std::clamp(deltaRotation.w, -1.0f, 1.0f));
		const float sinHalfAngle = std::sin(halfAngle);
		if (std::abs(sinHalfAngle) <= Math::EPSILON) {
			return {};
		}

		const Vector3 axis{ deltaRotation.x / sinHalfAngle, deltaRotation.y / sinHalfAngle, deltaRotation.z / sinHalfAngle };
		return axis * (halfAngle * 2.0f / frameTime);
	}

	struct StaticContact {
		Vector3 point{};
		Vector3 normal{};
		float penetrationDepth{};
	};

	inline std::optional<StaticContact> FindContact(const Collision::Sphere& particle, const Collision::Sphere& collider) {
		const Vector3 offset = particle.center - collider.center;
		const float minimumDistance = particle.radius + collider.radius;
		const float distanceSq = Math::LengthSq(offset);
		if (distanceSq > minimumDistance * minimumDistance) {
			return std::nullopt;
		}
		const float distance = std::sqrt(std::max(distanceSq, 0.0f));
		const Vector3 normal = Collision::Internal::SafeNormal(offset, { 0.0f, 1.0f, 0.0f });
		return StaticContact{
			.point = collider.center + normal * collider.radius,
			.normal = normal,
			.penetrationDepth = minimumDistance - distance,
		};
	}

	inline std::optional<StaticContact> FindContact(const Collision::Sphere& particle, const Collision::SphericalCap& collider) {
		if (!Collision::IsValid(particle) || !Collision::IsValid(collider)) {
			return std::nullopt;
		}

		const Vector3 axis = Math::Normalize(collider.axis);
		const float sphereRadius =
			(collider.baseRadius * collider.baseRadius +
				collider.height * collider.height) /
			(2.0f * collider.height);
		const Vector3 sphereCenter = collider.apex + axis * sphereRadius;
		const Vector3 centerOffset = particle.center - sphereCenter;
		const float centerDistance = Math::Length(centerOffset);
		const Vector3 surfaceDirection = Collision::Internal::SafeNormal(centerOffset, -axis);
		const Vector3 surfacePoint = sphereCenter + surfaceDirection * sphereRadius;
		const float surfaceHeight = Math::Dot(surfacePoint - collider.apex, axis);
		if (surfaceHeight < -Math::EPSILON ||
			surfaceHeight > collider.height + Math::EPSILON) {
			return std::nullopt;
		}

		const float penetrationDepth = centerDistance + particle.radius - sphereRadius;
		if (penetrationDepth < 0.0f) {
			return std::nullopt;
		}
		return StaticContact{
			.point = surfacePoint,
			.normal = -surfaceDirection,
			.penetrationDepth = penetrationDepth,
		};
	}

	inline std::optional<StaticContact> FindContact(const Collision::Sphere& particle, const Collision::SphericalShellCap& collider) {
		const std::optional<Collision::Contact> contact = Collision::Collide(particle, collider);
		if (!contact) {
			return std::nullopt;
		}
		const bool centerInsideMaterial = Collision::Contains(collider, particle.center);
		return StaticContact{
			.point = contact->point,
			.normal = centerInsideMaterial ? contact->normal : -contact->normal,
			.penetrationDepth = contact->penetrationDepth,
		};
	}

	inline std::optional<StaticContact> FindContact(const Collision::Sphere& particle, const Collision::AABB& collider) {
		const Vector3 closestPoint = Collision::ClosestPoint(particle.center, collider);
		const Vector3 offset = particle.center - closestPoint;
		const float distanceSq = Math::LengthSq(offset);
		if (distanceSq > particle.radius * particle.radius) {
			return std::nullopt;
		}
		if (distanceSq > Math::EPSILON * Math::EPSILON) {
			const float distance = std::sqrt(distanceSq);
			return StaticContact{
				.point = closestPoint,
				.normal = offset / distance,
				.penetrationDepth = particle.radius - distance,
			};
		}
		if (!Collision::Contains(collider, particle.center)) {
			return std::nullopt;
		}

		const std::array<float, 6> distances{
			particle.center.x - collider.min.x,
			collider.max.x - particle.center.x,
			particle.center.y - collider.min.y,
			collider.max.y - particle.center.y,
			particle.center.z - collider.min.z,
			collider.max.z - particle.center.z,
		};
		const std::array<Vector3, 6> normals{
			Vector3{ -1.0f, 0.0f, 0.0f },
			Vector3{ 1.0f, 0.0f, 0.0f },
			Vector3{ 0.0f, -1.0f, 0.0f },
			Vector3{ 0.0f, 1.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, -1.0f },
			Vector3{ 0.0f, 0.0f, 1.0f },
		};
		const auto nearestDistance = std::min_element(distances.begin(), distances.end());
		const std::size_t normalIndex = static_cast<std::size_t>(nearestDistance - distances.begin());
		return StaticContact{
			.point = particle.center + normals[normalIndex] * *nearestDistance,
			.normal = normals[normalIndex],
			.penetrationDepth = particle.radius + *nearestDistance,
		};
	}

	inline std::optional<StaticContact> FindContact(const Collision::Sphere& particle, const Collision::OBB& collider) {
		const std::optional<Collision::Contact> contact = Collision::Collide(particle, collider);
		if (!contact) {
			return std::nullopt;
		}
		const bool centerInside = Collision::Contains(collider, particle.center);
		return StaticContact{
			.point = contact->point,
			.normal = centerInside ? contact->normal : -contact->normal,
			.penetrationDepth = contact->penetrationDepth,
		};
	}

	inline std::optional<StaticContact> FindContact(const Collision::Sphere& particle, const StaticObjectShape& staticObject) {
		return std::visit([&](const auto& shape) {
			return FindContact(particle, shape);
			}, staticObject);
	}

}
