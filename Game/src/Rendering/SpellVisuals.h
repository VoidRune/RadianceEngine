#pragma once
#include "Rendering/Particles.h"
#include <GameShared/Content/Definitions.h>
#include <glm/glm.hpp>
#include <cstdint>

namespace Game
{
	class WorldRenderer;

	struct RuneCircle
	{
		glm::vec3 Center{ 0.0f };
		float Radius = 1.0f;
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float Spin = 0.35f;
		float Progress = -1.0f;
		float Appear = 1.0f;
		float Inner = 0.0f;
		uint32_t Seed = 0;
	};

	float RingExtent(float radius, float thickness);

	void DrawRuneCircle(WorldRenderer& renderer, const RuneCircle& circle);
	void DrawGroundRing(WorldRenderer& renderer, const glm::vec3& center, float radius, const glm::vec3& color, float intensity, float thickness, float dashes = 0.0f, float spin = 0.0f);
	void DrawGroundGlow(WorldRenderer& renderer, const glm::vec3& center, float radius, const glm::vec3& color, float intensity);
	void DrawSectorTelegraph(WorldRenderer& renderer, const glm::vec3& apex, float facing, float reach, float halfArc, const glm::vec3& color, float intensity, float progress, float appear = 1.0f);
	void DrawLaneTelegraph(WorldRenderer& renderer, const glm::vec3& start, float facing, float length, float width, const glm::vec3& color, float intensity, float progress, float appear = 1.0f);

	void DrawGlow(WorldRenderer& renderer, const glm::vec3& position, float size, const glm::vec3& color, float softness = 0.2f);
	void DrawStar(WorldRenderer& renderer, const glm::vec3& position, float size, const glm::vec3& color, float rotation = 0.0f);
	void DrawReticle(WorldRenderer& renderer, const glm::vec3& position, float radius, const glm::vec3& color);
	void DrawFloatingRing(WorldRenderer& renderer, const glm::vec3& center, float radius, const glm::vec3& color, float thickness);
	void DrawBeam(WorldRenderer& renderer, const glm::vec3& base, float height, float width, const glm::vec3& color);
	void DrawStreak(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, float width, const glm::vec3& color);
	void DrawSlash(WorldRenderer& renderer, const glm::vec3& center, float facing, float reach, float halfArc, float sweep, const glm::vec3& color, bool mirrored, float inner = 0.45f);
	void DrawProjectileVisual(WorldRenderer& renderer, AbilityId ability, const glm::vec3& center, float facing, const glm::vec3& color, float time, uint32_t seed);
}
