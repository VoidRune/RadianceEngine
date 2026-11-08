#pragma once
#include "GameShared/Core/Math.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <queue>
#include <span>
#include <utility>
#include <vector>

namespace Game
{
	inline constexpr Vec2 ForgeCenter{ 24.0f, 24.0f };
	inline constexpr float ForgeReach = 16.0f;
	inline constexpr float ForgePitMargin = 0.5f;
	inline constexpr float ForgeBandEdges[] = { 16.0f, 13.0f, 9.5f, 6.0f, 0.0f };
	inline constexpr uint32_t ForgeBands = 4;
	inline constexpr float ForgeRingEdges[] = { 16.0f, 15.1f, 14.7f, 13.1f, 12.9f, 9.6f, 9.4f, 6.1f, 5.9f, 0.0f };
	inline constexpr float ForgeRingHeights[] = { 0.42f, -0.06f, 0.2f, -0.06f, 0.0f, -0.06f, 0.0f, -0.06f, 0.0f };
	inline constexpr uint32_t ForgeRings = 9;
	inline constexpr uint32_t ForgeGrooveRings[] = { 1, 3, 5, 7 };
	inline constexpr float ForgeSpawnRing = 10.0f;
	inline constexpr float ForgeSpawnClearance = 1.4f;

	inline constexpr Vec2 PommelCenter{ 0.0f, 12.0f };
	inline constexpr float PommelRadius = 0.95f;
	inline constexpr float PommelRingOuter = 1.55f;
	inline constexpr float PommelRingTop = 0.18f;
	inline constexpr float GripHalf = 0.42f;
	inline constexpr float GripLow = 7.9f;
	inline constexpr float GripHigh = 11.3f;
	inline constexpr float GuardY = 7.2f;
	inline constexpr float GuardSpan = 0.9f;
	inline constexpr Vec2 GuardOutline[] = { { 0.0f, 8.15f }, { 0.8f, 8.15f }, { 1.15f, 7.6f }, { 5.45f, 7.6f }, { 6.55f, 8.1f }, { 6.2f, 7.2f }, { 6.55f, 6.3f }, { 5.45f, 6.8f }, { 1.15f, 6.8f }, { 0.9f, 6.4f }, { -0.9f, 6.4f }, { -1.15f, 6.8f }, { -5.45f, 6.8f }, { -6.55f, 6.3f }, { -6.2f, 7.2f }, { -6.55f, 8.1f }, { -5.45f, 7.6f }, { -1.15f, 7.6f }, { -0.8f, 8.15f } };
	inline constexpr Vec2 BladeOutline[] = { { -1.4f, 6.6f }, { 1.4f, 6.6f }, { 0.55f, -11.4f }, { 0.0f, -14.0f }, { -0.55f, -11.4f } };
	inline constexpr float FullerHigh = 5.6f;
	inline constexpr float FullerLow = -10.4f;
	inline constexpr float FullerHalf = 0.12f;
	inline constexpr float FullerTop = -0.33f;
	inline constexpr float BridgeYs[] = { 2.0f, -3.2f, -8.0f };
	inline constexpr float BridgeHalfX = 1.8f;
	inline constexpr float BridgeHalfY = 0.7f;
	inline constexpr float BridgeTop = 0.36f;
	inline constexpr float CurbWidth = 0.3f;
	inline constexpr float CurbTop = 0.24f;
	inline constexpr float AnchorTop = 1.25f;
	inline constexpr float AnchorInset = 0.45f;
	inline constexpr float AnchorHalf = 0.86f;
	inline constexpr float ChainFootRadius = 0.5f;
	inline constexpr float ChainFootReach = 1.3f;

	struct ForgeCollider
	{
		Vec2 Center;
		Vec2 Half;
		float Yaw;
	};

