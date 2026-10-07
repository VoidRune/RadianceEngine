#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr int32_t CaveSize = 64;
		constexpr int32_t RoomGap = 2;
		constexpr int32_t BorderMargin = 2;
		constexpr int32_t SpawnRoomSize = 9;
		constexpr int32_t LairSize = 15;
		constexpr float DaisHeight = 0.5f;

		struct Point
		{
			int32_t X = 0;
			int32_t Y = 0;
		};

		Point CenterOf(const TileRect& room)
		{
			return { room.X + room.Width / 2, room.Y + room.Height / 2 };
		}

		bool Overlaps(const TileRect& a, const TileRect& b, int32_t gap)
		{
			return a.X - gap < b.Right() && b.X - gap < a.Right() && a.Y - gap < b.Top() && b.Y - gap < a.Top();
		}

		int32_t DistanceSquared(Point a, Point b)
		{
			return (a.X - b.X) * (a.X - b.X) + (a.Y - b.Y) * (a.Y - b.Y);
		}

		void CarveFloor(TileMap& map, int32_t x, int32_t y)
		{
			if (x >= 1 && y >= 1 && x < map.GetWidth() - 1 && y < map.GetHeight() - 1)
				map.At(x, y).Terrain = TerrainId::CaveFloor;
		}

		void CarveSegment(TileMap& map, Point from, Point to, int32_t width)
		{
			const int32_t stepX = to.X > from.X ? 1 : to.X < from.X ? -1 : 0;
			const int32_t stepY = to.Y > from.Y ? 1 : to.Y < from.Y ? -1 : 0;
			Point point = from;
			while (true)
			{
				for (int32_t dy = 0; dy < width; dy++)
					for (int32_t dx = 0; dx < width; dx++)
						CarveFloor(map, point.X + dx - width / 2, point.Y + dy - width / 2);
				if (point.X == to.X && point.Y == to.Y)
					return;
				point.X += stepX;
				point.Y += stepY;
			}
		}

		void Erode(TileMap& map, Random& random)
		{
			std::vector<Point> openings;
			for (int32_t y = 2; y < map.GetHeight() - 2; y++)
				for (int32_t x = 2; x < map.GetWidth() - 2; x++)
				{
					if (map.GetTerrain(x, y) != TerrainId::Rock)
						continue;
					const int32_t floors = int32_t(map.IsWalkable(x + 1, y)) + int32_t(map.IsWalkable(x - 1, y)) + int32_t(map.IsWalkable(x, y + 1)) + int32_t(map.IsWalkable(x, y - 1));
					if (floors >= 1 && random.Below(100) < 7)
						openings.push_back({ x, y });
				}
			for (Point point : openings)
				map.At(point.X, point.Y).Terrain = TerrainId::CaveFloor;
		}

		void RemoveUnreachable(TileMap& map, Point start)
		{
			const int32_t width = map.GetWidth();
			std::vector<uint8_t> reached(size_t(width) * size_t(map.GetHeight()), 0);
			std::vector<Point> stack = { start };
			reached[size_t(start.Y) * width + start.X] = 1;
			while (!stack.empty())
			{
				const Point point = stack.back();
				stack.pop_back();
				const Point neighbors[4] = { { point.X + 1, point.Y }, { point.X - 1, point.Y }, { point.X, point.Y + 1 }, { point.X, point.Y - 1 } };
				for (Point next : neighbors)
				{
					if (!map.IsWalkable(next.X, next.Y) || reached[size_t(next.Y) * width + next.X])
						continue;
					reached[size_t(next.Y) * width + next.X] = 1;
					stack.push_back(next);
				}
			}
			for (int32_t y = 0; y < map.GetHeight(); y++)
				for (int32_t x = 0; x < width; x++)
					if (map.IsWalkable(x, y) && !reached[size_t(y) * width + x])
						map.At(x, y).Terrain = TerrainId::Rock;
		}

		void AddDais(LevelBuilder& builder, const TileRect& room)
		{
			const TileRect dais = room.Shrink(2);
			if (dais.Width < 4 || dais.Height < 4)
				return;
			builder.Raise(dais, DaisHeight);
			const int32_t middleX = dais.X + dais.Width / 2 - 1;
			const int32_t middleY = dais.Y + dais.Height / 2 - 1;
			builder.Stairs({ middleX, dais.Y - 2, 2, 2 }, Direction::North, 0.0f, DaisHeight, TerrainId::CaveFloor);
			builder.Stairs({ middleX, dais.Top(), 2, 2 }, Direction::South, 0.0f, DaisHeight, TerrainId::CaveFloor);
			builder.Stairs({ dais.X - 2, middleY, 2, 2 }, Direction::East, 0.0f, DaisHeight, TerrainId::CaveFloor);
			builder.Stairs({ dais.Right(), middleY, 2, 2 }, Direction::West, 0.0f, DaisHeight, TerrainId::CaveFloor);
			builder.Reserve({ middleX, dais.Y - 2, 2, 2 });
			builder.Reserve({ middleX, dais.Top(), 2, 2 });
			builder.Reserve({ dais.X - 2, middleY, 2, 2 });
			builder.Reserve({ dais.Right(), middleY, 2, 2 });
			builder.TryProp(PropId::Brazier, dais.GetCenter());
		}

		void AddPillars(LevelBuilder& builder, const TileRect& room, Random& random)
		{
			for (int32_t y = room.Y + 2; y + 2 <= room.Top() - 2; y += 4)
				for (int32_t x = room.X + 2; x + 2 <= room.Right() - 2; x += 4)
					if (random.Below(100) < 70)
						builder.TryProp(random.Below(4) == 0 ? PropId::BrokenPillar : PropId::StonePillar, { float(x) + 1.0f, float(y) + 1.0f }, random.Float() * 6.28f, 0.0f);
		}

		void AddPool(LevelBuilder& builder, const TileRect& room)
		{
			const float radiusX = float(room.Width) * 0.5f - 2.2f;
			const float radiusY = float(room.Height) * 0.5f - 2.2f;
			const Vec2 center = room.GetCenter();
			TileMap& map = builder.GetMap();
			for (int32_t y = room.Y; y < room.Top(); y++)
				for (int32_t x = room.X; x < room.Right(); x++)
				{
					const float dx = (float(x) + 0.5f - center.x) / radiusX;
					const float dy = (float(y) + 0.5f - center.y) / radiusY;
					if (dx * dx + dy * dy <= 1.0f && map.GetTerrain(x, y) == TerrainId::CaveFloor)
						map.At(x, y).Terrain = TerrainId::Water;
				}
		}

		float TerrainGap(const TileMap& map, Vec2 position)
		{
			float gap = 3.0f;
			for (int32_t y = int32_t(std::floor(position.y)) - 3; y <= int32_t(std::floor(position.y)) + 3; y++)
				for (int32_t x = int32_t(std::floor(position.x)) - 3; x <= int32_t(std::floor(position.x)) + 3; x++)
					if (!map.IsWalkable(x, y))
					{
						const float dx = std::max({ float(x) - position.x, 0.0f, position.x - float(x + 1) });
						const float dy = std::max({ float(y) - position.y, 0.0f, position.y - float(y + 1) });
						gap = std::min(gap, std::sqrt(dx * dx + dy * dy));
					}
			return gap;
		}

		void Scatter(LevelBuilder& builder, const TileRect& room, Random& random)
		{
			const auto spot = [&]
			{
				return Vec2(float(room.X) + 0.5f + random.Float() * float(room.Width - 1), float(room.Y) + 0.5f + random.Float() * float(room.Height - 1));
			};
			const uint32_t clutter = uint32_t(room.Width * room.Height / 14);
			for (uint32_t i = 0; i < clutter; i++)
			{
				const uint32_t roll = random.Below(100);
				const PropId id = roll < 22 ? PropId::Crystal : roll < 40 ? PropId::Mushrooms : roll < 56 ? PropId::Rubble : roll < 66 ? PropId::Bones : roll < 76 ? PropId::Rock : roll < 86 ? PropId::Barrel : roll < 94 ? PropId::Crate : PropId::Boulder;
				const Vec2 position = spot();
				const float yaw = random.Float() * 6.28f;
				const std::span<const PropCollider> colliders = GetProp(id).Colliders;
				if (colliders.empty())
				{
					builder.TryProp(id, position, yaw, 0.2f);
					continue;
				}
				float reach = 0.0f;
				for (const PropCollider& collider : colliders)
					reach = std::max(reach, Length(collider.Offset) + std::max(collider.Radius, Length(collider.Half)));
				if (TerrainGap(builder.GetMap(), position) >= reach + 1.6f)
					builder.TryProp(id, position, yaw, 1.6f);
			}
		}
	}

	Level BuildCavesLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Caves, seed, CaveSize, CaveSize, TerrainId::Rock);
		TileMap& map = builder.GetMap();
		Random& random = builder.GetRandom();

		std::vector<TileRect> rooms;
		rooms.push_back({ CaveSize / 2 - SpawnRoomSize / 2, CaveSize / 2 - SpawnRoomSize / 2, SpawnRoomSize, SpawnRoomSize });
		const int32_t lairX = random.Below(2) == 0 ? BorderMargin + 1 : CaveSize - BorderMargin - 1 - LairSize;
		const int32_t lairY = random.Below(2) == 0 ? BorderMargin + 1 : CaveSize - BorderMargin - 1 - LairSize;
		rooms.push_back({ lairX, lairY, LairSize, LairSize });
		const size_t maxRooms = size_t(std::max(4, CaveSize * CaveSize / 280));
		for (int32_t attempt = 0; attempt < 800 && rooms.size() < maxRooms; attempt++)
		{
			const int32_t roomWidth = random.Range(6, 13);
			const int32_t roomHeight = random.Range(6, 13);
			const TileRect room{ random.Range(BorderMargin, CaveSize - BorderMargin - roomWidth), random.Range(BorderMargin, CaveSize - BorderMargin - roomHeight), roomWidth, roomHeight };
			if (std::none_of(rooms.begin(), rooms.end(), [&](const TileRect& other) { return Overlaps(room, other, RoomGap); }))
				rooms.push_back(room);
		}
		for (const TileRect& room : rooms)
			for (int32_t y = room.Y; y < room.Top(); y++)
				for (int32_t x = room.X; x < room.Right(); x++)
					CarveFloor(map, x, y);

		const size_t count = rooms.size();
		std::vector<std::pair<size_t, size_t>> edges;
		std::vector<uint8_t> connected(count, 0);
		connected[0] = 1;
		for (size_t added = 1; added < count; added++)
		{
			std::pair<size_t, size_t> best = { 0, 0 };
			int32_t bestDistance = INT32_MAX;
			for (size_t a = 0; a < count; a++)
				for (size_t b = 0; b < count; b++)
					if (connected[a] && !connected[b])
						if (const int32_t distance = DistanceSquared(CenterOf(rooms[a]), CenterOf(rooms[b])); distance < bestDistance)
						{
							bestDistance = distance;
							best = { a, b };
						}
			connected[best.second] = 1;
			edges.push_back(best);
		}
		for (size_t a = 0; a < count; a++)
			for (size_t b = a + 1; b < count; b++)
				if (DistanceSquared(CenterOf(rooms[a]), CenterOf(rooms[b])) < 22 * 22 && random.Below(100) < 15)
					edges.push_back({ a, b });
		for (const auto& [a, b] : edges)
		{
			const Point from = CenterOf(rooms[a]);
			const Point to = CenterOf(rooms[b]);
			const int32_t width = random.Below(3) == 0 ? 3 : 2;
			const Point corner = random.Below(2) == 0 ? Point{ to.X, from.Y } : Point{ from.X, to.Y };
			CarveSegment(map, from, corner, width);
			CarveSegment(map, corner, to, width);
		}

		std::vector<uint8_t> roomKind(count, 0);
		for (size_t i = 2; i < count; i++)
		{
			const TileRect& room = rooms[i];
			const uint32_t roll = random.Below(100);
			if (room.Width * room.Height >= 64 && roll < 30)
				roomKind[i] = 1;
			else if (room.Width * room.Height >= 64 && roll < 55)
				roomKind[i] = 2;
			else if (room.Width * room.Height >= 48 && roll < 75)
				roomKind[i] = 3;
		}
		for (size_t i = 1; i < count; i++)
			if (roomKind[i] == 3)
				AddPool(builder, rooms[i]);

		Erode(map, random);
		for (int32_t x = 0; x < CaveSize; x++)
		{
			map.At(x, 0).Terrain = TerrainId::Rock;
			map.At(x, CaveSize - 1).Terrain = TerrainId::Rock;
		}
		for (int32_t y = 0; y < CaveSize; y++)
		{
			map.At(0, y).Terrain = TerrainId::Rock;
			map.At(CaveSize - 1, y).Terrain = TerrainId::Rock;
		}
		RemoveUnreachable(map, CenterOf(rooms[0]));

		builder.SetPlayerSpawn(rooms[0].GetCenter());
		builder.Reserve(rooms[0]);
		builder.Reserve(rooms[1]);
		builder.SetBossArena(rooms[1].GetCenter(), float(LairSize) * 0.5f - 1.0f);
		for (size_t i = 1; i < count; i++)
		{
			if (roomKind[i] == 1)
				AddDais(builder, rooms[i]);
			else if (roomKind[i] == 2)
				AddPillars(builder, rooms[i], random);
		}
		for (size_t i = 1; i < count; i++)
		{
			if (i == 1)
			{
				builder.Region("Wyrm's lair", rooms[i], false);
				continue;
			}
			Scatter(builder, rooms[i], random);
			builder.Region("Cavern " + std::to_string(i), rooms[i]);
		}
		return builder.Finish();
	}
}
