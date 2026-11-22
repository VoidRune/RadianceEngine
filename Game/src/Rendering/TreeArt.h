#pragma once
#include "Rendering/Animation.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace Game
{
	class WorldRenderer;
	struct Level;
	struct LevelView;
	struct PlacedProp;

	inline constexpr uint32_t TreeLeafStreams = 3;
	glm::vec3 TreeLeafSpawn(const PlacedProp& tree, uint32_t stream, float time);

	class TreeArt
	{
	public:
		void Build(const WorldRenderer& renderer, const Level& level);
		void Clear();
		void Draw(WorldRenderer& renderer, const LevelView& view) const;

	private:
		enum class Part : uint8_t
		{
			Root,
			Trunk,
			Crown,
			Cluster,
		};

		struct Joint
		{
			int32_t Parent = -1;
			glm::vec3 Rest{ 0.0f };
			Part Kind = Part::Root;
			float Seed = 0.0f;
			float Reach = 0.0f;
		};

		struct Tree
		{
			glm::mat4 Place{ 1.0f };
			glm::vec2 Center{ 0.0f };
			float Base = 0.0f;
			float Phase = 0.0f;
		};

		std::vector<Tree> m_Trees;
		std::vector<Joint> m_Joints;
		std::vector<uint32_t> m_Bark;
		std::vector<uint32_t> m_Leaves;
	};
}
