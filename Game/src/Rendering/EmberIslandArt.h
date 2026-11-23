#pragma once
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <vector>

namespace Game
{
	class WorldRenderer;
	class SunRenderer;
	struct Level;
	struct LevelView;
	struct EmberSunPulse;

	class EmberIslandArt
	{
	public:
		void Build(WorldRenderer& renderer, const Level& level);
		void Clear();
		bool IsBuilt() const { return !m_Parts.empty(); }
		void Draw(WorldRenderer& renderer, const LevelView& view) const;

	private:
		static constexpr int RuneSlots = 40;

		struct Part
		{
			uint32_t Mesh = 0;
			glm::vec3 Min{ 0.0f };
			glm::vec3 Max{ 0.0f };
		};

		void DrawRunes(WorldRenderer& renderer, float time, const EmberSunPulse& pulse) const;
		void Smoulder(WorldRenderer& renderer, SunRenderer& sun, float time, const glm::vec3& center, const glm::vec3& spanA, const glm::vec3& spanB, const glm::vec3& lamp, float heat, uint32_t seed) const;

		glm::mat4 m_Place{ 1.0f };
		glm::vec3 m_Heart{ 0.0f };
		std::vector<Part> m_Parts;
		std::array<uint8_t, RuneSlots> m_Glyphs{};
		std::vector<std::pair<int, uint32_t>> m_Drifts;

		struct WallRune
		{
			glm::vec3 Center{ 0.0f };
			glm::vec3 Out{ 1.0f, 0.0f, 0.0f };
			float Azimuth = 0.0f;
			uint8_t Glyph = 0;
		};

		std::vector<WallRune> m_WallRunes;
		std::vector<glm::vec4> m_Falls;
	};
}
