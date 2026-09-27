#include "LGF/Collision/Collision3D.h"

#include "LGF/Math/Functions/VectorMath.h"
#include "LGF/Math/MathConstants.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <iterator>

namespace {

	using namespace LGF;

	Vector3 Reverse(const Vector3& value) {
		return { -value.x, -value.y, -value.z };
	}

	Vector3 SafeNormal(const Vector3& value, const Vector3& fallback) {
		const float lengthSq = Math::LengthSq(value);
		if (lengthSq <= Math::EPSILON * Math::EPSILON) {
			return fallback;
		}
		return value / std::sqrt(lengthSq);
	}

	LGF::Collision::Contact ReverseContact(const LGF::Collision::Contact& contact) {
		return {
			.point = contact.point,
			.normal = Reverse(contact.normal),
			.penetrationDepth = contact.penetrationDepth,
		};
	}

}

namespace LGF::Collision {

	bool IsValid(const Sphere& sphere) {
		return sphere.radius >= 0.0f;
	}

	bool IsValid(const AABB& box) {
		return box.min.x <= box.max.x &&
			box.min.y <= box.max.y &&
			box.min.z <= box.max.z;
	}

	bool IsValid(const Capsule& capsule) {
		return capsule.radius >= 0.0f;
	}

	bool IsValid(const Ray& ray) {
		return Math::LengthSq(ray.direction) > Math::EPSILON * Math::EPSILON;
	}

	bool IsValid(const Plane& plane) {
		return Math::LengthSq(plane.normal) > Math::EPSILON * Math::EPSILON;
	}

	Vector3 ClosestPoint(const Vector3& point, const AABB& box) {
		return {
			std::clamp(point.x, box.min.x, box.max.x),
			std::clamp(point.y, box.min.y, box.max.y),
			std::clamp(point.z, box.min.z, box.max.z),
		};
	}

	Vector3 ClosestPoint(const Vector3& point, const Capsule& capsule) {
		const Vector3 segment = capsule.end - capsule.start;
		const float lengthSq = Math::LengthSq(segment);
		if (lengthSq <= Math::EPSILON * Math::EPSILON) {
			return capsule.start;
		}

		const float t = std::clamp(
			Math::Dot(point - capsule.start, segment) / lengthSq,
			0.0f,
			1.0f);
		return capsule.start + segment * t;
	}

	bool Contains(const Sphere& sphere, const Vector3& point) {
		return IsValid(sphere) &&
			Math::DistanceSq(sphere.center, point) <= sphere.radius * sphere.radius;
	}

	bool Contains(const AABB& box, const Vector3& point) {
		return IsValid(box) &&
			point.x >= box.min.x && point.x <= box.max.x &&
			point.y >= box.min.y && point.y <= box.max.y &&
			point.z >= box.min.z && point.z <= box.max.z;
	}

	bool Contains(const Capsule& capsule, const Vector3& point) {
		if (!IsValid(capsule)) {
			return false;
		}
		const Vector3 closest = ClosestPoint(point, capsule);
		return Math::DistanceSq(closest, point) <= capsule.radius * capsule.radius;
	}

	std::optional<Contact> Collide(const Sphere& first, const Sphere& second) {
		if (!IsValid(first) || !IsValid(second)) {
			return std::nullopt;
		}

		const Vector3 delta = second.center - first.center;
		const float radiusSum = first.radius + second.radius;
		const float distanceSq = Math::LengthSq(delta);
		if (distanceSq > radiusSum * radiusSum) {
			return std::nullopt;
		}

		const float distance = std::sqrt(distanceSq);
		const Vector3 normal = SafeNormal(delta, { 1.0f, 0.0f, 0.0f });
		const float penetrationDepth = radiusSum - distance;
		return Contact{
			.point = first.center + normal * (first.radius - penetrationDepth * 0.5f),
			.normal = normal,
			.penetrationDepth = penetrationDepth,
		};
	}

