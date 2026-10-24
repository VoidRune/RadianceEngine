#pragma once
#include "Rendering/LevelArt.h"
#include <GameShared/Content/Brawl.h>
#include <GameShared/World/Level.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <optional>
#include <vector>

namespace Game
{
	class WorldRenderer;

	inline constexpr float CameraFov = 40.0f;
	inline constexpr float CameraHeight = 15.3f;
	inline constexpr float CameraBack = 9.9f;
	inline constexpr float CameraFocusOffset = 1.2f;
	inline constexpr float LavaSurface = -0.3f;
	inline constexpr float LavaWading = -0.42f;
	inline constexpr float CollapseFallSeconds = 1.1f;
	inline constexpr float RestoreSeconds = 1.3f;

	struct LevelView
	{
		Vec2 Min;
		Vec2 Max;
		Vec2 CameraMin{ -1e9f, -1e9f };
		Vec2 CameraMax{ 1e9f, 1e9f };
		Vec2 Focus;
		float FocusHeight = 0.0f;
		bool Cutaway = true;
		float Time = 0.0f;
		int32_t HoveredPortal = -1;
		int32_t ReachablePortal = -1;
		const BrawlStatus* Brawl = nullptr;
		const std::vector<uint8_t>* Risen = nullptr;
		float Restore = 1.0f;
	};

	class IslandArt
	{
	public:
		void Build(WorldRenderer& renderer, const Level& level);
		void Clear();
		bool IsBuilt() const { return !m_Chunks.empty(); }
		void Draw(WorldRenderer& renderer, const LevelView& view) const;
		std::optional<float> GroundHeight(const BrawlStatus* brawl, Vec2 position) const;

	private:
		struct Corner
		{
			Vec2 Position;
			float Height = 0.0f;
		};

		struct Cell
		{
			CellCoord Coord;
			Vec2 Seed;
			float Peak = 0.0f;
			uint32_t Chunk = 0;
			uint32_t FirstCorner = 0;
			uint32_t CornerCount = 0;
		};

		struct Chunk
		{
			CellCoord Coord;
			glm::vec3 Pivot{ 0.0f };
			Vec2 Min{ 1e9f, 1e9f };
			Vec2 Max{ -1e9f, -1e9f };
			uint32_t Mesh = 0;
			uint32_t Tile = 0;
			bool Coast = false;
			std::vector<uint32_t> Cells;
			std::vector<uint32_t> Neighbours;
		};

		const Cell* FindCell(CellCoord coord) const;
		float SurfaceHeight(const Cell& cell, Vec2 position) const;

		std::vector<Cell> m_Cells;
		std::vector<Corner> m_Corners;
		std::vector<Chunk> m_Chunks;
		std::vector<int32_t> m_Lookup;
		mutable std::vector<uint8_t> m_Solid;
		int32_t m_Width = 0;
		bool m_Frozen = false;
		int32_t m_Height = 0;
	};

	class LevelRenderer
	{
	public:
		void Draw(WorldRenderer& renderer, const Level& level, const LevelView& view);
		float GroundHeight(const Level& level, const BrawlStatus* brawl, Vec2 position) const;
		const std::vector<LevelFire>& GetFires() const { return m_Scene.Fires; }

	private:
		struct SceneKey
		{
			const Level* Source = nullptr;
			MapKind Kind = MapKind::Count;
			uint32_t Seed = 0;
			size_t Props = 0;
			size_t Walls = 0;

			bool operator==(const SceneKey&) const = default;
		};

		void Prepare(WorldRenderer& renderer, const Level& level);
		void DrawTerrain(WorldRenderer& renderer, const Level& level, const LevelView& view) const;
		void DrawLava(WorldRenderer& renderer, const LevelView& view) const;
		void DrawWalls(WorldRenderer& renderer, const Level& level, const LevelView& view) const;
		void DrawProps(WorldRenderer& renderer, const Level& level, const LevelView& view) const;
		void DrawScene(WorldRenderer& renderer, const LevelView& view) const;
		void DrawPortals(WorldRenderer& renderer, const Level& level, const LevelView& view) const;

		SceneKey m_Key;
		LevelScene m_Scene;
		IslandArt m_Island;
		std::vector<float> m_PropReach;
		std::vector<float> m_PropTop;
		std::vector<glm::vec4> m_LavaRuns;
	};

	glm::vec3 ToColor(uint32_t rgb);
}
