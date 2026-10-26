#pragma once
#include "GameShared/Core/Random.h"
#include "GameShared/World/Level.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Game
{
	class LevelBuilder
	{
	public:
		LevelBuilder(MapKind kind, uint32_t seed, int32_t width, int32_t height, TerrainId fill);

		TileMap& GetMap() { return m_Level.Map; }
		Random& GetRandom() { return m_Random; }

		void Fill(const TileRect& area, TerrainId terrain);
		void Fill(const TileRect& area, TerrainId terrain, float height);
		void Raise(const TileRect& area, float height);
		void Stairs(const TileRect& area, Direction up, float bottom, float top, TerrainId terrain);
		void Disc(Vec2 center, float radius, TerrainId terrain);
		void Stroke(Vec2 from, Vec2 to, float width, TerrainId terrain);
		void Reserve(const TileRect& area);
		void Reserve(Vec2 center, float radius);

		void Prop(PropId id, Vec2 position, float yaw = 0.0f);
		bool TryProp(PropId id, Vec2 position, float yaw = 0.0f, float spacing = 0.2f);
		void Wall(WallId style, Vec2 from, Vec2 to);
		int32_t Portal(Vec2 position, Vec2 destination, float destinationFacing, std::string label, bool hidden = false);
		void Region(std::string name, const TileRect& area, bool enemySpawns = true);
		void SetPlayerSpawn(Vec2 position);
		void SetMissionStart(Vec2 position);
		void SetBossArena(Vec2 center, float radius);
		void Station(StationKind kind, Vec2 position, float radius = 2.4f);
		void Dummy(Vec2 position, Vec2 patrol, bool duck = false);
		void SpawnPoint(Vec2 position);
		void LootSpot(Vec2 position, bool vault = false);
		void Pad(Vec2 position, PadKind kind);
		void Roof(const TileRect& area, float height);
		void SetRounded();
		const Level& GetLevel() const { return m_Level; }
		Level Finish();

	private:
		bool IsFootprintFree(const std::vector<Shape>& footprint, float spacing) const;
		std::vector<Shape> Footprint(PropId id, Vec2 position, float yaw) const;

		Level m_Level;
		Random m_Random;
		std::vector<uint8_t> m_Reserved;
		std::vector<Shape> m_Occupied;
	};

	int8_t ToLevel(float height);

	struct Lobe
	{
		Vec2 Middle;
		float Radius = 1.0f;
		float Phase[3] = {};
		float Wobble = 0.0f;

		float Reach(float angle) const;
		bool Contains(Vec2 point) const;
	};

	Lobe MakeLobe(Random& random, Vec2 middle, float radius, float wobble);
}
