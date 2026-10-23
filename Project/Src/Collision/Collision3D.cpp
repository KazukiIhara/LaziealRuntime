#include "LGF/Collision/Collision3D.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <iterator>
#include <limits>

#include "CollisionUtility.h"
#include "LGF/Math/Functions/MatrixMath.h"
#include "LGF/Math/Functions/QuaternionMath.h"
#include "LGF/Math/Functions/TransformMath.h"
#include "LGF/Math/Functions/VectorMath.h"
#include "LGF/Math/MathConstants.h"

namespace {

	using namespace LGF;

	Vector3 Reverse(const Vector3& value) {
		return { -value.x, -value.y, -value.z };
	}

	LGF::Collision::Contact ReverseContact(const LGF::Collision::Contact& contact) {
		return {
			.point = contact.point,
			.normal = Reverse(contact.normal),
			.penetrationDepth = contact.penetrationDepth,
		};
	}

	using OBBAxes = std::array<Vector3, 3>;

	OBBAxes GetAxes(const LGF::Collision::OBB& box) {
		const Matrix4x4 rotation = Math::MakeRotateMatrix(
			Math::Normalize(box.rotation));
		return {
			Math::TransformNormal({ 1.0f, 0.0f, 0.0f }, rotation),
			Math::TransformNormal({ 0.0f, 1.0f, 0.0f }, rotation),
			Math::TransformNormal({ 0.0f, 0.0f, 1.0f }, rotation),
		};
	}

	Vector3 ToLocalDirection(const Vector3& direction, const OBBAxes& axes) {
		return {
			Math::Dot(direction, axes[0]),
			Math::Dot(direction, axes[1]),
			Math::Dot(direction, axes[2]),
		};
	}

	Vector3 ToWorldDirection(const Vector3& direction, const OBBAxes& axes) {
		return axes[0] * direction.x +
			axes[1] * direction.y +
			axes[2] * direction.z;
	}

	float CalculateProjectionRadius(
		const LGF::Collision::OBB& box,
		const OBBAxes& axes,
		const Vector3& axis) {
		return box.halfExtents.x * std::abs(Math::Dot(axes[0], axis)) +
			box.halfExtents.y * std::abs(Math::Dot(axes[1], axis)) +
			box.halfExtents.z * std::abs(Math::Dot(axes[2], axis));
	}

	bool TestSeparatingAxis(
		const LGF::Collision::OBB& first,
		const OBBAxes& firstAxes,
		const LGF::Collision::OBB& second,
		const OBBAxes& secondAxes,
		const Vector3& centerOffset,
		const Vector3& candidateAxis,
		float& minimumPenetration,
		Vector3& contactNormal) {
		const float axisLengthSq = Math::LengthSq(candidateAxis);
		if (axisLengthSq <= Math::EPSILON * Math::EPSILON) {
			// 平行な辺同士の外積は軸として使えないため無視する。
			return true;
		}

		const Vector3 axis = candidateAxis / std::sqrt(axisLengthSq);
		const float firstRadius = CalculateProjectionRadius(first, firstAxes, axis);
		const float secondRadius = CalculateProjectionRadius(second, secondAxes, axis);
		const float signedDistance = Math::Dot(centerOffset, axis);
		const float penetration =
			firstRadius + secondRadius - std::abs(signedDistance);
		if (penetration < 0.0f) {
			return false;
		}

		if (penetration < minimumPenetration) {
			minimumPenetration = penetration;
			contactNormal = signedDistance >= 0.0f ? axis : -axis;
		}
		return true;
	}

	Vector3 SupportPoint(
		const LGF::Collision::OBB& box,
		const OBBAxes& axes,
		const Vector3& direction) {
		Vector3 result = box.center;
		for (std::size_t axisIndex = 0u; axisIndex < axes.size(); ++axisIndex) {
			const float projection = Math::Dot(axes[axisIndex], direction);
			if (std::abs(projection) <= Math::EPSILON) {
				continue;
			}
			const float extent = axisIndex == 0u
				? box.halfExtents.x
				: axisIndex == 1u ? box.halfExtents.y : box.halfExtents.z;
			result += axes[axisIndex] * (projection > 0.0f ? extent : -extent);
		}
		return result;
	}

	struct SphericalShellCapGeometry {
		Vector3 axis{};
		Vector3 sphereCenter{};
		float openingPlaneOffset{};
		float outerOpeningRadius{};
		float innerOpeningRadius{};
	};

