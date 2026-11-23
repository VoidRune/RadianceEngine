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
#include <vector>

namespace Game
{
	inline constexpr int32_t HubWidth = 132;
	inline constexpr int32_t HubHeight = 70;

	inline constexpr Vec2 HubOutline[] = {
		{ -4.5f, -27.5f }, { 4.5f, -27.5f }, { 5.5f, -21.5f }, { 8.5f, -20.5f }, { 10.5f, -22.5f }, { 14.0f, -21.5f }, { 16.0f, -18.5f }, { 19.5f, -19.5f },
		{ 23.5f, -19.0f }, { 31.0f, -19.5f }, { 40.5f, -18.5f }, { 41.5f, -13.0f }, { 40.0f, -9.0f }, { 35.5f, -7.5f }, { 31.5f, -4.5f }, { 33.0f, 0.0f },
		{ 36.5f, 2.5f }, { 40.5f, 3.0f }, { 45.0f, 6.0f }, { 46.0f, 12.0f }, { 44.5f, 19.0f }, { 45.5f, 24.0f }, { 41.0f, 27.5f }, { 33.0f, 28.0f },
		{ 26.0f, 27.0f }, { 19.0f, 27.5f }, { 14.5f, 24.5f }, { 13.0f, 19.5f }, { 11.5f, 22.0f }, { 10.5f, 26.5f }, { 5.0f, 28.0f }, { -4.0f, 28.5f },
		{ -10.0f, 26.5f }, { -11.5f, 22.0f }, { -13.5f, 19.5f }, { -16.0f, 22.5f }, { -20.0f, 25.0f }, { -26.0f, 24.5f }, { -31.0f, 21.0f }, { -32.0f, 16.0f },
		{ -36.5f, 13.0f }, { -41.0f, 13.5f }, { -45.5f, 9.0f }, { -47.5f, 3.0f }, { -46.5f, -4.0f }, { -47.0f, -9.0f }, { -43.0f, -13.0f }, { -37.0f, -13.5f },
		{ -33.0f, -16.5f }, { -28.5f, -14.0f }, { -24.5f, -16.0f }, { -21.0f, -17.0f }, { -22.5f, -21.0f }, { -20.0f, -24.0f }, { -16.0f, -22.0f }, { -14.5f, -18.5f },
		{ -11.0f, -19.5f }, { -7.0f, -21.5f }, { -4.5f, -21.5f },
	};
	inline constexpr float HubJagAmplitude = 0.9f;
	inline constexpr float HubJagScale = 0.42f;

	inline constexpr float HubNorthLevel = 1.0f;
	inline constexpr float HubAcademyLevel = 0.75f;
	inline constexpr float HubWestLevel = 0.75f;
	inline constexpr float HubRiftLevel = 1.5f;
	inline constexpr float HubKnollLevel = 0.5f;

