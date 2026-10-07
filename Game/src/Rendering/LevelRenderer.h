#pragma once
#include <GameShared/World/Level.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace Game
{
	class WorldRenderer;

	struct LevelView
	{
		Vec2 Min;
		Vec2 Max;
		Vec2 Focus;
		float FocusHeight = 0.0f;
		bool Cutaway = true;
		float Time = 0.0f;
		int32_t HoveredPortal = -1;
		int32_t ReachablePortal = -1;
	};

	class LevelRenderer
	{
	public:
		void Draw(WorldRenderer& renderer, const Level& level, const LevelView& view);

	private:
		void DrawTerrain(WorldRenderer& renderer, const Level& level, const LevelView& view) const;
		void DrawWalls(WorldRenderer& renderer, const Level& level, const LevelView& view) const;
		void DrawProps(WorldRenderer& renderer, const Level& level, const LevelView& view);
		void DrawPortals(WorldRenderer& renderer, const Level& level, const LevelView& view) const;

		const Level* m_Level = nullptr;
		std::vector<float> m_PropReach;
		std::vector<float> m_PropTop;
	};

	glm::vec3 ToColor(uint32_t rgb);
}