	SphericalShellCapGeometry MakeGeometry(
		const LGF::Collision::SphericalShellCap& shape) {
		const Vector3 axis = Math::Normalize(shape.axis);
		const float openingPlaneOffset = shape.height - shape.outerRadius;
		return {
			.axis = axis,
			.sphereCenter = shape.outerApex + axis * shape.outerRadius,
			.openingPlaneOffset = openingPlaneOffset,
			.outerOpeningRadius = std::sqrt(std::max(
				shape.outerRadius * shape.outerRadius -
					openingPlaneOffset * openingPlaneOffset,
				0.0f)),
			.innerOpeningRadius = std::sqrt(std::max(
				shape.innerRadius * shape.innerRadius -
					openingPlaneOffset * openingPlaneOffset,
				0.0f)),
		};
	}

	Vector3 PerpendicularTo(const Vector3& axis) {
		const Vector3 reference = std::abs(axis.y) < 0.9f
			? Vector3{ 0.0f, 1.0f, 0.0f }
			: Vector3{ 1.0f, 0.0f, 0.0f };
		return Math::Normalize(Math::Cross(axis, reference));
	}

	Vector3 ClosestPointOnCircle(
		const Vector3& point,
		const Vector3& circleCenter,
		const Vector3& axis,
		float radius) {
		const Vector3 offset = point - circleCenter;
		const Vector3 radial = offset - axis * Math::Dot(offset, axis);
		return circleCenter +
			Collision::Internal::SafeNormal(radial, PerpendicularTo(axis)) * radius;
	}

	Vector3 ClosestPointOnSphericalCapSurface(
		const Vector3& point,
		const SphericalShellCapGeometry& geometry,
		float radius,
		float openingRadius) {
		const Vector3 direction = Collision::Internal::SafeNormal(
			point - geometry.sphereCenter,
			-geometry.axis);
		const Vector3 spherePoint = geometry.sphereCenter + direction * radius;
		if (Math::Dot(spherePoint - geometry.sphereCenter, geometry.axis) <=
			geometry.openingPlaneOffset) {
			return spherePoint;
		}

		const Vector3 circleCenter = geometry.sphereCenter +
			geometry.axis * geometry.openingPlaneOffset;
		return ClosestPointOnCircle(point, circleCenter, geometry.axis, openingRadius);
	}

	Vector3 ClosestPointOnOpeningRing(
		const Vector3& point,
		const SphericalShellCapGeometry& geometry) {
		const Vector3 ringCenter = geometry.sphereCenter +
			geometry.axis * geometry.openingPlaneOffset;
		const Vector3 offset = point - ringCenter;
		const Vector3 radial = offset - geometry.axis * Math::Dot(offset, geometry.axis);
		const float radialLength = Math::Length(radial);
		const Vector3 radialDirection = Collision::Internal::SafeNormal(
			radial,
			PerpendicularTo(geometry.axis));
		const float radius = std::clamp(
			radialLength,
			geometry.innerOpeningRadius,
			geometry.outerOpeningRadius);
		return ringCenter + radialDirection * radius;
	}

	Vector3 ClosestPointOnSphericalShellCapBoundary(
		const Vector3& point,
		const LGF::Collision::SphericalShellCap& shape) {
		const SphericalShellCapGeometry geometry = MakeGeometry(shape);
		const std::array<Vector3, 3> candidates{
			ClosestPointOnSphericalCapSurface(
				point,
				geometry,
				shape.outerRadius,
				geometry.outerOpeningRadius),
			ClosestPointOnSphericalCapSurface(
				point,
				geometry,
				shape.innerRadius,
				geometry.innerOpeningRadius),
			ClosestPointOnOpeningRing(point, geometry),
		};

		Vector3 closest = candidates[0];
		float closestDistanceSq = Math::DistanceSq(point, closest);
		for (std::size_t index = 1u; index < candidates.size(); ++index) {
			const float distanceSq = Math::DistanceSq(point, candidates[index]);
			if (distanceSq < closestDistanceSq) {
				closest = candidates[index];
				closestDistanceSq = distanceSq;
			}
		}
		return closest;
	}

	float GetComponent(const Vector3& value, std::size_t index) {
		switch (index) {
		case 0u:
			return value.x;
		case 1u:
			return value.y;
		default:
			return value.z;
		}
	}

