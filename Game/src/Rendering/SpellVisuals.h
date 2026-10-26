#pragma once
#include "Rendering/Particles.h"
#include <GameShared/Content/Definitions.h>
#include <glm/glm.hpp>
#include <cstdint>

namespace Game
{
	class WorldRenderer;

	inline constexpr float SpearLength = 0.6f;
	inline constexpr float SpearHoldHeight = 1.65f;
	inline constexpr float SpearHoldSide = 0.3f;
	inline constexpr float SpearCrownSteps[] = { 0.15f, 0.4f, 0.65f, 0.9f };

	struct SpellMeshes
	{
		uint32_t Spear = UINT32_MAX;
	};

	struct IceSpearLook
	{
		glm::vec3 Center{ 0.0f };
		float Facing = 0.0f;
		float Pitch = 0.0f;
		float Roll = 0.0f;
		float Size = 1.0f;
		float Grow = 1.0f;
		float Power = 0.0f;
		float Over = 0.0f;
		float Flash = 0.0f;
		float Ground = 0.0f;
		bool Flying = false;
	};

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

	SpellMeshes RegisterSpellMeshes(WorldRenderer& renderer);
	void DrawIceSpear(WorldRenderer& renderer, const SpellMeshes& meshes, const IceSpearLook& look, float time, uint32_t seed);

	void DrawRuneCircle(WorldRenderer& renderer, const RuneCircle& circle);
	void DrawGroundRing(WorldRenderer& renderer, const glm::vec3& center, float radius, const glm::vec3& color, float intensity, float thickness, float dashes = 0.0f, float spin = 0.0f, uint32_t pattern = 0);
	void DrawGroundGlow(WorldRenderer& renderer, const glm::vec3& center, float radius, const glm::vec3& color, float intensity);
	void DrawSectorTelegraph(WorldRenderer& renderer, const glm::vec3& apex, float facing, float reach, float halfArc, const glm::vec3& color, float intensity, float progress, float appear = 1.0f);
	void DrawLaneTelegraph(WorldRenderer& renderer, const glm::vec3& start, float facing, float length, float width, const glm::vec3& color, float intensity, float progress, float appear = 1.0f);

	void DrawGlow(WorldRenderer& renderer, const glm::vec3& position, float size, const glm::vec3& color, float softness = 0.2f);
	void DrawStar(WorldRenderer& renderer, const glm::vec3& position, float size, const glm::vec3& color, float rotation = 0.0f);
	void DrawReticle(WorldRenderer& renderer, const glm::vec3& position, float radius, const glm::vec3& color);
	void DrawFloatingRing(WorldRenderer& renderer, const glm::vec3& center, float radius, const glm::vec3& color, float thickness);
	void DrawBeam(WorldRenderer& renderer, const glm::vec3& base, float height, float width, const glm::vec3& color, float emission = 1.0f);
	void DrawStreak(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, float width, const glm::vec3& color);
	void DrawSlash(WorldRenderer& renderer, const glm::vec3& center, float facing, float reach, float halfArc, float sweep, const glm::vec3& color, bool mirrored, float inner = 0.45f);
	void DrawProjectileVisual(WorldRenderer& renderer, AbilityId ability, const glm::vec3& center, float facing, const glm::vec3& color, float time, uint32_t seed, const glm::vec3& owner = glm::vec3(0.0f));
	void DrawChain(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, float sag, float glow);
	void DrawBolt(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, uint32_t seed, float jitter, float width, const glm::vec3& color, float emission = 0.25f);
}