	inline constexpr ForgeCollider ForgePitColliders[] = {
		{ { 6.9f, 15.35f }, { 3.9f, 0.0f }, 0.0f },
		{ { -6.9f, 15.35f }, { 3.9f, 0.0f }, 0.0f },
		{ { 0.0f, 28.6f }, { 7.75f, 3.75f }, 0.0f },
		{ { 11.5f, 21.0f }, { 1.6f, 1.6f }, 0.0f },
		{ { -11.5f, 21.0f }, { 1.6f, 1.6f }, 0.0f },
		{ { 25.9f, 16.2f }, { 6.6f, 0.0f }, 0.0f },
		{ { -25.9f, 16.2f }, { 6.3f, 0.0f }, 0.0f },
		{ { 30.8f, 4.0f }, { 6.0f, 0.0f }, 0.0f },
		{ { -30.6f, -2.3f }, { 5.6f, 0.0f }, 0.0f },
		{ { 22.6f, -20.6f }, { 3.8f, 0.0f }, 0.0f },
		{ { -23.2f, -12.9f }, { 3.6f, 0.0f }, 0.0f },
		{ { 24.5f, -9.8f }, { 3.5f, 0.0f }, 0.0f },
		{ { -24.5f, 7.0f }, { 3.5f, 0.0f }, 0.0f },
		{ { 22.8f, -3.0f }, { 2.3f, 0.0f }, 0.0f },
		{ { -20.6f, -6.6f }, { 2.3f, 0.0f }, 0.0f },
		{ { 15.5f, -14.0f }, { 3.3f, 4.9f }, 0.0f },
		{ { 19.5f, -12.0f }, { 1.4f, 0.0f }, 0.0f },
		{ { -16.0f, -12.5f }, { 1.4f, 0.0f }, 0.0f },
		{ { 17.6f, 11.0f }, { 2.15f, 1.0f }, 0.5585f },
		{ { -17.6f, 11.0f }, { 2.15f, 1.0f }, -0.5585f },
		{ { 4.0f, -27.0f }, { 1.1f, 6.0f }, 0.0f },
		{ { -18.6f, 1.0f }, { 1.9f, 0.0f }, 0.0f },
		{ { -11.5f, -10.5f }, { 1.6f, 0.0f }, 0.0f },
		{ { -15.8f, -5.2f }, { 1.3f, 0.0f }, 0.0f },
		{ { 12.2f, -10.0f }, { 1.8f, 0.0f }, 0.0f },
		{ { -7.0f, -18.0f }, { 2.2f, 0.0f }, 0.0f },
		{ { 7.0f, -18.0f }, { 2.2f, 0.0f }, 0.0f },
		{ { 0.0f, -19.6f }, { 2.0f, 0.0f }, 0.0f },
	};
	inline constexpr Vec2 ForgeIslandCenters[] = { { 17.5f, -15.5f }, { 25.5f, -1.5f }, { -17.5f, -14.5f }, { 9.0f, -22.0f }, { -24.5f, 8.0f }, { 21.5f, 4.0f }, { -9.5f, -22.5f } };
	inline constexpr float ForgeIslandRadii[] = { 6.0f, 4.5f, 5.2f, 2.4f, 2.6f, 1.8f, 2.0f };
	inline constexpr float ForgeIslandTops[] = { -1.6f, -1.2f, -1.4f, -2.1f, -1.8f, -2.3f, -2.2f };

	inline constexpr float TrenchFloor = -0.45f;
	inline constexpr float MetalHotLevel = -0.12f;
	inline constexpr float MetalCooledLevel = -0.3f;
	inline constexpr float ForgePitSurface = -3.0f;
	inline constexpr float ForgePlateBottom = -2.6f;

	inline constexpr float PourTiltSeconds = 2.5f;
	inline constexpr float FlowSpeed = 3.0f;
	inline constexpr float MetalHotSeconds = 9.0f;
	inline constexpr float MetalCoolWarning = 1.5f;

	struct ForgePour
	{
		float Tilt = 0.0f;
		bool HiltOnly = false;
	};

	inline constexpr float ForgeHiltLine = 6.2f;
	inline constexpr float SluiceThickness = 0.12f;
	inline constexpr float SluiceHalfWidth = 1.52f;
	inline constexpr float SluicePostX = 1.53f;
	inline constexpr Vec2 SluicePostHalf{ 0.12f, 0.17f };

	inline constexpr ForgePour ForgePours[] = { { 8.0f, true }, { 23.0f, false }, { 37.0f, false } };
	inline constexpr float PourRepeatStart = 51.0f;
	inline constexpr float PourRepeat = 14.0f;

	inline constexpr float FloodTimes[] = { 40.0f, 52.0f, 64.0f, 76.0f };
	inline constexpr float FloodSpreads[] = { 2.0f, 2.0f, 2.0f, 8.0f };
	inline constexpr float FloodWarning = 3.0f;

	inline constexpr float ForgeCell = 0.25f;
	inline constexpr int32_t ForgeGridSize = 128;
	inline constexpr uint16_t ForgeUnreached = 0xFFFFu;
	inline constexpr float ForgeCostMeters = ForgeCell / 10.0f;