	struct SegmentClosestPoints {
		Vector3 first{};
		Vector3 second{};
	};

	SegmentClosestPoints ClosestPointsBetweenSegments(
		const Vector3& firstStart,
		const Vector3& firstEnd,
		const Vector3& secondStart,
		const Vector3& secondEnd) {
		const Vector3 firstDirection = firstEnd - firstStart;
		const Vector3 secondDirection = secondEnd - secondStart;
		const Vector3 offset = firstStart - secondStart;
		const float firstLengthSq = Math::LengthSq(firstDirection);
		const float secondLengthSq = Math::LengthSq(secondDirection);
		const float secondProjection = Math::Dot(secondDirection, offset);

		float firstT = 0.0f;
		float secondT = 0.0f;
		if (firstLengthSq <= Math::EPSILON * Math::EPSILON &&
			secondLengthSq <= Math::EPSILON * Math::EPSILON) {
			return { firstStart, secondStart };
		}

		if (firstLengthSq <= Math::EPSILON * Math::EPSILON) {
			secondT = std::clamp(secondProjection / secondLengthSq, 0.0f, 1.0f);
		} else {
			const float firstProjection = Math::Dot(firstDirection, offset);
			if (secondLengthSq <= Math::EPSILON * Math::EPSILON) {
				firstT = std::clamp(-firstProjection / firstLengthSq, 0.0f, 1.0f);
			} else {
				const float directionsDot = Math::Dot(firstDirection, secondDirection);
				const float denominator =
					firstLengthSq * secondLengthSq - directionsDot * directionsDot;
				if (std::abs(denominator) > Math::EPSILON) {
					firstT = std::clamp(
						(directionsDot * secondProjection -
							firstProjection * secondLengthSq) / denominator,
						0.0f,
						1.0f);
				}

				secondT =
					(directionsDot * firstT + secondProjection) / secondLengthSq;
				if (secondT < 0.0f) {
					secondT = 0.0f;
					firstT = std::clamp(-firstProjection / firstLengthSq, 0.0f, 1.0f);
				} else if (secondT > 1.0f) {
					secondT = 1.0f;
					firstT = std::clamp(
						(directionsDot - firstProjection) / firstLengthSq,
						0.0f,
						1.0f);
				}
			}
		}

		return {
			firstStart + firstDirection * firstT,
			secondStart + secondDirection * secondT,
		};
	}

	SegmentClosestPoints ClosestPointsBetweenSegmentAndAABB(
		const Vector3& start,
		const Vector3& end,
		const LGF::Collision::AABB& box) {
		const Vector3 direction = end - start;
		std::array<float, 8> breakpoints{};
		std::size_t breakpointCount = 0u;
		breakpoints[breakpointCount++] = 0.0f;
		breakpoints[breakpointCount++] = 1.0f;

		for (std::size_t axis = 0u; axis < 3u; ++axis) {
			const float axisDirection = GetComponent(direction, axis);
			if (std::abs(axisDirection) <= Math::EPSILON) {
				continue;
			}

			const float axisStart = GetComponent(start, axis);
			const float bounds[2]{
				GetComponent(box.min, axis),
				GetComponent(box.max, axis),
			};
			for (const float bound : bounds) {
				const float t = (bound - axisStart) / axisDirection;
				if (t > 0.0f && t < 1.0f) {
					breakpoints[breakpointCount++] = t;
				}
			}
		}

		std::sort(breakpoints.begin(), breakpoints.begin() + breakpointCount);
		float bestDistanceSq = std::numeric_limits<float>::infinity();
		SegmentClosestPoints result{};
		const auto evaluate = [&](float t) {
			const Vector3 segmentPoint = start + direction * t;
			const Vector3 boxPoint{
				std::clamp(segmentPoint.x, box.min.x, box.max.x),
				std::clamp(segmentPoint.y, box.min.y, box.max.y),
				std::clamp(segmentPoint.z, box.min.z, box.max.z),
			};
			const float distanceSq = Math::DistanceSq(segmentPoint, boxPoint);
			if (distanceSq < bestDistanceSq) {
				bestDistanceSq = distanceSq;
				result = { segmentPoint, boxPoint };
			}
		};

		for (std::size_t index = 0u; index < breakpointCount; ++index) {
			evaluate(breakpoints[index]);
			if (index + 1u >= breakpointCount) {
				continue;
			}

			const float intervalStart = breakpoints[index];
			const float intervalEnd = breakpoints[index + 1u];
			if (intervalEnd - intervalStart <= Math::EPSILON) {
				continue;
			}

			const float midpoint = (intervalStart + intervalEnd) * 0.5f;
			float quadratic = 0.0f;
			float linear = 0.0f;
			for (std::size_t axis = 0u; axis < 3u; ++axis) {
				const float midpointValue =
					GetComponent(start, axis) + GetComponent(direction, axis) * midpoint;
				float bound = 0.0f;
				if (midpointValue < GetComponent(box.min, axis)) {
					bound = GetComponent(box.min, axis);
				} else if (midpointValue > GetComponent(box.max, axis)) {
					bound = GetComponent(box.max, axis);
				} else {
					continue;
				}

				const float axisDirection = GetComponent(direction, axis);
				quadratic += axisDirection * axisDirection;
				linear += axisDirection * (GetComponent(start, axis) - bound);
			}

			if (quadratic > Math::EPSILON * Math::EPSILON) {
				evaluate(std::clamp(-linear / quadratic, intervalStart, intervalEnd));
			}
		}

		return result;
	}

}

