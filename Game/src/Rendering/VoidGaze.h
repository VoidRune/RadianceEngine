#pragma once
#include "Rendering/Trails.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace Game
{
	class WorldRenderer;

	inline constexpr float VoidCoreHeight = 1.3f;
	inline constexpr float VoidCoreRadius = 0.82f;
	inline constexpr float VoidSeamStart = 0.13f;
	inline constexpr float VoidOpenStart = 0.16f;
	inline constexpr float VoidOpenEnd = 0.28f;
	inline constexpr float VoidCloseStart = 0.06f;
	inline constexpr float VoidCloseEnd = 0.3f;
	inline constexpr float VoidSlitEnd = 0.4f;
	inline constexpr float VoidCollapseStart = 0.4f;
	inline constexpr float VoidCollapseEnd = 0.6f;
	inline constexpr float VoidDetonate = 0.68f;
	inline constexpr float VoidBlastSeconds = 0.65f;
	inline constexpr float VoidFieldBox = 1.04f;
	inline constexpr glm::vec3 VoidViolet{ 0.45f, 0.12f, 1.2f };
	inline constexpr glm::vec3 VoidLavender{ 0.72f, 0.6f, 1.45f };

	struct VoidGazeLook
	{
		glm::vec3 Ground{ 0.0f };
		glm::vec3 Eye{ 0.0f };
		float Radius = 4.0f;
		float Scale = 1.0f;
		float Age = 0.0f;
		float PullAge = -1.0f;
		float Pull = -1.0f;
		float Tempo = 1.0f;
		float Fade = 1.0f;
		glm::vec3 Gaze{ 0.0f };
		float Gazing = 0.0f;
		uint32_t Seed = 0;
		float Time = 0.0f;
	};

	glm::vec3 VoidCore(const VoidGazeLook& look);
	float VoidCoreSize(float scale);
	float VoidLifetime(float tempo);
	TrailStyle VoidWispTrail();
	void DrawVoidGaze(WorldRenderer& renderer, const VoidGazeLook& look, float lightGain);
}
