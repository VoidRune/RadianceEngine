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
		constexpr float PlateauArea = 540.0f;
		constexpr float CornerReach = 13.0f;
		constexpr float MinTipAngle = 1.0f;
		constexpr float ConvexFillet = 0.55f;
		constexpr float ConcaveFillet = 0.45f;
		constexpr int32_t FilletSteps = 4;
		constexpr float JagStep = 3.2f;
		constexpr float JagDepth = 0.4f;
		constexpr float NeckWidth = 1.0f;
		constexpr float ChasmMouth = 0.85f;
		constexpr float ChasmTip = 0.14f;
		constexpr float ChasmStep = 0.35f;
		constexpr float ChasmGap = 9.0f;
		constexpr float ChasmInner = 7.0f;
		constexpr float RitualRadius = 3.6f;
		constexpr uint32_t Monoliths = 4;
		constexpr float MonolithNear = 6.1f;
		constexpr float MonolithFar = 7.8f;
		constexpr float MonolithReach = 0.81f;
		constexpr float SpireReach = 0.62f;
		constexpr float LaneWidth = 4.6f;
		constexpr float RimClearance = 3.0f;
		constexpr uint32_t Boulders = 2;
		constexpr float BoulderReach = 0.8f;
		constexpr float BoulderLane = 4.0f;
		constexpr float BoulderRim = 2.4f;
		constexpr uint32_t Crystals = 4;
		constexpr float CrystalReach = 0.4f;
		constexpr float CrystalLane = 2.5f;
		constexpr float CrystalRim = 0.6f;
		constexpr float SpawnClearance = 2.5f;
		constexpr float SpawnRimMin = 2.0f;
		constexpr float SpawnRimMax = 4.2f;
		constexpr uint32_t SpawnSlots = 36;
		constexpr float SpawnGap = 2.2f;

		float Cross(Vec2 a, Vec2 b)
		{
			return a.x * b.y - a.y * b.x;
		}

		float PolygonArea(const std::vector<Vec2>& points)
		{
			float sum = 0.0f;
			for (size_t k = 0; k < points.size(); k++)
				sum += Cross(points[k] - Middle, points[(k + 1) % points.size()] - Middle);
			return 0.5f * sum;
		}

		void ScaleTo(std::vector<Vec2>& points, float area)
		{
			const float scale = std::sqrt(area / std::max(PolygonArea(points), 1.0f));
			for (Vec2& point : points)
				point = Middle + (point - Middle) * scale;
		}

		float TipAngle(const std::vector<Vec2>& points, size_t k, bool& convex)
		{
			const Vec2 previous = points[(k + points.size() - 1) % points.size()];
			const Vec2 next = points[(k + 1) % points.size()];
			convex = Cross(points[k] - previous, next - points[k]) > 0.0f;
			return std::acos(std::clamp(Dot(Normalize(previous - points[k]), Normalize(next - points[k])), -1.0f, 1.0f));
		}

		std::vector<Vec2> Corners(Random& random)
		{
			const uint32_t count = 8 + random.Below(3);
			const float turn = random.Float() * 2.0f * Pi;
			std::vector<float> angles(count);
			std::vector<float> reach(count);
			for (uint32_t k = 0; k < count; k++)
			{
				angles[k] = turn + 2.0f * Pi * (float(k) + 0.32f * (random.Float() - 0.5f)) / float(count);
				reach[k] = 0.88f + 0.2f * random.Float();
			}
			const uint32_t spikes = 2 + random.Below(2);
			const uint32_t first = random.Below(count);
			for (uint32_t s = 0; s < spikes; s++)
				reach[(first + s * count / spikes + random.Below(2)) % count] = 1.28f + 0.18f * random.Float();
			const uint32_t notches = 1 + random.Below(2);
			for (uint32_t n = 0; n < notches; n++)
				if (float& notch = reach[random.Below(count)]; notch < 1.15f)
					notch = 0.7f + 0.1f * random.Float();
			std::vector<Vec2> corners(count);
			for (int32_t pass = 0; pass < 16; pass++)
			{
				for (uint32_t k = 0; k < count; k++)
					corners[k] = Middle + FromAngle(angles[k]) * (reach[k] * CornerReach);
				bool sharp = false;
				for (uint32_t k = 0; k < count; k++)
				{
					bool convex = false;
					if (TipAngle(corners, k, convex) < MinTipAngle && convex)
					{
						reach[k] = std::lerp(reach[k], 0.5f * (reach[(k + count - 1) % count] + reach[(k + 1) % count]), 0.35f);
						sharp = true;
					}
				}
				if (!sharp)
					break;
			}
			return corners;
		}

		std::vector<Vec2> Jagged(Random& random, const std::vector<Vec2>& corners)
		{
			std::vector<Vec2> points;
			for (size_t k = 0; k < corners.size(); k++)
			{
				const Vec2 from = corners[k];
				const Vec2 to = corners[(k + 1) % corners.size()];
				points.push_back(from);
				const float length = Length(to - from);
				const Vec2 outward = Perpendicular(to - from) * (-1.0f / std::max(length, 1e-4f));
				const int32_t kinks = int32_t(length / JagStep);
				for (int32_t i = 1; i <= kinks; i++)
				{
					const float t = (float(i) + 0.3f * (random.Float() - 0.5f)) / float(kinks + 1);
					points.push_back(Lerp(from, to, t) + outward * (JagDepth * (2.0f * random.Float() - 1.0f)));
				}
			}
			return points;
		}

		std::vector<Vec2> Filleted(const std::vector<Vec2>& points)
		{
			std::vector<Vec2> outline;
			for (size_t k = 0; k < points.size(); k++)
			{
				const Vec2 corner = points[k];
				const Vec2 previous = points[(k + points.size() - 1) % points.size()];
				const Vec2 next = points[(k + 1) % points.size()];
				bool convex = false;
				const float angle = TipAngle(points, k, convex);
				if (angle > Pi - 0.02f)
				{
					outline.push_back(corner);
					continue;
				}
				const Vec2 back = Normalize(previous - corner);
				const Vec2 ahead = Normalize(next - corner);
				const float half = 0.5f * angle;
				const float tangent = std::min((convex ? ConvexFillet : ConcaveFillet) / std::tan(half), 0.45f * std::min(Length(previous - corner), Length(next - corner)));
				const float radius = tangent * std::tan(half);
				const Vec2 center = corner + Normalize(back + ahead) * (radius / std::sin(half));
				const Vec2 start = corner + back * tangent - center;
				const float sweep = std::remainder(AngleOf(corner + ahead * tangent - center) - AngleOf(start), 2.0f * Pi);
				for (int32_t s = 0; s <= FilletSteps; s++)
					outline.push_back(center + FromAngle(AngleOf(start) + sweep * float(s) / float(FilletSteps)) * radius);
			}
			return outline;
		}

		std::vector<float> OutlineField(const Plateau& peak, const std::vector<Vec2>& outline)
		{
			std::vector<float> field(size_t(peak.Columns) * size_t(peak.Rows));
			for (int32_t j = 0; j < peak.Rows; j++)
				for (int32_t i = 0; i < peak.Columns; i++)
				{
					const Vec2 point = peak.Origin + Vec2(float(i), float(j)) * peak.Spacing;
					float nearest = 1e9f;
					bool inside = false;
					for (size_t k = 0; k < outline.size(); k++)
					{
						const Vec2 a = outline[k];
						const Vec2 b = outline[(k + 1) % outline.size()];
						nearest = std::min(nearest, PointSegmentDistance(point, a, b));
						if ((a.y > point.y) != (b.y > point.y) && point.x < a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y))
							inside = !inside;
					}
					field[size_t(j) * size_t(peak.Columns) + size_t(i)] = inside ? nearest : -nearest;
				}
			return field;
		}

		bool Connected(const Plateau& peak)
		{
			const auto open = [&](size_t k) { return peak.Rim[k] >= NeckWidth; };
			std::vector<uint8_t> seen(peak.Rim.size(), 0);
			const Plateau::Spot middle = peak.Locate(peak.Center);
			std::vector<size_t> queue = { middle.Index };
			seen[middle.Index] = 1;
			for (size_t n = 0; n < queue.size(); n++)
			{
				const int32_t i = int32_t(queue[n] % size_t(peak.Columns));
				const int32_t j = int32_t(queue[n] / size_t(peak.Columns));
				for (const auto [di, dj] : { std::pair{ 1, 0 }, std::pair{ -1, 0 }, std::pair{ 0, 1 }, std::pair{ 0, -1 } })
				{
					if (i + di < 0 || j + dj < 0 || i + di >= peak.Columns || j + dj >= peak.Rows)
						continue;
					const size_t k = size_t(j + dj) * size_t(peak.Columns) + size_t(i + di);
					if (!seen[k] && open(k))
					{
						seen[k] = 1;
						queue.push_back(k);
					}
				}
			}
			size_t total = 0;
			for (size_t k = 0; k < peak.Rim.size(); k++)
				total += open(k) ? 1 : 0;
			return queue.size() + total / 200 >= total;
		}

		std::vector<float> Combined(const std::vector<float>& outline, const Plateau& peak)
		{
			std::vector<float> rim = outline;
			if (peak.Chasms.empty())
				return rim;
			for (int32_t j = 0; j < peak.Rows; j++)
				for (int32_t i = 0; i < peak.Columns; i++)
				{
					const size_t k = size_t(j) * size_t(peak.Columns) + size_t(i);
					if (rim[k] > -3.0f)
						rim[k] = std::min(rim[k], peak.ChasmDistance(peak.Origin + Vec2(float(i), float(j)) * peak.Spacing));
				}
			return rim;
		}

		std::optional<Plateau::Chasm> Carve(Random& random, const std::vector<Vec2>& outline, const std::vector<Vec2>& corners, const std::vector<Vec2>& mouths)
		{
			std::vector<float> arcs = { 0.0f };
			for (size_t k = 0; k < outline.size(); k++)
				arcs.push_back(arcs.back() + Length(outline[(k + 1) % outline.size()] - outline[k]));
			const float at = random.Float() * arcs.back();
			const size_t edge = size_t(std::upper_bound(arcs.begin(), arcs.end(), at) - arcs.begin()) - 1;
			const Vec2 from = outline[edge];
			const Vec2 to = outline[(edge + 1) % outline.size()];
			const Vec2 mouth = Lerp(from, to, (at - arcs[edge]) / std::max(arcs[edge + 1] - arcs[edge], 1e-4f));
			for (const Vec2 corner : corners)
				if (Length(corner - Middle) > 1.15f * CornerReach && Length(corner - mouth) < 3.0f)
					return std::nullopt;
			for (const Vec2 other : mouths)
				if (Length(other - mouth) < ChasmGap)
					return std::nullopt;
			const Vec2 inward = Normalize(Normalize(Perpendicular(to - from)) + Normalize(Middle - mouth) * 0.8f);
			const Vec2 direction = FromAngle(AngleOf(inward) + 0.5f * (random.Float() - 0.5f));
			const Vec2 side = Perpendicular(direction);
			float length = 3.4f + 1.8f * random.Float();
			while (length > 2.4f && Length(mouth + direction * length - Middle) < ChasmInner)
				length -= 0.2f;
			if (length <= 2.4f)
				return std::nullopt;
			const std::array<Vec2, 4> controls = { mouth - direction * 1.6f, mouth, mouth + direction * (0.5f * length) + side * (0.9f * (random.Float() - 0.5f)), mouth + direction * length + side * (0.6f * (random.Float() - 0.5f)) };
			const float wide = ChasmMouth + 0.3f * random.Float();
			Plateau::Chasm chasm;
			float travelled = 0.0f;
			for (size_t c = 0; c + 1 < controls.size(); c++)
			{
				const float span = Length(controls[c + 1] - controls[c]);
				const int32_t steps = std::max(1, int32_t(std::ceil(span / ChasmStep)));
				for (int32_t s = 0; s < steps; s++)
				{
					const float inside = c == 0 ? 0.0f : std::clamp((travelled + span * float(s) / float(steps) - 1.6f) / length, 0.0f, 1.0f);
					chasm.Spine.push_back(Lerp(controls[c], controls[c + 1], float(s) / float(steps)));
					chasm.Half.push_back(std::max(ChasmTip + (wide - ChasmTip) * std::pow(1.0f - inside, 0.8f) + 0.07f * (2.0f * random.Float() - 1.0f) * (1.0f - inside), 0.12f));
				}
				travelled += span;
			}
			chasm.Spine.push_back(controls.back());
			chasm.Half.push_back(ChasmTip);
			return chasm;
		}

		Plateau Survey(Random& random)
		{
			Plateau peak;
			peak.Spacing = GridSpacing;
			peak.Columns = int32_t(float(PeakSize) / GridSpacing) + 1;
			peak.Rows = peak.Columns;
			peak.Center = Middle;
			peak.Ritual = RitualRadius;
			std::vector<float> field;
			std::vector<Vec2> outline;
			for (int32_t attempt = 0; attempt < 12; attempt++)
			{
				peak.Corners = Corners(random);
				ScaleTo(peak.Corners, PlateauArea);
				outline = Filleted(Jagged(random, peak.Corners));
				ScaleTo(outline, PlateauArea);
				field = OutlineField(peak, outline);
				peak.Rim = field;
				if (Connected(peak))
					break;
			}
			const uint32_t chasms = 1 + random.Below(2);
			std::vector<Vec2> mouths;
			for (uint32_t tries = 0; tries < 60 && peak.Chasms.size() < chasms; tries++)
				if (std::optional<Plateau::Chasm> chasm = Carve(random, outline, peak.Corners, mouths))
				{
					peak.Chasms.push_back(std::move(*chasm));
					peak.Rim = Combined(field, peak);
					if (Connected(peak))
						mouths.push_back(peak.Chasms.back().Spine[size_t(std::ceil(1.6f / ChasmStep))]);
					else
					{
						peak.Chasms.pop_back();
						peak.Rim = Combined(field, peak);
					}
				}
			return peak;
		}

		struct Blocker
		{
			Vec2 Position;
			float Reach = 0.0f;
		};

		bool Clear(const std::vector<Blocker>& placed, Vec2 spot, float reach, float lane)
		{
			for (const Blocker& other : placed)
				if (Length(spot - other.Position) - other.Reach - reach < lane)
					return false;
			return true;
		}

		std::vector<Blocker> PlaceMonoliths(Random& random, const Plateau& peak)
		{
			for (uint32_t count = Monoliths; count >= 3; count--)
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
						fits = peak.RimDistance(spot) >= RimClearance + MonolithReach && Clear(placed, spot, MonolithReach, LaneWidth);
						placed.push_back({ spot, MonolithReach });
					}
					if (fits)
						return placed;
				}
			return { { Middle, SpireReach } };
		}

		std::vector<Vec2> PlaceBoulders(Random& random, const Plateau& peak, std::vector<Blocker>& blockers)
		{
			std::vector<Vec2> boulders;
			for (uint32_t attempt = 0; attempt < 600 && boulders.size() < Boulders; attempt++)
			{
				const Vec2 spot = Middle + FromAngle(random.Float() * 2.0f * Pi) * (4.8f + 6.0f * random.Float());
				if (peak.RimDistance(spot) < BoulderRim + BoulderReach || !Clear(blockers, spot, BoulderReach, BoulderLane))
					continue;
				blockers.push_back({ spot, BoulderReach });
				boulders.push_back(spot);
			}
			return boulders;
		}

		std::vector<Vec2> PlaceCrystals(Random& random, const Plateau& peak, std::vector<Blocker>& blockers)
		{
			std::vector<Vec2> candidates;
			float mean = 0.0f;
			for (const Vec2 corner : peak.Corners)
				mean += Length(corner - Middle) / float(peak.Corners.size());
			for (const Vec2 corner : peak.Corners)
				if (Length(corner - Middle) > 0.97f * mean)
					candidates.push_back(corner);
			for (size_t k = candidates.size(); k > 1; k--)
				std::swap(candidates[k - 1], candidates[random.Below(uint32_t(k))]);
			candidates.push_back(Middle + FromAngle(random.Float() * 2.0f * Pi) * 40.0f);
			std::vector<Vec2> crystals;
			for (const Vec2 candidate : candidates)
			{
				if (crystals.size() >= Crystals)
					break;
				const Vec2 inward = Normalize(Middle - candidate);
				Vec2 spot = candidate;
				for (int32_t step = 0; step < 400 && peak.RimDistance(spot) < CrystalRim; step++)
					spot = spot + inward * 0.1f;
				spot = spot + Perpendicular(inward) * (0.8f * (random.Float() - 0.5f));
				if (peak.RimDistance(spot) < CrystalRim - 0.2f || !Clear(blockers, spot, CrystalReach, CrystalLane))
					continue;
				blockers.push_back({ spot, CrystalReach });
				crystals.push_back(spot);
			}
			return crystals;
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
		Plateau peak = Survey(random);
		for (int32_t y = 0; y < PeakSize; y++)
			for (int32_t x = 0; x < PeakSize; x++)
				if (peak.RimDistance({ float(x) + 0.5f, float(y) + 0.5f }) > -0.3f)
					builder.Fill({ x, y, 1, 1 }, TerrainId::Slate);
		std::vector<Blocker> blockers = PlaceMonoliths(random, peak);
		builder.Prop(PropId::StormSpire, Middle, random.Float() * 2.0f * Pi);
		for (size_t i = 1; i < blockers.size(); i++)
			builder.Prop(PropId::Monolith, blockers[i].Position, AngleOf(blockers[i].Position - Middle) + Pi * 0.5f + (random.Float() - 0.5f) * 0.5f);
		for (const Vec2 boulder : PlaceBoulders(random, peak, blockers))
			builder.Prop(PropId::SummitBoulder, boulder, random.Float() * 2.0f * Pi);
		for (const Vec2 crystal : PlaceCrystals(random, peak, blockers))
			builder.Prop(PropId::SummitCrystal, crystal, random.Float() * 2.0f * Pi);
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