namespace LGF::Collision {

	bool IsValid(const Sphere& sphere) {
		return sphere.radius >= 0.0f;
	}

	bool IsValid(const SphericalCap& sphericalCap) {
		return sphericalCap.baseRadius > 0.0f &&
			sphericalCap.height > 0.0f &&
			Math::LengthSq(sphericalCap.axis) > Math::EPSILON * Math::EPSILON;
	}

	bool IsValid(const SphericalShellCap& sphericalShellCap) {
		const float thickness =
			sphericalShellCap.outerRadius - sphericalShellCap.innerRadius;
		return sphericalShellCap.outerRadius > 0.0f &&
			sphericalShellCap.innerRadius > 0.0f &&
			sphericalShellCap.outerRadius > sphericalShellCap.innerRadius &&
			sphericalShellCap.height > thickness &&
			sphericalShellCap.height <= sphericalShellCap.outerRadius &&
			Math::LengthSq(sphericalShellCap.axis) > Math::EPSILON * Math::EPSILON;
	}

	bool IsValid(const AABB& box) {
		return box.min.x <= box.max.x &&
			box.min.y <= box.max.y &&
			box.min.z <= box.max.z;
	}

	bool IsValid(const OBB& box) {
		return box.halfExtents.x >= 0.0f &&
			box.halfExtents.y >= 0.0f &&
			box.halfExtents.z >= 0.0f &&
			Math::LengthSq(box.rotation) > Math::EPSILON * Math::EPSILON;
	}

	bool IsValid(const Capsule& capsule) {
		return capsule.radius >= 0.0f;
	}

