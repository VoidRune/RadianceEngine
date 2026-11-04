#pragma once
#include "Rendering/WorldRenderer.h"
#include <GameShared/Content/Brawl.h>
#include <GameShared/World/Level.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace Game
{
	struct LevelView;

	class ObservatoryArt
	{
	public:
		void Build(WorldRenderer& renderer, const Level& level);
		void Clear();
		bool IsBuilt() const { return m_Level != nullptr; }
		void Draw(WorldRenderer& renderer, const LevelView& view) const;
		float GroundHeight() const;
		bool IsOverSky(Vec2 position) const;

	private:
		struct Item
		{
			uint32_t Mesh = 0;
			ModelInstance Instance{};
			Vec2 Center;
			float Reach = 0.0f;
		};

		struct Fragment
		{
			uint32_t Mesh = 0;
			glm::vec3 Position{ 0.0f };
			glm::vec3 Axis{ 0.0f, 1.0f, 0.0f };
			float Scale = 1.0f;
			float Phase = 0.0f;
		};

		void BuildGeometry(WorldRenderer& renderer, const Level& level);
		void Dress(WorldRenderer& renderer, const Level& level);
		void AddItem(const WorldRenderer& renderer, uint32_t mesh, const glm::mat4& transform, const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
		void DrawArmillary(WorldRenderer& renderer, const LevelView& view) const;

		const Level* m_Level = nullptr;
		std::vector<Item> m_Items;
		std::vector<std::vector<uint32_t>> m_Armillary;
		std::vector<Fragment> m_Fragments;
	};
}
