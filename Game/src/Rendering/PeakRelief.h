#pragma once
#include <GameShared/World/Level.h>
#include <array>
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
		static constexpr float StepRise = 0.26f;
		static constexpr float StepDepth = 0.55f;
		static constexpr float ShelfFade = 1.6f;

		struct Shelf
		{
			float Angle = 0.0f;
			float Span = 0.0f;
			float Radius = 0.0f;
			int32_t Steps = 1;
			bool Outer = false;
			std::array<float, 3> Rise{};
			uint32_t Salt = 0;
		};

		struct Crater
		{
			Vec2 Center;
			float Radius = 1.0f;
		};

		static float Edge(const Shelf& shelf, int32_t step, float angle);

		void Build(const Level& level);
		void Clear();
		bool IsEmpty() const { return m_Heights.empty(); }
		float Height(Vec2 position) const;
		Vec2 Slope(Vec2 position) const;
		float Footing(Vec2 position, float radius) const;
		float Hollow(Vec2 position) const;
		float Scree(Vec2 position) const;
		float Seam(Vec2 position) const;
		float StepShadow(Vec2 position) const;
		float GetLowest() const { return m_Lowest; }
		float GetHighest() const { return m_Highest; }
		float GetPlane() const { return 0.5f * (m_Lowest + m_Highest); }
		float GetSteepest() const { return m_Steepest; }
		const Plateau& GetPlateau() const { return *m_Peak; }
		const std::vector<Shelf>& GetShelves() const { return m_Shelves; }
		const std::vector<Crater>& GetCraters() const { return m_Craters; }

	private:
		bool Fits(const Level& level, const Shelf& shelf) const;
		float Lift(const Shelf& shelf, Vec2 offset, float& near, float& fade) const;

		const Plateau* m_Peak = nullptr;
		std::vector<float> m_Heights;
		std::vector<float> m_Hollows;
		std::vector<float> m_Screes;
		std::vector<float> m_Seams;
		std::vector<Shelf> m_Shelves;
		std::vector<Crater> m_Craters;
		float m_Lowest = 0.0f;
		float m_Highest = 0.0f;
		float m_Steepest = 0.0f;
	};
}
