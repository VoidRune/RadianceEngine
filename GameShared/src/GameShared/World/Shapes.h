#pragma once
#include "GameShared/Core/Math.h"
#include <cstdint>

namespace Game
{
	enum class ShapeKind : uint8_t
	{
		Circle,
		Box,
	};

	enum class Block : uint8_t
	{
		None = 0,
		Movement = 1 << 0,
		Projectiles = 1 << 1,
		All = Movement | Projectiles,
	};

	constexpr bool Blocks(Block set, Block flag)
	{
		return (uint8_t(set) & uint8_t(flag)) != 0;
	}

	struct Shape
	{
		ShapeKind Kind = ShapeKind::Box;
		Block Blocks = Block::All;
		Vec2 Center;
		Vec2 Axis = { 1.0f, 0.0f };
		Vec2 Half;
		float Radius = 0.0f;
	};

	constexpr Vec2 Perpendicular(Vec2 v) { return { -v.y, v.x }; }
	constexpr Vec2 Rotate(Vec2 v, Vec2 axis) { return axis * v.x + Perpendicular(axis) * v.y; }

	Shape MakeCircle(Vec2 center, float radius, Block blocks);
	Shape MakeBox(Vec2 center, Vec2 half, float yaw, Block blocks);
	void GetBounds(const Shape& shape, Vec2& min, Vec2& max);
	float DistanceTo(const Shape& shape, Vec2 point);
	bool PushCircleOut(const Shape& shape, Vec2& position, float radius);
	bool SegmentHits(const Shape& shape, Vec2 from, Vec2 to, float& parameter);
	float SegmentDistance(const Shape& shape, Vec2 from, Vec2 to);
	float PointSegmentDistance(Vec2 point, Vec2 from, Vec2 to);
}