	enum class ForgeHazard : uint8_t
	{
		None,
		Pit,
		Metal,
		Smoulder,
		Flood,
	};

	inline Vec2 ForgeLocal(Vec2 position)
	{
		return position - ForgeCenter;
	}

	inline float ForgeL1(Vec2 local)
	{
		return std::abs(local.x) + std::abs(local.y);
	}

	inline bool OnForgePlate(Vec2 position)
	{
		return ForgeL1(ForgeLocal(position)) <= ForgeReach;
	}

	inline uint32_t ForgeBandOf(Vec2 local)
	{
		const float l1 = ForgeL1(local);
		uint32_t band = 0;
		while (band + 1 < ForgeBands && l1 < ForgeBandEdges[band + 1])
			band++;
		return band;
	}

	inline float ForgeRingHeight(Vec2 local)
	{
		const float l1 = ForgeL1(local);
		for (uint32_t ring = 0; ring < ForgeRings; ring++)
			if (l1 >= ForgeRingEdges[ring + 1])
				return ForgeRingHeights[ring];
		return ForgeRingHeights[ForgeRings - 1];
	}

	inline float BoxDistance(Vec2 p, Vec2 half)
	{
		const Vec2 d(std::abs(p.x) - half.x, std::abs(p.y) - half.y);
		const Vec2 outside(std::max(d.x, 0.0f), std::max(d.y, 0.0f));
		return Length(outside) + std::min(std::max(d.x, d.y), 0.0f);
	}

	inline float SegmentDistance(Vec2 p, Vec2 a, Vec2 b)
	{
		return Length(p - Lerp(a, b, ClosestPointParameter(a, b, p)));
	}

	inline float PolygonDistance(Vec2 p, std::span<const Vec2> points)
	{
		float best = LengthSquared(p - points[0]);
		float sign = 1.0f;
		for (size_t i = 0, j = points.size() - 1; i < points.size(); j = i, i++)
		{
			const Vec2 e = points[j] - points[i];
			const Vec2 w = p - points[i];
			const Vec2 b = w - e * std::clamp(Dot(w, e) / Dot(e, e), 0.0f, 1.0f);
			best = std::min(best, LengthSquared(b));
			const bool c0 = p.y >= points[i].y;
			const bool c1 = p.y < points[j].y;
			const bool c2 = e.x * w.y > e.y * w.x;
			if ((c0 && c1 && c2) || (!c0 && !c1 && !c2))
				sign = -sign;
		}
		return sign * std::sqrt(best);
	}

	inline float TrenchDistance(Vec2 local)
	{
		const float pommel = Length(local - PommelCenter) - PommelRadius;
		const float grip = BoxDistance(local - Vec2(0.0f, 0.5f * (GripLow + GripHigh)), Vec2(GripHalf, 0.5f * (GripHigh - GripLow)));
		const float guard = PolygonDistance(local, GuardOutline);
		const float blade = PolygonDistance(local, BladeOutline);
		return std::min({ pommel, grip, guard, blade });
	}

	inline bool InTrench(Vec2 local)
	{
		return TrenchDistance(local) < 0.0f;
	}

	inline bool OnBridge(Vec2 local)
	{
		if (std::abs(local.x) > BridgeHalfX)
			return false;
		for (const float y : BridgeYs)
			if (std::abs(local.y - y) <= BridgeHalfY)
				return true;
		return false;
	}

	inline float ForgeSurfaceHeight(Vec2 local)
	{
		if (ForgeL1(local) > ForgeReach)
			return ForgePitSurface;
		if (OnBridge(local))
			return BridgeTop;
		const float trench = TrenchDistance(local);
		if (trench < 0.0f)
			return TrenchFloor;
		if (trench < CurbWidth)
			return CurbTop;
		if (Length(local - PommelCenter) < PommelRingOuter)
			return PommelRingTop;
		return ForgeRingHeight(local);
	}

	inline Vec2 TrenchOutward(Vec2 local)
	{
		constexpr float h = 0.04f;
		const Vec2 gradient(TrenchDistance(local + Vec2(h, 0.0f)) - TrenchDistance(local - Vec2(h, 0.0f)), TrenchDistance(local + Vec2(0.0f, h)) - TrenchDistance(local - Vec2(0.0f, h)));
		const Vec2 direction = Normalize(gradient);
		return LengthSquared(direction) > 0.0f ? direction : Vec2(local.x >= 0.0f ? 1.0f : -1.0f, 0.0f);
	}

	struct ForgeGrid
	{
		std::vector<uint16_t> Cost;

