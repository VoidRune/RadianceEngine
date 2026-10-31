#include "GameShared/Content/Brawl.h"
#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr int32_t PeakSize = 64;
		constexpr Vec2 Middle = { 32.0f, 32.0f };
		constexpr float GridSpacing = 0.25f;
		constexpr float PlateauArea = 590.0f;
		constexpr int32_t RimSamples = 720;
		constexpr float ExactBand = 4.0f;
		constexpr float MinRimBend = 3.5f;
		constexpr float RitualRadius = 3.6f;
		constexpr uint32_t Monoliths = 6;
		constexpr float MonolithNear = 6.95f;
		constexpr float MonolithFar = 9.0f;
		constexpr float MonolithReach = 0.81f;
		constexpr float SpireReach = 0.62f;
		constexpr float LaneWidth = 5.5f;
		constexpr float RimClearance = 4.0f;
		constexpr float SpawnClearance = 2.5f;
		constexpr float SpawnRimMin = 2.0f;
		constexpr float SpawnRimMax = 4.2f;
		constexpr uint32_t SpawnSlots = 36;
		constexpr float SpawnGap = 2.2f;
		constexpr std::array<int32_t, 3> Harmonics = { 3, 5, 7 };
		constexpr std::array<float, 3> Waves = { 0.07f, 0.05f, 0.03f };

		struct Bay
		{
			float Angle = 0.0f;
			float Width = 0.5f;
			float Depth = 0.0f;
		};

		class Outline
		{
		public:
			explicit Outline(Random& random)
			{
				for (size_t k = 0; k < Harmonics.size(); k++)
				{
					m_Amplitudes[k] = Waves[k] * (0.6f + 0.4f * random.Float());
					m_Phases[k] = random.Float() * 2.0f * Pi;
				}
				const uint32_t bays = 1 + random.Below(2);
				const float first = random.Float() * 2.0f * Pi;
				for (uint32_t i = 0; i < bays; i++)
					m_Bays.push_back({ first + Pi * float(i) + (random.Float() - 0.5f) * 1.2f, 0.42f + 0.13f * random.Float(), 1.0f + 0.5f * random.Float() });
				Normalize();
				for (int32_t pass = 0; pass < 8 && TightestBend() < MinRimBend; pass++)
				{
					for (float& amplitude : m_Amplitudes)
						amplitude *= 0.85f;
					for (Bay& bay : m_Bays)
						bay.Depth *= 0.85f;
					Normalize();
				}
			}

			float At(float angle) const
			{
				float radius = m_Radius;
				for (size_t k = 0; k < Harmonics.size(); k++)
					radius += m_Radius * m_Amplitudes[k] * std::sin(float(Harmonics[k]) * angle + m_Phases[k]);
				for (const Bay& bay : m_Bays)
					if (const float t = std::remainder(angle - bay.Angle, 2.0f * Pi) / bay.Width; std::abs(t) < 1.0f)
					{
						const float c = std::cos(t * Pi * 0.5f);
						radius -= bay.Depth * c * c;
					}
				return radius;
			}

		private:
			float Area() const
			{
				double sum = 0.0;
				for (int32_t i = 0; i < RimSamples; i++)
				{
					const double r = At(2.0f * Pi * float(i) / float(RimSamples));
					sum += r * r;
				}
				return float(sum * Pi / double(RimSamples));
			}

			void Normalize()
			{
				for (int32_t pass = 0; pass < 4; pass++)
					m_Radius *= std::sqrt(PlateauArea / Area());
			}

			float TightestBend() const
			{
				const float step = 2.0f * Pi / float(RimSamples);
				float tightest = 1e9f;
				for (int32_t i = 0; i < RimSamples; i++)
				{
					const float angle = step * float(i);
					const float r = At(angle);
					const float d1 = (At(angle + step) - At(angle - step)) / (2.0f * step);
					const float d2 = (At(angle + step) - 2.0f * r + At(angle - step)) / (step * step);
					const float bend = std::abs(r * r + 2.0f * d1 * d1 - r * d2) / std::pow(r * r + d1 * d1, 1.5f);
					tightest = std::min(tightest, 1.0f / std::max(bend, 1e-6f));
				}
				return tightest;
			}

			std::array<float, Harmonics.size()> m_Amplitudes{};
			std::array<float, Harmonics.size()> m_Phases{};
			std::vector<Bay> m_Bays;
			float m_Radius = 13.7f;
		};

		Plateau Survey(const Outline& outline)
		{
			Plateau peak;
			peak.Spacing = GridSpacing;
			peak.Columns = int32_t(float(PeakSize) / GridSpacing) + 1;
			peak.Rows = peak.Columns;
			peak.Center = Middle;
			peak.Ritual = RitualRadius;
			std::vector<Vec2> edge(RimSamples);
			for (int32_t i = 0; i < RimSamples; i++)
			{
				const float angle = 2.0f * Pi * float(i) / float(RimSamples);
				edge[size_t(i)] = Middle + FromAngle(angle) * outline.At(angle);
			}
			const auto wrap = [](int32_t k) { return size_t(((k % RimSamples) + RimSamples) % RimSamples); };
			const int32_t window = RimSamples / 10;
			peak.Rim.resize(size_t(peak.Columns) * size_t(peak.Rows));
			for (int32_t j = 0; j < peak.Rows; j++)
				for (int32_t i = 0; i < peak.Columns; i++)
				{
					const Vec2 point = peak.Origin + Vec2(float(i), float(j)) * peak.Spacing;
					const Vec2 offset = point - Middle;
					const float reach = Length(offset);
					const float angle = reach > 1e-4f ? AngleOf(offset) : 0.0f;
					const float slope = (outline.At(angle + 1e-3f) - outline.At(angle - 1e-3f)) / 2e-3f;
					float distance = (outline.At(angle) - reach) / std::sqrt(1.0f + slope * slope / std::max(reach * reach, 1.0f));
					if (std::abs(distance) < ExactBand)
					{
						const int32_t near = int32_t(std::lround(angle / (2.0f * Pi) * float(RimSamples)));
						float closest = 1e9f;
						for (int32_t k = near - window; k <= near + window; k++)
							closest = std::min(closest, PointSegmentDistance(point, edge[wrap(k)], edge[wrap(k + 1)]));
						const float exact = std::clamp(ExactBand - std::abs(distance), 0.0f, 1.0f);
						distance = std::lerp(distance, std::copysign(closest, distance), exact * exact * (3.0f - 2.0f * exact));
					}
					peak.Rim[size_t(j) * size_t(peak.Columns) + size_t(i)] = distance;
				}
			return peak;
		}

		struct Blocker
		{
			Vec2 Position;
			float Reach = 0.0f;
		};

		std::vector<Blocker> PlaceMonoliths(Random& random, const Plateau& peak)
		{
			for (uint32_t count = Monoliths; count >= 5; count--)
				for (uint32_t attempt = 0; attempt < 400; attempt++)
				{
					std::vector<Blocker> placed = { { Middle, SpireReach } };
					const float turn = random.Float() * 2.0f * Pi;
					bool fits = true;
					for (uint32_t k = 0; k < count && fits; k++)
					{
						const float angle = turn + 2.0f * Pi * (float(k) + 0.3f * (random.Float() - 0.5f)) / float(count);
						const float radius = MonolithNear + (MonolithFar - MonolithNear) * ((k % 2 == 0 ? 0.0f : 0.55f) + 0.45f * random.Float());
						const Vec2 spot = Middle + FromAngle(angle) * radius;
						fits = peak.RimDistance(spot) >= RimClearance + MonolithReach;
						for (const Blocker& other : placed)
							fits = fits && Length(spot - other.Position) - other.Reach - MonolithReach >= LaneWidth;
						placed.push_back({ spot, MonolithReach });
					}
					if (fits)
						return placed;
				}
			return { { Middle, SpireReach } };
		}

		std::vector<Vec2> PlaceSpawns(const Plateau& peak, const std::vector<Blocker>& blockers)
		{
			std::vector<std::pair<Vec2, float>> open;
			for (float y = 0.5f; y < float(PeakSize); y += 0.5f)
				for (float x = 0.5f; x < float(PeakSize); x += 0.5f)
				{
					const Vec2 spot(x, y);
					const float rim = peak.RimDistance(spot);
					if (rim < SpawnRimMin || rim > SpawnRimMax)
						continue;
					bool clear = true;
					for (const Blocker& blocker : blockers)
						clear = clear && Length(spot - blocker.Position) - blocker.Reach >= SpawnClearance;
					if (clear)
						open.push_back({ spot, 1.0f - std::abs(rim - 0.5f * (SpawnRimMin + SpawnRimMax)) });
				}
			std::vector<Vec2> spawns;
			while (spawns.size() < SpawnSlots && !open.empty())
			{
				size_t best = open.size();
				float score = -1e9f;
				for (size_t n = 0; n < open.size(); n++)
				{
					float nearest = 1e9f;
					for (const Vec2 taken : spawns)
						nearest = std::min(nearest, LengthSquared(open[n].first - taken));
					if (nearest < SpawnGap * SpawnGap)
						continue;
					if (const float value = std::min(nearest, 100.0f) + open[n].second; value > score)
					{
						score = value;
						best = n;
					}
				}
				if (best == open.size())
					break;
				spawns.push_back(open[best].first);
				open.erase(open.begin() + std::ptrdiff_t(best));
			}
			std::sort(spawns.begin(), spawns.end(), [](Vec2 a, Vec2 b) { return AngleOf(a - Middle) < AngleOf(b - Middle); });
			return spawns;
		}
	}

	Level BuildStormPeakLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::StormPeak, seed, PeakSize, PeakSize, TerrainId::Sky);
		Random& random = builder.GetRandom();
		const Outline outline(random);
		Plateau peak = Survey(outline);
		for (int32_t y = 0; y < PeakSize; y++)
			for (int32_t x = 0; x < PeakSize; x++)
				if (peak.RimDistance({ float(x) + 0.5f, float(y) + 0.5f }) > -0.3f)
					builder.Fill({ x, y, 1, 1 }, TerrainId::Slate);
		const std::vector<Blocker> blockers = PlaceMonoliths(random, peak);
		builder.Prop(PropId::StormSpire, Middle, random.Float() * 2.0f * Pi);
		for (size_t i = 1; i < blockers.size(); i++)
			builder.Prop(PropId::Monolith, blockers[i].Position, AngleOf(blockers[i].Position - Middle) + Pi * 0.5f + (random.Float() - 0.5f) * 0.5f);
		for (const Vec2 spawn : PlaceSpawns(peak, blockers))
			builder.SpawnPoint(spawn);
		builder.SetPlayerSpawn(Middle);
		builder.Region("The Storm Peak", { 0, 0, PeakSize, PeakSize }, false);
		Level level = builder.Finish();
		level.Skyborne = true;
		level.Peak = std::move(peak);
		return level;
	}
}
