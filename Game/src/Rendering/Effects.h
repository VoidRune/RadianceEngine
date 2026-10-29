#pragma once
#include "Rendering/Particles.h"
#include <GameShared/Content/Definitions.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Game
{
	class WorldRenderer;
	struct SpellMeshes;

	enum class ParticleKind : uint8_t
	{
		Glow,
		Fire,
		Ember,
		Smoke,
		Spark,
		Frost,
		IceShard,
		Lightning,
		Arcane,
		Poison,
		Heal,
		Dust,
		Bone,
		Shockwave,
		Flash,
		Gold,
		Snow,
		Wind,
		Rain,
		Mote,
		VioletFire,
		VioletEmber,
		Streak,
		HexShard,
		DiamondShard,
		Obsidian,
		Count,
	};

	enum class Fx : uint8_t
	{
		FireTrail,
		FrostTrail,
		BoltTrail,
		IceTrail,
		ArrowTrail,
		PoisonTrail,
		ArcaneTrail,
		ShardTrail,
		Explosion,
		MeteorImpact,
		FrostNova,
		FrostBurst,
		LightningBurst,
		ArrowImpact,
		PoisonSplash,
		ArcaneBurst,
		FireBurst,
		HitPhysical,
		HitFire,
		HitCold,
		HitLightning,
		HitPoison,
		Critical,
		Death,
		Dodge,
		Blink,
		Heal,
		Slam,
		GroundSlam,
		Cyclone,
		Debris,
		CrystalBurst,
		LootSparkle,
		GatewaySwirl,
		CastFire,
		CastFrost,
		CastLightning,
		CastArcane,
		CastNature,
		MeteorTrail,
		Campfire,
		LanternFlame,
		CandleFlame,
		Gust,
		Starstep,
		BalloonPop,
		CoinToss,
		BombBurst,
		Snowfall,
		Crumble,
		WindStreak,
		Rain,
		SpearAura,
		SpearReady,
		SpearRelease,
		SpearCrack,
		SpearShatter,
		SpearTrail,
		SpikeTremor,
		SpikeEruption,
		SpikeCrumble,
		IceCrumble,
		FireballKindle,
		FireballAura,
		FireballReady,
		FireballRing,
		FireballRelease,
		FireballSputter,
		FireballTrail,
		FireballBlast,
		ShieldRaise,
		ShieldImpact,
		ShieldCrack,
		ShieldShatter,
		WardForm,
		WardDeflect,
		WardCrack,
		WardShatter,
		ObsidianForm,
		ObsidianRelease,
		ObsidianTrail,
		ObsidianImpact,
		ObsidianCrumble,
		Count,
	};

	std::vector<ParticleMaterial> MakeParticleMaterials();
	std::optional<Fx> TrailFor(AbilityId ability);
	std::optional<Fx> ProjectileImpactFor(AbilityId ability);
	std::optional<Fx> EffectBurstFor(AbilityId ability);
	std::optional<Fx> CastFor(AbilityId ability);
	Fx HitFor(Element element);

	class EffectSystem
	{
	public:
		using HeightAt = std::function<float(const glm::vec2&)>;

		explicit EffectSystem(ParticleSystem& particles) : m_Particles(particles) {}

		void Burst(Fx fx, const glm::vec3& position, const glm::vec3& direction = { 0.0f, 1.0f, 0.0f }, float scale = 1.0f, const glm::vec3& tint = glm::vec3(1.0f));
		void Trail(Fx fx, uint64_t key, const glm::vec3& position, float dt, float scale = 1.0f, const glm::vec3& tint = glm::vec3(1.0f));
		void Stream(Fx fx, const glm::vec3& center, const glm::vec3& direction, float rate, float dt);
		void Converge(uint64_t key, const glm::vec3& target, float radius, float rate, float life, float dt, float size = 1.0f, ParticleKind kind = ParticleKind::Mote, const glm::vec3& tint = glm::vec3(1.0f), float swirl = 0.0f, float arrive = 1.0f);
		void Mark(const GroundDecal& decal, float life, float fadeIn = 0.05f, float startScale = 1.0f, float endScale = 1.0f, float growTime = 0.25f, float cooling = 0.0f);
		void Spike(const glm::vec3& base, float height, float yaw, float lean, uint32_t variant, float life);
		void Impact(AbilityId ability, const glm::vec3& ground, float facing, float scale, const glm::vec3& color, uint32_t seed, const HeightAt& heightAt);
		void Update(float dt);
		void Draw(WorldRenderer& renderer, const SpellMeshes& meshes) const;
		void EndFrame();
		void Clear();

	private:
		struct TrailState
		{
			glm::vec3 Last{ 0.0f };
			std::vector<float> Carry;
			bool Touched = false;
		};

		struct DecalMark
		{
			GroundDecal Decal;
			float Life = 1.0f;
			float FadeIn = 0.05f;
			float StartScale = 1.0f;
			float EndScale = 1.0f;
			float GrowTime = 0.25f;
			float Cooling = 0.0f;
			float Age = 0.0f;
		};

		struct SpikeMark
		{
			glm::vec3 Base{ 0.0f };
			float Height = 0.5f;
			float Lean = 0.0f;
			float Yaw = 0.0f;
			uint32_t Variant = 0;
			float Life = 1.0f;
			float Age = 0.0f;
		};

		struct ConvergeState
		{
			float Carry = 0.0f;
			uint32_t Count = 0;
			bool Touched = false;
		};

		ParticleSystem& m_Particles;
		std::unordered_map<uint64_t, TrailState> m_Trails;
		std::unordered_map<uint64_t, ConvergeState> m_Converges;
		std::vector<DecalMark> m_Marks;
		std::vector<SpikeMark> m_Spikes;
		std::array<float, size_t(Fx::Count)> m_StreamCarry{};
	};
}
