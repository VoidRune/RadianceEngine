#pragma once
#include "GameShared/World/Level.h"
#include <span>

namespace Game
{
	struct BodyCircle
	{
		Vec2 Position;
		float Radius = 0.0f;
	};

	Vec2 MoveCircle(const Level& level, Vec2 position, float radius, Vec2 delta, std::span<const BodyCircle> bodies = {});
	Vec2 ResolveCircle(const Level& level, Vec2 position, float radius);
	float CastSegment(const Level& level, Vec2 from, Vec2 to, Block mask);
	bool HasLineOfFire(const Level& level, Vec2 from, Vec2 to);
	bool IsPathClear(const Level& level, Vec2 from, Vec2 to, float radius);
	float GetClearance(const Level& level, Vec2 point, float limit);
	bool IsFree(const Level& level, Vec2 position, float radius);
}