	inline constexpr Vec2 HubNorthPoints[] = { { -4.5f, 9.5f }, { 4.5f, 9.5f }, { 7.5f, 12.5f }, { 10.5f, 14.5f }, { 12.0f, 19.0f }, { 11.5f, 31.0f }, { -11.5f, 31.0f }, { -12.0f, 19.0f }, { -10.5f, 14.5f }, { -7.5f, 12.5f } };
	inline constexpr float HubNorthBanks[] = { 0.0f, 0.0f, 0.0f, 0.3f, 1.0f, 1.0f, 1.0f, 1.0f, 0.3f, 0.0f };
	inline constexpr Vec2 HubAcademyPoints[] = { { 23.5f, 2.5f }, { 32.5f, 2.5f }, { 35.5f, 5.5f }, { 40.0f, 6.5f }, { 44.0f, 9.0f }, { 50.0f, 9.0f }, { 50.0f, 31.0f }, { 15.0f, 31.0f }, { 14.5f, 15.0f }, { 16.5f, 9.0f }, { 20.5f, 5.5f } };
	inline constexpr float HubAcademyBanks[] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.4f, 0.0f };
	inline constexpr Vec2 HubWestPoints[] = { { -23.5f, 2.5f }, { -23.5f, -2.5f }, { -26.5f, -4.5f }, { -28.0f, -8.5f }, { -31.0f, -11.5f }, { -34.0f, -16.0f }, { -52.0f, -16.0f }, { -52.0f, 12.5f }, { -34.0f, 12.5f }, { -29.5f, 9.5f }, { -26.5f, 5.0f } };
	inline constexpr float HubWestBanks[] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.4f };
	inline constexpr Vec2 HubRiftPoints[] = { { -32.5f, 2.5f }, { -32.5f, -2.5f }, { -35.5f, -4.5f }, { -37.5f, -7.5f }, { -40.0f, -9.5f }, { -52.0f, -9.5f }, { -52.0f, 9.5f }, { -40.0f, 9.5f }, { -37.5f, 7.5f }, { -35.5f, 4.5f } };
	inline constexpr float HubRiftBanks[] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };

	struct HubTerrace
	{
		const Vec2* Points = nullptr;
		const float* Banks = nullptr;
		size_t Count = 0;
		float Rise = 0.0f;
	};

	inline constexpr HubTerrace HubTerraces[] = {
		{ HubNorthPoints, HubNorthBanks, std::size(HubNorthPoints), HubNorthLevel },
		{ HubAcademyPoints, HubAcademyBanks, std::size(HubAcademyPoints), HubAcademyLevel },
		{ HubWestPoints, HubWestBanks, std::size(HubWestPoints), HubWestLevel },
		{ HubRiftPoints, HubRiftBanks, std::size(HubRiftPoints), HubRiftLevel - HubWestLevel },
	};
	inline constexpr float HubCliffHalf = 0.14f;
	inline constexpr float HubBankSlope = 0.17f;
	inline constexpr float HubTerraceWarp = 0.75f;

	inline constexpr Vec2 HubKnollCenter{ -24.0f, 16.0f };
	inline constexpr float HubKnollRadii[] = { 3.6f, 7.6f };

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

	inline constexpr HubStair HubStairs[] = {
		{ -3, 10, 7, 4, Direction::North, 0.0f, HubNorthLevel },
		{ -26, -2, 3, 5, Direction::West, 0.0f, HubWestLevel },
		{ -35, -2, 3, 5, Direction::West, HubWestLevel, HubRiftLevel },
		{ 25, 3, 7, 3, Direction::North, 0.0f, HubAcademyLevel },
	};
	inline constexpr float HubStepRise = 0.14f;

	struct HubPath
	{
		const Vec2* Points = nullptr;
		size_t Count = 0;
		float Half = 1.2f;
		TerrainId Stone = TerrainId::Flagstone;
	};

	inline constexpr Vec2 HubWestPath[] = { { -9.0f, 0.3f }, { -14.0f, 1.3f }, { -19.0f, -0.7f }, { -23.6f, 0.0f } };
	inline constexpr Vec2 HubRisePath[] = { { -26.4f, 0.0f }, { -29.5f, 0.9f }, { -32.6f, 0.0f } };
	inline constexpr Vec2 HubRiftPath[] = { { -35.4f, 0.0f }, { -38.5f, -0.4f } };
	inline constexpr Vec2 HubSocialPath[] = { { -6.8f, 6.6f }, { -9.4f, 8.8f }, { -11.4f, 9.6f } };
	inline constexpr Vec2 HubKnollPath[] = { { -16.4f, 12.2f }, { -18.6f, 13.7f } };
	inline constexpr Vec2 HubEastPath[] = { { 9.0f, 0.3f }, { 14.0f, 1.7f }, { 19.0f, 0.5f }, { 24.0f, 1.5f }, { 28.0f, 2.6f } };
	inline constexpr Vec2 HubDockPath[] = { { 6.6f, -6.6f }, { 11.0f, -10.0f }, { 16.0f, -10.4f }, { 21.0f, -12.4f }, { 25.5f, -13.2f } };
	inline constexpr Vec2 HubAvenuePath[] = { { 0.0f, -9.0f }, { 0.4f, -14.0f }, { -0.4f, -20.0f }, { 0.0f, -27.5f } };
	inline constexpr Vec2 HubBrinkPath[] = { { -6.6f, -6.6f }, { -9.0f, -10.6f }, { -13.0f, -13.4f } };

	inline constexpr HubPath HubPaths[] = {
		{ HubWestPath, std::size(HubWestPath), 1.3f },
		{ HubRisePath, std::size(HubRisePath), 1.3f },
		{ HubRiftPath, std::size(HubRiftPath), 1.3f },
		{ HubSocialPath, std::size(HubSocialPath), 1.0f },
		{ HubKnollPath, std::size(HubKnollPath), 0.8f },
		{ HubEastPath, std::size(HubEastPath), 1.3f },
		{ HubDockPath, std::size(HubDockPath), 1.2f, TerrainId::Cobblestone },
		{ HubAvenuePath, std::size(HubAvenuePath), 2.1f, TerrainId::Cobblestone },
		{ HubBrinkPath, std::size(HubBrinkPath), 1.0f },
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
		{ { 0.0f, 17.2f }, 6.2f },
		{ { -41.0f, 0.0f }, 4.3f },
		{ { -13.5f, 10.5f }, 3.9f },
		{ { -14.5f, -14.5f }, 2.0f },
		{ { 64.0f, 14.4f }, 3.4f },
	};
	inline constexpr Vec2 HubForecourt[] = { { 23.5f, 5.6f }, { 28.0f, 5.3f }, { 32.5f, 5.6f }, { 35.5f, 6.9f }, { 36.3f, 9.0f }, { 35.0f, 10.8f }, { 30.0f, 10.3f }, { 25.0f, 10.9f }, { 20.5f, 10.2f }, { 19.8f, 8.4f }, { 21.0f, 6.8f } };
	inline constexpr Vec2 HubQuay[] = { { 24.5f, -10.6f }, { 28.0f, -9.9f }, { 33.0f, -10.7f }, { 37.5f, -10.1f }, { 39.5f, -12.0f }, { 39.8f, -14.5f }, { 38.2f, -16.8f }, { 33.5f, -17.3f }, { 29.0f, -16.5f }, { 25.5f, -17.4f }, { 24.0f, -14.5f } };

	inline constexpr Vec2 HubGateSpot{ 0.0f, 20.0f };
	inline constexpr Vec2 HubRiftSpot{ -41.0f, 0.0f };
	inline constexpr Vec2 HubChessSpot{ -13.5f, 10.5f };
	inline constexpr Vec2 HubMapleSpot = HubKnollCenter;
	inline constexpr Vec2 HubBoardSpot{ -13.0f, -15.8f };
	inline constexpr Vec2 HubWishSpot{ 39.2f, -13.4f };
	inline constexpr Vec2 HubSpawnSpot{ 0.0f, -16.0f };
	inline constexpr Vec2 HubDoorSpot{ 28.0f, 11.7f };
	inline constexpr Vec2 HubDoorReturn{ 28.0f, 9.4f };
	inline constexpr Vec2 HubTrainingSpot{ 64.0f, 14.0f };
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
		{ HubGateSpot, 8.5f },
		{ HubChessSpot, 4.6f },
		{ HubKnollCenter, 4.2f },
		{ HubRiftSpot, 5.2f },
		{ HubTrainingSpot, 7.5f },
		{ { 28.0f, 16.0f }, 8.5f },
		{ { 19.0f, 17.5f }, 4.6f },
		{ { 38.5f, 17.5f }, 4.6f },
		{ { 32.0f, -14.0f }, 7.0f },
		{ HubWishSpot, 2.0f },
		{ HubBoardSpot, 1.6f },
	};
	inline constexpr float HubFlatBlend = 2.5f;
	inline constexpr float HubReliefAmplitude = 0.32f;
	inline constexpr float HubBrowDrop = 0.55f;
	inline constexpr float HubBrowReach = 1.4f;
	inline constexpr float HubSurfaceReach = -0.15f;

	enum class HubBlockKind : uint8_t
	{
		Building,
		Tower,
		Roof,
		Door,
		Pier,
		Envelope,
		Gondola,
		Bridge,
		Scaffold,
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
	};

	inline constexpr HubBlock HubBlocks[] = {
		{ HubBlockKind::Building, MeshKind::Box, { 28.0f, 17.5f }, { 7.5f, 5.0f }, 11.0f },
		{ HubBlockKind::Building, MeshKind::Box, { 19.0f, 17.5f }, { 2.8f, 3.6f }, 7.0f },
		{ HubBlockKind::Building, MeshKind::Box, { 38.5f, 17.5f }, { 3.0f, 3.6f }, 7.0f },
		{ HubBlockKind::Tower, MeshKind::Cylinder, { 28.0f, 21.0f }, { 3.4f, 3.4f }, 26.0f },
		{ HubBlockKind::Roof, MeshKind::Cone, { 28.0f, 21.0f }, { 3.9f, 3.9f }, 6.0f, 26.0f, 0.0f, false },
		{ HubBlockKind::Door, MeshKind::Box, { 28.0f, 12.45f }, { 1.7f, 0.12f }, 6.5f, 0.0f, 0.0f, false },
		{ HubBlockKind::Pier, MeshKind::Box, { 46.5f, -13.0f }, { 5.5f, 1.4f }, 0.35f, -0.3f, 0.0f, false },
		{ HubBlockKind::Pier, MeshKind::Box, { 31.0f, -24.5f }, { 1.4f, 5.5f }, 0.35f, -0.3f, 0.0f, false },
		{ HubBlockKind::Envelope, MeshKind::Sphere, { 48.5f, -6.5f }, { 7.5f, 2.6f }, 5.2f, 4.5f, 0.0f, false },
		{ HubBlockKind::Gondola, MeshKind::Box, { 48.5f, -6.5f }, { 2.4f, 0.9f }, 1.2f, 3.0f, 0.0f, false },
		{ HubBlockKind::Envelope, MeshKind::Sphere, { 36.5f, -32.0f }, { 2.4f, 6.5f }, 4.8f, 3.5f, 0.0f, false },
		{ HubBlockKind::Gondola, MeshKind::Box, { 36.5f, -32.0f }, { 0.9f, 2.2f }, 1.1f, 2.1f, 0.0f, false },
		{ HubBlockKind::Bridge, MeshKind::Box, { 0.0f, -29.6f }, { 2.3f, 1.2f }, 0.6f, -0.5f, 0.0f, false },
		{ HubBlockKind::Bridge, MeshKind::Box, { 0.4f, -32.4f }, { 2.0f, 1.0f }, 0.6f, -1.1f, 0.15f, false },
		{ HubBlockKind::Bridge, MeshKind::Box, { -0.5f, -34.9f }, { 1.6f, 0.9f }, 0.6f, -1.9f, -0.2f, false },
		{ HubBlockKind::Scaffold, MeshKind::Box, { -37.5f, 2.8f }, { 0.6f, 0.6f }, 4.5f },
		{ HubBlockKind::Scaffold, MeshKind::Box, { -37.5f, -2.8f }, { 0.6f, 0.6f }, 4.5f },
		{ HubBlockKind::Scaffold, MeshKind::Box, { -29.5f, 6.0f }, { 1.6f, 1.6f }, 0.15f, 3.0f, 0.0f, false },
		{ HubBlockKind::Scaffold, MeshKind::Box, { -29.5f, -6.0f }, { 1.6f, 1.6f }, 0.15f, 3.0f, 0.0f, false },
	};
	inline constexpr Vec2 HubScaffoldPosts[] = { { -29.5f, 6.0f }, { -29.5f, -6.0f } };
	inline constexpr float HubScaffoldHalf = 1.45f;
	inline constexpr float HubScaffoldPost = 0.08f;
	inline constexpr float HubScaffoldHeight = 3.0f;

	struct HubSign
	{
		Vec2 Position;
		float Yaw = 0.0f;
		const char* Title = "";
	};

	inline constexpr HubSign HubSigns[] = {
		{ { 23.5f, 8.8f }, 0.0f, "Academy" },
		{ { 27.5f, -11.2f }, 0.0f, "Sky dock" },
		{ { -28.2f, 3.0f }, 0.0f, "Shardfall wing" },
		{ { 3.4f, -25.0f }, 0.0f, "Broken bridge" },
		{ { -17.8f, 7.4f }, 0.0f, "Social corner" },
	};
	inline constexpr float HubSignReach = 6.0f;

	inline constexpr Vec2 HubCrystals[] = {
		{ -20.5f, 6.5f }, { -21.5f, -7.5f }, { -24.0f, -12.0f }, { -28.0f, 8.6f }, { -29.5f, -9.0f }, { -32.5f, 10.5f }, { -31.0f, -3.4f }, { -34.0f, 5.0f },
		{ -36.5f, -11.0f }, { -38.5f, 6.5f }, { -39.0f, -7.0f }, { -42.0f, 8.0f }, { -43.5f, -6.0f }, { -44.5f, 3.5f }, { -45.0f, -3.0f }, { -44.0f, -10.0f },
	};
	inline constexpr Vec2 HubSpires[] = { { -40.5f, 7.0f }, { -42.5f, -6.8f }, { -45.0f, 0.5f } };

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

	inline float HubKnollHeight(Vec2 local)
	{
		const Vec2 offset = local - HubKnollCenter;
		const float angle = std::atan2(offset.y, offset.x);
		const float r = Length(offset) / (1.0f + 0.08f * (std::sin(3.0f * angle + 0.7f) + 0.6f * std::sin(5.0f * angle + 2.1f)));
		return HubKnollLevel * (1.0f - HubSmooth(HubKnollRadii[0], HubKnollRadii[1], r));
	}

	inline float HubTerraceHeight(Vec2 local)
	{
		const float warp = HubWarp(local);
		float height = HubKnollHeight(local);
		for (const HubTerrace& terrace : HubTerraces)
			height += terrace.Rise * HubTerraceProfile(terrace, local, warp);
		return height;
	}

	inline float HubFlatness(Vec2 local)
	{
		float flat = 1.0f;
		for (const HubFlat& zone : HubFlats)
			flat = std::min(flat, HubSmooth(0.0f, HubFlatBlend, Length(local - zone.Center) - zone.Radius));
		flat = std::min(flat, HubSmooth(0.3f, 1.8f, HubNearestStair(local)));
		return flat;
	}

	inline float HubRelief(Vec2 local)
	{
		const float rolling = 0.75f * HubSigned(local * 0.085f + Vec2(5.3f, -2.9f)) + 0.35f * HubSigned(local * 0.26f + Vec2(-8.1f, 4.4f)) + 0.12f * HubSigned(local * 0.8f);
		const float hummocks = 0.9f * HubSmooth(0.58f, 0.86f, HubNoise(local * 0.19f + Vec2(11.0f, 23.0f)));
		return HubReliefAmplitude * (rolling + hummocks);
	}

	inline float HubFloor(Vec2 local)
	{
		return HubTerraceHeight(local) + HubRelief(local) * HubFlatness(local);
	}

	struct HubTrack
	{
		std::vector<Vec2> Points;
		std::vector<float> Arc;
		float Half = 1.0f;
		TerrainId Stone = TerrainId::Flagstone;
	};

	inline Vec2 HubCatmull(Vec2 a, Vec2 b, Vec2 c, Vec2 d, float t)
	{
		const float t2 = t * t;
		const float t3 = t2 * t;
		return (b * 2.0f + (c - a) * t + (a * 2.0f - b * 5.0f + c * 4.0f - d) * t2 + (b * 3.0f - a - c * 3.0f + d) * t3) * 0.5f;
	}

	inline const std::vector<HubTrack>& HubTracks()
	{
		static const std::vector<HubTrack> tracks = []
		{
			std::vector<HubTrack> result;
			for (const HubPath& path : HubPaths)
			{
				HubTrack track;
				track.Half = path.Half;
				track.Stone = path.Stone;
				for (size_t i = 0; i + 1 < path.Count; i++)
				{
					const Vec2 a = path.Points[i > 0 ? i - 1 : i];
					const Vec2 b = path.Points[i];
					const Vec2 c = path.Points[i + 1];
					const Vec2 d = path.Points[i + 2 < path.Count ? i + 2 : i + 1];
					for (int32_t s = 0; s < HubPathSamples; s++)
						track.Points.push_back(HubCatmull(a, b, c, d, float(s) / float(HubPathSamples)));
				}
				track.Points.push_back(path.Points[path.Count - 1]);
				float arc = 0.0f;
				for (size_t i = 0; i < track.Points.size(); i++)
				{
					if (i > 0)
						arc += Length(track.Points[i] - track.Points[i - 1]);
					track.Arc.push_back(arc);
				}
				result.push_back(std::move(track));
			}
			return result;
		}();
		return tracks;
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
			float nearest = 1e9f;
			float arc = 0.0f;
			for (size_t i = 0; i + 1 < track.Points.size(); i++)
			{
				const Vec2 span = track.Points[i + 1] - track.Points[i];
				const float t = std::clamp(Dot(local - track.Points[i], span) / std::max(LengthSquared(span), 1e-6f), 0.0f, 1.0f);
				const float distance = Length(local - (track.Points[i] + span * t));
				if (distance < nearest)
				{
					nearest = distance;
					arc = track.Arc[i] + (track.Arc[i + 1] - track.Arc[i]) * t;
				}
			}
			const float half = track.Half * (1.0f + 0.22f * HubSigned(Vec2(arc * 0.31f, track.Half * 7.0f)));
			offer(half - nearest, track.Stone);
		}
		for (const HubPlaza& plaza : HubPlazas)
			offer(plaza.Radius + 0.5f * HubSigned((local - plaza.Center) * 0.5f + plaza.Center) - Length(local - plaza.Center), plaza.Stone);
		const auto polygon = [&](const Vec2* points, size_t count, TerrainId stone)
		{
			offer(HubPolygonEdge(points, nullptr, count, local).Distance + 0.55f * HubSigned(local * 0.3f) + 0.2f * HubSigned(local * 0.9f + Vec2(4.0f, 2.0f)), stone);
		};
		polygon(HubForecourt, std::size(HubForecourt), TerrainId::Flagstone);
		polygon(HubQuay, std::size(HubQuay), TerrainId::Cobblestone);
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

	inline std::optional<float> HubDeckBase(Vec2 position)
	{
		const Vec2 local = position - EmberCenter;
		if (Length(local) <= HubDeckRadius + 0.01f)
			return std::nullopt;
		return HubNorthLevel;
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
		return std::nullopt;
	}
}
