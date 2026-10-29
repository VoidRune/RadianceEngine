#pragma once
#include "Rendering/Particles.h"
#include <GameShared/Content/Definitions.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Game
{
	class WorldRenderer;

	inline constexpr float SpearLength = 0.6f;
	inline constexpr float SpearHoldHeight = 1.65f;
	inline constexpr float SpearHoldSide = 0.3f;
	inline constexpr float SpearCrownSteps[] = { 0.15f, 0.4f, 0.65f, 0.9f };
	inline constexpr float SpikeLength = 3.5f;
	inline constexpr float SpikeTilt = 0.75f;
	inline constexpr float SpikeCollarDepth = 1.0f;
	inline constexpr float FireballHoldHeight = 1.2f;
	inline constexpr float FireballHoldReach = 0.45f;
	inline constexpr float FireballBallRadius = 0.2f;
	inline constexpr float FireballBlastHeight = 0.75f;
	inline constexpr float FireballSteps[] = { 0.33f, 0.66f };
	inline constexpr float ObsidianLength = 0.8f;
	inline constexpr float ObsidianHoldHeight = 1.15f;
	inline constexpr float ObsidianHoldReach = 0.5f;
	inline constexpr float ObsidianFanRadius = 0.42f;
	inline constexpr float ObsidianFirstForm = 0.14f;
	inline constexpr float ObsidianFormWindow = 0.85f;
	inline constexpr float ObsidianPullReach = 0.75f;
	inline constexpr float ObsidianRecoilSeconds = 0.14f;
	inline constexpr glm::vec3 ObsidianSheen{ 0.42f, 0.16f, 0.85f };
	inline constexpr float HexShieldCellSize = 0.16f;
	inline constexpr float HexShieldSphere = 1.25f;
	inline constexpr float HexShieldBack = 0.25f;
	inline constexpr float HexShieldLift = 1.0f;
	inline constexpr float HexShieldTilt = 0.15f;
	inline constexpr float HexShieldHalfWidth = 1.1f;
	inline constexpr float HexShieldHalfHeight = 0.8f;
	inline constexpr float HexShieldRaiseSeconds = 0.12f;
	inline constexpr float HexShieldLowerSeconds = 0.16f;
	inline constexpr float HexShieldCracks[] = { 0.66f, 0.33f };
	inline constexpr glm::vec3 HexShieldColor{ 0.3f, 0.72f, 1.0f };
	inline constexpr size_t HexShieldImpactSlots = 8;
	inline constexpr size_t HexShieldScarSlots = 6;
	inline constexpr float DiamondFoot = 0.03f;
	inline constexpr float DiamondPavilion = 0.42f;
	inline constexpr float DiamondPavilionRadius = 0.5f;
	inline constexpr float DiamondGirdle = 0.95f;
	inline constexpr float DiamondGirdleRadius = 0.8f;
	inline constexpr float DiamondCrown = 1.6f;
	inline constexpr float DiamondCrownRadius = 0.56f;
	inline constexpr float DiamondApex = 2.75f;
	inline constexpr float DiamondImpactHeight = 1.15f;
	inline constexpr float DiamondFormSeconds = 0.28f;
	inline constexpr float DiamondFadeSeconds = 0.24f;
	inline constexpr float DiamondCracks[] = { 0.66f, 0.33f };
	inline constexpr glm::vec3 DiamondGold{ 1.0f, 0.85f, 0.55f };
	inline constexpr glm::vec3 DiamondViolet{ 0.78f, 0.62f, 1.0f };
	inline constexpr size_t DiamondImpactSlots = 6;
	inline constexpr size_t DiamondScarSlots = 8;

	struct SpellMeshes
	{
		uint32_t Spear = UINT32_MAX;
		std::array<uint32_t, 2> Spike{ UINT32_MAX, UINT32_MAX };
		uint32_t SpikeCollar = UINT32_MAX;
		std::array<uint32_t, 6> Ice{ UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };
		std::array<uint32_t, 3> Obsidian{ UINT32_MAX, UINT32_MAX, UINT32_MAX };
	};

	struct EarthSpikeLook
	{
		glm::vec3 Base{ 0.0f };
		float Facing = 0.0f;
		float Extend = 1.0f;
		float Collar = 1.0f;
		float Tilt = SpikeTilt;
		float Sway = 0.0f;
		float Scale = 1.0f;
		uint32_t Seed = 0;
	};

	struct IceClusterLook
	{
		glm::vec3 Base{ 0.0f };
		float Yaw = 0.0f;
		float Lean = 0.0f;
		float Scale = 1.0f;
		float Sink = 0.0f;
		float Frost = 0.0f;
		float Glow = 0.0f;
		uint32_t Variant = 0;
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

	struct ObsidianLook
	{
		glm::vec3 Center{ 0.0f };
		glm::vec3 Forward{ 1.0f, 0.0f, 0.0f };
		float Roll = 0.0f;
		float Size = 1.0f;
		float Grow = 1.0f;
		float Glint = 0.0f;
		float Form = 1.0f;
		glm::vec3 From{ 0.0f };
		float Ground = 0.0f;
		uint32_t Variant = 0;
		bool Flying = false;
	};

	struct FireballLook
	{
		glm::vec3 Center{ 0.0f };
		glm::vec3 Forward{ 1.0f, 0.0f, 0.0f };
		float Size = 1.0f;
		float Grow = 1.0f;
		float Power = 0.0f;
		float Over = 0.0f;
		float Flash = 0.0f;
		bool Flying = false;
	};

	struct FireballBlastLook
	{
		glm::vec3 Center{ 0.0f };
		glm::vec3 Ground{ 0.0f };
		float Radius = 1.0f;
		float Power = 0.0f;
		float Age = 0.0f;
		float Life = 0.6f;
	};

	struct HexShieldImpact
	{
		glm::vec2 Point{ 0.0f };
		float Age = 1e3f;
		float Strength = 0.0f;
	};

	struct HexShieldLook
	{
		glm::vec3 Base{ 0.0f };
		float Facing = 0.0f;
		float Raise = 0.0f;
		float Guard = 1.0f;
		float Flash = 0.0f;
		std::array<HexShieldImpact, HexShieldImpactSlots> Impacts{};
		std::array<glm::vec2, HexShieldScarSlots> Scars{};
		uint32_t ScarCount = 0;
		uint32_t Seed = 0;
	};

	struct HexShieldCell
	{
		glm::vec2 Point{ 0.0f };
		float Rim = 0.0f;
		uint32_t Seed = 0;
	};

	struct HexShieldShard
	{
		glm::vec3 Position{ 0.0f };
		glm::vec3 Velocity{ 0.0f };
		glm::vec3 Normal{ 0.0f, 0.0f, 1.0f };
		glm::vec3 Axis{ 0.0f, 1.0f, 0.0f };
		float Spin = 0.0f;
		float Size = 0.1f;
		float Floor = 0.0f;
		float Age = 0.0f;
		float Life = 1.0f;
		uint32_t Seed = 0;
	};

	struct DiamondImpact
	{
		glm::vec3 Point{ 0.0f };
		float Age = 1e3f;
		float Strength = 0.0f;
	};

	struct DiamondWardLook
	{
		glm::vec3 Base{ 0.0f };
		float Age = 0.0f;
		float Fade = 1.0f;
		float Ward = 1.0f;
		float Flash = 0.0f;
		float Spin = 0.0f;
		std::array<DiamondImpact, DiamondImpactSlots> Impacts{};
		std::array<glm::vec3, DiamondScarSlots> Scars{};
		uint32_t ScarCount = 0;
		uint32_t Seed = 0;
	};

	struct DiamondShard
	{
		std::array<glm::vec3, 3> Corners{};
		glm::vec3 Center{ 0.0f };
		glm::vec3 Velocity{ 0.0f };
		glm::vec3 Axis{ 0.0f, 1.0f, 0.0f };
		float Spin = 0.0f;
		float Angle = 0.0f;
		float Floor = 0.0f;
		float Age = 0.0f;
		float Life = 1.0f;
		uint32_t Seed = 0;
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
	float SpikeThrust(float age);
	void DrawEarthSpike(WorldRenderer& renderer, const SpellMeshes& meshes, const EarthSpikeLook& look);
	void DrawIceCluster(WorldRenderer& renderer, const SpellMeshes& meshes, const IceClusterLook& look);
	void DrawObsidianShard(WorldRenderer& renderer, const SpellMeshes& meshes, const ObsidianLook& look, float time, uint32_t seed);
	float FireballViolet(float power);
	glm::vec3 FireballColor(float power);
	void DrawPurpleFireball(WorldRenderer& renderer, const FireballLook& look, float time, uint32_t seed);
	void DrawFireballBlast(WorldRenderer& renderer, const FireballBlastLook& look, uint32_t seed);
	std::span<const HexShieldCell> HexShieldCells();
	glm::vec3 HexShieldPoint(const glm::vec3& base, float facing, const glm::vec2& point, glm::vec3* normal = nullptr);
	glm::vec2 HexShieldLocal(const glm::vec3& base, float facing, const glm::vec3& world);
	void DrawHexShield(WorldRenderer& renderer, const HexShieldLook& look, float time);
	void ShatterHexShield(const HexShieldLook& look, std::vector<HexShieldShard>& shards);
	void UpdateHexShards(std::vector<HexShieldShard>& shards, float dt);
	void DrawHexShards(WorldRenderer& renderer, std::span<const HexShieldShard> shards);
	glm::vec3 DiamondPoint(const glm::vec3& base, const glm::vec3& toward, float height, glm::vec3* normal = nullptr);
	glm::vec3 DiamondLocal(const DiamondWardLook& look, const glm::vec3& world);
	void DrawDiamondWard(WorldRenderer& renderer, const DiamondWardLook& look, float time);
	void ShatterDiamondWard(const DiamondWardLook& look, std::vector<DiamondShard>& shards);
	void UpdateDiamondShards(std::vector<DiamondShard>& shards, float dt);
	void DrawDiamondShards(WorldRenderer& renderer, std::span<const DiamondShard> shards);

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

	void DrawShockwave(WorldRenderer& renderer, const glm::vec3& ground, float radius, float width, float strength, float facing = 0.0f, float halfArc = 3.1416f);
	void DrawHeatHaze(WorldRenderer& renderer, const glm::vec3& center, float radius, float strength, float frequency, float rise, uint32_t seed);
	void DrawImplosion(WorldRenderer& renderer, const glm::vec3& center, float radius, float strength, float phase, uint32_t pulses = 3);
	void DrawGravityLens(WorldRenderer& renderer, const glm::vec3& center, float horizon, float einstein, const glm::vec3& ring, float strength = 1.0f);
}
