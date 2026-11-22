#pragma once
#include "Rendering/Animation.h"
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <vector>

namespace Game
{
	class WorldRenderer;
	struct Level;
	struct LevelView;
	struct PlacedProp;

	inline constexpr uint32_t PortalRimStreams = 4;
	glm::vec3 PortalRimPoint(const PlacedProp& gate, float along);

	class PortalArt
	{
	public:
		void Build(WorldRenderer& renderer, const Level& level);
		void Clear();
		void Draw(WorldRenderer& renderer, const LevelView& view);

	private:
		struct Cloth
		{
			glm::mat4 Place{ 1.0f };
			glm::vec3 WallPoint{ 0.0f };
			glm::vec3 WallNormal{ 0.0f, 0.0f, 1.0f };
			std::vector<glm::vec3> Position;
			std::vector<glm::vec3> Previous;
			float Seed = 0.0f;
		};

		struct Gate
		{
			glm::mat4 Place{ 1.0f };
			std::array<Cloth, 2> Banners;
			uint32_t Seed = 0;
		};

		struct Link
		{
			uint16_t A = 0;
			uint16_t B = 0;
			float Length = 0.0f;
			float Stiffness = 1.0f;
		};

		glm::vec3 Point(const std::vector<glm::vec3>& points, int32_t column, int32_t row) const;
		glm::mat4 Frame(const std::vector<glm::vec3>& points, int32_t column, int32_t row) const;
		void Reset(Cloth& cloth) const;
		void Step(Cloth& cloth, float dt, float time) const;

		std::vector<Gate> m_Gates;
		std::vector<uint32_t> m_BannerMeshes;
		std::vector<glm::vec3> m_Rest;
		std::vector<uint32_t> m_Joint;
		std::vector<glm::mat4> m_RestInverse;
		std::vector<Link> m_Links;
		uint32_t m_JointCount = 0;
		int32_t m_Columns = 0;
		int32_t m_Rows = 0;
		float m_LastTime = -1.0f;
		float m_Carry = 0.0f;
		float m_Surge = 0.0f;
	};
}