		static int32_t CellOf(float local) { return std::clamp(int32_t(std::floor((local + ForgeReach) / ForgeCell)), 0, ForgeGridSize - 1); }
		static Vec2 CellCenter(int32_t x, int32_t y) { return { (float(x) + 0.5f) * ForgeCell - ForgeReach, (float(y) + 0.5f) * ForgeCell - ForgeReach }; }
		uint16_t At(int32_t x, int32_t y) const { return Cost[size_t(y) * ForgeGridSize + size_t(x)]; }
		uint16_t At(Vec2 local) const { return At(CellOf(local.x), CellOf(local.y)); }
	};

	inline ForgeGrid BuildForgeGrid()
	{
		constexpr float Overlap = 0.18f;
		ForgeGrid grid;
		grid.Cost.assign(size_t(ForgeGridSize) * ForgeGridSize, ForgeUnreached);
		std::vector<uint8_t> open(grid.Cost.size(), 0);
		for (int32_t y = 0; y < ForgeGridSize; y++)
			for (int32_t x = 0; x < ForgeGridSize; x++)
				open[size_t(y) * ForgeGridSize + size_t(x)] = TrenchDistance(ForgeGrid::CellCenter(x, y)) < Overlap ? 1 : 0;
		using Entry = std::pair<uint32_t, uint32_t>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
		for (const float side : { -0.5f * ForgeCell, 0.5f * ForgeCell })
		{
			const uint32_t start = uint32_t(ForgeGrid::CellOf(PommelCenter.y)) * ForgeGridSize + uint32_t(ForgeGrid::CellOf(PommelCenter.x + side));
			grid.Cost[start] = 0;
			queue.push({ 0u, start });
		}
		constexpr int32_t Steps[8][3] = { { 1, 0, 10 }, { -1, 0, 10 }, { 0, 1, 10 }, { 0, -1, 10 }, { 1, 1, 14 }, { 1, -1, 14 }, { -1, 1, 14 }, { -1, -1, 14 } };
		while (!queue.empty())
		{
			const auto [cost, index] = queue.top();
			queue.pop();
			if (cost != grid.Cost[index])
				continue;
			const int32_t x = int32_t(index % ForgeGridSize);
			const int32_t y = int32_t(index / ForgeGridSize);
			for (const auto& step : Steps)
			{
				const int32_t nx = x + step[0];
				const int32_t ny = y + step[1];
				if (nx < 0 || ny < 0 || nx >= ForgeGridSize || ny >= ForgeGridSize)
					continue;
				const uint32_t next = uint32_t(ny) * ForgeGridSize + uint32_t(nx);
				if (!open[next] || (step[2] == 14 && (!open[uint32_t(y) * ForgeGridSize + uint32_t(nx)] || !open[uint32_t(ny) * ForgeGridSize + uint32_t(x)])))
					continue;
				const uint32_t through = cost + uint32_t(step[2]);
				if (through < grid.Cost[next])
				{
					grid.Cost[next] = uint16_t(through);
					queue.push({ through, next });
				}
			}
		}
		return grid;
	}

	inline const ForgeGrid& GetForgeGrid()
	{
		static const ForgeGrid grid = BuildForgeGrid();
		return grid;
	}

	inline float ArrivalMeters(Vec2 local)
	{
		const uint16_t cost = GetForgeGrid().At(local);
		return cost == ForgeUnreached ? -1.0f : float(cost) * ForgeCostMeters;
	}

	inline uint32_t ForgePourCount(float clock)
	{
		uint32_t count = 0;
		for (const ForgePour& pour : ForgePours)
			count += pour.Tilt <= clock ? 1u : 0u;
		if (clock >= PourRepeatStart)
			count += 1u + uint32_t((clock - PourRepeatStart) / PourRepeat);
		return count;
	}

	inline ForgePour ForgePourAt(uint32_t index)
	{
		if (index < std::size(ForgePours))
			return ForgePours[index];
		return { PourRepeatStart + float(index - uint32_t(std::size(ForgePours))) * PourRepeat, false };
	}

	inline float PourLanding(uint32_t index)
	{
		return ForgePourAt(index).Tilt + PourTiltSeconds;
	}

	inline bool PourReaches(uint32_t index, Vec2 local)
	{
		return !ForgePourAt(index).HiltOnly || local.y >= ForgeHiltLine;
	}

	inline float PourFrontTime(uint32_t index, Vec2 local, float meters)
	{
		return meters >= 0.0f && PourReaches(index, local) ? PourLanding(index) + meters / FlowSpeed : 1e9f;
	}

