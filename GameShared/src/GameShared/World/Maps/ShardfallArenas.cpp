#include "GameShared/Content/RunMap.h"
#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr int32_t ArenaSize = 52;
		constexpr TileRect ArenaBounds = { 2, 2, 48, 48 };
		constexpr float SocketSpacing = 6.0f;
		constexpr float SocketClearance = 1.5f;
		constexpr float SocketPropClearance = 2.0f;
		constexpr float SocketSpawnClearance = 7.0f;
		constexpr uint32_t MaxSockets = 16;
		constexpr float TrailWidth = 2.2f;
		constexpr float RingWidth = 1.2f;
		constexpr float FringeSpacing = 1.8f;

		struct LobeSpec
		{
			Vec2 Middle;
			float Radius = 6.0f;
			float Wobble = 0.1f;
		};

		struct Outline
		{
			std::vector<Lobe> Lobes;

			bool Contains(Vec2 point) const
			{
				return std::any_of(Lobes.begin(), Lobes.end(), [&](const Lobe& lobe) { return lobe.Contains(point); });
			}
		};

		Outline MakeOutline(MapKind kind, std::initializer_list<LobeSpec> specs)
		{
			Random random(0x5A4D3ull * (uint64_t(GetMapDef(kind).Salt) + 1ull));
			Outline outline;
			for (const LobeSpec& spec : specs)
				outline.Lobes.push_back(MakeLobe(random, spec.Middle, spec.Radius, spec.Wobble));
			return outline;
		}

		Vec2 TileCenter(int32_t x, int32_t y)
		{
			return { float(x) + 0.5f, float(y) + 0.5f };
		}

		void Floor(LevelBuilder& builder, const Outline& outline, TerrainId terrain)
		{
			TileMap& map = builder.GetMap();
			for (int32_t y = 0; y < map.GetHeight(); y++)
				for (int32_t x = 0; x < map.GetWidth(); x++)
					if (outline.Contains(TileCenter(x, y)))
						map.At(x, y).Terrain = terrain;
		}

		void Patch(LevelBuilder& builder, const Outline& outline, Vec2 center, float radius, TerrainId terrain)
		{
			TileMap& map = builder.GetMap();
			for (int32_t y = int32_t(center.y - radius); y <= int32_t(center.y + radius); y++)
				for (int32_t x = int32_t(center.x - radius); x <= int32_t(center.x + radius); x++)
					if (map.IsInside(x, y) && map.IsWalkable(x, y) && outline.Contains(TileCenter(x, y)) && LengthSquared(TileCenter(x, y) - center) <= radius * radius)
						map.At(x, y).Terrain = terrain;
		}

		void Trail(LevelBuilder& builder, const Outline& outline, std::initializer_list<Vec2> points, TerrainId terrain, float width = TrailWidth)
		{
			const Vec2* previous = nullptr;
			for (const Vec2& point : points)
			{
				if (previous)
				{
					const float length = Length(point - *previous);
					const int32_t steps = std::max(int32_t(length * 2.0f), 1);
					for (int32_t i = 0; i <= steps; i++)
						Patch(builder, outline, Lerp(*previous, point, float(i) / float(steps)), width * 0.5f, terrain);
				}
				previous = &point;
			}
		}

		void Knoll(LevelBuilder& builder, Vec2 center, float radius)
		{
			builder.Disc(center, radius, TerrainId::Rock);
			builder.Reserve(center, radius + 0.6f);
		}

		void Pond(LevelBuilder& builder, Vec2 center, float radius)
		{
			TileMap& map = builder.GetMap();
			for (int32_t y = int32_t(center.y - radius); y <= int32_t(center.y + radius); y++)
				for (int32_t x = int32_t(center.x - radius); x <= int32_t(center.x + radius); x++)
					if (map.IsInside(x, y) && map.IsWalkable(x, y) && LengthSquared(TileCenter(x, y) - center) <= radius * radius)
						map.At(x, y).Terrain = TerrainId::Water;
			builder.Reserve(center, radius + 0.8f);
		}

		bool Open(const TileMap& map, int32_t x, int32_t y)
		{
			return map.IsInside(x, y) && map.IsWalkable(x, y);
		}

		void Seal(LevelBuilder& builder, Vec2 spawn)
		{
			TileMap& map = builder.GetMap();
			for (int32_t pass = 0; pass < 3; pass++)
				for (int32_t y = 0; y < map.GetHeight(); y++)
					for (int32_t x = 0; x < map.GetWidth(); x++)
					{
						if (!Open(map, x, y))
							continue;
						int32_t closed = 0;
						for (int32_t dy = -1; dy <= 1; dy++)
							for (int32_t dx = -1; dx <= 1; dx++)
								closed += (dx != 0 || dy != 0) && !Open(map, x + dx, y + dy) ? 1 : 0;
						const bool pinched = (!Open(map, x - 1, y) && !Open(map, x + 1, y)) || (!Open(map, x, y - 1) && !Open(map, x, y + 1));
						if (closed >= 6 || pinched)
							map.At(x, y).Terrain = TerrainId::Rock;
					}
			const int32_t width = map.GetWidth();
			std::vector<uint8_t> reached(size_t(width) * size_t(map.GetHeight()), 0);
			const TileCoord start = map.CellAt(spawn);
			std::vector<TileCoord> queue{ start };
			reached[size_t(start.Y * width + start.X)] = 1;
			for (size_t head = 0; head < queue.size(); head++)
			{
				const TileCoord cell = queue[head];
				for (const TileCoord step : { TileCoord{ 1, 0 }, TileCoord{ -1, 0 }, TileCoord{ 0, 1 }, TileCoord{ 0, -1 } })
				{
					const int32_t x = cell.X + step.X;
					const int32_t y = cell.Y + step.Y;
					if (!map.IsInside(x, y) || reached[size_t(y * width + x)] || !Open(map, x, y) || map.GetTerrain(x, y) == TerrainId::Water)
						continue;
					reached[size_t(y * width + x)] = 1;
					queue.push_back({ x, y });
				}
			}
			for (int32_t y = 0; y < map.GetHeight(); y++)
				for (int32_t x = 0; x < width; x++)
					if (Open(map, x, y) && map.GetTerrain(x, y) != TerrainId::Water && !reached[size_t(y * width + x)])
						map.At(x, y).Terrain = TerrainId::Rock;
		}

		void Feature(LevelBuilder& builder, PropId id, Vec2 center, float yaw)
		{
			builder.Prop(id, center, yaw);
			builder.Reserve(center, 1.7f);
		}

		void GuardianRing(LevelBuilder& builder, Vec2 center, float radius, TerrainId floor)
		{
			builder.Disc(center, radius - 0.6f, floor);
			constexpr int32_t Segments = 48;
			for (int32_t i = 0; i < Segments; i++)
				builder.Stroke(center + FromAngle(2.0f * Pi * float(i) / Segments) * (radius - 0.3f), center + FromAngle(2.0f * Pi * float(i + 1) / Segments) * (radius - 0.3f), RingWidth, TerrainId::Cobblestone);
			builder.Reserve(center, radius);
			builder.SetBossArena(center, radius);
			for (const float angle : { -2.6f, -0.5f, 0.9f, 2.3f })
				builder.TryProp(PropId::Crystal, center + FromAngle(angle) * (radius + 0.9f), angle, 0.3f);
		}

		bool Clear(const Level& level, Vec2 point, float radius)
		{
			const TileMap& map = level.Map;
			for (int32_t y = int32_t(std::floor(point.y - radius)); y <= int32_t(std::floor(point.y + radius)); y++)
				for (int32_t x = int32_t(std::floor(point.x - radius)); x <= int32_t(std::floor(point.x + radius)); x++)
					if (!map.IsWalkable(x, y) || map.GetTerrain(x, y) == TerrainId::Water)
						return false;
			return true;
		}

		void PlaceSockets(LevelBuilder& builder, Vec2 spawn, float spawnClearance = SocketSpawnClearance)
		{
			const Level& level = builder.GetLevel();
			const TileMap& map = level.Map;
			std::vector<Vec2> candidates;
			for (int32_t y = 0; y < map.GetHeight(); y++)
				for (int32_t x = 0; x < map.GetWidth(); x++)
				{
					const Vec2 point = TileCenter(x, y);
					if (!Clear(level, point, SocketClearance) || LengthSquared(point - spawn) < spawnClearance * spawnClearance)
						continue;
					if (level.BossArenaRadius > 0.0f && LengthSquared(point - level.BossArena) < (level.BossArenaRadius + 1.0f) * (level.BossArenaRadius + 1.0f))
						continue;
					const bool crowded = std::any_of(level.Props.begin(), level.Props.end(), [&](const PlacedProp& prop) { return LengthSquared(prop.Position - point) < SocketPropClearance * SocketPropClearance; });
					if (!crowded)
						candidates.push_back(point);
				}
			std::vector<Vec2> chosen;
			while (chosen.size() < MaxSockets && !candidates.empty())
			{
				float best = -1.0f;
				size_t pick = 0;
				for (size_t i = 0; i < candidates.size(); i++)
				{
					float nearest = LengthSquared(candidates[i] - spawn) * 0.25f;
					for (const Vec2 socket : chosen)
						nearest = std::min(nearest, LengthSquared(candidates[i] - socket));
					if (nearest > best)
					{
						best = nearest;
						pick = i;
					}
				}
				if (!chosen.empty() && best < SocketSpacing * SocketSpacing)
					break;
				chosen.push_back(candidates[pick]);
			}
			for (const Vec2 socket : chosen)
				builder.Socket(socket);
		}

		void Scatter(LevelBuilder& builder, const Outline& outline, std::initializer_list<PropId> kinds, int32_t count, float spacing)
		{
			Random& random = builder.GetRandom();
			const TileMap& map = builder.GetMap();
			const std::vector<PropId> options(kinds);
			for (int32_t placed = 0, attempt = 0; placed < count && attempt < count * 12; attempt++)
			{
				const Lobe& lobe = outline.Lobes[random.Below(uint32_t(outline.Lobes.size()))];
				const Vec2 spot = lobe.Middle + FromAngle(random.Float() * 2.0f * Pi) * (lobe.Radius * std::sqrt(random.Float()));
				const TileCoord cell = map.CellAt(spot);
				if (!outline.Contains(spot) || !map.IsWalkable(cell.X, cell.Y))
					continue;
				placed += builder.TryProp(options[size_t(attempt) % options.size()], spot, random.Float() * 2.0f * Pi, spacing) ? 1 : 0;
			}
		}

		void Fringe(LevelBuilder& builder, const Outline& outline, PropId kind, int32_t count)
		{
			Random& random = builder.GetRandom();
			for (int32_t placed = 0, attempt = 0; placed < count && attempt < count * 10; attempt++)
			{
				const Lobe& lobe = outline.Lobes[random.Below(uint32_t(outline.Lobes.size()))];
				const float angle = random.Float() * 2.0f * Pi;
				const Vec2 spot = lobe.Middle + FromAngle(angle) * (lobe.Reach(angle) + 0.9f + 1.2f * random.Float());
				const TileCoord cell = builder.GetMap().CellAt(spot);
				if (outline.Contains(spot) || !builder.GetMap().IsInside(cell.X, cell.Y) || builder.GetMap().IsWalkable(cell.X, cell.Y))
					continue;
				const std::vector<PlacedProp>& props = builder.GetLevel().Props;
				if (std::any_of(props.begin(), props.end(), [&](const PlacedProp& prop) { return LengthSquared(prop.Position - spot) < FringeSpacing * FringeSpacing; }))
					continue;
				builder.Prop(kind, spot, random.Float() * 2.0f * Pi);
				placed++;
			}
		}
	}

	Level BuildProvingHollowLevel(uint32_t seed)
	{
		constexpr Vec2 Spawn = { 16.0f, 10.0f };
		constexpr Vec2 Guardian = { 27.0f, 41.5f };
		LevelBuilder builder(MapKind::ProvingHollow, seed, ArenaSize, ArenaSize, TerrainId::Rock);
		const Outline outline = MakeOutline(MapKind::ProvingHollow, {
			{ { 16.0f, 13.0f }, 8.0f, 0.14f },
			{ { 26.0f, 14.0f }, 7.5f, 0.16f },
			{ { 35.0f, 19.0f }, 7.5f, 0.12f },
			{ { 24.0f, 24.0f }, 7.0f, 0.12f },
			{ { 13.0f, 27.0f }, 7.0f, 0.18f },
			{ { 36.0f, 30.0f }, 6.5f, 0.15f },
			{ { 23.0f, 34.0f }, 7.0f, 0.1f },
			{ { 27.0f, 41.0f }, 7.5f, 0.05f },
		});
		Floor(builder, outline, TerrainId::Grass);
		Trail(builder, outline, { Spawn, { 22.0f, 18.0f }, { 24.0f, 25.0f }, { 23.0f, 33.0f }, Guardian }, TerrainId::Dirt);
		Trail(builder, outline, { { 22.0f, 18.0f }, { 34.0f, 19.0f }, { 36.0f, 29.0f } }, TerrainId::Dirt);
		Patch(builder, outline, { 13.0f, 28.0f }, 3.0f, TerrainId::Dirt);
		GuardianRing(builder, Guardian, 6.0f, TerrainId::Dirt);
		builder.SetPlayerSpawn(Spawn);
		builder.Reserve(Spawn, 2.5f);

		Pond(builder, { 9.5f, 16.0f }, 2.4f);
		Knoll(builder, { 35.0f, 24.5f }, 1.8f);
		Knoll(builder, { 7.5f, 26.5f }, 1.6f);
		Knoll(builder, { 19.5f, 38.5f }, 1.5f);
		Feature(builder, PropId::GreatOak, { 29.0f, 12.0f }, 0.4f);
		Feature(builder, PropId::GreatOak, { 17.0f, 24.0f }, 2.1f);
		Feature(builder, PropId::GreatOak, { 31.5f, 31.0f }, 4.0f);
		builder.TryProp(PropId::MossBoulder, { 21.0f, 11.0f }, 0.7f, 0.5f);
		builder.TryProp(PropId::MossBoulder, { 40.0f, 18.0f }, 2.2f, 0.5f);
		builder.TryProp(PropId::MossBoulder, { 26.5f, 29.5f }, 4.4f, 0.5f);

		Seal(builder, Spawn);
		PlaceSockets(builder, Spawn);
		Fringe(builder, outline, PropId::OakTree, 18);
		Fringe(builder, outline, PropId::PineTree, 10);
		Scatter(builder, outline, { PropId::Mushrooms, PropId::Flowers, PropId::GrassTuft, PropId::GrassTuft, PropId::Bush }, 46, 0.4f);
		builder.Region("Proving Hollow", ArenaBounds);
		return builder.Finish();
	}

	Level BuildCrystalQuarryLevel(uint32_t seed)
	{
		constexpr Vec2 Spawn = { 14.0f, 8.0f };
		constexpr Vec2 Guardian = { 32.5f, 39.5f };
		LevelBuilder builder(MapKind::CrystalQuarry, seed, ArenaSize, ArenaSize, TerrainId::Rock);
		const Outline outline = MakeOutline(MapKind::CrystalQuarry, {
			{ { 14.0f, 10.5f }, 7.0f, 0.12f },
			{ { 24.0f, 11.5f }, 6.5f, 0.18f },
			{ { 33.5f, 15.5f }, 7.0f, 0.15f },
			{ { 40.0f, 23.5f }, 5.5f, 0.2f },
			{ { 27.0f, 23.0f }, 6.5f, 0.16f },
			{ { 17.0f, 27.5f }, 7.0f, 0.14f },
			{ { 11.5f, 36.5f }, 6.5f, 0.18f },
			{ { 21.5f, 40.5f }, 6.5f, 0.15f },
			{ { 32.5f, 39.0f }, 7.5f, 0.05f },
		});
		Floor(builder, outline, TerrainId::Dirt);
		Patch(builder, outline, { 33.0f, 16.0f }, 4.5f, TerrainId::CaveFloor);
		Patch(builder, outline, { 12.0f, 36.0f }, 4.0f, TerrainId::CaveFloor);
		Patch(builder, outline, { 27.0f, 23.0f }, 3.0f, TerrainId::Sand);
		Patch(builder, outline, { 24.5f, 9.0f }, 3.0f, TerrainId::Grass);
		Patch(builder, outline, { 19.0f, 31.0f }, 2.5f, TerrainId::Grass);
		Patch(builder, outline, { 21.0f, 43.0f }, 2.8f, TerrainId::Grass);
		Patch(builder, outline, { 41.0f, 26.0f }, 2.2f, TerrainId::Grass);
		GuardianRing(builder, Guardian, 6.0f, TerrainId::CaveFloor);
		builder.SetPlayerSpawn(Spawn);
		builder.Reserve(Spawn, 2.5f);

		Knoll(builder, { 19.5f, 13.5f }, 1.6f);
		Knoll(builder, { 29.0f, 18.5f }, 1.7f);
		Knoll(builder, { 22.0f, 26.0f }, 1.5f);
		Knoll(builder, { 14.5f, 31.5f }, 1.6f);
		Knoll(builder, { 26.0f, 40.0f }, 1.4f);
		Knoll(builder, { 37.5f, 21.0f }, 1.4f);
		Knoll(builder, { 9.5f, 10.0f }, 1.5f);
		Feature(builder, PropId::CrystalSpire, { 35.5f, 11.5f }, 0.3f);
		Feature(builder, PropId::CrystalSpire, { 12.0f, 25.5f }, 1.9f);
		Feature(builder, PropId::CrystalSpire, { 31.0f, 26.0f }, 3.3f);
		Feature(builder, PropId::CrystalSpire, { 17.5f, 42.0f }, 5.0f);
		Feature(builder, PropId::CrystalSpire, { 42.0f, 25.5f }, 4.2f);

		Seal(builder, Spawn);
		PlaceSockets(builder, Spawn);
		Fringe(builder, outline, PropId::Crystal, 22);
		Fringe(builder, outline, PropId::SummitBoulder, 10);
		Scatter(builder, outline, { PropId::Rubble, PropId::Crystal, PropId::Rubble, PropId::Mushrooms }, 28, 0.5f);
		builder.Region("Crystal Quarry", ArenaBounds);
		return builder.Finish();
	}

	Level BuildGlassTerraceLevel(uint32_t seed)
	{
		constexpr Vec2 Spawn = { 25.0f, 6.5f };
		constexpr Vec2 Guardian = { 25.0f, 40.0f };
		LevelBuilder builder(MapKind::GlassTerrace, seed, ArenaSize, ArenaSize, TerrainId::Rock);
		const Outline outline = MakeOutline(MapKind::GlassTerrace, {
			{ { 25.0f, 10.5f }, 8.0f, 0.12f },
			{ { 13.0f, 22.5f }, 8.0f, 0.16f },
			{ { 37.0f, 22.5f }, 8.0f, 0.16f },
			{ { 25.0f, 24.0f }, 8.5f, 0.08f },
			{ { 25.0f, 39.5f }, 8.5f, 0.06f },
		});
		Floor(builder, outline, TerrainId::Grass);
		Patch(builder, outline, { 25.0f, 24.0f }, 6.5f, TerrainId::Flagstone);
		Trail(builder, outline, { Spawn, { 25.0f, 17.0f } }, TerrainId::Flagstone, 3.0f);
		Trail(builder, outline, { { 25.0f, 30.0f }, Guardian }, TerrainId::Flagstone, 3.0f);
		Trail(builder, outline, { { 13.0f, 22.5f }, { 37.0f, 22.5f } }, TerrainId::Cobblestone, 2.4f);
		GuardianRing(builder, Guardian, 6.5f, TerrainId::Flagstone);
		builder.SetPlayerSpawn(Spawn);
		builder.Reserve(Spawn, 2.5f);

		Pond(builder, { 17.0f, 30.5f }, 2.4f);
		Pond(builder, { 33.0f, 30.5f }, 2.4f);
		Pond(builder, { 25.0f, 15.5f }, 1.6f);
		Feature(builder, PropId::CrystalSpire, { 11.0f, 21.0f }, 0.8f);
		Feature(builder, PropId::CrystalSpire, { 39.0f, 24.0f }, 2.6f);
		Feature(builder, PropId::CrystalSpire, { 25.0f, 24.0f }, 4.1f);
		for (const Vec2 spot : { Vec2(20.0f, 20.0f), Vec2(30.0f, 20.0f), Vec2(20.0f, 28.0f), Vec2(30.0f, 28.0f) })
			builder.TryProp(PropId::RuinBrokenColumn, spot, spot.x * 0.7f, 0.4f);

		Seal(builder, Spawn);
		PlaceSockets(builder, Spawn);
		Fringe(builder, outline, PropId::RuinStump, 8);
		Fringe(builder, outline, PropId::Crystal, 14);
		Fringe(builder, outline, PropId::PineTree, 8);
		Scatter(builder, outline, { PropId::Flowers, PropId::GrassTuft, PropId::RuinRubble, PropId::GrassTuft }, 30, 0.5f);
		builder.Region("Glass Terrace", ArenaBounds);
		return builder.Finish();
	}

	namespace
	{
		constexpr int32_t RoomSize = 44;
		constexpr TileRect RoomBounds = { 2, 2, 40, 40 };
		constexpr Vec2 RoomSpawn = { 22.0f, 7.0f };
		constexpr Vec2 RoomDoors[DoorSlots] = { { 10.5f, 35.0f }, { 22.0f, 38.0f }, { 33.5f, 35.0f } };
		constexpr Vec2 RoomAltars[] = { { 17.0f, 22.0f }, { 27.0f, 22.0f } };
		constexpr Vec2 RoomHeart = { 22.0f, 24.0f };
		constexpr Vec2 RoomNeck = { 22.0f, 28.0f };
		constexpr float RoomSocketClearance = 10.0f;
		constexpr uint32_t RoomAttempts = 4;
		constexpr uint32_t MinRoomSockets = 3;

		enum class Obstacle : uint8_t
		{
			Knoll,
			Pond,
			Oak,
			Spire,
			Column,
		};

		struct RoomStyle
		{
			TerrainId Floor;
			TerrainId Trail;
			TerrainId Pad;
			std::initializer_list<Obstacle> Obstacles;
		};

		RoomStyle StyleOf(MapKind kind)
		{
			switch (kind)
			{
			case MapKind::QuarryRoom:
				return { TerrainId::Dirt, TerrainId::CaveFloor, TerrainId::Cobblestone, { Obstacle::Knoll, Obstacle::Knoll, Obstacle::Spire } };
			case MapKind::TerraceRoom:
				return { TerrainId::Grass, TerrainId::Flagstone, TerrainId::Flagstone, { Obstacle::Pond, Obstacle::Spire, Obstacle::Column } };
			default:
				return { TerrainId::Grass, TerrainId::Dirt, TerrainId::Cobblestone, { Obstacle::Pond, Obstacle::Knoll, Obstacle::Oak, Obstacle::Oak } };
			}
		}

		bool Standable(const TileMap& map, Vec2 point)
		{
			const TileCoord cell = map.CellAt(point);
			return map.IsInside(cell.X, cell.Y) && map.IsWalkable(cell.X, cell.Y) && map.GetTerrain(cell.X, cell.Y) != TerrainId::Water;
		}

		std::optional<Level> TryBuildRoom(MapKind kind, uint32_t seed, uint32_t attempt)
		{
			const uint8_t doors = RoomSeedDoors(seed);
			const bool rest = RoomSeedRest(seed);
			const RoomStyle style = StyleOf(kind);
			LevelBuilder builder(kind, seed, RoomSize, RoomSize, TerrainId::Rock);
			Random& random = builder.GetRandom();
			for (uint32_t i = 0; i < attempt * 17u; i++)
				random.Next();
			const auto jitter = [&](Vec2 point, float amount) { return point + Vec2(random.Float() * 2.0f - 1.0f, random.Float() * 2.0f - 1.0f) * amount; };
			Outline outline;
			const auto lobe = [&](Vec2 middle, float radius, float wobble) { outline.Lobes.push_back(MakeLobe(random, middle, radius, wobble)); };
			lobe(jitter({ 22.0f, 10.0f }, 1.0f), 5.5f + random.Float(), 0.12f);
			lobe({ 22.0f, 15.0f }, 4.0f, 0.1f);
			const size_t middle = outline.Lobes.size();
			switch (random.Below(4))
			{
			case 0:
				lobe(jitter(RoomHeart, 1.5f), 9.0f + random.Float(), 0.14f);
				break;
			case 1:
			{
				const float side = random.Below(2) == 0 ? 1.0f : -1.0f;
				lobe(jitter({ 22.0f - 4.0f * side, 16.0f }, 1.0f), 6.5f, 0.16f);
				lobe(jitter({ 22.0f + 4.0f * side, 22.0f }, 1.0f), 6.5f, 0.16f);
				lobe(jitter({ 22.0f - 4.0f * side, 28.0f }, 1.0f), 6.5f, 0.16f);
				break;
			}
			case 2:
				lobe(jitter({ 15.0f, 21.0f }, 1.0f), 7.0f, 0.15f);
				lobe(jitter({ 29.0f, 21.0f }, 1.0f), 7.0f, 0.15f);
				lobe(jitter({ 22.0f, 27.0f }, 1.0f), 6.5f, 0.12f);
				break;
			default:
				lobe(jitter({ 14.0f, 18.0f }, 1.0f), 6.5f, 0.16f);
				lobe(jitter({ 30.0f, 18.0f }, 1.0f), 6.5f, 0.16f);
				lobe(jitter({ 22.0f, 25.0f }, 1.0f), 6.0f, 0.14f);
				break;
			}
			const size_t middleEnd = outline.Lobes.size();
			for (uint8_t slot = 0; slot < DoorSlots; slot++)
				if (doors & (1u << slot))
				{
					lobe(RoomDoors[slot], 3.4f, 0.06f);
					lobe(Lerp(RoomDoors[slot], RoomNeck, 0.5f), 3.8f, 0.1f);
				}
			if (rest)
				for (const Vec2 altar : RoomAltars)
					lobe(altar, 3.0f, 0.05f);

			Floor(builder, outline, style.Floor);
			if (kind == MapKind::QuarryRoom)
			{
				Patch(builder, outline, jitter(RoomHeart, 4.0f), 3.5f, TerrainId::CaveFloor);
				Patch(builder, outline, jitter({ 14.0f, 14.0f }, 2.0f), 2.2f, TerrainId::Grass);
				Patch(builder, outline, jitter({ 31.0f, 27.0f }, 2.0f), 2.2f, TerrainId::Grass);
				Patch(builder, outline, jitter({ 22.0f, 31.0f }, 2.0f), 2.5f, TerrainId::Sand);
			}
			else if (kind == MapKind::TerraceRoom)
				Patch(builder, outline, jitter(RoomHeart, 1.5f), 5.0f, TerrainId::Flagstone);
			for (uint8_t slot = 0; slot < DoorSlots; slot++)
				if (doors & (1u << slot))
					Trail(builder, outline, { RoomSpawn, { 22.0f, 18.0f }, Lerp(RoomDoors[slot], RoomNeck, 0.5f), RoomDoors[slot] }, style.Trail, kind == MapKind::TerraceRoom ? 3.0f : TrailWidth);
			for (uint8_t slot = 0; slot < DoorSlots; slot++)
				if (doors & (1u << slot))
				{
					builder.Disc(RoomDoors[slot], 1.7f, style.Pad);
					builder.Reserve(RoomDoors[slot], 2.8f);
				}
			if (rest)
				for (const Vec2 altar : RoomAltars)
				{
					builder.Disc(altar, 1.5f, style.Pad);
					builder.Reserve(altar, 2.6f);
				}
			builder.SetPlayerSpawn(RoomSpawn);
			builder.Reserve(RoomSpawn, 2.5f);

			const std::vector<Obstacle> kinds(style.Obstacles);
			const uint32_t wanted = 2u + random.Below(3);
			const uint32_t count = wanted > attempt ? wanted - attempt : 0u;
			std::vector<std::pair<Vec2, float>> placed;
			for (uint32_t tries = 0; placed.size() < count && tries < count * 20u; tries++)
			{
				const Lobe& host = outline.Lobes[middle + random.Below(uint32_t(middleEnd - middle))];
				const Vec2 spot = host.Middle + FromAngle(random.Float() * 2.0f * Pi) * (host.Radius * 0.75f * std::sqrt(random.Float()));
				const Obstacle obstacle = kinds[random.Below(uint32_t(kinds.size()))];
				const float radius = obstacle == Obstacle::Pond ? 1.6f + 0.6f * random.Float() : obstacle == Obstacle::Knoll ? 1.3f + 0.5f * random.Float() : 1.2f;
				bool blocked = !outline.Contains(spot) || LengthSquared(spot - RoomSpawn) < 6.5f * 6.5f;
				for (uint8_t slot = 0; slot < DoorSlots && !blocked; slot++)
					blocked = LengthSquared(spot - RoomDoors[slot]) < 5.5f * 5.5f;
				for (const Vec2 altar : RoomAltars)
					blocked = blocked || (rest && LengthSquared(spot - altar) < 4.5f * 4.5f);
				for (const auto& [other, size] : placed)
					blocked = blocked || Length(spot - other) < radius + size + 3.0f;
				if (blocked)
					continue;
				switch (obstacle)
				{
				case Obstacle::Knoll:
					Knoll(builder, spot, radius);
					break;
				case Obstacle::Pond:
					Pond(builder, spot, radius);
					break;
				case Obstacle::Oak:
					Feature(builder, PropId::GreatOak, spot, random.Float() * 2.0f * Pi);
					break;
				case Obstacle::Spire:
					Feature(builder, PropId::CrystalSpire, spot, random.Float() * 2.0f * Pi);
					break;
				case Obstacle::Column:
					builder.TryProp(PropId::RuinBrokenColumn, spot, random.Float() * 2.0f * Pi, 0.4f);
					builder.Reserve(spot, 1.6f);
					break;
				}
				placed.push_back({ spot, radius });
			}

			Seal(builder, RoomSpawn);
			const TileMap& map = builder.GetMap();
			for (uint8_t slot = 0; slot < DoorSlots; slot++)
				if ((doors & (1u << slot)) && !Standable(map, RoomDoors[slot]))
					return std::nullopt;
			if (rest && !std::all_of(std::begin(RoomAltars), std::end(RoomAltars), [&](Vec2 altar) { return Standable(map, altar); }))
				return std::nullopt;
			PlaceSockets(builder, RoomSpawn, RoomSocketClearance);
			if (builder.GetLevel().Sockets.size() < MinRoomSockets && attempt + 1 < RoomAttempts)
				return std::nullopt;

			switch (kind)
			{
			case MapKind::QuarryRoom:
				Fringe(builder, outline, PropId::Crystal, 14);
				Fringe(builder, outline, PropId::SummitBoulder, 6);
				Scatter(builder, outline, { PropId::Rubble, PropId::Crystal, PropId::Rubble, PropId::Mushrooms }, 16, 0.5f);
				break;
			case MapKind::TerraceRoom:
				Fringe(builder, outline, PropId::RuinStump, 5);
				Fringe(builder, outline, PropId::Crystal, 9);
				Fringe(builder, outline, PropId::PineTree, 5);
				Scatter(builder, outline, { PropId::Flowers, PropId::GrassTuft, PropId::RuinRubble, PropId::GrassTuft }, 18, 0.5f);
				break;
			default:
				Fringe(builder, outline, PropId::OakTree, 12);
				Fringe(builder, outline, PropId::PineTree, 6);
				Scatter(builder, outline, { PropId::Mushrooms, PropId::Flowers, PropId::GrassTuft, PropId::GrassTuft, PropId::Bush }, 26, 0.4f);
				break;
			}
			builder.Region(GetMapDef(kind).Name, RoomBounds);
			Level level = builder.Finish();
			level.Doors.assign(std::begin(RoomDoors), std::end(RoomDoors));
			level.DoorMask = doors;
			if (rest)
				level.Altars.assign(std::begin(RoomAltars), std::end(RoomAltars));
			return level;
		}
	}

	Level BuildShardRoomLevel(MapKind kind, uint32_t seed)
	{
		for (uint32_t attempt = 0; attempt + 1 < RoomAttempts; attempt++)
			if (std::optional<Level> level = TryBuildRoom(kind, seed, attempt))
				return std::move(*level);
		std::optional<Level> level = TryBuildRoom(kind, seed, RoomAttempts + 2);
		return level ? std::move(*level) : BuildProvingHollowLevel(seed);
	}
}