	std::optional<Contact> Collide(const Sphere& sphere, const AABB& box) {
		if (!IsValid(sphere) || !IsValid(box)) {
			return std::nullopt;
		}

		const Vector3 closest = ClosestPoint(sphere.center, box);
		const Vector3 delta = closest - sphere.center;
		const float distanceSq = Math::LengthSq(delta);
		if (distanceSq > sphere.radius * sphere.radius) {
			return std::nullopt;
		}

		if (distanceSq > Math::EPSILON * Math::EPSILON) {
			const float distance = std::sqrt(distanceSq);
			return Contact{
				.point = closest,
				.normal = delta / distance,
				.penetrationDepth = sphere.radius - distance,
			};
		}

		const float distances[6]{
			sphere.center.x - box.min.x,
			box.max.x - sphere.center.x,
			sphere.center.y - box.min.y,
			box.max.y - sphere.center.y,
			sphere.center.z - box.min.z,
			box.max.z - sphere.center.z,
		};
		const Vector3 normals[6]{
			{ -1.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, -1.0f },
			{ 0.0f, 0.0f, 1.0f },
		};

		const auto nearest = std::min_element(std::begin(distances), std::end(distances));
		const std::size_t index = static_cast<std::size_t>(nearest - std::begin(distances));
		return Contact{
			.point = sphere.center + normals[index] * *nearest,
			.normal = normals[index],
			.penetrationDepth = sphere.radius + *nearest,
		};
	}

	std::optional<Contact> Collide(const AABB& box, const Sphere& sphere) {
		const std::optional<Contact> contact = Collide(sphere, box);
		return contact ? std::optional<Contact>(ReverseContact(*contact)) : std::nullopt;
	}

	std::optional<Contact> Collide(const AABB& first, const AABB& second) {
		if (!IsValid(first) || !IsValid(second)) {
			return std::nullopt;
		}

		const Vector3 overlap{
			std::min(first.max.x, second.max.x) - std::max(first.min.x, second.min.x),
			std::min(first.max.y, second.max.y) - std::max(first.min.y, second.min.y),
			std::min(first.max.z, second.max.z) - std::max(first.min.z, second.min.z),
		};
		if (overlap.x < 0.0f || overlap.y < 0.0f || overlap.z < 0.0f) {
			return std::nullopt;
		}

		const Vector3 firstCenter = (first.min + first.max) * 0.5f;
		const Vector3 secondCenter = (second.min + second.max) * 0.5f;
		Vector3 normal{};
		float penetrationDepth = overlap.x;
		normal.x = secondCenter.x >= firstCenter.x ? 1.0f : -1.0f;

		if (overlap.y < penetrationDepth) {
			penetrationDepth = overlap.y;
			normal = { 0.0f, secondCenter.y >= firstCenter.y ? 1.0f : -1.0f, 0.0f };
		}
		if (overlap.z < penetrationDepth) {
			penetrationDepth = overlap.z;
			normal = { 0.0f, 0.0f, secondCenter.z >= firstCenter.z ? 1.0f : -1.0f };
		}

		return Contact{
			.point = {
				(std::max(first.min.x, second.min.x) + std::min(first.max.x, second.max.x)) * 0.5f,
				(std::max(first.min.y, second.min.y) + std::min(first.max.y, second.max.y)) * 0.5f,
				(std::max(first.min.z, second.min.z) + std::min(first.max.z, second.max.z)) * 0.5f,
			},
			.normal = normal,
			.penetrationDepth = penetrationDepth,
		};
	}

	std::optional<Contact> Collide(const Capsule& capsule, const Sphere& sphere) {
		if (!IsValid(capsule) || !IsValid(sphere)) {
			return std::nullopt;
		}

		const Vector3 closest = ClosestPoint(sphere.center, capsule);
		const Vector3 delta = sphere.center - closest;
		const float radiusSum = capsule.radius + sphere.radius;
		const float distanceSq = Math::LengthSq(delta);
		if (distanceSq > radiusSum * radiusSum) {
			return std::nullopt;
		}

		const float distance = std::sqrt(distanceSq);
		const Vector3 normal = SafeNormal(delta, { 1.0f, 0.0f, 0.0f });
		const float penetrationDepth = radiusSum - distance;
		return Contact{
			.point = closest + normal * (capsule.radius - penetrationDepth * 0.5f),
			.normal = normal,
			.penetrationDepth = penetrationDepth,
		};
	}

	std::optional<Contact> Collide(const Sphere& sphere, const Capsule& capsule) {
		const std::optional<Contact> contact = Collide(capsule, sphere);
		return contact ? std::optional<Contact>(ReverseContact(*contact)) : std::nullopt;
	}

	bool Intersects(const Sphere& first, const Sphere& second) {
		return Collide(first, second).has_value();
	}

	bool Intersects(const Sphere& sphere, const AABB& box) {
		return Collide(sphere, box).has_value();
	}

	bool Intersects(const AABB& box, const Sphere& sphere) {
		return Intersects(sphere, box);
	}

