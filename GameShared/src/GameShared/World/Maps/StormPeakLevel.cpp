#include "GameShared/Content/Brawl.h"
#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr int32_t PeakSize = 64;
		constexpr Vec2 Middle = { 32.0f, 32.0f };
		constexpr float GridSpacing = 0.25f;
		constexpr float SummitLevel = 4.0f;
		constexpr float ConeFlat = 3.6f;
		constexpr float ConeRun = 2.2f;
		constexpr float RidgeFlat = 3.0f;
		constexpr float RidgeFall = 1.5f;
		constexpr float Blend = 0.35f;
		constexpr float Roughness = 0.4f;
		constexpr float Warp = 3.2f;
		constexpr float RampSpacing = 11.0f;
		constexpr float RampGap = 6.5f;
		constexpr float MinGradient = 0.2f;
		constexpr float RampReach = 1.25f;
		constexpr float RampTaper = 0.9f;
		constexpr float StairWidth = 0.95f;
		constexpr float ScreeWidth = 1.3f;
		constexpr float RitualRadius = 3.0f;
		constexpr float StoneRing = 3.6f;
		constexpr uint32_t StoneSlots = 8;
		constexpr uint32_t SpawnSlots = 36;
		constexpr float SpawnGap = 2.2f;

		float Smooth(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float LatticeNoise(Vec2 point, uint32_t salt)
		{
			const float fx = std::floor(point.x);
			const float fy = std::floor(point.y);
			const int32_t x = int32_t(fx);
			const int32_t y = int32_t(fy);
			const float u = Smooth(point.x - fx);
			const float v = Smooth(point.y - fy);
			return std::lerp(std::lerp(CollapseNoise(x, y, salt), CollapseNoise(x + 1, y, salt), u), std::lerp(CollapseNoise(x, y + 1, salt), CollapseNoise(x + 1, y + 1, salt), u), v);
		}

		float SoftMax(float a, float b)
		{
			return std::max(a, b) + Blend * std::log1p(std::exp(-std::abs(a - b) / Blend));
		}

		struct Ridge
		{
			Vec2 Direction;
			float Run = 3.4f;
			float Crest = 2.0f;
			float Bend = 1.0f;
			float Phase = 0.0f;
		};

		struct Ramp
		{
			int32_t Cliff = 0;
			Vec2 Center;
			Vec2 Uphill;
			float Width = 0.0f;
			bool Stair = false;
		};

		struct Bump
		{
			Vec2 Center;
			float Radius = 1.0f;
			float Amount = 0.0f;
		};

		class Mountain
		{
		public:
			explicit Mountain(Random& random)
			{
				Summit = Middle + FromAngle(random.Float() * 2.0f * Pi) * (0.5f + random.Float());
				Salt = random.Next();
				const uint32_t count = 3 + random.Below(2);
				const float turn = random.Float() * 2.0f * Pi;
				for (uint32_t i = 0; i < count; i++)
				{
					Ridge ridge;
					ridge.Direction = FromAngle(turn + 2.0f * Pi * float(i) / float(count) + (random.Float() - 0.5f) * 0.6f);
					ridge.Run = i == 0 ? 3.6f : 2.6f + 1.2f * random.Float();
					ridge.Crest = 1.5f + 1.0f * random.Float();
					ridge.Bend = (0.6f + random.Float()) * (random.Float() < 0.5f ? -1.0f : 1.0f);
					ridge.Phase = random.Float() * 2.0f * Pi;
					Ridges.push_back(ridge);
				}
				const uint32_t knolls = 4 + random.Below(3);
				for (uint32_t attempt = 0; attempt < 60 && Bumps.size() < knolls; attempt++)
				{
					const Vec2 center = Summit + FromAngle(random.Float() * 2.0f * Pi) * (6.5f + 7.0f * random.Float());
					if (Level(center) < 0.8f || Crowded(center, 4.0f))
						continue;
					Bumps.push_back({ center, 2.0f + 1.6f * random.Float(), 0.75f + 0.35f * random.Float() });
				}
				const size_t hollows = Bumps.size() + 2 + random.Below(2);
				for (uint32_t attempt = 0; attempt < 60 && Bumps.size() < hollows; attempt++)
				{
					const Vec2 center = Summit + FromAngle(random.Float() * 2.0f * Pi) * (5.5f + 5.5f * random.Float());
					if (Level(center) < 1.9f || Crowded(center, 3.5f))
						continue;
					Bumps.push_back({ center, 1.8f + 1.2f * random.Float(), -(0.7f + 0.3f * random.Float()) });
				}
				const float spin = random.Float() * 2.0f * Pi;
				const uint32_t lobes = 6 + random.Below(3);
				for (uint32_t n = 0; n < lobes; n++)
				{
					const Vec2 direction = FromAngle(spin + 2.0f * Pi * (float(n) + 0.35f * (random.Float() - 0.5f)) / float(lobes));
					float reach = ConeFlat;
					while (reach < 30.0f && Level(Summit + direction * reach) > 0.0f)
						reach += 0.25f;
					if (n % 2 == 0)
						Bumps.push_back({ Summit + direction * (reach + 0.3f), 2.8f + 1.6f * random.Float(), -(1.0f + 0.4f * random.Float()) });
					else
						Bumps.push_back({ Summit + direction * (reach + 1.2f), 2.6f + 1.2f * random.Float(), 0.9f + 0.4f * random.Float() });
				}
				for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
					if (const std::optional<Vec2> crossing = CrestCrossing(Ridges.front(), float(cliff + 1)))
						Ramps.push_back({ cliff, *crossing, Uphill(*crossing), StairWidth, true });
				for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
					PlaceRamps(random, cliff);
			}

			bool Crowded(Vec2 point, float spacing) const
			{
				for (const Bump& bump : Bumps)
					if (LengthSquared(bump.Center - point) < spacing * spacing)
						return true;
				return false;
			}

			void PlaceRamps(Random& random, int32_t cliff)
			{
				constexpr int32_t Cells = PeakSize * 2;
				constexpr float Step = 0.5f;
				const float level = float(cliff + 1);
				std::vector<float> field(size_t(Cells + 1) * size_t(Cells + 1));
				for (int32_t j = 0; j <= Cells; j++)
					for (int32_t i = 0; i <= Cells; i++)
						field[size_t(j) * size_t(Cells + 1) + size_t(i)] = Level(Vec2(float(i), float(j)) * Step);
				const auto at = [&](int32_t i, int32_t j) { return field[size_t(j) * size_t(Cells + 1) + size_t(i)]; };
				std::vector<int32_t> owner(size_t(Cells) * size_t(Cells), -1);
				std::vector<Vec2> crossing(owner.size());
				for (int32_t j = 0; j < Cells; j++)
					for (int32_t i = 0; i < Cells; i++)
					{
						const float a = at(i, j) - level;
						const float b = at(i + 1, j) - level;
						const float c = at(i, j + 1) - level;
						if ((a >= 0.0f) != (b >= 0.0f))
							crossing[size_t(j) * size_t(Cells) + size_t(i)] = Vec2(float(i) + a / (a - b), float(j)) * Step;
						else if ((a >= 0.0f) != (c >= 0.0f))
							crossing[size_t(j) * size_t(Cells) + size_t(i)] = Vec2(float(i), float(j) + a / (a - c)) * Step;
						else
							continue;
						owner[size_t(j) * size_t(Cells) + size_t(i)] = 0;
					}
				int32_t components = 0;
				std::vector<std::vector<int32_t>> members;
				for (int32_t seed = 0; seed < int32_t(owner.size()); seed++)
				{
					if (owner[size_t(seed)] != 0)
						continue;
					members.emplace_back();
					std::vector<int32_t> stack = { seed };
					owner[size_t(seed)] = ++components;
					while (!stack.empty())
					{
						const int32_t cell = stack.back();
						stack.pop_back();
						members.back().push_back(cell);
						for (int32_t dy = -1; dy <= 1; dy++)
							for (int32_t dx = -1; dx <= 1; dx++)
							{
								const int32_t x = cell % Cells + dx;
								const int32_t y = cell / Cells + dy;
								if (x < 0 || y < 0 || x >= Cells || y >= Cells || owner[size_t(y * Cells + x)] != 0)
									continue;
								owner[size_t(y * Cells + x)] = components;
								stack.push_back(y * Cells + x);
							}
					}
				}
				for (const std::vector<int32_t>& component : members)
				{
					std::vector<Vec2> candidates;
					for (const int32_t cell : component)
					{
						const Vec2 point = crossing[size_t(cell)];
						const Vec2 up = Uphill(point);
						const bool solid = Level(point + up * 1.5f) > level + 0.12f && Level(point + up * 2.6f) > level + 0.05f && Level(point - up * 1.5f) < level - 0.12f;
						if (solid && Level(point + up * 1.9f) < level + 0.98f && Level(point - up * 1.7f) > level - 0.98f && Level(point - up * 1.7f) > 0.15f && Length(point - Summit) > ConeFlat)
							candidates.push_back(point);
					}
					if (candidates.empty())
						continue;
					const int32_t wanted = std::max(1, int32_t(std::lround(float(component.size()) * Step / RampSpacing)));
					std::vector<Vec2> chosen;
					for (const Ramp& ramp : Ramps)
						if (ramp.Cliff == cliff)
							chosen.push_back(ramp.Center);
					for (int32_t n = 0; n < wanted; n++)
					{
						float best = -1.0f;
						Vec2 pick = candidates[random.Below(uint32_t(candidates.size()))];
						if (!chosen.empty())
							for (const Vec2 candidate : candidates)
							{
								float nearest = 1e9f;
								for (const Vec2 other : chosen)
									nearest = std::min(nearest, LengthSquared(candidate - other));
								if (nearest > best)
								{
									best = nearest;
									pick = candidate;
								}
							}
						if (best >= 0.0f && best < RampGap * RampGap)
							break;
						chosen.push_back(pick);
						Ramps.push_back({ cliff, pick, Uphill(pick), ScreeWidth + 0.5f * random.Float(), false });
					}
				}
			}

			Vec2 CrestPoint(const Ridge& ridge, float t) const
			{
				return Summit + ridge.Direction * t + Perpendicular(ridge.Direction) * (ridge.Bend * std::sin(t * 0.22f + ridge.Phase) * Smooth(t / 4.0f));
			}

			float Level(Vec2 point) const
			{
				const float near = Length(point - Summit);
				const Vec2 bend = Vec2(LatticeNoise(point * 0.06f, Salt ^ 0x7171u) - 0.5f, LatticeNoise(point * 0.06f, Salt ^ 0x1717u) - 0.5f) * (2.0f * Warp);
				const Vec2 offset = point + bend * Smooth((near - RitualRadius - 1.0f) / 3.0f) - Summit;
				const float reach = Length(offset);
				float level = SummitLevel - std::max(reach - ConeFlat, 0.0f) / ConeRun;
				for (const Ridge& ridge : Ridges)
				{
					const float t = Dot(offset, ridge.Direction);
					const float lateral = t <= 0.0f ? reach : std::abs(Dot(offset, Perpendicular(ridge.Direction)) - ridge.Bend * std::sin(t * 0.22f + ridge.Phase) * Smooth(t / 4.0f));
					const float width = ridge.Crest * (0.5f + LatticeNoise(Vec2(t * 0.24f, ridge.Phase * 5.0f), Salt ^ 0x2B2Bu));
					level = SoftMax(level, SummitLevel - std::max(t - RidgeFlat, 0.0f) / ridge.Run - std::max(lateral - width, 0.0f) / RidgeFall);
				}
				const float calm = Smooth((near - RitualRadius - 1.0f) / 1.5f);
				for (const Bump& bump : Bumps)
				{
					const float d = Length(point - bump.Center) / bump.Radius;
					if (d < 1.0f)
						level += bump.Amount * Smooth(1.0f - d) * calm;
				}
				const float rough = 0.55f * LatticeNoise(point * 0.09f, Salt) + 0.3f * LatticeNoise(point * 0.21f, Salt ^ 0x5A5Au) + 0.15f * LatticeNoise(point * 0.47f, Salt ^ 0x3C3Cu);
				return level + Roughness * (2.0f * rough - 1.0f) * calm;
			}

			Vec2 Slope(Vec2 point) const
			{
				const float h = 0.1f;
				return Vec2(Level(point + Vec2(h, 0.0f)) - Level(point - Vec2(h, 0.0f)), Level(point + Vec2(0.0f, h)) - Level(point - Vec2(0.0f, h))) / (2.0f * h);
			}

			Vec2 Uphill(Vec2 point) const
			{
				const Vec2 slope = Slope(point);
				return LengthSquared(slope) > 1e-8f ? Normalize(slope) : Normalize(Summit - point);
			}

			std::optional<Vec2> CrestCrossing(const Ridge& ridge, float level) const
			{
				for (float t = RidgeFlat; t < 30.0f; t += 0.05f)
					if (const Vec2 point = CrestPoint(ridge, t); Level(point) < level)
						return point;
				return std::nullopt;
			}

			float RampShare(int32_t cliff, Vec2 point) const
			{
				float share = 0.0f;
				for (const Ramp& ramp : Ramps)
				{
					if (ramp.Cliff != cliff)
						continue;
					const Vec2 offset = point - ramp.Center;
					const float lateral = std::abs(Dot(offset, Perpendicular(ramp.Uphill)));
					const float along = std::abs(Dot(offset, ramp.Uphill));
					share = std::max(share, Smooth((ramp.Width + RampTaper - lateral) / RampTaper) * Smooth((RampReach + 1.4f - along) / 0.8f));
				}
				return share;
			}

			Vec2 Summit;
			uint32_t Salt = 0;
			std::vector<Ridge> Ridges;
			std::vector<Bump> Bumps;
			std::vector<Ramp> Ramps;
		};

		Elevation Survey(const Mountain& mountain)
		{
			Elevation relief;
			relief.Origin = { 0.0f, 0.0f };
			relief.Spacing = GridSpacing;
			relief.Columns = int32_t(float(PeakSize) / GridSpacing) + 1;
			relief.Rows = relief.Columns;
			const size_t count = size_t(relief.Columns) * size_t(relief.Rows);
			relief.Floor.assign(count, 0.0f);
			relief.Rim.assign(count, 0.0f);
			for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
			{
				relief.Edges[size_t(cliff)].assign(count, 0.0f);
				relief.Drops[size_t(cliff)].assign(count, 0.0f);
			}
			for (int32_t j = 0; j < relief.Rows; j++)
				for (int32_t i = 0; i < relief.Columns; i++)
				{
					const size_t k = size_t(j) * size_t(relief.Columns) + size_t(i);
					const Vec2 point = relief.Origin + Vec2(float(i), float(j)) * relief.Spacing;
					const float level = mountain.Level(point);
					const float steep = std::max(Length(mountain.Slope(point)), MinGradient);
					const float summit = Length(point - mountain.Summit);
					float floor = (0.13f * (2.0f * LatticeNoise(point * 0.16f, mountain.Salt ^ 0x4444u) - 1.0f) + 0.04f * (2.0f * LatticeNoise(point * 0.55f, mountain.Salt ^ 0x1111u) - 1.0f) + 0.015f * (2.0f * LatticeNoise(point * 1.3f, mountain.Salt ^ 0x2222u) - 1.0f) + 0.08f * (0.5f - std::abs(2.0f * LatticeNoise(point * 0.34f, mountain.Salt ^ 0x6666u) - 1.0f))) * Smooth((summit - RitualRadius - 0.4f) / 1.2f);
					for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
					{
						const float edge = (level - float(cliff + 1)) / steep;
						const float share = mountain.RampShare(cliff, point);
						floor += TierHeight * share * Smooth((edge / RampReach + 1.0f) * 0.5f);
						relief.Edges[size_t(cliff)][k] = edge;
						relief.Drops[size_t(cliff)][k] = TierHeight * (1.0f - share);
					}
					relief.Floor[k] = floor;
					relief.Rim[k] = level / steep;
				}
			relief.Summit = mountain.Summit;
			relief.Ritual = RitualRadius;
			for (const Ramp& ramp : mountain.Ramps)
				relief.Ramps.push_back({ .Foot = ramp.Center, .Uphill = ramp.Uphill, .Width = ramp.Width, .Cliff = uint8_t(ramp.Cliff), .Stair = ramp.Stair });
			return relief;
		}

		class Site
		{
		public:
			Site(LevelBuilder& builder, const Mountain& mountain, const Elevation& relief)
				: m_Builder(builder), m_Mountain(mountain), m_Relief(relief)
			{
			}

			float Clearance(Vec2 point) const
			{
				float clearance = m_Relief.RimDistance(point) - 0.6f;
				for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
				{
					const float edge = std::abs(m_Relief.EdgeDistance(cliff, point));
					const float ramp = m_Mountain.RampShare(cliff, point) > 0.02f ? RampReach : 0.0f;
					clearance = std::min(clearance, edge - ramp);
				}
				return clearance;
			}

			int32_t TierOf(Vec2 point) const
			{
				int32_t tier = 0;
				for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
					tier += m_Relief.EdgeDistance(cliff, point) >= 0.0f ? 1 : 0;
				return tier;
			}

			bool Place(PropId id, Vec2 position, float yaw, float margin, float spacing = 0.3f)
			{
				return Clearance(position) >= margin && m_Builder.TryProp(id, position, yaw, spacing);
			}

			bool Scatter(PropId id, Random& random, float margin, float spacing, int32_t lowest, int32_t highest)
			{
				for (uint32_t attempt = 0; attempt < 80; attempt++)
				{
					const Vec2 spot = m_Mountain.Summit + FromAngle(random.Float() * 2.0f * Pi) * (RitualRadius + 1.0f + random.Float() * 14.0f);
					const int32_t tier = TierOf(spot);
					if (tier >= lowest && tier <= highest && m_Relief.RimDistance(spot) > 0.0f && Place(id, spot, random.Float() * 2.0f * Pi, margin, spacing))
						return true;
				}
				return false;
			}

		private:
			LevelBuilder& m_Builder;
			const Mountain& m_Mountain;
			const Elevation& m_Relief;
		};
	}

	Level BuildStormPeakLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::StormPeak, seed, PeakSize, PeakSize, TerrainId::Sky);
		Random& random = builder.GetRandom();
		const Mountain mountain(random);
		Elevation relief = Survey(mountain);
		for (int32_t y = 0; y < PeakSize; y++)
			for (int32_t x = 0; x < PeakSize; x++)
				if (relief.RimDistance({ float(x) + 0.5f, float(y) + 0.5f }) > -0.3f)
					builder.Fill({ x, y, 1, 1 }, TerrainId::Slate);
		Site site(builder, mountain, relief);
		const Vec2 summit = mountain.Summit;
		const float stair = AngleOf(mountain.Ridges.front().Direction);

		builder.Prop(PropId::StormSpire, summit, random.Float() * 2.0f * Pi);
		for (uint32_t slot = 0; slot < StoneSlots; slot++)
		{
			const float angle = stair + (float(slot) + 0.5f) * 2.0f * Pi / float(StoneSlots) + (random.Float() - 0.5f) * 0.08f;
			builder.TryProp(PropId::StandingStone, summit + FromAngle(angle) * StoneRing, angle + Pi * 0.5f + (random.Float() - 0.5f) * 0.12f, 0.1f);
		}
		for (const float flank : { -1.0f, 1.0f })
			builder.TryProp(PropId::Brazier, summit + FromAngle(stair + flank * 0.45f) * (StoneRing + 0.75f), random.Float() * 2.0f * Pi, 0.1f);
		for (const ElevationRamp& ramp : relief.Ramps)
		{
			if (!ramp.Stair)
				continue;
			const Vec2 side = Perpendicular(ramp.Uphill);
			const Vec2 top = ramp.Foot + ramp.Uphill * (RampReach + 0.55f);
			const Vec2 bottom = ramp.Foot - ramp.Uphill * (RampReach + 0.5f);
			const float hand = random.Float() < 0.5f ? -1.0f : 1.0f;
			if (ramp.Cliff + 1 < ElevationCliffs)
			{
				for (const float flank : { -1.0f, 1.0f })
					builder.TryProp(PropId::Brazier, top + side * (flank * (StairWidth + 0.6f)), random.Float() * 2.0f * Pi, 0.1f);
				builder.TryProp(PropId::PrayerPole, top + side * (hand * (StairWidth + 1.6f)) + ramp.Uphill * 0.4f, AngleOf(side), 0.2f);
			}
			if (const Vec2 gate = bottom - ramp.Uphill * 0.4f; ramp.Cliff == 0 && relief.RimDistance(gate) > 0.8f)
				builder.TryProp(PropId::StoneGate, gate, AngleOf(side), 0.1f);
			else if (ramp.Cliff > 0)
				builder.TryProp(PropId::GlyphStone, bottom - side * (hand * (StairWidth + 0.75f)), AngleOf(side) + (random.Float() - 0.5f) * 0.3f, 0.2f);
		}

		const auto clearOfProps = [&](Vec2 spot)
		{
			for (const PlacedProp& prop : builder.GetLevel().Props)
				if (LengthSquared(prop.Position - spot) < 1.6f * 1.6f)
					return false;
			return true;
		};
		std::vector<std::pair<Vec2, float>> open;
		for (float y = 0.5f; y < float(PeakSize); y += 0.5f)
			for (float x = 0.5f; x < float(PeakSize); x += 0.5f)
				if (const Vec2 spot(x, y); site.TierOf(spot) == 1)
					if (const float clearance = site.Clearance(spot); clearance > 0.7f && clearOfProps(spot))
						open.push_back({ spot, clearance });
		std::vector<Vec2> spawns;
		while (spawns.size() < SpawnSlots && !open.empty())
		{
			size_t best = 0;
			float score = -1.0f;
			for (size_t n = 0; n < open.size(); n++)
			{
				float nearest = 1e9f;
				for (const Vec2 taken : spawns)
					nearest = std::min(nearest, LengthSquared(open[n].first - taken));
				if (const float value = std::min(nearest, 100.0f) + open[n].second; value > score)
				{
					score = value;
					best = n;
				}
			}
			if (!spawns.empty() && score < SpawnGap * SpawnGap)
				break;
			spawns.push_back(open[best].first);
			open.erase(open.begin() + std::ptrdiff_t(best));
		}
		std::sort(spawns.begin(), spawns.end(), [&](Vec2 a, Vec2 b) { return std::remainder(AngleOf(a - summit) - stair, 2.0f * Pi) < std::remainder(AngleOf(b - summit) - stair, 2.0f * Pi); });
		for (const Vec2 spawn : spawns)
			builder.SpawnPoint(spawn);
		for (uint32_t placed = 0, attempt = 0; placed < 2 && attempt < 6; attempt++)
			placed += site.Scatter(PropId::GlyphStone, random, 1.0f, 2.5f, 1, 2) ? 1u : 0u;
		for (uint32_t placed = 0, attempt = 0; placed < 5 && attempt < 10; attempt++)
			placed += site.Scatter(PropId::Cairn, random, 0.9f, 1.4f, 0, 2) ? 1u : 0u;
		for (uint32_t placed = 0, attempt = 0; placed < 4 && attempt < 10; attempt++)
			placed += site.Scatter(PropId::GraniteBoulder, random, 1.0f, 1.0f, 2, 2) ? 1u : 0u;
		for (uint32_t placed = 0, attempt = 0; placed < 6 && attempt < 12; attempt++)
			placed += site.Scatter(PropId::GraniteBoulder, random, 1.0f, 1.0f, 0, 1) ? 1u : 0u;

		builder.SetPlayerSpawn(summit);
		builder.Region("The Storm Peak", { 0, 0, PeakSize, PeakSize }, false);
		Level level = builder.Finish();
		level.Skyborne = true;
		level.Relief = std::move(relief);
		for (PlacedProp& prop : level.Props)
			prop.Base = level.Relief.Height(prop.Position);
		return level;
	}
}
