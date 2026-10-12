#pragma once
#include "GameShared/Content/Props.h"
#include "GameShared/Content/Terrain.h"
#include "GameShared/World/Shapes.h"
#include "GameShared/World/TileMap.h"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

namespace Game
{
	enum class MapKind : uint8_t
	{
		Town,
		Caves,
		Hub,
		Crucible,
		Ashlands,
		Abbey,
		Caldera,
		Count,
	};

	struct MapDef
	{
		MapKind Id = MapKind::Town;
		const char* Name = "";
		const char* Description = "";
	};

	inline constexpr MapDef Maps[] = {
		{ MapKind::Town, "Old Town", "A paved square below a grand library raised on a terrace, a wooded grove and the old quarter. The Archivist haunts the square." },
		{ MapKind::Caves, "Caves", "Random caverns joined by tunnels, with raised ledges, pools, pillars and glowing crystals. A crystal wyrm sleeps in the deepest cavern." },
		{ MapKind::Hub, "Emberhold", "The camp between expeditions, with a stash, a forge, a lapidary, the mission board and the proving grounds." },
		{ MapKind::Crucible, "The Crucible", "A walled fighting pit around a low dais, with broken colonnades, cover walls and pools of black water. Shrines hand out healing and power." },
		{ MapKind::Ashlands, "The Ashlands", "A burned island: a ruined village, a drowned chapel, a walled graveyard, crystal hollows, a sunken library, farmland and the watchtower hill. Chests everywhere, and the ash storm closes in." },
		{ MapKind::Abbey, "Vesper Abbey", "A rain-soaked abbey courtyard at dusk, with a walled garden, a storage yard and a candlelit crypt." },
		{ MapKind::Caldera, "The Caldera", "A disc of basalt floating on a lake of lava inside a volcano. It crumbles from the edge inward every round." },
	};
	static_assert(std::size(Maps) == size_t(MapKind::Count));

	inline const MapDef& GetMapDef(MapKind kind)
	{
		return Maps[size_t(kind) < std::size(Maps) ? size_t(kind) : 0];
	}

	inline bool IsMissionMap(MapKind kind)
	{
		return kind == MapKind::Town || kind == MapKind::Caves;
	}

	inline bool IsArenaMap(MapKind kind)
	{
		return kind == MapKind::Crucible;
	}

	inline bool IsRoyaleMap(MapKind kind)
	{
		return kind == MapKind::Ashlands || IsMissionMap(kind);
	}

	inline bool IsBrawlMap(MapKind kind)
	{
		return kind == MapKind::Caldera;
	}

	enum class StationKind : uint8_t
	{
		Stash,
		Forge,
		Lapidary,
		MissionBoard,
		ProvingGrounds,
		Count,
	};

	struct StationDef
	{
		StationKind Id = StationKind::Stash;
		const char* Name = "";
		const char* Action = "";
	};

	inline constexpr StationDef Stations[] = {
		{ StationKind::Stash, "Stash", "open the stash" },
		{ StationKind::Forge, "Forge", "work at the forge" },
		{ StationKind::Lapidary, "Lapidary", "cut and corrupt gems" },
		{ StationKind::MissionBoard, "Mission Board", "choose a mission" },
		{ StationKind::ProvingGrounds, "Proving Grounds", "fight other players" },
	};
	static_assert(std::size(Stations) == size_t(StationKind::Count));

	inline const StationDef& GetStation(StationKind kind)
	{
		return Stations[size_t(kind) < std::size(Stations) ? size_t(kind) : 0];
	}

	struct Station
	{
		StationKind Kind = StationKind::Stash;
		Vec2 Position;
		float Radius = 2.4f;
	};

	struct PlacedProp
	{
		PropId Id = PropId::Barrel;
		Vec2 Position;
		float Yaw = 0.0f;
		float Base = 0.0f;
	};

	struct WallSegment
	{
		WallId Style = WallId::Stone;
		Vec2 From;
		Vec2 To;
		float Bottom = 0.0f;
		float Base = 0.0f;
	};

	struct Portal
	{
		Vec2 Position;
		Vec2 Destination;
		float DestinationFacing = 0.0f;
		float Radius = 1.6f;
		std::string Label;
	};

	struct Region
	{
		std::string Name;
		Vec2 Min;
		Vec2 Max;
		bool EnemySpawns = true;

		bool Contains(Vec2 position) const { return position.x >= Min.x && position.y >= Min.y && position.x < Max.x && position.y < Max.y; }
		float GetArea() const { return (Max.x - Min.x) * (Max.y - Min.y); }
		Vec2 GetCenter() const { return (Min + Max) * 0.5f; }
	};

	enum class PadKind : uint8_t
	{
		Health,
		Mana,
		Power,
		Count,
	};

	struct PickupPad
	{
		Vec2 Position;
		PadKind Kind = PadKind::Health;
	};

	struct LootSpot
	{
		Vec2 Position;
		bool Vault = false;
	};

	struct Roof
	{
		Vec2 Min;
		Vec2 Max;
		float Height = 3.0f;
	};

	struct Level
	{
		MapKind Kind = MapKind::Caves;
		uint32_t Seed = 0;
		TileMap Map;
		std::vector<PlacedProp> Props;
		std::vector<WallSegment> Walls;
		std::vector<Portal> Portals;
		std::vector<Region> Regions;
		std::vector<Station> Stations;
		std::vector<Vec2> SpawnPoints;
		std::vector<LootSpot> LootSpots;
		std::vector<PickupPad> Pads;
		std::vector<Roof> Roofs;
		Vec2 PlayerSpawn;
		Vec2 MissionStart;
		Vec2 BossArena;
		float BossArenaRadius = 0.0f;
		std::vector<Shape> Shapes;
		std::vector<uint32_t> BucketStart;
		std::vector<uint32_t> BucketShapes;

		void Finalize();
		const Portal* FindPortal(uint32_t index) const { return index < Portals.size() ? &Portals[index] : nullptr; }
		const Station* FindStation(StationKind kind) const
		{
			for (const Station& station : Stations)
				if (station.Kind == kind)
					return &station;
			return nullptr;
		}
		const Region* FindRegion(Vec2 position) const
		{
			for (const Region& region : Regions)
				if (region.Contains(position))
					return &region;
			return nullptr;
		}

		template<typename Function>
		void ForEachShape(int32_t minX, int32_t minY, int32_t maxX, int32_t maxY, Function&& function) const
		{
			minX = std::max(minX, 0);
			minY = std::max(minY, 0);
			maxX = std::min(maxX, Map.GetWidth() - 1);
			maxY = std::min(maxY, Map.GetHeight() - 1);
			for (int32_t y = minY; y <= maxY; y++)
				for (int32_t x = minX; x <= maxX; x++)
				{
					const size_t tile = size_t(y) * size_t(Map.GetWidth()) + size_t(x);
					for (uint32_t i = BucketStart[tile]; i < BucketStart[tile + 1]; i++)
						function(Shapes[BucketShapes[i]]);
				}
		}
	};

	void AppendPropShapes(const PlacedProp& prop, std::vector<Shape>& shapes);
	Shape MakeWallShape(const WallSegment& wall);
	Level BuildLevel(MapKind kind, uint32_t seed);
	Level BuildTownLevel(uint32_t seed);
	Level BuildCavesLevel(uint32_t seed);
	Level BuildHubLevel(uint32_t seed);
	Level BuildCrucibleLevel(uint32_t seed);
	Level BuildAshlandsLevel(uint32_t seed);
	Level BuildAbbeyLevel(uint32_t seed);
	Level BuildCalderaLevel(uint32_t seed);
}
