#pragma once
#include "GameShared/Content/EmberIsland.h"
#include "GameShared/Content/PortalTerrace.h"
#include "GameShared/Content/Props.h"
#include "GameShared/World/Shapes.h"
#include "GameShared/World/TileMap.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace Game
{
	inline constexpr int32_t HubWidth = 132;
	inline constexpr int32_t HubHeight = 70;

	inline constexpr Vec2 HubOutline[] = {
		{ -4.5f, -23.5f }, { 4.5f, -23.5f }, { 6.0f, -19.5f }, { 10.0f, -18.5f }, { 15.0f, -17.5f }, { 20.0f, -17.0f }, { 26.0f, -17.5f }, { 31.0f, -16.5f },
		{ 35.0f, -13.0f }, { 36.0f, -8.0f }, { 34.0f, -4.0f }, { 35.5f, 0.0f }, { 38.5f, 4.0f }, { 40.5f, 9.0f }, { 41.0f, 15.0f }, { 39.5f, 21.0f },
		{ 36.0f, 25.0f }, { 29.0f, 27.0f }, { 21.0f, 27.5f }, { 14.0f, 27.5f }, { 8.0f, 28.0f }, { 0.0f, 28.5f }, { -8.0f, 28.0f }, { -14.0f, 27.0f },
		{ -20.0f, 26.0f }, { -26.0f, 24.0f }, { -31.0f, 20.5f }, { -35.0f, 15.5f }, { -39.0f, 11.0f }, { -41.5f, 5.0f }, { -41.0f, -1.0f }, { -39.0f, -6.0f },
		{ -35.5f, -10.0f }, { -32.0f, -12.5f }, { -27.0f, -14.5f }, { -21.0f, -15.5f }, { -15.0f, -17.5f }, { -10.0f, -19.5f }, { -6.0f, -21.0f },
	};
	inline constexpr float HubJagAmplitude = 0.9f;
	inline constexpr float HubJagScale = 0.42f;

	inline constexpr float HubNorthLevel = 1.25f;
	inline constexpr float HubAcademyLevel = 0.75f;
	inline constexpr float HubMapleLevel = 1.0f;
	inline constexpr float HubRiftLevel = 0.5f;
	inline constexpr float HubSouthDrop = 0.4f;

	inline constexpr Vec2 HubNorthPoints[] = { { -4.5f, 12.5f }, { 4.5f, 12.5f }, { 8.0f, 13.5f }, { 9.0f, 18.0f }, { 8.5f, 27.5f }, { -8.5f, 27.5f }, { -9.0f, 18.0f }, { -8.0f, 13.5f } };
	inline constexpr float HubNorthBanks[] = { 0.0f, 0.0f, 0.6f, 1.0f, 1.0f, 1.0f, 1.0f, 0.6f };
	inline constexpr Vec2 HubAcademyPoints[] = { { 8.5f, 5.0f }, { 16.0f, 3.5f }, { 24.0f, 3.0f }, { 31.0f, 2.5f }, { 36.0f, 4.5f }, { 39.5f, 10.0f }, { 40.0f, 19.0f }, { 36.0f, 25.0f }, { 13.0f, 27.5f }, { 10.0f, 22.0f }, { 8.5f, 14.0f } };
	inline constexpr float HubAcademyBanks[] = { 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.3f, 0.0f };
	inline constexpr Vec2 HubMaplePoints[] = { { -27.0f, 13.0f }, { -22.0f, 11.5f }, { -17.5f, 14.0f }, { -16.5f, 19.0f }, { -20.0f, 22.5f }, { -26.0f, 22.0f }, { -29.0f, 18.0f } };
	inline constexpr float HubMapleBanks[] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
	inline constexpr Vec2 HubRiftPoints[] = { { -27.0f, -3.0f }, { -29.0f, -7.0f }, { -35.0f, -7.5f }, { -39.5f, -3.0f }, { -40.0f, 6.0f }, { -36.0f, 10.5f }, { -30.0f, 9.5f }, { -27.0f, 6.0f } };
	inline constexpr float HubRiftBanks[] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

	struct HubTerrace
	{
		const Vec2* Points = nullptr;
		const float* Banks = nullptr;
		size_t Count = 0;
		float Rise = 0.0f;
		bool Rocks = true;
	};

	inline constexpr HubTerrace HubTerraces[] = {
		{ HubNorthPoints, HubNorthBanks, std::size(HubNorthPoints), HubNorthLevel },
		{ HubAcademyPoints, HubAcademyBanks, std::size(HubAcademyPoints), HubAcademyLevel, false },
		{ HubMaplePoints, HubMapleBanks, std::size(HubMaplePoints), HubMapleLevel },
		{ HubRiftPoints, HubRiftBanks, std::size(HubRiftPoints), HubRiftLevel },
	};
	inline constexpr float HubCliffHalf = 0.14f;
	inline constexpr float HubBankSlope = 0.17f;
	inline constexpr float HubTerraceWarp = 0.75f;

	struct HubStair
	{
		int32_t X = 0;
		int32_t Y = 0;
		int32_t Width = 1;
		int32_t Height = 1;
		Direction Up = Direction::North;
		float Bottom = 0.0f;
		float Top = 0.0f;
	};

	inline constexpr float HubDockLow = 0.3f;
	inline constexpr float HubDockHigh = 1.3f;
	inline constexpr HubStair HubStairs[] = {
		{ -4, 13, 9, 3, Direction::North, 0.0f, HubNorthLevel },
		{ 9, 7, 2, 4, Direction::East, 0.0f, HubAcademyLevel },
		{ 30, -14, 2, 3, Direction::East, HubDockLow, HubDockHigh },
	};
	inline constexpr float HubStepRise = 0.14f;

	inline constexpr Vec2 HubStreamPoints[] = { { -5.0f, 22.5f }, { -7.0f, 20.0f }, { -9.0f, 16.5f }, { -10.5f, 13.0f }, { -12.5f, 10.0f }, { -14.5f, 6.0f }, { -16.5f, 0.5f }, { -18.5f, -4.0f }, { -22.0f, -8.0f }, { -27.0f, -11.0f }, { -31.8f, -12.9f }, { -34.0f, -13.8f } };
	inline constexpr float HubStreamWidth[] = { 0.55f, 1.05f };
	inline constexpr float HubStreamBank = 1.4f;
	inline constexpr float HubStreamDepth = 0.28f;
	inline constexpr float HubWaterAboveBed = 0.16f;

	enum class HubTier : uint8_t
	{
		Spine,
		Branch,
		Trail,
	};

	struct HubPath
	{
		const Vec2* Points = nullptr;
		size_t Count = 0;
		HubTier Tier = HubTier::Branch;
		TerrainId Stone = TerrainId::Flagstone;
	};

	inline constexpr float HubTierHalf[] = { 2.0f, 1.3f, 0.6f };

	inline constexpr Vec2 HubSpineSouth[] = { { 0.0f, -23.5f }, { -1.8f, -19.0f }, { 1.2f, -14.0f }, { 0.0f, -9.0f } };
	inline constexpr Vec2 HubSpineNorth[] = { { 0.0f, 9.0f }, { -0.8f, 11.0f }, { 0.0f, 12.6f } };
	inline constexpr Vec2 HubAcademyPath[] = { { 6.4f, 7.0f }, { 7.6f, 8.2f }, { 8.6f, 8.6f } };
	inline constexpr Vec2 HubDockPath[] = { { 9.0f, -2.6f }, { 14.0f, -5.5f }, { 19.0f, -8.0f }, { 21.5f, -9.6f } };
	inline constexpr Vec2 HubRiftPath[] = { { -9.2f, 0.2f }, { -13.0f, 0.8f }, { -16.5f, 0.4f }, { -20.0f, 0.9f }, { -24.0f, 1.8f }, { -27.8f, 2.0f } };
	inline constexpr Vec2 HubSocialPath[] = { { -6.6f, 6.8f }, { -9.5f, 8.6f }, { -12.5f, 10.0f }, { -15.2f, 11.6f } };
	inline constexpr Vec2 HubMapleTrail[] = { { -19.5f, 14.6f }, { -21.0f, 15.8f } };
	inline constexpr Vec2 HubKnollTrail[] = { { -5.5f, 17.5f }, { -8.3f, 18.2f }, { -13.0f, 18.3f }, { -17.5f, 17.5f } };
	inline constexpr Vec2 HubOverlookTrail[] = { { -1.4f, -17.0f }, { -8.0f, -15.6f }, { -15.0f, -14.6f }, { -22.0f, -13.3f }, { -27.0f, -13.0f } };
	inline constexpr Vec2 HubEastTrail[] = { { 36.5f, 11.0f }, { 36.6f, 3.5f }, { 33.5f, -2.5f }, { 30.5f, -6.8f } };
	inline constexpr Vec2 HubGardenTrail[] = { { 14.5f, -5.8f }, { 13.5f, -10.0f }, { 12.5f, -12.2f } };
	inline constexpr Vec2 HubCampNorth[] = { { -34.5f, 5.5f }, { -36.2f, 7.6f } };
	inline constexpr Vec2 HubCampSouth[] = { { -34.5f, -1.8f }, { -36.5f, -4.2f } };

	inline constexpr HubPath HubPaths[] = {
		{ HubSpineSouth, std::size(HubSpineSouth), HubTier::Spine },
		{ HubSpineNorth, std::size(HubSpineNorth), HubTier::Spine },
		{ HubAcademyPath, std::size(HubAcademyPath), HubTier::Branch },
		{ HubDockPath, std::size(HubDockPath), HubTier::Branch, TerrainId::Cobblestone },
		{ HubRiftPath, std::size(HubRiftPath), HubTier::Branch },
		{ HubSocialPath, std::size(HubSocialPath), HubTier::Branch, TerrainId::Cobblestone },
		{ HubMapleTrail, std::size(HubMapleTrail), HubTier::Trail, TerrainId::Dirt },
		{ HubKnollTrail, std::size(HubKnollTrail), HubTier::Trail, TerrainId::Dirt },
		{ HubOverlookTrail, std::size(HubOverlookTrail), HubTier::Trail, TerrainId::Dirt },
		{ HubEastTrail, std::size(HubEastTrail), HubTier::Trail, TerrainId::Dirt },
		{ HubGardenTrail, std::size(HubGardenTrail), HubTier::Trail, TerrainId::Dirt },
		{ HubCampNorth, std::size(HubCampNorth), HubTier::Trail, TerrainId::Dirt },
		{ HubCampSouth, std::size(HubCampSouth), HubTier::Trail, TerrainId::Dirt },
	};
	inline constexpr int32_t HubPathSamples = 10;

	struct HubPlaza
	{
		Vec2 Center;
		float Radius = 1.0f;
		TerrainId Stone = TerrainId::Flagstone;
	};

	inline constexpr float HubCourtRadius = 9.5f;
	inline constexpr HubPlaza HubPlazas[] = {
		{ { 0.0f, 18.0f }, 5.0f },
		{ { -32.0f, 2.0f }, 4.5f },
		{ { -17.5f, 12.5f }, 3.4f, TerrainId::Cobblestone },
		{ { -28.0f, -13.2f }, 1.6f },
		{ { 59.0f, 14.4f }, 3.4f },
	};
	inline constexpr Vec2 HubCourtyard[] = { { 10.4f, 6.5f }, { 25.0f, 6.0f }, { 25.0f, 17.6f }, { 11.0f, 17.6f }, { 10.4f, 12.0f } };
	inline constexpr Vec2 HubDockYard[] = { { 20.5f, -7.6f }, { 30.0f, -7.4f }, { 31.0f, -10.0f }, { 31.0f, -17.0f }, { 21.5f, -17.0f }, { 20.0f, -12.5f } };

	inline constexpr Vec2 HubGateSpot{ 0.0f, 21.0f };
	inline constexpr Vec2 HubRiftSpot{ -32.0f, 2.0f };
	inline constexpr Vec2 HubChessSpot{ -17.5f, 12.5f };
	inline constexpr Vec2 HubMapleSpot{ -22.5f, 17.5f };
	inline constexpr Vec2 HubBoardSpot{ -4.0f, -16.6f };
	inline constexpr Vec2 HubWishSpot{ 38.4f, -12.5f };
	inline constexpr Vec2 HubSpawnSpot{ 0.0f, 11.2f };
	inline constexpr Vec2 HubDoorSpot{ 18.0f, 17.25f };
	inline constexpr Vec2 HubDoorReturn{ 18.0f, 15.2f };
	inline constexpr Vec2 HubFountainSpot{ 18.0f, 11.5f };
	inline constexpr Vec2 HubWellSpot{ 5.0f, -15.0f };
	inline constexpr Vec2 HubTrainingSpot{ 59.0f, 14.0f };
	inline constexpr float HubTrainingRadius = 6.3f;

	inline constexpr float HubDeckRadius = 7.2f;
	inline constexpr float HubHeartTop = 0.014f;
	inline constexpr float HubHeartSink = 0.15f;
	inline constexpr float HubGroundHole = 7.0f;

	struct HubFlat
	{
		Vec2 Center;
		float Radius = 1.0f;
	};

	inline constexpr HubFlat HubFlats[] = {
		{ { 0.0f, 0.0f }, 10.5f },
		{ { 0.0f, 18.5f }, 6.5f },
		{ HubChessSpot, 4.0f },
		{ HubMapleSpot, 4.2f },
		{ HubRiftSpot, 5.2f },
		{ HubTrainingSpot, 7.5f },
		{ { 17.5f, 12.0f }, 7.5f },
		{ { 21.0f, 21.0f }, 7.0f },
		{ { 28.0f, 12.0f }, 5.0f },
		{ { 26.0f, -12.3f }, 6.5f },
		{ HubBoardSpot, 1.6f },
		{ { -28.0f, -13.2f }, 2.0f },
		{ HubWellSpot, 1.8f },
	};
	inline constexpr float HubFlatBlend = 2.5f;
	inline constexpr float HubReliefAmplitude = 0.3f;
	inline constexpr float HubBrowDrop = 0.55f;
	inline constexpr float HubBrowReach = 1.4f;
	inline constexpr float HubSurfaceReach = -0.15f;

	struct HubFootbridge
	{
		Vec2 Center;
		Vec2 Along;
		float HalfLength = 2.4f;
		float HalfWidth = 1.2f;
	};

	inline constexpr HubFootbridge HubFootbridges[] = {
		{ { -12.6f, 10.0f }, { -0.88f, -0.48f }, 2.5f, 1.1f },
		{ { -16.6f, 0.45f }, { -1.0f, 0.02f }, 2.6f, 1.3f },
	};
	inline constexpr float HubBridgeArch = 0.32f;

	enum class HubBlockKind : uint8_t
	{
		Building,
		Roof,
		Tower,
		Door,
		Deck,
		Pier,
		Envelope,
		Gondola,
		Bridge,
		Footbridge,
		Wall,
		Fence,
		Tent,
	};

	struct HubBlock
	{
		HubBlockKind Kind = HubBlockKind::Building;
		MeshKind Mesh = MeshKind::Box;
		Vec2 Center;
		Vec2 Half;
		float Height = 1.0f;
		float Lift = 0.0f;
		float Yaw = 0.0f;
		bool Solid = true;
		bool Absolute = false;
	};

	struct HubSign
	{
		Vec2 Position;
		float Yaw = 0.0f;
		const char* Title = "";
	};

	inline constexpr HubSign HubSigns[] = {
		{ { 6.8f, 4.8f }, 0.0f, "Academy" },
		{ { 18.4f, -6.4f }, 0.0f, "Sky dock" },
		{ { -22.8f, 3.4f }, 0.0f, "Shardfall camp" },
		{ { 3.6f, -21.2f }, 0.0f, "Broken bridge" },
		{ { -14.6f, 9.0f }, 0.0f, "Social corner" },
	};
	inline constexpr float HubSignReach = 6.0f;

	inline constexpr Vec2 HubCrystals[] = { { -36.0f, -6.0f }, { -38.6f, -2.0f }, { -39.2f, 4.0f }, { -37.6f, 8.0f }, { -33.8f, 8.8f }, { -29.2f, -5.8f }, { -28.0f, 7.6f }, { -35.0f, -3.4f } };
	inline constexpr Vec2 HubSpires[] = { { -38.4f, 1.4f }, { -36.6f, -6.8f }, { -35.4f, 9.4f } };
	inline constexpr Vec2 HubTents[] = { { -36.5f, 8.6f }, { -37.4f, -5.0f } };

	inline float HubSmooth(float low, float high, float x)
	{
		const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	inline float HubHash(int32_t x, int32_t y)
	{
		uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u;
		h = (h ^ (h >> 13)) * 1274126177u;
		return float((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f;
	}

	inline float HubNoise(Vec2 point)
	{
		const float fx = std::floor(point.x);
		const float fy = std::floor(point.y);
		const int32_t x = int32_t(fx);
		const int32_t y = int32_t(fy);
		const float u = point.x - fx;
		const float v = point.y - fy;
		const float su = u * u * (3.0f - 2.0f * u);
		const float sv = v * v * (3.0f - 2.0f * v);
		const float bottom = HubHash(x, y) + (HubHash(x + 1, y) - HubHash(x, y)) * su;
		const float top = HubHash(x, y + 1) + (HubHash(x + 1, y + 1) - HubHash(x, y + 1)) * su;
		return bottom + (top - bottom) * sv;
	}

	inline float HubSigned(Vec2 point)
	{
		return 2.0f * HubNoise(point) - 1.0f;
	}

	inline bool HubInside(const Vec2* points, size_t count, Vec2 local)
	{
		bool inside = false;
		for (size_t i = 0, j = count - 1; i < count; j = i++)
			if ((points[i].y > local.y) != (points[j].y > local.y) && local.x < (points[j].x - points[i].x) * (local.y - points[i].y) / (points[j].y - points[i].y) + points[i].x)
				inside = !inside;
		return inside;
	}

	template <size_t N>
	bool HubInside(const Vec2 (&points)[N], Vec2 local)
	{
		return HubInside(points, N, local);
	}

	struct HubEdge
	{
		float Distance = 0.0f;
		float Bank = 0.0f;
	};

	inline HubEdge HubPolygonEdge(const Vec2* points, const float* banks, size_t count, Vec2 local)
	{
		HubEdge best{ 1e9f, 0.0f };
		for (size_t i = 0, j = count - 1; i < count; j = i++)
		{
			const Vec2 span = points[i] - points[j];
			const float t = std::clamp(Dot(local - points[j], span) / std::max(LengthSquared(span), 1e-6f), 0.0f, 1.0f);
			const float distance = Length(local - (points[j] + span * t));
			if (distance < best.Distance)
			{
				const float s = t * t * (3.0f - 2.0f * t);
				best = { distance, banks ? banks[j] + (banks[i] - banks[j]) * s : 0.0f };
			}
		}
		if (!HubInside(points, count, local))
			best.Distance = -best.Distance;
		return best;
	}

	inline float HubIslandDistance(Vec2 local)
	{
		const HubEdge edge = HubPolygonEdge(HubOutline, nullptr, std::size(HubOutline), local);
		const float jag = HubJagAmplitude * HubSigned(local * HubJagScale) + 0.45f * HubJagAmplitude * HubSigned(local * (2.6f * HubJagScale) + Vec2(17.3f, -5.1f));
		return edge.Distance + jag;
	}

	inline float HubTrainingDistance(Vec2 local)
	{
		const Vec2 offset = local - HubTrainingSpot;
		const float angle = std::atan2(offset.y, offset.x);
		const float edge = HubTrainingRadius * (1.0f + 0.08f * (0.6f * std::sin(3.0f * angle + 1.9f) + 0.4f * std::sin(5.0f * angle + 4.37f)));
		return edge - Length(offset) + 0.35f * HubSigned(local * 0.9f);
	}

	inline float HubEdgeDistance(Vec2 local)
	{
		return std::max(HubIslandDistance(local), HubTrainingDistance(local));
	}

	inline bool HubIsLand(Vec2 local)
	{
		return HubEdgeDistance(local) > 0.0f;
	}

	inline Vec2 HubStairMin(const HubStair& stair)
	{
		return { float(stair.X) - 0.5f, float(stair.Y) - 0.5f };
	}

	inline Vec2 HubStairMax(const HubStair& stair)
	{
		return { float(stair.X + stair.Width) - 0.5f, float(stair.Y + stair.Height) - 0.5f };
	}

	inline float HubStairDistance(const HubStair& stair, Vec2 local)
	{
		const Vec2 low = HubStairMin(stair);
		const Vec2 high = HubStairMax(stair);
		const float dx = std::max(std::max(low.x - local.x, local.x - high.x), 0.0f);
		const float dy = std::max(std::max(low.y - local.y, local.y - high.y), 0.0f);
		return std::sqrt(dx * dx + dy * dy);
	}

	inline float HubNearestStair(Vec2 local)
	{
		float best = 1e9f;
		for (const HubStair& stair : HubStairs)
			best = std::min(best, HubStairDistance(stair, local));
		return best;
	}

	inline int32_t HubStepCount(const HubStair& stair)
	{
		return std::max(int32_t(std::lround((stair.Top - stair.Bottom) / HubStepRise)), 1);
	}

	inline float HubStairRun(const HubStair& stair, Vec2 local, float& run)
	{
		const Vec2 low = HubStairMin(stair);
		const Vec2 high = HubStairMax(stair);
		switch (stair.Up)
		{
		case Direction::North:
			run = high.y - low.y;
			return local.y - low.y;
		case Direction::South:
			run = high.y - low.y;
			return high.y - local.y;
		case Direction::East:
			run = high.x - low.x;
			return local.x - low.x;
		default:
			run = high.x - low.x;
			return high.x - local.x;
		}
	}

	inline std::optional<float> HubStairTread(Vec2 local)
	{
		for (const HubStair& stair : HubStairs)
		{
			if (HubStairDistance(stair, local) > 0.0f)
				continue;
			float run = 1.0f;
			const float along = HubStairRun(stair, local, run);
			const int32_t steps = HubStepCount(stair);
			const int32_t step = std::clamp(int32_t(std::floor(along / (run / float(steps)))), 0, steps - 1);
			return stair.Bottom + (stair.Top - stair.Bottom) * float(step + 1) / float(steps);
		}
		return std::nullopt;
	}

	inline float HubWarp(Vec2 local)
	{
		const float fade = HubSmooth(0.6f, 2.4f, HubNearestStair(local));
		return fade * HubTerraceWarp * (HubSigned(local * 0.23f + Vec2(3.1f, 8.7f)) + 0.4f * HubSigned(local * 0.71f + Vec2(-6.2f, 1.3f)));
	}

	inline float HubTerraceProfile(const HubTerrace& terrace, Vec2 local, float warp, float* bank = nullptr)
	{
		const HubEdge edge = HubPolygonEdge(terrace.Points, terrace.Banks, terrace.Count, local);
		if (bank)
			*bank = edge.Bank;
		const float half = HubCliffHalf + (terrace.Rise / HubBankSlope * 0.5f - HubCliffHalf) * edge.Bank;
		return HubSmooth(-half, half, edge.Distance + warp * (1.0f - edge.Bank));
	}

	inline float HubTerraceHeight(Vec2 local)
	{
		const float warp = HubWarp(local);
		float height = 0.0f;
		for (const HubTerrace& terrace : HubTerraces)
			height = std::max(height, terrace.Rise * HubTerraceProfile(terrace, local, warp));
		return height - HubSouthDrop * HubSmooth(-9.0f, -19.0f, local.y);
	}

	struct HubTrack
	{
		std::vector<Vec2> Points;
		std::vector<float> Arc;
		float Half = 1.0f;
		TerrainId Stone = TerrainId::Flagstone;

		float Length() const { return Arc.empty() ? 0.0f : Arc.back(); }

		float Nearest(Vec2 local, float& arc) const
		{
			float nearest = 1e9f;
			arc = 0.0f;
			for (size_t i = 0; i + 1 < Points.size(); i++)
			{
				const Vec2 span = Points[i + 1] - Points[i];
				const float t = std::clamp(Dot(local - Points[i], span) / std::max(LengthSquared(span), 1e-6f), 0.0f, 1.0f);
				const float distance = Game::Length(local - (Points[i] + span * t));
				if (distance < nearest)
				{
					nearest = distance;
					arc = Arc[i] + (Arc[i + 1] - Arc[i]) * t;
				}
			}
			return nearest;
		}

		Vec2 At(float arc) const
		{
			const size_t count = Points.size();
			for (size_t i = 0; i + 1 < count; i++)
				if (arc <= Arc[i + 1] || i + 2 == count)
				{
					const float span = std::max(Arc[i + 1] - Arc[i], 1e-6f);
					return Points[i] + (Points[i + 1] - Points[i]) * std::clamp((arc - Arc[i]) / span, 0.0f, 1.0f);
				}
			return Points.empty() ? Vec2() : Points.back();
		}
	};

	inline Vec2 HubCatmull(Vec2 a, Vec2 b, Vec2 c, Vec2 d, float t)
	{
		const float t2 = t * t;
		const float t3 = t2 * t;
		return (b * 2.0f + (c - a) * t + (a * 2.0f - b * 5.0f + c * 4.0f - d) * t2 + (b * 3.0f - a - c * 3.0f + d) * t3) * 0.5f;
	}

	inline HubTrack HubSpline(const Vec2* points, size_t count, float half, TerrainId stone)
	{
		HubTrack track;
		track.Half = half;
		track.Stone = stone;
		for (size_t i = 0; i + 1 < count; i++)
		{
			const Vec2 a = points[i > 0 ? i - 1 : i];
			const Vec2 b = points[i];
			const Vec2 c = points[i + 1];
			const Vec2 d = points[i + 2 < count ? i + 2 : i + 1];
			for (int32_t s = 0; s < HubPathSamples; s++)
				track.Points.push_back(HubCatmull(a, b, c, d, float(s) / float(HubPathSamples)));
		}
		track.Points.push_back(points[count - 1]);
		float arc = 0.0f;
		for (size_t i = 0; i < track.Points.size(); i++)
		{
			if (i > 0)
				arc += Length(track.Points[i] - track.Points[i - 1]);
			track.Arc.push_back(arc);
		}
		return track;
	}

	inline const std::vector<HubTrack>& HubTracks()
	{
		static const std::vector<HubTrack> tracks = []
		{
			std::vector<HubTrack> result;
			for (const HubPath& path : HubPaths)
				result.push_back(HubSpline(path.Points, path.Count, HubTierHalf[size_t(path.Tier)], path.Stone));
			return result;
		}();
		return tracks;
	}

	inline const HubTrack& HubStream()
	{
		static const HubTrack stream = HubSpline(HubStreamPoints, std::size(HubStreamPoints), HubStreamWidth[1], TerrainId::Water);
		return stream;
	}

	inline float HubStreamHalf(float arc)
	{
		return HubStreamWidth[0] + (HubStreamWidth[1] - HubStreamWidth[0]) * std::clamp(arc / std::max(HubStream().Length(), 1.0f), 0.0f, 1.0f);
	}

	inline float HubStreamClearance(Vec2 local)
	{
		static const std::pair<Vec2, Vec2> box = []
		{
			Vec2 low(1e9f, 1e9f);
			Vec2 high(-1e9f, -1e9f);
			for (const Vec2 point : HubStream().Points)
			{
				low = { std::min(low.x, point.x), std::min(low.y, point.y) };
				high = { std::max(high.x, point.x), std::max(high.y, point.y) };
			}
			return std::pair{ low, high };
		}();
		const float outside = std::max({ box.first.x - local.x, local.x - box.second.x, box.first.y - local.y, local.y - box.second.y });
		if (outside > 6.0f)
			return outside - HubStreamWidth[1];
		float arc = 0.0f;
		const float distance = HubStream().Nearest(local, arc);
		return distance - HubStreamHalf(arc);
	}

	inline float HubStreamCut(Vec2 local)
	{
		return HubStreamDepth * (1.0f - HubSmooth(0.0f, HubStreamBank, HubStreamClearance(local)));
	}

	inline float HubFlatness(Vec2 local)
	{
		float flat = 1.0f;
		for (const HubFlat& zone : HubFlats)
			flat = std::min(flat, HubSmooth(0.0f, HubFlatBlend, Length(local - zone.Center) - zone.Radius));
		flat = std::min(flat, HubSmooth(0.3f, 1.8f, HubNearestStair(local)));
		flat = std::min(flat, HubSmooth(0.5f, 3.0f, HubStreamClearance(local)));
		return flat;
	}

	inline float HubRelief(Vec2 local)
	{
		const float rolling = 0.75f * HubSigned(local * 0.085f + Vec2(5.3f, -2.9f)) + 0.35f * HubSigned(local * 0.26f + Vec2(-8.1f, 4.4f)) + 0.12f * HubSigned(local * 0.8f);
		const float hummocks = 0.9f * HubSmooth(0.58f, 0.86f, HubNoise(local * 0.19f + Vec2(11.0f, 23.0f)));
		return HubReliefAmplitude * (rolling + hummocks);
	}

	inline float HubBaseFloor(Vec2 local)
	{
		return HubTerraceHeight(local) + HubRelief(local) * HubFlatness(local);
	}

	inline float HubFloor(Vec2 local)
	{
		return HubBaseFloor(local) - HubStreamCut(local);
	}

	inline float HubWaterLevel(Vec2 center)
	{
		return HubBaseFloor(center) - HubStreamDepth + HubWaterAboveBed;
	}

	struct HubPave
	{
		float Inside = -1e9f;
		TerrainId Stone = TerrainId::Flagstone;
	};

	inline HubPave HubPaving(Vec2 local)
	{
		HubPave best;
		const auto offer = [&](float inside, TerrainId stone)
		{
			if (inside > best.Inside)
				best = { inside, stone };
		};
		const float r = Length(local);
		offer(HubCourtRadius + 0.45f * HubSigned(local * 0.35f) - r, TerrainId::Flagstone);
		for (const HubTrack& track : HubTracks())
		{
			float arc = 0.0f;
			const float nearest = track.Nearest(local, arc);
			const float wobble = track.Half < 1.0f ? 0.3f : 0.22f;
			const float half = track.Half * (1.0f + wobble * HubSigned(Vec2(arc * 0.31f, track.Half * 7.0f)));
			offer(half - nearest, track.Stone);
		}
		for (const HubPlaza& plaza : HubPlazas)
			offer(plaza.Radius + 0.5f * HubSigned((local - plaza.Center) * 0.5f + plaza.Center) - Length(local - plaza.Center), plaza.Stone);
		const auto polygon = [&](const Vec2* points, size_t count, TerrainId stone, float noise)
		{
			offer(HubPolygonEdge(points, nullptr, count, local).Distance + noise * (0.55f * HubSigned(local * 0.3f) + 0.2f * HubSigned(local * 0.9f + Vec2(4.0f, 2.0f))), stone);
		};
		polygon(HubCourtyard, std::size(HubCourtyard), TerrainId::Flagstone, 0.6f);
		polygon(HubDockYard, std::size(HubDockYard), TerrainId::Dirt, 1.0f);
		best.Inside = std::min(best.Inside, HubStreamClearance(local) - 0.5f);
		return best;
	}

	inline float HubBrow(float edge)
	{
		const float brow = 1.0f - HubSmooth(-0.4f, HubBrowReach, edge);
		return HubBrowDrop * brow * brow;
	}

	inline float HubSurfaceFrom(Vec2 local, float floor, float edge, float paved)
	{
		if (const std::optional<float> tread = HubStairTread(local))
			return *tread - 0.12f;
		float height = floor - HubBrow(edge);
		height -= 0.03f * HubSmooth(-0.2f, 0.2f, paved);
		height -= HubHeartSink * (1.0f - HubSmooth(EmberSurroundRadius - 0.25f, EmberSurroundRadius + 0.05f, Length(local)));
		return height;
	}

	inline float HubSurface(Vec2 local)
	{
		return HubSurfaceFrom(local, HubFloor(local), HubEdgeDistance(local), HubPaving(local).Inside);
	}

	inline Vec2 HubDockLowMin() { return { 20.5f, -17.0f }; }
	inline Vec2 HubDockLowMax() { return { 31.5f, -7.6f }; }
	inline Vec2 HubDockHighMin() { return { 31.5f, -16.0f }; }
	inline Vec2 HubDockHighMax() { return { 39.6f, -9.0f }; }

	inline bool HubInBox(Vec2 local, Vec2 low, Vec2 high)
	{
		return local.x >= low.x && local.y >= low.y && local.x <= high.x && local.y <= high.y;
	}

	inline std::optional<float> HubDeckHeight(Vec2 local)
	{
		if (HubInBox(local, HubDockLowMin(), HubDockLowMax()))
			return HubDockLow;
		if (HubInBox(local, HubDockHighMin(), HubDockHighMax()))
			return HubDockHigh;
		for (const HubFootbridge& bridge : HubFootbridges)
		{
			const Vec2 along = Normalize(bridge.Along);
			const Vec2 offset = local - bridge.Center;
			const float u = Dot(offset, along);
			const float v = Dot(offset, Perpendicular(along));
			if (std::abs(u) > bridge.HalfLength || std::abs(v) > bridge.HalfWidth)
				continue;
			const float ends = std::max(HubBaseFloor(bridge.Center + along * bridge.HalfLength), HubBaseFloor(bridge.Center - along * bridge.HalfLength));
			const float t = u / bridge.HalfLength;
			return ends + 0.1f + HubBridgeArch * (1.0f - t * t);
		}
		return std::nullopt;
	}

	inline std::optional<float> HubGround(Vec2 position)
	{
		const Vec2 local = position - EmberCenter;
		if (Length(local) < EmberSurroundRadius)
			return HubHeartTop;
		if (const std::optional<float> terrace = PortalTerraceGround(local - HubGateSpot))
			return HubNorthLevel + *terrace;
		if (const std::optional<float> tread = HubStairTread(local))
			return *tread;
		return HubDeckHeight(local);
	}

	inline void HubAddWall(std::vector<HubBlock>& blocks, const Vec2* points, size_t count, HubBlockKind kind, float height, float thickness, float lift = 0.0f, bool absolute = false)
	{
		for (size_t i = 0; i + 1 < count; i++)
		{
			const Vec2 a = points[i];
			const Vec2 b = points[i + 1];
			const float span = Length(b - a);
			const int32_t pieces = std::max(int32_t(std::ceil(span / 1.8f)), 1);
			for (int32_t p = 0; p < pieces; p++)
			{
				const Vec2 c = a + (b - a) * ((float(p) + 0.5f) / float(pieces));
				const Vec2 axis = (b - a) / std::max(span, 1e-4f);
				blocks.push_back({ kind, MeshKind::Box, c, { 0.5f * span / float(pieces) + 0.04f, thickness }, height, lift, std::atan2(axis.y, axis.x), true, absolute });
			}
		}
	}

	inline void HubAddArc(std::vector<HubBlock>& blocks, float radius, float from, float to, float height)
	{
		const int32_t pieces = std::max(int32_t(std::ceil(radius * (to - from) * Pi / 180.0f / 1.6f)), 1);
		std::vector<Vec2> points;
		for (int32_t p = 0; p <= pieces; p++)
		{
			const float angle = (from + (to - from) * float(p) / float(pieces)) * Pi / 180.0f;
			points.push_back({ radius * std::cos(angle), radius * std::sin(angle) });
		}
		HubAddWall(blocks, points.data(), points.size(), HubBlockKind::Wall, height, 0.28f);
	}

	inline constexpr float HubWallArcs[][2] = { { 197.0f, 252.0f }, { 52.0f, 80.0f }, { 288.0f, 328.0f } };
	inline constexpr float HubWallArcRadius = 10.5f;

	inline const std::vector<HubBlock>& HubBlocks()
	{
		static const std::vector<HubBlock> blocks = []
		{
			std::vector<HubBlock> result = {
				{ HubBlockKind::Building, MeshKind::Box, { 21.0f, 21.0f }, { 10.0f, 3.0f }, 6.5f },
				{ HubBlockKind::Building, MeshKind::Box, { 28.0f, 12.0f }, { 3.0f, 6.0f }, 6.5f },
				{ HubBlockKind::Roof, MeshKind::Box, { 21.0f, 21.0f }, { 10.4f, 3.4f }, 2.6f, 6.5f, 0.0f, false },
				{ HubBlockKind::Roof, MeshKind::Box, { 28.0f, 12.0f }, { 3.4f, 6.4f }, 2.6f, 6.5f, 0.0f, false },
				{ HubBlockKind::Tower, MeshKind::Cylinder, { 30.0f, 23.5f }, { 3.2f, 3.2f }, 13.0f },
				{ HubBlockKind::Roof, MeshKind::Cone, { 30.0f, 23.5f }, { 3.7f, 3.7f }, 5.0f, 13.0f, 0.0f, false },
				{ HubBlockKind::Door, MeshKind::Box, { 18.0f, 18.02f }, { 1.5f, 0.12f }, 4.2f, 0.0f, 0.0f, false },
				{ HubBlockKind::Deck, MeshKind::Box, { 26.0f, -12.3f }, { 5.5f, 4.7f }, 0.35f, HubDockLow - 0.35f, 0.0f, false, true },
				{ HubBlockKind::Deck, MeshKind::Box, { 35.55f, -12.5f }, { 4.05f, 3.5f }, 0.35f, HubDockHigh - 0.35f, 0.0f, false, true },
				{ HubBlockKind::Pier, MeshKind::Box, { 31.8f, -9.3f }, { 0.25f, 0.25f }, 1.65f, -0.35f, 0.0f, true, true },
				{ HubBlockKind::Pier, MeshKind::Box, { 31.8f, -15.7f }, { 0.25f, 0.25f }, 1.65f, -0.35f, 0.0f, true, true },
				{ HubBlockKind::Pier, MeshKind::Box, { 39.3f, -9.3f }, { 0.25f, 0.25f }, 4.0f, -2.7f, 0.0f, true, true },
				{ HubBlockKind::Pier, MeshKind::Box, { 39.3f, -15.7f }, { 0.25f, 0.25f }, 4.0f, -2.7f, 0.0f, true, true },
				{ HubBlockKind::Pier, MeshKind::Box, { 35.5f, -15.7f }, { 0.25f, 0.25f }, 3.5f, -2.2f, 0.0f, true, true },
				{ HubBlockKind::Envelope, MeshKind::Sphere, { 45.5f, -6.0f }, { 6.5f, 2.4f }, 4.6f, 4.6f, 0.0f, false },
				{ HubBlockKind::Gondola, MeshKind::Box, { 45.5f, -6.0f }, { 2.2f, 0.9f }, 1.2f, 2.8f, 0.0f, false },
				{ HubBlockKind::Envelope, MeshKind::Sphere, { 37.0f, -24.0f }, { 2.2f, 6.0f }, 4.4f, 4.0f, 0.0f, false },
				{ HubBlockKind::Gondola, MeshKind::Box, { 37.0f, -24.0f }, { 0.9f, 2.0f }, 1.1f, 2.3f, 0.0f, false },
				{ HubBlockKind::Bridge, MeshKind::Box, { 0.0f, -25.6f }, { 2.3f, 1.2f }, 0.6f, -0.9f, 0.0f, false, true },
				{ HubBlockKind::Bridge, MeshKind::Box, { 0.3f, -28.6f }, { 2.0f, 1.0f }, 0.6f, -1.5f, 0.15f, false, true },
				{ HubBlockKind::Bridge, MeshKind::Box, { -0.3f, -31.4f }, { 1.6f, 0.9f }, 0.6f, -2.3f, -0.2f, false, true },
			};
			for (const HubFootbridge& bridge : HubFootbridges)
			{
				const Vec2 along = Normalize(bridge.Along);
				const float top = *HubDeckHeight(bridge.Center);
				result.push_back({ HubBlockKind::Footbridge, MeshKind::Box, bridge.Center, { bridge.HalfLength, bridge.HalfWidth }, 0.22f, top - 0.22f, std::atan2(along.y, along.x), false, true });
				for (const float side : { -1.0f, 1.0f })
					result.push_back({ HubBlockKind::Fence, MeshKind::Box, bridge.Center + Perpendicular(along) * (side * (bridge.HalfWidth - 0.06f)), { bridge.HalfLength, 0.05f }, 0.75f, top, std::atan2(along.y, along.x), true, true });
			}
			for (const Vec2 tent : HubTents)
				result.push_back({ HubBlockKind::Tent, MeshKind::Cone, tent, { 1.9f, 1.9f }, 2.3f });
			const Vec2 academyFront[] = { { 8.75f, 10.6f }, { 8.75f, 14.0f }, { 10.25f, 21.6f } };
			const Vec2 academySouth[] = { { 8.75f, 6.4f }, { 8.75f, 5.25f }, { 16.0f, 3.75f } };
			HubAddWall(result, academyFront, std::size(academyFront), HubBlockKind::Wall, 1.3f, 0.3f, 0.0f, true);
			HubAddWall(result, academySouth, std::size(academySouth), HubBlockKind::Wall, 1.3f, 0.3f, 0.0f, true);
			for (const float side : { -1.0f, 1.0f })
			{
				const Vec2 cheek[] = { { side * 4.75f, 12.35f }, { side * 4.75f, 15.6f } };
				HubAddWall(result, cheek, std::size(cheek), HubBlockKind::Wall, HubNorthLevel + 0.6f, 0.28f, 0.0f, true);
				const Vec2 approach[] = { { side * 2.7f, -20.2f }, { side * 2.5f, -23.4f } };
				HubAddWall(result, approach, std::size(approach), HubBlockKind::Wall, 0.7f, 0.28f);
			}
			for (const auto& arc : HubWallArcs)
				HubAddArc(result, HubWallArcRadius, arc[0], arc[1], 0.6f);
			const Vec2 dockFence[] = { { 14.4f, -3.4f }, { 19.6f, -6.2f }, { 20.4f, -7.2f } };
			const Vec2 garden[] = { { 9.5f, -12.0f }, { 15.0f, -12.2f }, { 15.5f, -16.0f }, { 9.5f, -16.0f }, { 9.5f, -13.6f } };
			const Vec2 overlook[] = { { -29.8f, -11.9f }, { -26.4f, -11.8f } };
			const Vec2 upperRail[] = { { 31.6f, -9.05f }, { 39.55f, -9.05f }, { 39.55f, -15.95f }, { 31.6f, -15.95f }, { 31.6f, -14.6f } };
			HubAddWall(result, dockFence, std::size(dockFence), HubBlockKind::Fence, 0.9f, 0.05f);
			HubAddWall(result, garden, std::size(garden), HubBlockKind::Fence, 0.8f, 0.05f);
			HubAddWall(result, overlook, std::size(overlook), HubBlockKind::Fence, 0.9f, 0.05f);
			const Vec2 upperRailWest[] = { { 31.6f, -11.4f }, { 31.6f, -9.05f } };
			HubAddWall(result, upperRail, std::size(upperRail), HubBlockKind::Fence, 0.9f, 0.05f, HubDockHigh, true);
			HubAddWall(result, upperRailWest, std::size(upperRailWest), HubBlockKind::Fence, 0.9f, 0.05f, HubDockHigh, true);
			return result;
		}();
		return blocks;
	}
}
