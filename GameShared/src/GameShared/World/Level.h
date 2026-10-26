#pragma once
#include "GameShared/Content/Chess.h"
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Props.h"
#include "GameShared/Content/Terrain.h"
#include "GameShared/World/Shapes.h"
#include "GameShared/World/TileMap.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
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
		FrozenLake,
		SkyRuins,
		StormPeak,
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
		{ MapKind::FrozenLake, "Frozen Lake", "A long frozen lake in a snowy valley. Shoves slide far on the ice, ice pillars bounce you back, and the thin ice gives way into freezing water around the shores and the air holes." },
		{ MapKind::SkyRuins, "Sky Ruins", "A temple broken into islands high above the clouds. There is no lava, only a long fall: drifting islands carry you out and back, wind gusts push everyone one way, and the ruins crumble into the sky." },
		{ MapKind::StormPeak, "Storm Peak", "A mountain summit high above the clouds, crowned by the old Stormcallers' ritual circle. Lightning hunts anyone who stands still, the storm closes in on the circle, and a shove knocks you down a ledge or off the mountain." },
	};
	static_assert(std::size(Maps) == size_t(MapKind::Count));

	inline constexpr MapKind BrawlMaps[] = { MapKind::Caldera, MapKind::FrozenLake, MapKind::SkyRuins, MapKind::StormPeak };
	inline constexpr MapKind RandomBrawlMap = MapKind::Count;

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
		return kind == MapKind::Caldera || kind == MapKind::FrozenLake || kind == MapKind::SkyRuins || kind == MapKind::StormPeak;
	}

	enum class StationKind : uint8_t
	{
		Stash,
		Forge,
		Lapidary,
		MissionBoard,
		ProvingGrounds,
		Spellbook,
		CalderaGate,
		WishingVoid,
		Chess,
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
		{ StationKind::Spellbook, "Spellbook", "choose your spells" },
		{ StationKind::CalderaGate, "Caldera Gate", "start a Spellbrawl" },
		{ StationKind::WishingVoid, "Wishing Void", "toss a gold coin into the void" },
		{ StationKind::Chess, "Chess Table", "play chess" },
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
		bool Hidden = false;
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

	struct DummySpot
	{
		Vec2 Position;
		Vec2 Patrol;
		bool Duck = false;
	};

	enum class LandmarkKind : uint8_t
	{
		FallBoard,
		Observatory,
		CatIslet,
	};

	struct Landmark
	{
		LandmarkKind Kind = LandmarkKind::FallBoard;
		Vec2 Position;
		float Radius = 2.0f;
	};

	struct Constellation
	{
		AbilityId Spell = AbilityId::EmberBolt;
		Vec2 Position;
		float Height = 0.0f;
		float Radius = 1.3f;
	};

	struct DriftLane
	{
		Vec2 From;
		Vec2 To;
	};

	struct Tether
	{
		Vec2 From;
		Vec2 To;
		float Sag = 0.5f;
		float Lift = 0.3f;
		float Scale = 1.0f;
	};

	struct ChessSpot
	{
		Vec2 Center;
		float Yaw = 0.0f;
	};

	struct DriftIsland
	{
		std::vector<TileCoord> Cells;
		Vec2 Center;
		Vec2 Axis;
		float Reach = 4.0f;
		float Period = 22.0f;
		float Phase = 0.0f;
	};

	inline constexpr int32_t ElevationCliffs = 3;

	struct ElevationRamp
	{
		Vec2 Foot;
		Vec2 Uphill;
		float Width = 0.0f;
		uint8_t Cliff = 0;
		bool Stair = false;
	};

	struct Elevation
	{
		struct Spot
		{
			size_t Index = 0;
			float U = 0.0f;
			float V = 0.0f;
		};

		Vec2 Origin;
		float Spacing = 0.25f;
		int32_t Columns = 0;
		int32_t Rows = 0;
		std::vector<float> Floor;
		std::vector<float> Rim;
		std::array<std::vector<float>, ElevationCliffs> Edges;
		std::array<std::vector<float>, ElevationCliffs> Drops;
		std::vector<ElevationRamp> Ramps;
		Vec2 Summit;
		float Ritual = 0.0f;

		bool IsEmpty() const { return Floor.empty(); }

		Spot Locate(Vec2 position) const
		{
			const float gx = std::clamp((position.x - Origin.x) / Spacing, 0.0f, float(Columns - 1) - 1e-3f);
			const float gy = std::clamp((position.y - Origin.y) / Spacing, 0.0f, float(Rows - 1) - 1e-3f);
			const int32_t i = int32_t(gx);
			const int32_t j = int32_t(gy);
			return { size_t(j) * size_t(Columns) + size_t(i), gx - float(i), gy - float(j) };
		}

		float Read(const std::vector<float>& grid, const Spot& spot) const
		{
			const float* row = grid.data() + spot.Index;
			const float* next = row + Columns;
			return std::lerp(std::lerp(row[0], row[1], spot.U), std::lerp(next[0], next[1], spot.U), spot.V);
		}

		float Height(Vec2 position) const
		{
			const Spot spot = Locate(position);
			float height = Read(Floor, spot);
			for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
				if (Read(Edges[size_t(cliff)], spot) >= 0.0f)
					height += Read(Drops[size_t(cliff)], spot);
			return height;
		}

		float RimDistance(Vec2 position) const { return Read(Rim, Locate(position)); }

		float EdgeDistance(int32_t cliff, Vec2 position) const { return Read(Edges[size_t(cliff)], Locate(position)); }

		Vec2 Gradient(const std::vector<float>& grid, Vec2 position) const
		{
			const float h = Spacing * 0.5f;
			const float dx = Read(grid, Locate(position + Vec2(h, 0.0f))) - Read(grid, Locate(position - Vec2(h, 0.0f)));
			const float dy = Read(grid, Locate(position + Vec2(0.0f, h))) - Read(grid, Locate(position - Vec2(0.0f, h)));
			return Vec2(dx, dy) / (2.0f * h);
		}
	};

	inline Vec2 ChessSquareCenter(const ChessSpot& spot, int32_t file, int32_t rank)
	{
		const Vec2 ranks = FromAngle(spot.Yaw);
		return spot.Center + ranks * ((float(rank) - 3.5f) * Chess::SquareSize) + Vec2(ranks.y, -ranks.x) * ((float(file) - 3.5f) * Chess::SquareSize);
	}

	enum class BoundaryKind : uint8_t
	{
		Ledge,
		Void,
		Blocked,
	};

	struct Boundary
	{
		Vec2 From;
		Vec2 To;
		Vec2 Inside;
		BoundaryKind Kind = BoundaryKind::Ledge;
	};

	struct TileRect
	{
		int32_t X = 0;
		int32_t Y = 0;
		int32_t Width = 0;
		int32_t Height = 0;

		int32_t Right() const { return X + Width; }
		int32_t Top() const { return Y + Height; }
		Vec2 GetCenter() const { return { float(X) + float(Width) * 0.5f, float(Y) + float(Height) * 0.5f }; }
		bool Contains(int32_t x, int32_t y) const { return x >= X && y >= Y && x < Right() && y < Top(); }
		TileRect Shrink(int32_t amount) const { return { X + amount, Y + amount, Width - 2 * amount, Height - 2 * amount }; }
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
		std::vector<DummySpot> Dummies;
		std::vector<Landmark> Landmarks;
		std::vector<Constellation> Constellations;
		std::vector<DriftLane> DriftLanes;
		std::vector<Tether> Tethers;
		std::vector<TileRect> Railings;
		std::optional<ChessSpot> ChessTable;
		std::vector<Boundary> Boundaries;
		std::vector<DriftIsland> Islands;
		Elevation Relief;
		bool Rounded = false;
		bool Floating = false;
		bool Bouncy = false;
		bool Skyborne = false;
		Vec2 PlayerSpawn;
		Vec2 MissionStart;
		Vec2 BossArena;
		float BossArenaRadius = 0.0f;
		std::vector<Shape> Shapes;
		std::vector<uint32_t> BucketStart;
		std::vector<uint32_t> BucketShapes;
		std::vector<uint32_t> ShapeProps;
		std::vector<uint8_t> Sunk;

		void Finalize();
		const Portal* FindPortal(uint32_t index) const { return index < Portals.size() ? &Portals[index] : nullptr; }
		const Station* FindStation(StationKind kind) const
		{
			for (const Station& station : Stations)
				if (station.Kind == kind)
					return &station;
			return nullptr;
		}
		const Landmark* FindLandmark(LandmarkKind kind) const
		{
			for (const Landmark& landmark : Landmarks)
				if (landmark.Kind == kind)
					return &landmark;
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
						if (Sunk.empty() || !Sunk[BucketShapes[i]])
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
	Level BuildFrozenLakeLevel(uint32_t seed);
	Level BuildSkyRuinsLevel(uint32_t seed);
	Level BuildStormPeakLevel(uint32_t seed);
}
