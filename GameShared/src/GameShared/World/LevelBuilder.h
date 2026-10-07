#pragma once
#include "GameShared/Core/Random.h"
#include "GameShared/World/Level.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Game
{
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
		int32_t Portal(Vec2 position, Vec2 destination, float destinationFacing, std::string label);
		void Region(std::string name, const TileRect& area, bool enemySpawns = true);
		void SetPlayerSpawn(Vec2 position);
		void SetMissionStart(Vec2 position);
		void SetBossArena(Vec2 center, float radius);
		void Station(StationKind kind, Vec2 position, float radius = 2.4f);
		void SpawnPoint(Vec2 position);
		void LootSpot(Vec2 position, bool vault = false);
		void Pad(Vec2 position, PadKind kind);
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
}
