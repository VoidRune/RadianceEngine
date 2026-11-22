#pragma once
#include "GameShared/Core/Math.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace Game
{
	inline constexpr float PortalTerraceHeight = 1.0f;
	inline constexpr int32_t PortalTerraceSteps = 6;
	inline constexpr float PortalTerraceRise = PortalTerraceHeight / float(PortalTerraceSteps);
	inline constexpr float PortalTerraceTread = 0.56f;
	inline constexpr float PortalTerraceStairIn = 2.5f;
	inline constexpr float PortalTerraceStairOut = PortalTerraceStairIn + float(PortalTerraceSteps - 1) * PortalTerraceTread;
	inline constexpr float PortalTerraceNose = 0.035f;
	inline constexpr float PortalTerraceTop = 3.8f;
	inline constexpr float PortalTerraceSector = 40.0f * Pi / 180.0f;
	inline constexpr float PortalTerraceReach = PortalTerraceStairOut + 0.8f;
	inline constexpr float PortalTerraceRamp[] = { PortalTerraceStairIn - 0.7f, PortalTerraceStairOut + 0.7f };

	inline float PortalTerraceAngle(Vec2 local)
	{
		return std::atan2(std::abs(local.x), -local.y);
	}

	inline std::optional<float> PortalTerraceGround(Vec2 local)
	{
		const float r = Length(local);
		if (PortalTerraceAngle(local) < PortalTerraceSector)
		{
			if (r < PortalTerraceStairIn)
				return PortalTerraceHeight;
			const int32_t step = int32_t(std::floor((r - PortalTerraceStairIn - PortalTerraceNose) / PortalTerraceTread)) + 1;
			if (step >= PortalTerraceSteps)
				return std::nullopt;
			return PortalTerraceHeight - float(step) * PortalTerraceRise;
		}
		if (r < PortalTerraceTop)
			return PortalTerraceHeight;
		return std::nullopt;
	}

	inline int32_t PortalTerraceLevel(Vec2 local, float unit)
	{
		const float r = Length(local);
		const int32_t top = int32_t(std::lround(PortalTerraceHeight / unit));
		if (PortalTerraceAngle(local) < PortalTerraceSector + 0.05f)
		{
			if (r > PortalTerraceRamp[1])
				return -1;
			const float t = std::clamp((PortalTerraceRamp[1] - r) / (PortalTerraceRamp[1] - PortalTerraceRamp[0]), 0.0f, 1.0f);
			return int32_t(std::lround(t * float(top)));
		}
		return r < PortalTerraceTop ? top : -1;
	}
}