	bool IsValid(const Cylinder& cylinder) {
		return cylinder.radius >= 0.0f &&
			Math::DistanceSq(cylinder.start, cylinder.end) >
				Math::EPSILON * Math::EPSILON;
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

	Vector3 ClosestPoint(const Vector3& point, const OBB& box) {
		if (!IsValid(box)) {
			return box.center;
		}

		const OBBAxes axes = GetAxes(box);
		const Vector3 localPoint = ToLocalDirection(point - box.center, axes);
		const Vector3 clampedPoint{
			std::clamp(localPoint.x, -box.halfExtents.x, box.halfExtents.x),
			std::clamp(localPoint.y, -box.halfExtents.y, box.halfExtents.y),
			std::clamp(localPoint.z, -box.halfExtents.z, box.halfExtents.z),
		};
		return box.center + ToWorldDirection(clampedPoint, axes);
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

	bool Contains(const SphericalShellCap& sphericalShellCap, const Vector3& point) {
		if (!IsValid(sphericalShellCap)) {
			return false;
		}

		const SphericalShellCapGeometry geometry = MakeGeometry(sphericalShellCap);
		const Vector3 offset = point - geometry.sphereCenter;
		const float distanceSq = Math::LengthSq(offset);
		const float axialOffset = Math::Dot(offset, geometry.axis);
		return axialOffset <= geometry.openingPlaneOffset &&
			distanceSq <= sphericalShellCap.outerRadius * sphericalShellCap.outerRadius &&
			distanceSq >= sphericalShellCap.innerRadius * sphericalShellCap.innerRadius;
	}

	bool Contains(const AABB& box, const Vector3& point) {
		return IsValid(box) &&
			point.x >= box.min.x && point.x <= box.max.x &&
			point.y >= box.min.y && point.y <= box.max.y &&
			point.z >= box.min.z && point.z <= box.max.z;
	}

	bool Contains(const OBB& box, const Vector3& point) {
		if (!IsValid(box)) {
			return false;
		}

		const Vector3 localPoint = ToLocalDirection(
			point - box.center,
			GetAxes(box));
		return std::abs(localPoint.x) <= box.halfExtents.x &&
			std::abs(localPoint.y) <= box.halfExtents.y &&
			std::abs(localPoint.z) <= box.halfExtents.z;
	}

	bool Contains(const Capsule& capsule, const Vector3& point) {
		if (!IsValid(capsule)) {
			return false;
		}
		const Vector3 closest = ClosestPoint(point, capsule);
		return Math::DistanceSq(closest, point) <= capsule.radius * capsule.radius;
	}

	bool Contains(const Cylinder& cylinder, const Vector3& point) {
		if (!IsValid(cylinder)) {
			return false;
		}

		const Vector3 axis = cylinder.end - cylinder.start;
		const float axisLengthSq = Math::LengthSq(axis);
		const float t = Math::Dot(point - cylinder.start, axis) / axisLengthSq;
		if (t < 0.0f || t > 1.0f) {
			return false;
		}

		const Vector3 closestOnAxis = cylinder.start + axis * t;
		return Math::DistanceSq(point, closestOnAxis) <=
			cylinder.radius * cylinder.radius;
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
		const Vector3 normal = Collision::Internal::SafeNormal(
			delta,
			{ 1.0f, 0.0f, 0.0f });
		const float penetrationDepth = radiusSum - distance;
		return Contact{
			.point = first.center + normal * (first.radius - penetrationDepth * 0.5f),
			.normal = normal,
			.penetrationDepth = penetrationDepth,
		};
	}

	std::optional<Contact> Collide(
		const Sphere& sphere,
		const SphericalCap& sphericalCap) {
		if (!IsValid(sphere) || !IsValid(sphericalCap)) {
			return std::nullopt;
		}

		const Vector3 axis = Math::Normalize(sphericalCap.axis);
		const float sphereRadius =
			(sphericalCap.baseRadius * sphericalCap.baseRadius +
				sphericalCap.height * sphericalCap.height) /
			(2.0f * sphericalCap.height);
		const Vector3 sphereCenter = sphericalCap.apex + axis * sphereRadius;
		const Vector3 centerOffset = sphere.center - sphereCenter;
		const float centerDistance = Math::Length(centerOffset);
		const Vector3 surfaceDirection = Collision::Internal::SafeNormal(
			centerOffset,
			Reverse(axis));
		const Vector3 surfacePoint = sphereCenter + surfaceDirection * sphereRadius;
		const float surfaceHeight = Math::Dot(
			surfacePoint - sphericalCap.apex,
			axis);

		// 円形境界より先には面がないため、開口部を通る球とは接触しない。
		if (surfaceHeight < -Math::EPSILON ||
			surfaceHeight > sphericalCap.height + Math::EPSILON) {
			return std::nullopt;
		}

		const float distanceToSurface = std::abs(centerDistance - sphereRadius);
		if (distanceToSurface > sphere.radius) {
			return std::nullopt;
		}

		return Contact{
			.point = surfacePoint,
			.normal = Collision::Internal::SafeNormal(
				surfacePoint - sphere.center,
				Reverse(surfaceDirection)),
			.penetrationDepth = sphere.radius - distanceToSurface,
		};
	}

	std::optional<Contact> Collide(
		const SphericalCap& sphericalCap,
		const Sphere& sphere) {
		const std::optional<Contact> contact = Collide(sphere, sphericalCap);
		return contact ? std::optional<Contact>(ReverseContact(*contact)) : std::nullopt;
	}

	std::optional<Contact> Collide(
		const Sphere& sphere,
		const SphericalShellCap& sphericalShellCap) {
		if (!IsValid(sphere) || !IsValid(sphericalShellCap)) {
			return std::nullopt;
		}

		const Vector3 closest = ClosestPointOnSphericalShellCapBoundary(
			sphere.center,
			sphericalShellCap);
		const Vector3 offset = closest - sphere.center;
		const float distanceSq = Math::LengthSq(offset);
		const bool centerInside = Contains(sphericalShellCap, sphere.center);
		if (!centerInside && distanceSq > sphere.radius * sphere.radius) {
			return std::nullopt;
		}

		const float distance = std::sqrt(std::max(distanceSq, 0.0f));
		const Vector3 fallback = centerInside
			? Math::Normalize(sphericalShellCap.axis)
			: -Math::Normalize(sphericalShellCap.axis);
		return Contact{
			.point = closest,
			.normal = Collision::Internal::SafeNormal(offset, fallback),
			.penetrationDepth = centerInside
				? sphere.radius + distance
				: sphere.radius - distance,
		};
	}

	std::optional<Contact> Collide(
		const SphericalShellCap& sphericalShellCap,
		const Sphere& sphere) {
		const std::optional<Contact> contact = Collide(sphere, sphericalShellCap);
		return contact ? std::optional<Contact>(ReverseContact(*contact)) : std::nullopt;
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

	std::optional<Contact> Collide(const Sphere& sphere, const OBB& box) {
		if (!IsValid(sphere) || !IsValid(box)) {
			return std::nullopt;
		}

		const OBBAxes axes = GetAxes(box);
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

		// 球の中心が箱の内側にある場合は、最も近い面へ向かう方向を法線にする。
		const Vector3 localCenter = ToLocalDirection(sphere.center - box.center, axes);
		const float distances[6]{
			localCenter.x + box.halfExtents.x,
			box.halfExtents.x - localCenter.x,
			localCenter.y + box.halfExtents.y,
			box.halfExtents.y - localCenter.y,
			localCenter.z + box.halfExtents.z,
			box.halfExtents.z - localCenter.z,
		};
		const Vector3 normals[6]{
			-axes[0], axes[0],
			-axes[1], axes[1],
			-axes[2], axes[2],
		};
		const auto nearest = std::min_element(std::begin(distances), std::end(distances));
		const std::size_t index = static_cast<std::size_t>(nearest - std::begin(distances));
		return Contact{
			.point = sphere.center + normals[index] * *nearest,
			.normal = normals[index],
			.penetrationDepth = sphere.radius + *nearest,
		};
	}

	std::optional<Contact> Collide(const OBB& box, const Sphere& sphere) {
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

	std::optional<Contact> Collide(const OBB& first, const OBB& second) {
		if (!IsValid(first) || !IsValid(second)) {
			return std::nullopt;
		}

		const OBBAxes firstAxes = GetAxes(first);
		const OBBAxes secondAxes = GetAxes(second);
		const Vector3 centerOffset = second.center - first.center;
		float minimumPenetration = std::numeric_limits<float>::infinity();
		Vector3 contactNormal{};

		// OBB同士は、両方の面法線6本と辺同士の外積9本の合計15軸を調べる。
		for (const Vector3& axis : firstAxes) {
			if (!TestSeparatingAxis(
				first,
				firstAxes,
				second,
				secondAxes,
				centerOffset,
				axis,
				minimumPenetration,
				contactNormal)) {
				return std::nullopt;
			}
		}
		for (const Vector3& axis : secondAxes) {
			if (!TestSeparatingAxis(
				first,
				firstAxes,
				second,
				secondAxes,
				centerOffset,
				axis,
				minimumPenetration,
				contactNormal)) {
				return std::nullopt;
			}
		}
		for (const Vector3& firstAxis : firstAxes) {
			for (const Vector3& secondAxis : secondAxes) {
				if (!TestSeparatingAxis(
					first,
					firstAxes,
					second,
					secondAxes,
					centerOffset,
					Math::Cross(firstAxis, secondAxis),
					minimumPenetration,
					contactNormal)) {
					return std::nullopt;
				}
			}
		}

		const Vector3 firstPoint = SupportPoint(first, firstAxes, contactNormal);
		const Vector3 secondPoint = SupportPoint(second, secondAxes, -contactNormal);
		return Contact{
			.point = (firstPoint + secondPoint) * 0.5f,
			.normal = contactNormal,
			.penetrationDepth = minimumPenetration,
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
		const Vector3 normal = Collision::Internal::SafeNormal(
			delta,
			{ 1.0f, 0.0f, 0.0f });
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

	std::optional<Contact> Collide(const Capsule& capsule, const AABB& box) {
		if (!IsValid(capsule) || !IsValid(box)) {
			return std::nullopt;
		}

		const SegmentClosestPoints closest =
			ClosestPointsBetweenSegmentAndAABB(capsule.start, capsule.end, box);
		const Vector3 delta = closest.second - closest.first;
		const float distanceSq = Math::LengthSq(delta);
		if (distanceSq > capsule.radius * capsule.radius) {
			return std::nullopt;
		}

		if (distanceSq > Math::EPSILON * Math::EPSILON) {
			const float distance = std::sqrt(distanceSq);
			return Contact{
				.point = closest.second,
				.normal = delta / distance,
				.penetrationDepth = capsule.radius - distance,
			};
		}

		const float distances[6]{
			closest.first.x - box.min.x,
			box.max.x - closest.first.x,
			closest.first.y - box.min.y,
			box.max.y - closest.first.y,
			closest.first.z - box.min.z,
			box.max.z - closest.first.z,
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
			.point = closest.first + normals[index] * *nearest,
			.normal = normals[index],
			.penetrationDepth = capsule.radius + *nearest,
		};
	}

	std::optional<Contact> Collide(const AABB& box, const Capsule& capsule) {
		const std::optional<Contact> contact = Collide(capsule, box);
		return contact ? std::optional<Contact>(ReverseContact(*contact)) : std::nullopt;
	}

	std::optional<Contact> Collide(const Capsule& first, const Capsule& second) {
		if (!IsValid(first) || !IsValid(second)) {
			return std::nullopt;
		}

		const SegmentClosestPoints closest = ClosestPointsBetweenSegments(
			first.start,
			first.end,
			second.start,
			second.end);
		const Vector3 delta = closest.second - closest.first;
		const float radiusSum = first.radius + second.radius;
		const float distanceSq = Math::LengthSq(delta);
		if (distanceSq > radiusSum * radiusSum) {
			return std::nullopt;
		}

		const float distance = std::sqrt(distanceSq);
		const Vector3 normal = Collision::Internal::SafeNormal(
			delta,
			{ 1.0f, 0.0f, 0.0f });
		const float penetrationDepth = radiusSum - distance;
		return Contact{
			.point = closest.first + normal *
				(first.radius - penetrationDepth * 0.5f),
			.normal = normal,
			.penetrationDepth = penetrationDepth,
		};
	}

	bool Intersects(const Sphere& first, const Sphere& second) {
		return Collide(first, second).has_value();
	}

	bool Intersects(const Sphere& sphere, const SphericalCap& sphericalCap) {
		return Collide(sphere, sphericalCap).has_value();
	}

	bool Intersects(const SphericalCap& sphericalCap, const Sphere& sphere) {
		return Intersects(sphere, sphericalCap);
	}

	bool Intersects(const Sphere& sphere, const SphericalShellCap& sphericalShellCap) {
		return Collide(sphere, sphericalShellCap).has_value();
	}

	bool Intersects(const SphericalShellCap& sphericalShellCap, const Sphere& sphere) {
		return Intersects(sphere, sphericalShellCap);
	}

	bool Intersects(const Sphere& sphere, const AABB& box) {
		return Collide(sphere, box).has_value();
	}

	bool Intersects(const AABB& box, const Sphere& sphere) {
		return Intersects(sphere, box);
	}

	bool Intersects(const Sphere& sphere, const OBB& box) {
		return Collide(sphere, box).has_value();
	}

	bool Intersects(const OBB& box, const Sphere& sphere) {
		return Intersects(sphere, box);
	}

	bool Intersects(const AABB& first, const AABB& second) {
		return Collide(first, second).has_value();
	}

	bool Intersects(const OBB& first, const OBB& second) {
		return Collide(first, second).has_value();
	}

	bool Intersects(const Capsule& capsule, const Sphere& sphere) {
		return Collide(capsule, sphere).has_value();
	}

	bool Intersects(const Sphere& sphere, const Capsule& capsule) {
		return Intersects(capsule, sphere);
	}

	bool Intersects(const Capsule& capsule, const AABB& box) {
		return Collide(capsule, box).has_value();
	}

	bool Intersects(const AABB& box, const Capsule& capsule) {
		return Intersects(capsule, box);
	}

	bool Intersects(const Capsule& first, const Capsule& second) {
		return Collide(first, second).has_value();
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
				.normal = Collision::Internal::SafeNormal(offset, Reverse(direction)),
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
			.normal = Collision::Internal::SafeNormal(
				point - sphere.center,
				Reverse(direction)),
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
		const OBB& box,
		float maxDistance) {
		if (!IsValid(ray) || !IsValid(box) || maxDistance < 0.0f) {
			return std::nullopt;
		}

		const OBBAxes axes = GetAxes(box);
		const Ray localRay{
			.origin = ToLocalDirection(ray.origin - box.center, axes),
			.direction = ToLocalDirection(Math::Normalize(ray.direction), axes),
		};
		const AABB localBox{
			.min = -box.halfExtents,
			.max = box.halfExtents,
		};
		const std::optional<RaycastHit> localHit =
			Raycast(localRay, localBox, maxDistance);
		if (!localHit) {
			return std::nullopt;
		}

		return RaycastHit{
			.point = box.center + ToWorldDirection(localHit->point, axes),
			.normal = ToWorldDirection(localHit->normal, axes),
			.distance = localHit->distance,
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

	std::optional<RaycastHit> Raycast(
		const Ray& ray,
		const Capsule& capsule,
		float maxDistance) {
		if (!IsValid(ray) || !IsValid(capsule) || maxDistance < 0.0f) {
			return std::nullopt;
		}

		const Vector3 direction = Math::Normalize(ray.direction);
		if (Contains(capsule, ray.origin)) {
			const Vector3 closest = ClosestPoint(ray.origin, capsule);
			return RaycastHit{
				.point = ray.origin,
				.normal = Collision::Internal::SafeNormal(
					ray.origin - closest,
					Reverse(direction)),
				.distance = 0.0f,
			};
		}

		const Vector3 axis = capsule.end - capsule.start;
		const float axisLengthSq = Math::LengthSq(axis);
		if (axisLengthSq <= Math::EPSILON * Math::EPSILON) {
			return Raycast(ray, Sphere{ capsule.start, capsule.radius }, maxDistance);
		}

		std::optional<RaycastHit> nearestHit;
		const Vector3 originOffset = ray.origin - capsule.start;
		const float axisRay = Math::Dot(axis, direction);
		const float axisOrigin = Math::Dot(axis, originOffset);
		const float rayOrigin = Math::Dot(direction, originOffset);
		const float originLengthSq = Math::LengthSq(originOffset);
		const float quadratic = axisLengthSq - axisRay * axisRay;
		const float linear = axisLengthSq * rayOrigin - axisOrigin * axisRay;
		const float constant = axisLengthSq * originLengthSq -
			axisOrigin * axisOrigin - capsule.radius * capsule.radius * axisLengthSq;
		const float discriminant = linear * linear - quadratic * constant;
		if (quadratic > Math::EPSILON * Math::EPSILON && discriminant >= 0.0f) {
			const float distance = (-linear - std::sqrt(discriminant)) / quadratic;
			const float axisPosition = axisOrigin + distance * axisRay;
			if (distance >= 0.0f && distance <= maxDistance &&
				axisPosition > 0.0f && axisPosition < axisLengthSq) {
				const Vector3 point = ray.origin + direction * distance;
				const Vector3 axisPoint =
					capsule.start + axis * (axisPosition / axisLengthSq);
				nearestHit = RaycastHit{
					.point = point,
					.normal = Collision::Internal::SafeNormal(
						point - axisPoint,
						Reverse(direction)),
					.distance = distance,
				};
			}
		}

		const auto considerCap = [&](const Vector3& center, bool isStart) {
			const std::optional<RaycastHit> hit =
				Raycast(ray, Sphere{ center, capsule.radius }, maxDistance);
			if (!hit) {
				return;
			}

			const float axisPosition = Math::Dot(hit->point - capsule.start, axis);
			const bool isOnHemisphere = isStart
				? axisPosition <= 0.0f
				: axisPosition >= axisLengthSq;
			if (isOnHemisphere && (!nearestHit || hit->distance < nearestHit->distance)) {
				nearestHit = hit;
			}
		};
		considerCap(capsule.start, true);
		considerCap(capsule.end, false);
		return nearestHit;
	}

}