	bool Intersects(const AABB& first, const AABB& second) {
		return Collide(first, second).has_value();
	}

	bool Intersects(const Capsule& capsule, const Sphere& sphere) {
		return Collide(capsule, sphere).has_value();
	}

	bool Intersects(const Sphere& sphere, const Capsule& capsule) {
		return Intersects(capsule, sphere);
	}

	std::optional<RaycastHit> Raycast(
		const Ray& ray,
		const Sphere& sphere,
		float maxDistance) {
		if (!IsValid(ray) || !IsValid(sphere) || maxDistance < 0.0f) {
			return std::nullopt;
		}

		const Vector3 direction = Math::Normalize(ray.direction);
		const Vector3 offset = ray.origin - sphere.center;
		if (Math::LengthSq(offset) <= sphere.radius * sphere.radius) {
			return RaycastHit{
				.point = ray.origin,
				.normal = SafeNormal(offset, Reverse(direction)),
				.distance = 0.0f,
			};
		}

		const float projected = Math::Dot(offset, direction);
		const float discriminant = projected * projected -
			(Math::LengthSq(offset) - sphere.radius * sphere.radius);
		if (discriminant < 0.0f) {
			return std::nullopt;
		}

		const float distance = -projected - std::sqrt(discriminant);
		if (distance < 0.0f || distance > maxDistance) {
			return std::nullopt;
		}

		const Vector3 point = ray.origin + direction * distance;
		return RaycastHit{
			.point = point,
			.normal = SafeNormal(point - sphere.center, Reverse(direction)),
			.distance = distance,
		};
	}

	std::optional<RaycastHit> Raycast(
		const Ray& ray,
		const AABB& box,
		float maxDistance) {
		if (!IsValid(ray) || !IsValid(box) || maxDistance < 0.0f) {
			return std::nullopt;
		}

		const Vector3 direction = Math::Normalize(ray.direction);
		if (Contains(box, ray.origin)) {
			return RaycastHit{
				.point = ray.origin,
				.normal = Reverse(direction),
				.distance = 0.0f,
			};
		}

		float nearDistance = 0.0f;
		float farDistance = maxDistance;
		Vector3 nearNormal{};
		const auto updateSlab = [&](float origin, float rayDirection, float minimum,
			float maximum, const Vector3& minimumNormal, const Vector3& maximumNormal) {
			if (std::abs(rayDirection) <= Math::EPSILON) {
				return origin >= minimum && origin <= maximum;
			}

			float firstDistance = (minimum - origin) / rayDirection;
			float secondDistance = (maximum - origin) / rayDirection;
			Vector3 firstNormal = minimumNormal;
			Vector3 secondNormal = maximumNormal;
			if (firstDistance > secondDistance) {
				std::swap(firstDistance, secondDistance);
				std::swap(firstNormal, secondNormal);
			}

			if (firstDistance > nearDistance) {
				nearDistance = firstDistance;
				nearNormal = firstNormal;
			}
			farDistance = std::min(farDistance, secondDistance);
			return nearDistance <= farDistance;
		};

		if (!updateSlab(ray.origin.x, direction.x, box.min.x, box.max.x,
			{ -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }) ||
			!updateSlab(ray.origin.y, direction.y, box.min.y, box.max.y,
				{ 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }) ||
			!updateSlab(ray.origin.z, direction.z, box.min.z, box.max.z,
				{ 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f })) {
			return std::nullopt;
		}

		return RaycastHit{
			.point = ray.origin + direction * nearDistance,
			.normal = nearNormal,
			.distance = nearDistance,
		};
	}

	std::optional<RaycastHit> Raycast(
		const Ray& ray,
		const Plane& plane,
		float maxDistance) {
		if (!IsValid(ray) || !IsValid(plane) || maxDistance < 0.0f) {
			return std::nullopt;
		}

		const Vector3 direction = Math::Normalize(ray.direction);
		const float normalLength = Math::Length(plane.normal);
		const Vector3 normal = plane.normal / normalLength;
		const float normalizedDistance = plane.distance / normalLength;
		const float denominator = Math::Dot(normal, direction);
		if (std::abs(denominator) <= Math::EPSILON) {
			return std::nullopt;
		}

		const float distance =
			-(Math::Dot(normal, ray.origin) + normalizedDistance) / denominator;
		if (distance < 0.0f || distance > maxDistance) {
			return std::nullopt;
		}

		return RaycastHit{
			.point = ray.origin + direction * distance,
			.normal = normal,
			.distance = distance,
		};
	}

}