	inline float MetalFrontAge(Vec2 local, float meters, float clock)
	{
		float age = -1.0f;
		const uint32_t pours = ForgePourCount(clock);
		for (uint32_t i = 0; i < pours; i++)
			if (const float front = PourFrontTime(i, local, meters); front <= clock)
				age = clock - front;
		return age;
	}

	inline bool MetalHotAt(Vec2 local, float clock)
	{
		const float meters = ArrivalMeters(local);
		if (meters < 0.0f)
			return false;
		const uint32_t pours = ForgePourCount(clock);
		for (uint32_t i = 0; i < pours; i++)
			if (const float front = PourFrontTime(i, local, meters); clock >= front && clock < front + MetalHotSeconds)
				return true;
		return false;
	}

	inline int32_t MetalFrontCrossing(Vec2 local, float from, float to)
	{
		const float meters = ArrivalMeters(local);
		if (meters < 0.0f)
			return -1;
		const uint32_t pours = ForgePourCount(to);
		for (uint32_t i = 0; i < pours; i++)
			if (const float front = PourFrontTime(i, local, meters); front > from && front <= to)
				return int32_t(i);
		return -1;
	}

	inline float NextMetalTime(Vec2 local, float clock, float horizon)
	{
		const float meters = ArrivalMeters(local);
		if (meters < 0.0f)
			return 1e9f;
		float first = 1e9f;
		const uint32_t pours = ForgePourCount(clock + horizon);
		for (uint32_t i = 0; i < pours; i++)
		{
			const float front = PourFrontTime(i, local, meters);
			if (clock >= front)
				return clock;
			if (front > clock && front <= clock + horizon)
				first = std::min(first, front);
		}
		return first;
	}

	inline float FloodLine(float clock)
	{
		float line = ForgeReach + 1.0f;
		for (uint32_t band = 0; band < ForgeBands; band++)
			if (clock >= FloodTimes[band])
			{
				const float t = std::clamp((clock - FloodTimes[band]) / FloodSpreads[band], 0.0f, 1.0f);
				line = std::min(line, ForgeBandEdges[band] + (ForgeBandEdges[band + 1] - ForgeBandEdges[band]) * t);
			}
		return line;
	}

	inline float FloodTimeAt(Vec2 local)
	{
		const float l1 = ForgeL1(local);
		for (uint32_t band = 0; band < ForgeBands; band++)
			if (l1 >= ForgeBandEdges[band + 1])
				return FloodTimes[band] + FloodSpreads[band] * std::clamp((ForgeBandEdges[band] - l1) / (ForgeBandEdges[band] - ForgeBandEdges[band + 1]), 0.0f, 1.0f);
		return FloodTimes[ForgeBands - 1] + FloodSpreads[ForgeBands - 1];
	}

	inline float FloodWarningOf(uint32_t band, float clock)
	{
		const float lead = FloodTimes[band] - clock;
		return lead > 0.0f && lead <= FloodWarning ? 1.0f - lead / FloodWarning : 0.0f;
	}

	inline ForgeHazard ForgeHazardAt(Vec2 position, float clock, bool steady)
	{
		const Vec2 local = ForgeLocal(position);
		if (ForgeL1(local) > ForgeReach)
			return ForgeHazard::Pit;
		if (!steady && ForgeL1(local) >= FloodLine(clock))
			return ForgeHazard::Flood;
		if (!InTrench(local) || OnBridge(local))
			return ForgeHazard::None;
		if (MetalHotAt(local, clock))
			return ForgeHazard::Metal;
		return MetalFrontAge(local, ArrivalMeters(local), clock) >= 0.0f ? ForgeHazard::Smoulder : ForgeHazard::None;
	}

	inline Vec2 KeepInPit(Vec2 position)
	{
		const Vec2 local = ForgeLocal(position);
		const float floor = ForgeReach + ForgePitMargin;
		const float l1 = ForgeL1(local);
		if (l1 >= floor)
			return position;
		const Vec2 normal(local.x >= 0.0f ? 1.0f : -1.0f, local.y >= 0.0f ? 1.0f : -1.0f);
		return position + normal * (0.5f * (floor - l1));
	}

	inline Vec2 ForgeRingPoint(float angle, float ring)
	{
		const Vec2 direction = FromAngle(angle);
		return ForgeCenter + direction * (ring / std::max(std::abs(direction.x) + std::abs(direction.y), 1e-3f));
	}
}
