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

	inline constexpr float VoidLedge = 0.15f;

	Vec2 MoveCircle(const Level& level, Vec2 position, float radius, Vec2 delta, std::span<const BodyCircle> bodies = {}, bool overVoid = false);
	Vec2 ResolveCircle(const Level& level, Vec2 position, float radius, bool overVoid = false);
	bool IsOverVoid(const Level& level, Vec2 position);
	bool CrossesVoid(const Level& level, Vec2 from, Vec2 to);
	float GetVoidClearance(const Level& level, Vec2 point, float limit);
	float FloorAt(const TileMap& map, Vec2 position, float fallback = NoFloor);
	float LaunchFloor(const Level& level, Vec2 position, float fallback = NoFloor);
	float CastSegment(const Level& level, Vec2 from, Vec2 to, Block mask, float* floor = nullptr);
	bool HasLineOfFire(const Level& level, Vec2 from, Vec2 to);
	bool IsPathClear(const Level& level, Vec2 from, Vec2 to, float radius);
	float GetClearance(const Level& level, Vec2 point, float limit);
	bool IsFree(const Level& level, Vec2 position, float radius);
}
