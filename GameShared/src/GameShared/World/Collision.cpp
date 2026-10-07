#include "Collision.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		Shape TileShape(int32_t x, int32_t y)
		{
			return MakeBox({ float(x) + 0.5f, float(y) + 0.5f }, { 0.5f, 0.5f }, 0.0f, Block::All);
		}

		float TileDistance(Vec2 point, int32_t x, int32_t y)
		{
			const float dx = std::max({ float(x) - point.x, 0.0f, point.x - float(x + 1) });
			const float dy = std::max({ float(y) - point.y, 0.0f, point.y - float(y + 1) });
			return std::sqrt(dx * dx + dy * dy);
		}

		bool TerrainBlocks(const TileMap& map, int32_t x, int32_t y, Block mask)
		{
			return (Blocks(mask, Block::Movement) && !map.IsWalkable(x, y)) || (Blocks(mask, Block::Projectiles) && map.BlocksProjectiles(x, y));
		}

		Vec2 KeepOut(Vec2 from, Vec2 to, float radius, const BodyCircle& body)
		{
			const float contact = radius + body.Radius;
			const Vec2 offset = to - body.Position;
			const float distanceSquared = Dot(offset, offset);
			if (distanceSquared >= contact * contact)
				return to;
			const float allowed = std::min(contact, Length(from - body.Position));
			const float distance = std::sqrt(distanceSquared);
			if (distance >= allowed)
				return to;
			return distance > 1e-5f ? body.Position + offset * (allowed / distance) : from;
		}
	}

	Vec2 MoveCircle(const Level& level, Vec2 position, float radius, Vec2 delta, std::span<const BodyCircle> bodies)
	{
		const int32_t steps = std::max(1, int32_t(std::ceil(Length(delta) / (radius * 0.5f))));
		const Vec2 step = delta / float(steps);
		for (int32_t i = 0; i < steps; i++)
		{
			const Vec2 from = position;
			position += step;
			for (int32_t pass = 0; pass < (bodies.empty() ? 0 : 2); pass++)
				for (const BodyCircle& body : bodies)
					position = KeepOut(from, position, radius, body);
			position = ResolveCircle(level, position, radius);
			position = ResolveCircle(level, position, radius);
		}
		return position;
	}

	Vec2 ResolveCircle(const Level& level, Vec2 position, float radius)
	{
		const TileMap& map = level.Map;
		const int32_t minX = int32_t(std::floor(position.x - radius));
		const int32_t maxX = int32_t(std::floor(position.x + radius));
		const int32_t minY = int32_t(std::floor(position.y - radius));
		const int32_t maxY = int32_t(std::floor(position.y + radius));
		for (int32_t y = minY; y <= maxY; y++)
		{
			for (int32_t x = minX; x <= maxX; x++)
			{
				if (map.IsWalkable(x, y))
					continue;
				const Vec2 closest = { std::clamp(position.x, float(x), float(x + 1)), std::clamp(position.y, float(y), float(y + 1)) };
				const Vec2 offset = position - closest;
				const float distanceSquared = Dot(offset, offset);
				if (distanceSquared >= radius * radius)
					continue;
				if (distanceSquared > 1e-12f)
				{
					const float distance = std::sqrt(distanceSquared);
					position += offset * ((radius - distance) / distance);
					continue;
				}
				const float left = position.x - float(x);
				const float right = float(x + 1) - position.x;
				const float down = position.y - float(y);
				const float up = float(y + 1) - position.y;
				const float nearest = std::min({ left, right, down, up });
				if (nearest == left)
					position.x = float(x) - radius;
				else if (nearest == right)
					position.x = float(x + 1) + radius;
				else if (nearest == down)
					position.y = float(y) - radius;
				else
					position.y = float(y + 1) + radius;
			}
		}
		level.ForEachShape(minX, minY, maxX, maxY, [&](const Shape& shape)
		{
			if (Blocks(shape.Blocks, Block::Movement))
				PushCircleOut(shape, position, radius);
		});
		return position;
	}

	float CastSegment(const Level& level, Vec2 from, Vec2 to, Block mask)
	{
		const TileMap& map = level.Map;
		const bool projectiles = Blocks(mask, Block::Projectiles);
		float hit = 1.0f;
		const auto testShapes = [&](int32_t x, int32_t y)
		{
			level.ForEachShape(x, y, x, y, [&](const Shape& shape)
			{
				float parameter = 0.0f;
				if ((uint8_t(shape.Blocks) & uint8_t(mask)) != 0 && SegmentHits(shape, from, to, parameter))
					hit = std::min(hit, parameter);
			});
		};

		int32_t x = int32_t(std::floor(from.x));
		int32_t y = int32_t(std::floor(from.y));
		if (TerrainBlocks(map, x, y, mask))
			return 0.0f;
		testShapes(x, y);
		const int32_t endX = int32_t(std::floor(to.x));
		const int32_t endY = int32_t(std::floor(to.y));
		const Vec2 delta = to - from;
		const int32_t stepX = delta.x > 0.0f ? 1 : -1;
		const int32_t stepY = delta.y > 0.0f ? 1 : -1;
		constexpr float Never = 1e30f;
		const float spanX = delta.x != 0.0f ? std::abs(1.0f / delta.x) : Never;
		const float spanY = delta.y != 0.0f ? std::abs(1.0f / delta.y) : Never;
		float nextX = delta.x != 0.0f ? (stepX > 0 ? float(x + 1) - from.x : from.x - float(x)) * spanX : Never;
		float nextY = delta.y != 0.0f ? (stepY > 0 ? float(y + 1) - from.y : from.y - float(y)) * spanY : Never;
		for (int32_t remaining = std::abs(endX - x) + std::abs(endY - y); remaining > 0 && (x != endX || y != endY); remaining--)
		{
			const float crossing = std::min(nextX, nextY);
			if (crossing >= hit)
				break;
			const Vec2 point = from + delta * crossing;
			const int32_t fromX = x;
			const int32_t fromY = y;
			if (std::abs(nextX - nextY) < 1e-5f)
			{
				if (TerrainBlocks(map, x + stepX, y, mask) || TerrainBlocks(map, x, y + stepY, mask))
					return std::min(hit, crossing);
				x += stepX;
				y += stepY;
				nextX += spanX;
				nextY += spanY;
				remaining--;
			}
			else if (nextX < nextY)
			{
				x += stepX;
				nextX += spanX;
			}
			else
			{
				y += stepY;
				nextY += spanY;
			}
			if (TerrainBlocks(map, x, y, mask) || (projectiles && map.IsUpwardLedge(fromX, fromY, x, y, point)))
				return std::min(hit, crossing);
			testShapes(x, y);
		}
		return hit;
	}

	bool HasLineOfFire(const Level& level, Vec2 from, Vec2 to)
	{
		return CastSegment(level, from, to, Block::Projectiles) >= 1.0f;
	}

	bool IsPathClear(const Level& level, Vec2 from, Vec2 to, float radius)
	{
		const TileMap& map = level.Map;
		const int32_t minX = int32_t(std::floor(std::min(from.x, to.x) - radius));
		const int32_t maxX = int32_t(std::floor(std::max(from.x, to.x) + radius));
		const int32_t minY = int32_t(std::floor(std::min(from.y, to.y) - radius));
		const int32_t maxY = int32_t(std::floor(std::max(from.y, to.y) + radius));
		for (int32_t y = minY; y <= maxY; y++)
			for (int32_t x = minX; x <= maxX; x++)
				if (!map.IsWalkable(x, y) && PointSegmentDistance({ float(x) + 0.5f, float(y) + 0.5f }, from, to) < radius + 0.7072f && SegmentDistance(TileShape(x, y), from, to) < radius)
					return false;
		bool clear = true;
		level.ForEachShape(minX, minY, maxX, maxY, [&](const Shape& shape)
		{
			if (clear && Blocks(shape.Blocks, Block::Movement) && SegmentDistance(shape, from, to) < radius)
				clear = false;
		});
		return clear;
	}

	float GetClearance(const Level& level, Vec2 point, float limit)
	{
		const TileMap& map = level.Map;
		const int32_t minX = int32_t(std::floor(point.x - limit));
		const int32_t maxX = int32_t(std::floor(point.x + limit));
		const int32_t minY = int32_t(std::floor(point.y - limit));
		const int32_t maxY = int32_t(std::floor(point.y + limit));
		float best = limit;
		for (int32_t y = minY; y <= maxY; y++)
			for (int32_t x = minX; x <= maxX; x++)
				if (!map.IsWalkable(x, y))
					best = std::min(best, TileDistance(point, x, y));
		if (best <= 0.0f)
			return 0.0f;
		level.ForEachShape(minX, minY, maxX, maxY, [&](const Shape& shape)
		{
			if (Blocks(shape.Blocks, Block::Movement))
				best = std::min(best, std::max(DistanceTo(shape, point), 0.0f));
		});
		return best;
	}

	bool IsFree(const Level& level, Vec2 position, float radius)
	{
		return level.Map.IsWalkable(int32_t(std::floor(position.x)), int32_t(std::floor(position.y))) && GetClearance(level, position, radius + 0.01f) >= radius;
	}
}
