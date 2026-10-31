#pragma once
#include <GameShared/World/Level.h>
#include <vector>

namespace Game
{
	class PeakRelief
	{
	public:
		static constexpr float MaxSlope = 0.25f;
		static constexpr float MaxRange = 1.45f;
		static constexpr float DaisTop = 3.6f;
		static constexpr float LevelLip = 1.2f;

		void Build(const Level& level);
		void Clear();
		bool IsEmpty() const { return m_Heights.empty(); }
		float Height(Vec2 position) const;
		Vec2 Slope(Vec2 position) const;
		float Footing(Vec2 position, float radius) const;
		float Hollow(Vec2 position) const;
		float Scree(Vec2 position) const;
		float GetLowest() const { return m_Lowest; }
		float GetHighest() const { return m_Highest; }
		float GetPlane() const { return 0.5f * (m_Lowest + m_Highest); }
		float GetSteepest() const { return m_Steepest; }
		const Plateau& GetPlateau() const { return *m_Peak; }

	private:
		const Plateau* m_Peak = nullptr;
		std::vector<float> m_Heights;
		std::vector<float> m_Hollows;
		std::vector<float> m_Screes;
		float m_Lowest = 0.0f;
		float m_Highest = 0.0f;
		float m_Steepest = 0.0f;
	};
}
