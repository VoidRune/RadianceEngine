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

	struct HubRiverPoint
	{
		Vec2 Point;
		float Half = 1.0f;
		float Depth = 0.4f;
	};

	inline constexpr HubRiverPoint HubRiverPoints[] = {
		{ { -13.4f, 23.2f }, 2.0f, 0.7f },
		{ { -13.0f, 21.6f }, 1.05f, 0.36f },
		{ { -13.3f, 18.3f }, 1.05f, 0.36f },
		{ { -12.6f, 15.0f }, 1.35f, 0.46f },
		{ { -12.2f, 12.6f }, 1.4f, 0.48f },
		{ { -13.1f, 10.3f }, 1.15f, 0.38f },
		{ { -14.3f, 6.4f }, 1.85f, 0.6f },
		{ { -15.9f, 2.7f }, 1.5f, 0.48f },
		{ { -16.8f, 0.5f }, 1.2f, 0.4f },
		{ { -17.6f, -2.4f }, 1.9f, 0.58f },
		{ { -19.6f, -5.4f }, 2.8f, 0.85f },
		{ { -22.8f, -8.4f }, 1.75f, 0.52f },
		{ { -26.6f, -10.0f }, 1.35f, 0.44f },
		{ { -30.4f, -10.6f }, 1.05f, 0.32f },
		{ { -34.2f, -11.4f }, 1.0f, 0.3f },
		{ { -37.4f, -12.2f }, 1.0f, 0.3f },
	};
	inline constexpr Vec2 HubBridgeCrossings[] = { { -13.3f, 18.2f }, { -13.3f, 10.5f }, { -16.8f, 0.45f } };
	inline constexpr float HubRiverSpacing = 0.2f;
	inline constexpr float HubRiverFreeboard = 0.1f;
	inline constexpr float HubRiverStep = 0.2f;
	inline constexpr float HubRiverBankSlope = 0.45f;
	inline constexpr float HubRiverBlend = 0.25f;
	inline constexpr float HubRiverWade = 0.2f;

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
		float HalfWidth = 1.0f;
		float Ends = 0.0f;
	};

	inline constexpr float HubBridgeArch = 0.28f;
	inline constexpr float HubBridgeReach = 1.5f;
	inline constexpr float HubBridgeHalfWidth = 0.95f;

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
		{ { -15.6f, 9.4f }, 0.0f, "Social corner" },
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

	struct HubRiverSample
	{
		Vec2 Point;
		Vec2 Tangent;
		float Half = 1.0f;
		float Depth = 0.4f;
	};

	struct HubRiverShape
	{
		HubTrack Track;
		std::vector<float> TrackHalf;
		std::vector<float> TrackDepth;
		std::vector<HubRiverSample> Samples;
		Vec2 Low;
		Vec2 High;
		float Widest = 0.0f;

		float Length() const { return Track.Length(); }
	};

	inline const HubRiverShape& HubRiver()
	{
		static const HubRiverShape shape = []
		{
			HubRiverShape result;
			constexpr size_t count = std::size(HubRiverPoints);
			Vec2 points[count];
			for (size_t i = 0; i < count; i++)
				points[i] = HubRiverPoints[i].Point;
			result.Track = HubSpline(points, count, 1.0f, TerrainId::Water);
			for (size_t i = 0; i + 1 < count; i++)
				for (int32_t s = 0; s < HubPathSamples; s++)
				{
					const float t = HubSmooth(0.0f, 1.0f, float(s) / float(HubPathSamples));
					result.TrackHalf.push_back(HubRiverPoints[i].Half + (HubRiverPoints[i + 1].Half - HubRiverPoints[i].Half) * t);
					result.TrackDepth.push_back(HubRiverPoints[i].Depth + (HubRiverPoints[i + 1].Depth - HubRiverPoints[i].Depth) * t);
				}
			result.TrackHalf.push_back(HubRiverPoints[count - 1].Half);
			result.TrackDepth.push_back(HubRiverPoints[count - 1].Depth);
			result.Low = Vec2(1e9f, 1e9f);
			result.High = Vec2(-1e9f, -1e9f);
			const HubTrack& track = result.Track;
			size_t segment = 0;
			for (float arc = 0.0f; arc <= track.Length() + 1e-3f; arc += HubRiverSpacing)
			{
				while (segment + 2 < track.Points.size() && arc > track.Arc[segment + 1])
					segment++;
				const float span = std::max(track.Arc[segment + 1] - track.Arc[segment], 1e-6f);
				const float t = std::clamp((arc - track.Arc[segment]) / span, 0.0f, 1.0f);
				HubRiverSample sample;
				sample.Point = track.Points[segment] + (track.Points[segment + 1] - track.Points[segment]) * t;
				sample.Half = result.TrackHalf[segment] + (result.TrackHalf[segment + 1] - result.TrackHalf[segment]) * t;
				sample.Depth = result.TrackDepth[segment] + (result.TrackDepth[segment + 1] - result.TrackDepth[segment]) * t;
				result.Samples.push_back(sample);
				result.Low = { std::min(result.Low.x, sample.Point.x), std::min(result.Low.y, sample.Point.y) };
				result.High = { std::max(result.High.x, sample.Point.x), std::max(result.High.y, sample.Point.y) };
				result.Widest = std::max(result.Widest, sample.Half);
			}
			const size_t samples = result.Samples.size();
			for (size_t i = 0; i < samples; i++)
				result.Samples[i].Tangent = Normalize(result.Samples[std::min(i + 2, samples - 1)].Point - result.Samples[i >= 2 ? i - 2 : 0].Point);
			return result;
		}();
		return shape;
	}

	inline HubRiverSample HubRiverSampleAt(float arc)
	{
		const std::vector<HubRiverSample>& samples = HubRiver().Samples;
		const float index = std::clamp(arc / HubRiverSpacing, 0.0f, float(samples.size() - 1));
		const size_t i = std::min(size_t(index), samples.size() - 2);
		const float t = index - float(i);
		const HubRiverSample& a = samples[i];
		const HubRiverSample& b = samples[i + 1];
		return { a.Point + (b.Point - a.Point) * t, Normalize(a.Tangent + (b.Tangent - a.Tangent) * t), a.Half + (b.Half - a.Half) * t, a.Depth + (b.Depth - a.Depth) * t };
	}

	struct HubRiverHit
	{
		float Clearance = 1e9f;
		float Arc = 0.0f;
		float Across = 0.0f;
		float Half = 1.0f;
	};

	inline HubRiverHit HubRiverAt(Vec2 local)
	{
		const HubRiverShape& river = HubRiver();
		HubRiverHit hit;
		const float outside = std::max({ river.Low.x - local.x, local.x - river.High.x, river.Low.y - local.y, local.y - river.High.y });
		if (outside > 6.0f)
		{
			hit.Clearance = outside - river.Widest * 1.2f;
			return hit;
		}
		const std::vector<Vec2>& points = river.Track.Points;
		float best = 1e18f;
		float side = 1.0f;
		for (size_t i = 0; i + 1 < points.size(); i++)
		{
			const Vec2 span = points[i + 1] - points[i];
			const float t = std::clamp(Dot(local - points[i], span) / std::max(LengthSquared(span), 1e-6f), 0.0f, 1.0f);
			const float distance = LengthSquared(local - (points[i] + span * t));
			if (distance < best)
			{
				best = distance;
				hit.Arc = river.Track.Arc[i] + (river.Track.Arc[i + 1] - river.Track.Arc[i]) * t;
				side = span.x * (local.y - points[i].y) - span.y * (local.x - points[i].x) >= 0.0f ? 1.0f : -1.0f;
			}
		}
		hit.Across = side * std::sqrt(best);
		const float wobble = 1.0f + 0.16f * HubSigned(Vec2(hit.Arc * 0.4f, side > 0.0f ? 2.3f : -5.9f)) + 0.07f * HubSigned(Vec2(hit.Arc * 1.3f, side * 4.1f));
		hit.Half = HubRiverSampleAt(hit.Arc).Half * wobble;
		hit.Clearance = std::abs(hit.Across) - hit.Half;
		return hit;
	}

	inline float HubStreamClearance(Vec2 local)
	{
		return HubRiverAt(local).Clearance;
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

	struct HubRiverFlow
	{
		std::vector<float> Level;
		std::vector<float> Rapid;
		size_t Lip = 0;
		float LipArc = 0.0f;
	};

	inline const HubRiverFlow& HubRiverLevels()
	{
		static const HubRiverFlow flow = []
		{
			const std::vector<HubRiverSample>& samples = HubRiver().Samples;
			const size_t count = samples.size();
			HubRiverFlow result;
			result.Level.assign(count, 0.0f);
			result.Rapid.assign(count, 0.0f);
			result.Lip = count - 1;
			for (size_t i = 0; i < count; i++)
				if (HubIslandDistance(samples[i].Point) < 0.3f)
				{
					result.Lip = i;
					break;
				}
			result.LipArc = float(result.Lip) * HubRiverSpacing;
			float running = 1e9f;
			float current = 1e9f;
			for (size_t i = 0; i <= result.Lip; i++)
			{
				const HubRiverSample& sample = samples[i];
				float bank = 1e9f;
				for (const float offset : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
					bank = std::min(bank, HubBaseFloor(sample.Point + Perpendicular(sample.Tangent) * (offset * (sample.Half * 1.25f + 0.3f))));
				running = std::min(running, bank - HubRiverFreeboard);
				if (i == 0)
					current = running;
				else if (running < current)
				{
					current = std::min(current - HubRiverStep, running);
					for (size_t k = i >= 3 ? i - 3 : 0; k < std::min(i + 8, count); k++)
						result.Rapid[k] = std::max(result.Rapid[k], k < i ? 0.6f : 1.0f - 0.1f * float(k - i));
				}
				result.Level[i] = current;
			}
			for (size_t i = result.Lip + 1; i < count; i++)
				result.Level[i] = current;
			for (size_t i = result.Lip >= 10 ? result.Lip - 10 : 0; i <= result.Lip; i++)
				result.Rapid[i] = std::max(result.Rapid[i], 0.5f + 0.05f * float(i + 10 - result.Lip));
			return result;
		}();
		return flow;
	}

	inline float HubRiverLevelAt(float arc)
	{
		const std::vector<float>& level = HubRiverLevels().Level;
		const float index = std::clamp(arc / HubRiverSpacing, 0.0f, float(level.size() - 1));
		const size_t i = std::min(size_t(index), level.size() - 2);
		return level[i] + (level[i + 1] - level[i]) * (index - float(i));
	}

	inline float HubRiverChannel(Vec2 local, const HubRiverHit& hit)
	{
		const float level = HubRiverLevelAt(hit.Arc);
		if (hit.Clearance >= 0.0f)
			return level + hit.Clearance * HubRiverBankSlope;
		const float u = std::clamp(-hit.Clearance / std::max(hit.Half, 0.1f), 0.0f, 1.0f);
		const float depth = HubRiverSampleAt(hit.Arc).Depth * (1.0f + 0.12f * HubSigned(local * 0.9f + Vec2(3.7f, 1.1f)));
		return level - depth * HubSmooth(0.0f, 0.85f, u) + 0.04f * u * HubSigned(local * 2.1f);
	}

	inline float HubSmoothMin(float a, float b, float k)
	{
		const float h = std::max(k - std::abs(a - b), 0.0f) / k;
		return std::min(a, b) - h * h * k * 0.25f;
	}

	inline float HubFloor(Vec2 local)
	{
		const float base = HubBaseFloor(local);
		const HubRiverHit hit = HubRiverAt(local);
		if (hit.Clearance > 3.0f)
			return base;
		return HubSmoothMin(base, HubRiverChannel(local, hit), HubRiverBlend);
	}

	inline float HubRiverDepth(Vec2 local)
	{
		const HubRiverHit hit = HubRiverAt(local);
		if (hit.Clearance > 0.0f || hit.Arc > HubRiverLevels().LipArc)
			return 0.0f;
		return std::max(HubRiverLevelAt(hit.Arc) - HubFloor(local), 0.0f);
	}

	inline const std::vector<HubFootbridge>& HubFootbridges()
	{
		static const std::vector<HubFootbridge> bridges = []
		{
			std::vector<HubFootbridge> result;
			for (const Vec2 crossing : HubBridgeCrossings)
			{
				const HubRiverSample sample = HubRiverSampleAt(HubRiverAt(crossing).Arc);
				HubFootbridge bridge;
				bridge.Center = sample.Point;
				bridge.Along = Perpendicular(sample.Tangent);
				bridge.HalfLength = sample.Half * 1.25f + HubBridgeReach;
				bridge.HalfWidth = HubBridgeHalfWidth;
				bridge.Ends = 0.5f * (HubFloor(bridge.Center + bridge.Along * bridge.HalfLength) + HubFloor(bridge.Center - bridge.Along * bridge.HalfLength));
				result.push_back(bridge);
			}
			return result;
		}();
		return bridges;
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

	inline float HubBridgeDeck(const HubFootbridge& bridge, float along)
	{
		const float t = std::clamp(along / bridge.HalfLength, -1.0f, 1.0f);
		return bridge.Ends + 0.06f + HubBridgeArch * (1.0f - t * t);
	}

	inline std::optional<float> HubDeckHeight(Vec2 local)
	{
		if (HubInBox(local, HubDockLowMin(), HubDockLowMax()))
			return HubDockLow;
		if (HubInBox(local, HubDockHighMin(), HubDockHighMax()))
			return HubDockHigh;
		for (const HubFootbridge& bridge : HubFootbridges())
		{
			const Vec2 offset = local - bridge.Center;
			const float u = Dot(offset, bridge.Along);
			const float v = Dot(offset, Perpendicular(bridge.Along));
			if (std::abs(u) > bridge.HalfLength || std::abs(v) > bridge.HalfWidth)
				continue;
			return HubBridgeDeck(bridge, u);
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
			for (const HubFootbridge& bridge : HubFootbridges())
			{
				const Vec2 along = bridge.Along;
				const float top = HubBridgeDeck(bridge, 0.0f);
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
