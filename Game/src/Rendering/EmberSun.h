#pragma once
#include <glm/glm.hpp>
#include <array>

namespace Game
{
	class SunRenderer;

	inline constexpr int EmberDrifts = 6;
	inline constexpr int EmberFallSlots = 4;

	struct EmberSunPulse
	{
		float Flash = 0.0f;
		float Flicker = 1.0f;
		float Swell = 0.0f;
		float BeatAge = -1.0f;
		float Radius = 1.0f;
		float Ward = 2.0f;
		glm::vec3 Impact{ 0.0f };
		float ImpactAge = -1.0f;
		float ImpactStrength = 0.0f;
		std::array<float, EmberDrifts> Drain{};
	};

	struct EmberSunLook
	{
		glm::vec3 Center{ 0.0f };
		float Radius = 1.0f;
		float Ceiling = 0.0f;
		float Time = 0.0f;
		std::array<glm::vec3, EmberDrifts> Sources{};
		int SourceCount = 0;
		std::array<glm::vec4, EmberFallSlots> Falls{};
		int FallCount = 0;
	};

	glm::mat4 EmberDriftTransform(const glm::vec3& heart, int index, float time);

	struct EmberEruptionPlan
	{
		float Start = 0.0f;
		glm::vec3 Site{ 0.0f, 1.0f, 0.0f };
		float Power = 1.0f;
	};

	inline constexpr int EmberEruptionLanes = 2;

	EmberEruptionPlan PlanEmberEruption(float time, int lane, int cycleOffset = 0);
	EmberSunPulse DrawEmberSun(SunRenderer& sun, const EmberSunLook& look);
}
