#pragma once

#include "LGF/Collision/Shapes3D.h"

#include <limits>
#include <optional>

namespace LGF::Collision {

	/// <summary>
	/// 2つの形状の接触情報。
	/// normal は第1引数から第2引数へ向かう向きになる。
	/// </summary>
	struct Contact {
		Vector3 point{};
		Vector3 normal{};
		float penetrationDepth = 0.0f;
	};

	struct RaycastHit {
		Vector3 point{};
		Vector3 normal{};
		float distance = 0.0f;
	};

	bool IsValid(const Sphere& sphere);
	bool IsValid(const SphericalCap& sphericalCap);
	bool IsValid(const SphericalShellCap& sphericalShellCap);
	bool IsValid(const AABB& box);
	bool IsValid(const Capsule& capsule);
	bool IsValid(const Cylinder& cylinder);
	bool IsValid(const Ray& ray);
	bool IsValid(const Plane& plane);

	Vector3 ClosestPoint(const Vector3& point, const AABB& box);
	Vector3 ClosestPoint(const Vector3& point, const Capsule& capsule);

	bool Contains(const Sphere& sphere, const Vector3& point);
	bool Contains(const SphericalShellCap& sphericalShellCap, const Vector3& point);
	bool Contains(const AABB& box, const Vector3& point);
	bool Contains(const Capsule& capsule, const Vector3& point);
	bool Contains(const Cylinder& cylinder, const Vector3& point);

	std::optional<Contact> Collide(const Sphere& first, const Sphere& second);
	std::optional<Contact> Collide(const Sphere& sphere, const SphericalCap& sphericalCap);
	std::optional<Contact> Collide(const SphericalCap& sphericalCap, const Sphere& sphere);
	std::optional<Contact> Collide(const Sphere& sphere, const SphericalShellCap& sphericalShellCap);
	std::optional<Contact> Collide(const SphericalShellCap& sphericalShellCap, const Sphere& sphere);
	std::optional<Contact> Collide(const Sphere& sphere, const AABB& box);
	std::optional<Contact> Collide(const AABB& box, const Sphere& sphere);
	std::optional<Contact> Collide(const AABB& first, const AABB& second);
	std::optional<Contact> Collide(const Capsule& capsule, const Sphere& sphere);
	std::optional<Contact> Collide(const Sphere& sphere, const Capsule& capsule);
	std::optional<Contact> Collide(const Capsule& capsule, const AABB& box);
	std::optional<Contact> Collide(const AABB& box, const Capsule& capsule);
	std::optional<Contact> Collide(const Capsule& first, const Capsule& second);

	bool Intersects(const Sphere& first, const Sphere& second);
	bool Intersects(const Sphere& sphere, const SphericalCap& sphericalCap);
	bool Intersects(const SphericalCap& sphericalCap, const Sphere& sphere);
	bool Intersects(const Sphere& sphere, const SphericalShellCap& sphericalShellCap);
	bool Intersects(const SphericalShellCap& sphericalShellCap, const Sphere& sphere);
	bool Intersects(const Sphere& sphere, const AABB& box);
	bool Intersects(const AABB& box, const Sphere& sphere);
	bool Intersects(const AABB& first, const AABB& second);
	bool Intersects(const Capsule& capsule, const Sphere& sphere);
	bool Intersects(const Sphere& sphere, const Capsule& capsule);
	bool Intersects(const Capsule& capsule, const AABB& box);
	bool Intersects(const AABB& box, const Capsule& capsule);
	bool Intersects(const Capsule& first, const Capsule& second);

	std::optional<RaycastHit> Raycast(
		const Ray& ray,
		const Sphere& sphere,
		float maxDistance = std::numeric_limits<float>::infinity());
	std::optional<RaycastHit> Raycast(
		const Ray& ray,
		const AABB& box,
		float maxDistance = std::numeric_limits<float>::infinity());
	std::optional<RaycastHit> Raycast(
		const Ray& ray,
		const Plane& plane,
		float maxDistance = std::numeric_limits<float>::infinity());
	std::optional<RaycastHit> Raycast(
		const Ray& ray,
		const Capsule& capsule,
		float maxDistance = std::numeric_limits<float>::infinity());

}
