#include "Shapes.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace Game
{
	namespace
	{
		Vec2 ToLocal(const Shape& shape, Vec2 point)
		{
			const Vec2 offset = point - shape.Center;
			return { Dot(offset, shape.Axis), Dot(offset, Perpendicular(shape.Axis)) };
		}

		Vec2 ToWorld(const Shape& shape, Vec2 local)
		{
			return shape.Center + Rotate(local, shape.Axis);
		}
	}

	Shape MakeCircle(Vec2 center, float radius, Block blocks)
	{
		Shape shape;
		shape.Kind = ShapeKind::Circle;
		shape.Blocks = blocks;
		shape.Center = center;
		shape.Half = { radius, radius };
		shape.Radius = radius;
		return shape;
	}

	Shape MakeBox(Vec2 center, Vec2 half, float yaw, Block blocks)
	{
		Shape shape;
		shape.Kind = ShapeKind::Box;
		shape.Blocks = blocks;
		shape.Center = center;
		shape.Axis = FromAngle(yaw);
		if (std::abs(shape.Axis.x) < 1e-6f)
			shape.Axis.x = 0.0f;
		if (std::abs(shape.Axis.y) < 1e-6f)
			shape.Axis.y = 0.0f;
		shape.Half = half;
		shape.Radius = Length(half);
		return shape;
	}

	void GetBounds(const Shape& shape, Vec2& min, Vec2& max)
	{
		Vec2 extent = { shape.Radius, shape.Radius };
		if (shape.Kind == ShapeKind::Box)
			extent = { std::abs(shape.Axis.x) * shape.Half.x + std::abs(shape.Axis.y) * shape.Half.y, std::abs(shape.Axis.y) * shape.Half.x + std::abs(shape.Axis.x) * shape.Half.y };
		min = shape.Center - extent;
		max = shape.Center + extent;
	}

	float DistanceTo(const Shape& shape, Vec2 point)
	{
		if (shape.Kind == ShapeKind::Circle)
			return Length(point - shape.Center) - shape.Radius;
		const Vec2 local = ToLocal(shape, point);
		const Vec2 excess = { std::abs(local.x) - shape.Half.x, std::abs(local.y) - shape.Half.y };
		const Vec2 outside = { std::max(excess.x, 0.0f), std::max(excess.y, 0.0f) };
		return Length(outside) + std::min(std::max(excess.x, excess.y), 0.0f);
	}

	bool PushCircleOut(const Shape& shape, Vec2& position, float radius)
	{
		if (shape.Kind == ShapeKind::Circle)
		{
			const Vec2 offset = position - shape.Center;
			const float minimum = shape.Radius + radius;
			const float distanceSquared = Dot(offset, offset);
			if (distanceSquared >= minimum * minimum)
				return false;
			if (distanceSquared > 1e-12f)
			{
				const float distance = std::sqrt(distanceSquared);
				position += offset * ((minimum - distance) / distance);
			}
			else
				position.x += minimum;
			return true;
		}

		const Vec2 local = ToLocal(shape, position);
		const Vec2 closest = { std::clamp(local.x, -shape.Half.x, shape.Half.x), std::clamp(local.y, -shape.Half.y, shape.Half.y) };
		const Vec2 offset = local - closest;
		const float distanceSquared = Dot(offset, offset);
		if (distanceSquared >= radius * radius)
			return false;
		Vec2 resolved = local;
		if (distanceSquared > 1e-12f)
		{
			const float distance = std::sqrt(distanceSquared);
			resolved += offset * ((radius - distance) / distance);
		}
		else if (shape.Half.x - std::abs(local.x) < shape.Half.y - std::abs(local.y))
			resolved.x = (local.x >= 0.0f ? 1.0f : -1.0f) * (shape.Half.x + radius);
		else
			resolved.y = (local.y >= 0.0f ? 1.0f : -1.0f) * (shape.Half.y + radius);
		position = ToWorld(shape, resolved);
		return true;
	}

	bool SegmentHits(const Shape& shape, Vec2 from, Vec2 to, float& parameter)
	{
		if (shape.Kind == ShapeKind::Circle)
		{
			const Vec2 direction = to - from;
			const Vec2 offset = from - shape.Center;
			const float c = Dot(offset, offset) - shape.Radius * shape.Radius;
			if (c <= 0.0f)
			{
				parameter = 0.0f;
				return true;
			}
			const float a = Dot(direction, direction);
			const float b = Dot(offset, direction);
			if (a < 1e-12f || b >= 0.0f)
				return false;
			const float discriminant = b * b - a * c;
			if (discriminant < 0.0f)
				return false;
			const float t = (-b - std::sqrt(discriminant)) / a;
			if (t > 1.0f)
				return false;
			parameter = std::max(t, 0.0f);
			return true;
		}

		const Vec2 start = ToLocal(shape, from);
		const Vec2 delta = ToLocal(shape, to) - start;
		float enter = 0.0f;
		float exit = 1.0f;
		const float starts[2] = { start.x, start.y };
		const float deltas[2] = { delta.x, delta.y };
		const float halves[2] = { shape.Half.x, shape.Half.y };
		for (int axis = 0; axis < 2; axis++)
		{
			if (std::abs(deltas[axis]) < 1e-9f)
			{
				if (starts[axis] < -halves[axis] || starts[axis] > halves[axis])
					return false;
				continue;
			}
			float first = (-halves[axis] - starts[axis]) / deltas[axis];
			float second = (halves[axis] - starts[axis]) / deltas[axis];
			if (first > second)
				std::swap(first, second);
			enter = std::max(enter, first);
			exit = std::min(exit, second);
			if (enter > exit)
				return false;
		}
		parameter = enter;
		return true;
	}

	float PointSegmentDistance(Vec2 point, Vec2 from, Vec2 to)
	{
		return Length(Lerp(from, to, ClosestPointParameter(from, to, point)) - point);
	}

	float SegmentDistance(const Shape& shape, Vec2 from, Vec2 to)
	{
		if (shape.Kind == ShapeKind::Circle)
			return PointSegmentDistance(shape.Center, from, to) - shape.Radius;
		float parameter = 0.0f;
		if (SegmentHits(shape, from, to, parameter))
			return 0.0f;
		float distance = std::min(DistanceTo(shape, from), DistanceTo(shape, to));
		for (const Vec2 corner : { Vec2(-1.0f, -1.0f), Vec2(1.0f, -1.0f), Vec2(1.0f, 1.0f), Vec2(-1.0f, 1.0f) })
			distance = std::min(distance, PointSegmentDistance(ToWorld(shape, { corner.x * shape.Half.x, corner.y * shape.Half.y }), from, to));
		return distance;
	}
}
