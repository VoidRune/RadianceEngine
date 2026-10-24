#include "Effects.h"
#include "Rendering/Models.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <utility>

namespace Game
{
	namespace
	{
		constexpr float FullSphere = 3.14159265f;
		constexpr glm::vec3 Up{ 0.0f, 1.0f, 0.0f };

		struct FxLayer
		{
			ParticleKind Kind = ParticleKind::Glow;
			float Count = 0.0f;
			float Density = 0.0f;
			SpawnShape Shape = SpawnShape::Sphere;
			bool Outward = false;
			bool Aimed = false;
			float Radius = 0.0f;
			glm::vec3 Direction{ 0.0f, 1.0f, 0.0f };
			float Cone = FullSphere;
			glm::vec2 Speed{ 0.0f };
			glm::vec2 Life{ 0.5f, 1.0f };
			glm::vec2 Size{ 0.1f, 0.2f };
			glm::vec3 Tint{ 1.0f };
			float Inherit = 0.0f;
			float Lift = 0.0f;
		};

		using FxLayers = std::vector<FxLayer>;

		void SetColors(ParticleMaterial& material, const glm::vec4& a, const glm::vec4& b, const glm::vec4& c, const glm::vec4& d, const glm::vec4& times)
		{
			material.Colors[0] = a;
			material.Colors[1] = b;
			material.Colors[2] = c;
			material.Colors[3] = d;
			material.ColorTimes = times;
		}

		const std::array<FxLayers, size_t(Fx::Count)>& FxTable()
		{
			static const std::array<FxLayers, size_t(Fx::Count)> table = []
			{
				using K = ParticleKind;
				std::array<FxLayers, size_t(Fx::Count)> fx;
				const auto set = [&](Fx id, FxLayers layers) { fx[size_t(id)] = std::move(layers); };

				set(Fx::Campfire, {
					{ .Kind = K::Fire, .Count = 60.0f, .Radius = 0.26f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.35f, .Speed = { 0.35f, 0.9f }, .Life = { 0.4f, 0.75f }, .Size = { 0.2f, 0.36f } },
					{ .Kind = K::Ember, .Count = 9.0f, .Radius = 0.22f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.5f, .Speed = { 0.8f, 2.2f }, .Life = { 0.8f, 1.6f }, .Size = { 0.02f, 0.035f } },
					{ .Kind = K::Smoke, .Count = 5.0f, .Radius = 0.22f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.3f, .Speed = { 0.4f, 0.8f }, .Life = { 1.6f, 2.6f }, .Size = { 0.35f, 0.7f }, .Tint = { 0.6f, 0.6f, 0.6f } },
				});
				set(Fx::LanternFlame, {
					{ .Kind = K::Fire, .Count = 16.0f, .Radius = 0.025f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.25f, .Speed = { 0.05f, 0.15f }, .Life = { 0.25f, 0.4f }, .Size = { 0.07f, 0.1f }, .Tint = { 0.9f, 0.85f, 0.75f } },
				});
				set(Fx::CandleFlame, {
					{ .Kind = K::Fire, .Count = 12.0f, .Radius = 0.008f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.2f, .Speed = { 0.03f, 0.08f }, .Life = { 0.18f, 0.28f }, .Size = { 0.035f, 0.05f }, .Tint = { 0.9f, 0.85f, 0.75f } },
				});
				set(Fx::FireTrail, {
					{ .Kind = K::Fire, .Count = 45.0f, .Density = 6.0f, .Shape = SpawnShape::Segment, .Radius = 0.06f, .Speed = { 0.2f, 0.8f }, .Life = { 0.2f, 0.38f }, .Size = { 0.14f, 0.24f } },
					{ .Kind = K::Ember, .Count = 18.0f, .Density = 2.0f, .Shape = SpawnShape::Segment, .Radius = 0.08f, .Speed = { 0.5f, 1.5f }, .Life = { 0.4f, 0.9f }, .Size = { 0.025f, 0.04f }, .Inherit = 0.15f },
					{ .Kind = K::Smoke, .Count = 14.0f, .Density = 1.5f, .Shape = SpawnShape::Segment, .Radius = 0.08f, .Speed = { 0.1f, 0.4f }, .Life = { 0.5f, 0.9f }, .Size = { 0.15f, 0.25f } },
				});
				set(Fx::FrostTrail, {
					{ .Kind = K::Frost, .Count = 16.0f, .Density = 3.0f, .Shape = SpawnShape::Segment, .Radius = 0.12f, .Speed = { 0.1f, 0.5f }, .Life = { 0.3f, 0.6f }, .Size = { 0.15f, 0.25f }, .Tint = { 0.6f, 0.6f, 0.6f } },
					{ .Kind = K::IceShard, .Count = 10.0f, .Density = 1.0f, .Shape = SpawnShape::Segment, .Radius = 0.1f, .Speed = { 0.5f, 1.5f }, .Life = { 0.4f, 0.8f }, .Size = { 0.04f, 0.07f } },
				});
				set(Fx::BoltTrail, {
					{ .Kind = K::Lightning, .Count = 60.0f, .Density = 8.0f, .Shape = SpawnShape::Segment, .Radius = 0.05f, .Speed = { 1.0f, 3.0f }, .Life = { 0.12f, 0.25f }, .Size = { 0.02f, 0.035f } },
				});
				set(Fx::IceTrail, {
					{ .Kind = K::Frost, .Count = 30.0f, .Density = 6.0f, .Shape = SpawnShape::Segment, .Radius = 0.08f, .Speed = { 0.1f, 0.4f }, .Life = { 0.25f, 0.5f }, .Size = { 0.1f, 0.18f } },
					{ .Kind = K::IceShard, .Count = 12.0f, .Density = 1.5f, .Shape = SpawnShape::Segment, .Radius = 0.06f, .Speed = { 0.3f, 1.2f }, .Life = { 0.3f, 0.6f }, .Size = { 0.035f, 0.06f } },
				});
				set(Fx::ArrowTrail, {
					{ .Kind = K::Glow, .Density = 6.0f, .Shape = SpawnShape::Segment, .Life = { 0.12f, 0.2f }, .Size = { 0.035f, 0.05f }, .Tint = { 0.6f, 0.57f, 0.48f } },
				});
				set(Fx::PoisonTrail, {
					{ .Kind = K::Poison, .Count = 20.0f, .Density = 3.0f, .Shape = SpawnShape::Segment, .Radius = 0.08f, .Speed = { 0.2f, 0.8f }, .Life = { 0.4f, 0.8f }, .Size = { 0.05f, 0.09f } },
				});
				set(Fx::ArcaneTrail, {
					{ .Kind = K::Arcane, .Count = 30.0f, .Density = 4.0f, .Shape = SpawnShape::Segment, .Radius = 0.12f, .Speed = { 0.2f, 0.7f }, .Life = { 0.3f, 0.6f }, .Size = { 0.07f, 0.12f } },
				});
				set(Fx::ShardTrail, {
					{ .Kind = K::IceShard, .Count = 10.0f, .Density = 2.0f, .Shape = SpawnShape::Segment, .Radius = 0.08f, .Speed = { 0.3f, 1.0f }, .Life = { 0.3f, 0.5f }, .Size = { 0.04f, 0.06f } },
					{ .Kind = K::Frost, .Count = 15.0f, .Density = 2.0f, .Shape = SpawnShape::Segment, .Radius = 0.1f, .Speed = { 0.1f, 0.3f }, .Life = { 0.25f, 0.45f }, .Size = { 0.1f, 0.16f } },
				});

				set(Fx::Explosion, {
					{ .Kind = K::Flash, .Count = 1.0f, .Life = { 0.12f, 0.12f }, .Size = { 1.1f, 1.1f } },
					{ .Kind = K::Fire, .Count = 8.0f, .Radius = 0.2f, .Speed = { 0.0f, 0.5f }, .Life = { 0.25f, 0.4f }, .Size = { 0.45f, 0.6f }, .Tint = { 0.55f, 0.55f, 0.55f } },
					{ .Kind = K::Fire, .Count = 36.0f, .Outward = true, .Radius = 0.45f, .Direction = { 0.0f, 0.6f, 0.0f }, .Cone = 0.5f, .Speed = { 1.2f, 3.2f }, .Life = { 0.3f, 0.55f }, .Size = { 0.28f, 0.45f }, .Tint = { 0.6f, 0.6f, 0.6f } },
					{ .Kind = K::Ember, .Count = 30.0f, .Outward = true, .Radius = 0.2f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.6f, .Speed = { 4.0f, 9.0f }, .Life = { 0.5f, 1.2f }, .Size = { 0.03f, 0.05f } },
					{ .Kind = K::Spark, .Count = 18.0f, .Outward = true, .Radius = 0.15f, .Direction = { 0.0f, 0.8f, 0.0f }, .Cone = 0.6f, .Speed = { 6.0f, 12.0f }, .Life = { 0.4f, 0.9f }, .Size = { 0.03f, 0.05f } },
					{ .Kind = K::Smoke, .Count = 18.0f, .Outward = true, .Radius = 0.4f, .Direction = { 0.0f, 0.5f, 0.0f }, .Cone = 0.5f, .Speed = { 0.5f, 1.5f }, .Life = { 1.0f, 1.8f }, .Size = { 0.45f, 0.8f } },
				});
				set(Fx::MeteorImpact, {
					{ .Kind = K::Flash, .Count = 1.0f, .Life = { 0.18f, 0.18f }, .Size = { 1.6f, 1.6f } },
					{ .Kind = K::Fire, .Count = 44.0f, .Outward = true, .Radius = 0.6f, .Direction = { 0.0f, 0.8f, 0.0f }, .Cone = 0.5f, .Speed = { 3.0f, 7.0f }, .Life = { 0.45f, 0.9f }, .Size = { 0.35f, 0.6f }, .Tint = { 0.6f, 0.6f, 0.6f } },
					{ .Kind = K::Ember, .Count = 50.0f, .Outward = true, .Radius = 0.4f, .Direction = { 0.0f, 1.2f, 0.0f }, .Cone = 0.6f, .Speed = { 5.0f, 12.0f }, .Life = { 0.7f, 1.6f }, .Size = { 0.035f, 0.06f } },
					{ .Kind = K::Spark, .Count = 26.0f, .Outward = true, .Radius = 0.3f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.6f, .Speed = { 7.0f, 14.0f }, .Life = { 0.5f, 1.1f }, .Size = { 0.035f, 0.06f } },
					{ .Kind = K::Smoke, .Count = 24.0f, .Outward = true, .Radius = 0.8f, .Direction = { 0.0f, 0.6f, 0.0f }, .Cone = 0.5f, .Speed = { 0.8f, 2.0f }, .Life = { 1.2f, 2.2f }, .Size = { 0.6f, 1.0f } },
					{ .Kind = K::Bone, .Count = 24.0f, .Outward = true, .Radius = 0.5f, .Direction = { 0.0f, 1.4f, 0.0f }, .Cone = 0.5f, .Speed = { 3.0f, 7.0f }, .Life = { 1.0f, 1.8f }, .Size = { 0.05f, 0.09f }, .Tint = { 0.35f, 0.3f, 0.27f } },
				});
				set(Fx::FrostNova, {
					{ .Kind = K::Shockwave, .Count = 1.0f, .Life = { 0.4f, 0.4f }, .Size = { 4.4f, 4.4f }, .Tint = { 0.5f, 0.9f, 1.6f }, .Lift = -0.45f },
					{ .Kind = K::Frost, .Count = 46.0f, .Shape = SpawnShape::Ring, .Outward = true, .Radius = 0.6f, .Direction = { 0.0f, 0.15f, 0.0f }, .Cone = 0.2f, .Speed = { 4.0f, 7.0f }, .Life = { 0.4f, 0.8f }, .Size = { 0.3f, 0.5f }, .Tint = { 0.6f, 0.6f, 0.6f } },
					{ .Kind = K::IceShard, .Count = 40.0f, .Shape = SpawnShape::Ring, .Outward = true, .Radius = 0.5f, .Direction = { 0.0f, 0.6f, 0.0f }, .Cone = 0.3f, .Speed = { 5.0f, 9.0f }, .Life = { 0.5f, 1.0f }, .Size = { 0.05f, 0.09f } },
				});
				set(Fx::FrostBurst, {
					{ .Kind = K::IceShard, .Count = 14.0f, .Outward = true, .Radius = 0.1f, .Direction = { 0.0f, 0.8f, 0.0f }, .Cone = 0.8f, .Speed = { 2.0f, 5.0f }, .Life = { 0.4f, 0.8f }, .Size = { 0.04f, 0.07f } },
					{ .Kind = K::Frost, .Count = 10.0f, .Radius = 0.2f, .Speed = { 0.5f, 1.5f }, .Life = { 0.3f, 0.6f }, .Size = { 0.2f, 0.35f } },
				});
				set(Fx::LightningBurst, {
					{ .Kind = K::Lightning, .Count = 28.0f, .Outward = true, .Radius = 0.1f, .Cone = FullSphere, .Speed = { 4.0f, 9.0f }, .Life = { 0.15f, 0.35f }, .Size = { 0.025f, 0.04f } },
					{ .Kind = K::Flash, .Count = 1.0f, .Life = { 0.1f, 0.1f }, .Size = { 0.45f, 0.45f }, .Tint = { 0.35f, 0.5f, 1.0f } },
				});
				set(Fx::ArrowImpact, {
					{ .Kind = K::Dust, .Count = 4.0f, .Radius = 0.1f, .Speed = { 0.3f, 0.8f }, .Life = { 0.3f, 0.5f }, .Size = { 0.12f, 0.2f } },
					{ .Kind = K::Spark, .Count = 6.0f, .Outward = true, .Radius = 0.05f, .Direction = { 0.0f, 0.5f, 0.0f }, .Cone = 1.0f, .Speed = { 2.0f, 4.0f }, .Life = { 0.2f, 0.4f }, .Size = { 0.02f, 0.03f }, .Tint = { 0.8f, 0.8f, 0.8f } },
				});
				set(Fx::PoisonSplash, {
					{ .Kind = K::Poison, .Count = 18.0f, .Outward = true, .Radius = 0.15f, .Direction = { 0.0f, 0.8f, 0.0f }, .Cone = 0.8f, .Speed = { 1.0f, 3.0f }, .Life = { 0.4f, 0.8f }, .Size = { 0.05f, 0.09f } },
				});
				set(Fx::ArcaneBurst, {
					{ .Kind = K::Arcane, .Count = 24.0f, .Outward = true, .Radius = 0.2f, .Cone = FullSphere, .Speed = { 1.5f, 4.0f }, .Life = { 0.4f, 0.8f }, .Size = { 0.07f, 0.12f } },
					{ .Kind = K::Flash, .Count = 1.0f, .Life = { 0.14f, 0.14f }, .Size = { 0.6f, 0.6f }, .Tint = { 0.5f, 0.3f, 1.0f } },
				});
				set(Fx::BombBurst, {
					{ .Kind = K::Flash, .Count = 1.0f, .Life = { 0.1f, 0.1f }, .Size = { 0.55f, 0.55f }, .Tint = { 0.6f, 0.4f, 0.25f } },
					{ .Kind = K::Fire, .Count = 24.0f, .Outward = true, .Radius = 0.35f, .Direction = { 0.0f, 0.7f, 0.0f }, .Cone = 0.5f, .Speed = { 1.2f, 3.0f }, .Life = { 0.25f, 0.45f }, .Size = { 0.2f, 0.34f }, .Tint = { 0.6f, 0.6f, 0.6f } },
					{ .Kind = K::Ember, .Count = 20.0f, .Outward = true, .Radius = 0.2f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.6f, .Speed = { 3.5f, 7.0f }, .Life = { 0.4f, 0.9f }, .Size = { 0.03f, 0.045f } },
					{ .Kind = K::Smoke, .Count = 10.0f, .Outward = true, .Radius = 0.35f, .Direction = { 0.0f, 0.5f, 0.0f }, .Cone = 0.5f, .Speed = { 0.4f, 1.2f }, .Life = { 0.8f, 1.4f }, .Size = { 0.4f, 0.65f } },
				});
				set(Fx::Snowfall, {
					{ .Kind = K::Snow, .Count = 150.0f, .Shape = SpawnShape::Disc, .Radius = 16.0f, .Direction = { 0.3f, -1.0f, 0.12f }, .Cone = 0.2f, .Speed = { 0.4f, 0.8f }, .Life = { 6.0f, 8.0f }, .Size = { 0.025f, 0.05f } },
				});
				set(Fx::FireBurst, {
					{ .Kind = K::Fire, .Count = 22.0f, .Outward = true, .Radius = 0.4f, .Direction = { 0.0f, 0.8f, 0.0f }, .Cone = 0.5f, .Speed = { 1.0f, 3.0f }, .Life = { 0.3f, 0.6f }, .Size = { 0.2f, 0.35f } },
					{ .Kind = K::Ember, .Count = 16.0f, .Outward = true, .Radius = 0.3f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.6f, .Speed = { 3.0f, 6.0f }, .Life = { 0.5f, 1.0f }, .Size = { 0.03f, 0.05f } },
					{ .Kind = K::Smoke, .Count = 6.0f, .Radius = 0.4f, .Speed = { 0.3f, 0.8f }, .Life = { 0.8f, 1.3f }, .Size = { 0.35f, 0.55f } },
				});
				set(Fx::HitPhysical, {
					{ .Kind = K::Spark, .Count = 8.0f, .Aimed = true, .Radius = 0.05f, .Cone = 0.9f, .Speed = { 2.0f, 5.0f }, .Life = { 0.15f, 0.35f }, .Size = { 0.02f, 0.03f }, .Tint = { 0.9f, 0.9f, 0.9f } },
				});
				set(Fx::HitFire, {
					{ .Kind = K::Ember, .Count = 12.0f, .Outward = true, .Radius = 0.1f, .Direction = { 0.0f, 0.6f, 0.0f }, .Cone = 0.9f, .Speed = { 1.5f, 4.0f }, .Life = { 0.3f, 0.7f }, .Size = { 0.025f, 0.04f } },
					{ .Kind = K::Fire, .Count = 4.0f, .Radius = 0.15f, .Speed = { 0.3f, 0.8f }, .Life = { 0.2f, 0.35f }, .Size = { 0.15f, 0.22f } },
				});
				set(Fx::HitCold, {
					{ .Kind = K::IceShard, .Count = 8.0f, .Outward = true, .Radius = 0.1f, .Direction = { 0.0f, 0.6f, 0.0f }, .Cone = 0.9f, .Speed = { 1.5f, 3.5f }, .Life = { 0.3f, 0.6f }, .Size = { 0.03f, 0.05f } },
					{ .Kind = K::Frost, .Count = 5.0f, .Radius = 0.15f, .Speed = { 0.2f, 0.6f }, .Life = { 0.25f, 0.45f }, .Size = { 0.15f, 0.25f } },
				});
				set(Fx::HitLightning, {
					{ .Kind = K::Lightning, .Count = 12.0f, .Outward = true, .Radius = 0.1f, .Cone = FullSphere, .Speed = { 3.0f, 6.0f }, .Life = { 0.1f, 0.25f }, .Size = { 0.02f, 0.035f } },
				});
				set(Fx::HitPoison, {
					{ .Kind = K::Poison, .Count = 10.0f, .Outward = true, .Radius = 0.1f, .Direction = { 0.0f, 0.6f, 0.0f }, .Cone = 0.9f, .Speed = { 1.0f, 2.5f }, .Life = { 0.3f, 0.6f }, .Size = { 0.04f, 0.07f } },
				});
				set(Fx::Critical, {
					{ .Kind = K::Flash, .Count = 1.0f, .Life = { 0.12f, 0.12f }, .Size = { 0.5f, 0.5f }, .Tint = { 0.8f, 0.65f, 0.3f } },
					{ .Kind = K::Spark, .Count = 10.0f, .Outward = true, .Radius = 0.05f, .Cone = FullSphere, .Speed = { 3.0f, 6.0f }, .Life = { 0.2f, 0.4f }, .Size = { 0.025f, 0.04f } },
				});
				set(Fx::Death, {
					{ .Kind = K::Bone, .Count = 14.0f, .Outward = true, .Radius = 0.3f, .Direction = { 0.0f, 1.5f, 0.0f }, .Cone = 0.8f, .Speed = { 1.5f, 3.5f }, .Life = { 0.8f, 1.4f }, .Size = { 0.03f, 0.05f } },
					{ .Kind = K::Dust, .Count = 6.0f, .Radius = 0.3f, .Speed = { 0.2f, 0.6f }, .Life = { 0.6f, 1.0f }, .Size = { 0.3f, 0.45f } },
				});
				set(Fx::Dodge, {
					{ .Kind = K::Dust, .Count = 8.0f, .Shape = SpawnShape::Disc, .Outward = true, .Radius = 0.3f, .Direction = { 0.0f, 0.3f, 0.0f }, .Cone = 0.3f, .Speed = { 0.5f, 1.5f }, .Life = { 0.4f, 0.7f }, .Size = { 0.15f, 0.25f } },
				});
				set(Fx::Blink, {
					{ .Kind = K::Arcane, .Count = 30.0f, .Outward = true, .Radius = 0.4f, .Cone = FullSphere, .Speed = { 1.0f, 3.0f }, .Life = { 0.4f, 0.8f }, .Size = { 0.07f, 0.12f } },
					{ .Kind = K::Flash, .Count = 1.0f, .Life = { 0.15f, 0.15f }, .Size = { 0.7f, 0.7f }, .Tint = { 0.45f, 0.3f, 0.95f } },
				});
				set(Fx::Heal, {
					{ .Kind = K::Heal, .Count = 36.0f, .Shape = SpawnShape::Disc, .Radius = 0.6f, .Speed = { 0.3f, 1.0f }, .Life = { 0.8f, 1.4f }, .Size = { 0.06f, 0.1f } },
					{ .Kind = K::Glow, .Count = 1.0f, .Life = { 0.5f, 0.5f }, .Size = { 1.0f, 1.0f }, .Tint = { 0.3f, 1.0f, 0.45f }, .Lift = 0.6f },
				});
				set(Fx::Slam, {
					{ .Kind = K::Dust, .Count = 16.0f, .Shape = SpawnShape::Ring, .Outward = true, .Radius = 0.5f, .Direction = { 0.0f, 0.3f, 0.0f }, .Cone = 0.3f, .Speed = { 2.0f, 4.0f }, .Life = { 0.5f, 0.9f }, .Size = { 0.25f, 0.4f } },
				});
				set(Fx::GroundSlam, {
					{ .Kind = K::Dust, .Count = 26.0f, .Aimed = true, .Radius = 0.5f, .Cone = 0.6f, .Speed = { 3.0f, 7.0f }, .Life = { 0.5f, 0.9f }, .Size = { 0.25f, 0.45f } },
					{ .Kind = K::Bone, .Count = 14.0f, .Aimed = true, .Radius = 0.4f, .Cone = 0.6f, .Speed = { 4.0f, 8.0f }, .Life = { 0.8f, 1.4f }, .Size = { 0.05f, 0.08f }, .Tint = { 0.4f, 0.35f, 0.3f } },
				});
				set(Fx::Cyclone, {
					{ .Kind = K::Dust, .Count = 10.0f, .Shape = SpawnShape::Ring, .Outward = true, .Radius = 0.8f, .Direction = { 0.0f, 0.2f, 0.0f }, .Cone = 0.3f, .Speed = { 1.5f, 3.0f }, .Life = { 0.4f, 0.7f }, .Size = { 0.2f, 0.3f } },
					{ .Kind = K::Spark, .Count = 10.0f, .Shape = SpawnShape::Ring, .Outward = true, .Radius = 1.0f, .Direction = { 0.0f, 0.3f, 0.0f }, .Cone = 0.4f, .Speed = { 3.0f, 6.0f }, .Life = { 0.2f, 0.4f }, .Size = { 0.025f, 0.04f }, .Tint = { 0.9f, 0.9f, 0.9f } },
				});
				set(Fx::Debris, {
					{ .Kind = K::Dust, .Count = 24.0f, .Shape = SpawnShape::Disc, .Outward = true, .Radius = 0.8f, .Direction = { 0.0f, 0.5f, 0.0f }, .Cone = 0.4f, .Speed = { 1.5f, 4.0f }, .Life = { 0.7f, 1.3f }, .Size = { 0.35f, 0.6f } },
					{ .Kind = K::Bone, .Count = 20.0f, .Outward = true, .Radius = 0.5f, .Direction = { 0.0f, 1.5f, 0.0f }, .Cone = 0.6f, .Speed = { 3.0f, 7.0f }, .Life = { 0.9f, 1.6f }, .Size = { 0.05f, 0.09f }, .Tint = { 0.4f, 0.35f, 0.3f } },
					{ .Kind = K::Shockwave, .Count = 1.0f, .Life = { 0.4f, 0.4f }, .Size = { 3.0f, 3.0f }, .Tint = { 0.7f, 0.55f, 0.4f }, .Lift = -0.45f },
				});
				set(Fx::CrystalBurst, {
					{ .Kind = K::IceShard, .Count = 20.0f, .Outward = true, .Radius = 0.3f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.6f, .Speed = { 2.0f, 6.0f }, .Life = { 0.5f, 1.0f }, .Size = { 0.05f, 0.09f } },
					{ .Kind = K::Frost, .Count = 10.0f, .Radius = 0.4f, .Speed = { 0.3f, 1.0f }, .Life = { 0.4f, 0.7f }, .Size = { 0.3f, 0.45f } },
				});
				set(Fx::LootSparkle, {
					{ .Kind = K::Gold, .Count = 8.0f, .Radius = 0.3f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.5f, .Speed = { 0.3f, 0.8f }, .Life = { 0.8f, 1.4f }, .Size = { 0.05f, 0.08f } },
				});
				set(Fx::Gust, {
					{ .Kind = K::Dust, .Count = 18.0f, .Shape = SpawnShape::Ring, .Outward = true, .Radius = 0.4f, .Direction = { 0.0f, 0.4f, 0.0f }, .Cone = 0.3f, .Speed = { 2.0f, 4.5f }, .Life = { 0.5f, 0.9f }, .Size = { 0.25f, 0.45f }, .Tint = { 0.75f, 0.8f, 0.95f } },
					{ .Kind = K::Spark, .Count = 14.0f, .Shape = SpawnShape::Ring, .Outward = true, .Radius = 0.6f, .Direction = { 0.0f, 1.2f, 0.0f }, .Cone = 0.5f, .Speed = { 2.0f, 5.0f }, .Life = { 0.3f, 0.6f }, .Size = { 0.02f, 0.035f }, .Tint = { 0.6f, 0.75f, 1.0f } },
				});
				set(Fx::Starstep, {
					{ .Kind = K::Glow, .Count = 14.0f, .Shape = SpawnShape::Disc, .Radius = 0.35f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.3f, .Speed = { 0.1f, 0.4f }, .Life = { 0.6f, 1.1f }, .Size = { 0.03f, 0.06f }, .Tint = { 0.55f, 0.7f, 1.3f } },
				});
				set(Fx::BalloonPop, {
					{ .Kind = K::Flash, .Count = 1.0f, .Life = { 0.1f, 0.1f }, .Size = { 0.9f, 0.9f }, .Tint = { 1.0f, 0.5f, 0.45f } },
					{ .Kind = K::Bone, .Count = 16.0f, .Outward = true, .Radius = 0.3f, .Cone = FullSphere, .Speed = { 2.0f, 5.0f }, .Life = { 0.6f, 1.1f }, .Size = { 0.04f, 0.07f }, .Tint = { 0.85f, 0.12f, 0.1f } },
				});
				set(Fx::CoinToss, {
					{ .Kind = K::Gold, .Count = 12.0f, .Radius = 0.1f, .Direction = { 0.0f, -1.0f, 0.0f }, .Cone = 0.4f, .Speed = { 1.0f, 2.5f }, .Life = { 0.8f, 1.4f }, .Size = { 0.05f, 0.08f } },
				});
				set(Fx::GatewaySwirl, {
					{ .Kind = K::Arcane, .Count = 30.0f, .Shape = SpawnShape::Ring, .Radius = 0.9f, .Direction = { 0.0f, 1.0f, 0.0f }, .Cone = 0.4f, .Speed = { 0.2f, 0.6f }, .Life = { 0.8f, 1.5f }, .Size = { 0.06f, 0.1f } },
				});
				const auto cast = [&](Fx id, ParticleKind kind, glm::vec3 tint)
				{
					set(id, { { .Kind = kind, .Count = 30.0f, .Radius = 0.1f, .Speed = { 0.2f, 0.6f }, .Life = { 0.2f, 0.45f }, .Size = { 0.04f, 0.07f }, .Tint = tint } });
				};
				cast(Fx::CastFire, ParticleKind::Ember, glm::vec3(1.0f));
				cast(Fx::CastFrost, ParticleKind::Frost, glm::vec3(1.0f));
				cast(Fx::CastLightning, ParticleKind::Lightning, glm::vec3(1.0f));
				cast(Fx::CastArcane, ParticleKind::Arcane, glm::vec3(1.0f));
				cast(Fx::CastNature, ParticleKind::Heal, glm::vec3(1.0f));
				set(Fx::MeteorTrail, {
					{ .Kind = K::Fire, .Count = 40.0f, .Density = 9.0f, .Shape = SpawnShape::Segment, .Radius = 0.3f, .Speed = { 0.3f, 1.2f }, .Life = { 0.25f, 0.5f }, .Size = { 0.3f, 0.5f }, .Tint = { 0.7f, 0.7f, 0.7f } },
					{ .Kind = K::Ember, .Count = 30.0f, .Density = 3.0f, .Shape = SpawnShape::Segment, .Radius = 0.35f, .Speed = { 1.0f, 3.0f }, .Life = { 0.5f, 1.1f }, .Size = { 0.03f, 0.05f }, .Inherit = 0.1f },
					{ .Kind = K::Smoke, .Count = 18.0f, .Density = 2.0f, .Shape = SpawnShape::Segment, .Radius = 0.35f, .Speed = { 0.2f, 0.6f }, .Life = { 0.7f, 1.2f }, .Size = { 0.4f, 0.7f } },
				});
				return fx;
			}();
			return table;
		}

		ParticleSpawn MakeSpawn(const FxLayer& layer, const glm::vec3& position, const glm::vec3& direction, float scale, const glm::vec3& tint)
		{
			const float grow = std::sqrt(std::max(scale, 0.01f));
			ParticleSpawn spawn;
			spawn.Material = uint32_t(layer.Kind);
			spawn.Shape = layer.Shape;
			spawn.Outward = layer.Outward;
			spawn.Origin = position + Up * layer.Lift;
			spawn.Radius = layer.Radius * scale;
			spawn.Direction = layer.Aimed ? direction : layer.Direction;
			spawn.Cone = layer.Cone;
			spawn.Speed = layer.Speed * grow;
			spawn.Life = layer.Life;
			spawn.Size = layer.Size * (layer.Kind == ParticleKind::Shockwave || layer.Kind == ParticleKind::Flash ? scale : grow);
			spawn.Tint = layer.Tint * tint;
			return spawn;
		}
	}

	std::vector<ParticleMaterial> MakeParticleMaterials()
	{
		std::vector<ParticleMaterial> materials(static_cast<size_t>(ParticleKind::Count));
		const auto define = [&](ParticleKind kind, ParticleShape shape, ParticleAlign align, float additive, float emission) -> ParticleMaterial&
		{
			ParticleMaterial& material = materials[size_t(kind)];
			material.Look = glm::vec4(float(shape), additive, emission, 0.0f);
			material.Motion = glm::vec4(0.0f, 0.25f, float(align), 0.0f);
			return material;
		};

		ParticleMaterial& glow = define(ParticleKind::Glow, ParticleShape::Glow, ParticleAlign::Billboard, 1.0f, 1.0f);
		SetColors(glow, { 1.0f, 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.6f }, { 1.0f, 1.0f, 1.0f, 0.0f }, { 0.0f, 0.15f, 0.6f, 1.0f });
		glow.Sizes = { 0.8f, 1.0f, 1.0f, 0.9f };
		glow.Physics = { 0.0f, 1.0f, 0.0f, 0.0f };

		ParticleMaterial& fire = define(ParticleKind::Fire, ParticleShape::Flame, ParticleAlign::Billboard, 1.0f, 1.2f);
		SetColors(fire, { 4.5f, 2.6f, 0.8f, 0.0f }, { 4.0f, 2.0f, 0.5f, 0.9f }, { 2.2f, 0.6f, 0.1f, 0.6f }, { 0.4f, 0.08f, 0.02f, 0.0f }, { 0.0f, 0.12f, 0.5f, 1.0f });
		fire.Sizes = { 0.5f, 1.0f, 0.85f, 0.4f };
		fire.SizeTimes = { 0.0f, 0.25f, 0.6f, 1.0f };
		fire.Physics = { 3.0f, 2.5f, 4.0f, 1.4f };
		fire.Motion.y = 0.3f;

		ParticleMaterial& ember = define(ParticleKind::Ember, ParticleShape::Spark, ParticleAlign::Velocity, 1.0f, 1.6f);
		SetColors(ember, { 5.0f, 2.5f, 0.6f, 1.0f }, { 3.5f, 1.3f, 0.25f, 1.0f }, { 1.5f, 0.35f, 0.05f, 0.8f }, { 0.4f, 0.05f, 0.0f, 0.0f }, { 0.0f, 0.4f, 0.8f, 1.0f });
		ember.Sizes = { 1.0f, 1.0f, 0.8f, 0.3f };
		ember.Physics = { 1.2f, 1.2f, 5.0f, 1.8f };
		ember.Look.w = 0.05f;

		ParticleMaterial& smoke = define(ParticleKind::Smoke, ParticleShape::Smoke, ParticleAlign::Billboard, 0.0f, 0.0f);
		SetColors(smoke, { 0.16f, 0.15f, 0.15f, 0.0f }, { 0.13f, 0.12f, 0.12f, 0.7f }, { 0.11f, 0.11f, 0.11f, 0.45f }, { 0.1f, 0.1f, 0.1f, 0.0f }, { 0.0f, 0.15f, 0.6f, 1.0f });
		smoke.Sizes = { 0.4f, 0.8f, 1.2f, 1.6f };
		smoke.SizeTimes = { 0.0f, 0.2f, 0.6f, 1.0f };
		smoke.Physics = { 0.8f, 1.8f, 1.2f, 0.7f };
		smoke.Motion.x = 0.5f;
		smoke.Motion.y = 0.6f;

		ParticleMaterial& spark = define(ParticleKind::Spark, ParticleShape::Spark, ParticleAlign::Velocity, 1.0f, 1.8f);
		SetColors(spark, { 6.0f, 4.5f, 2.5f, 1.0f }, { 4.0f, 2.2f, 0.7f, 1.0f }, { 1.5f, 0.4f, 0.08f, 0.8f }, { 0.3f, 0.05f, 0.0f, 0.0f }, { 0.0f, 0.3f, 0.75f, 1.0f });
		spark.Sizes = { 1.0f, 1.0f, 0.8f, 0.5f };
		spark.Physics = { -11.0f, 0.5f, 0.0f, 0.0f };
		spark.Collision = { 0.45f, 0.3f, 1.0f, 0.12f };
		spark.Look.w = 0.035f;

		ParticleMaterial& frost = define(ParticleKind::Frost, ParticleShape::Glow, ParticleAlign::Billboard, 0.6f, 0.5f);
		SetColors(frost, { 0.7f, 1.1f, 1.6f, 0.0f }, { 0.6f, 1.0f, 1.5f, 0.55f }, { 0.4f, 0.75f, 1.2f, 0.3f }, { 0.2f, 0.4f, 0.8f, 0.0f }, { 0.0f, 0.2f, 0.6f, 1.0f });
		frost.Sizes = { 0.5f, 1.0f, 1.3f, 1.5f };
		frost.Physics = { -0.4f, 2.5f, 1.5f, 0.9f };
		frost.Motion.y = 0.4f;

		ParticleMaterial& shard = define(ParticleKind::IceShard, ParticleShape::Shard, ParticleAlign::Billboard, 0.35f, 0.5f);
		SetColors(shard, { 1.4f, 1.9f, 2.4f, 1.0f }, { 1.0f, 1.5f, 2.2f, 1.0f }, { 0.6f, 1.0f, 1.8f, 0.8f }, { 0.3f, 0.6f, 1.2f, 0.0f }, { 0.0f, 0.5f, 0.85f, 1.0f });
		shard.Sizes = { 1.0f, 1.0f, 1.0f, 0.6f };
		shard.Physics = { -12.0f, 0.3f, 0.0f, 0.0f };
		shard.Collision = { 0.3f, 0.5f, 1.0f, 0.08f };
		shard.Motion.x = 7.0f;

		ParticleMaterial& lightning = define(ParticleKind::Lightning, ParticleShape::Spark, ParticleAlign::Velocity, 1.0f, 2.2f);
		SetColors(lightning, { 4.0f, 4.5f, 6.0f, 1.0f }, { 2.0f, 2.8f, 5.5f, 1.0f }, { 0.8f, 1.2f, 3.5f, 0.6f }, { 0.2f, 0.3f, 1.2f, 0.0f }, { 0.0f, 0.3f, 0.7f, 1.0f });
		lightning.Sizes = { 1.0f, 0.9f, 0.7f, 0.4f };
		lightning.Physics = { 0.0f, 3.0f, 12.0f, 3.0f };
		lightning.Look.w = 0.03f;

		ParticleMaterial& arcane = define(ParticleKind::Arcane, ParticleShape::Star, ParticleAlign::Billboard, 1.0f, 1.4f);
		SetColors(arcane, { 2.2f, 1.2f, 4.0f, 0.0f }, { 1.8f, 0.9f, 3.6f, 1.0f }, { 0.9f, 0.35f, 2.2f, 0.6f }, { 0.3f, 0.1f, 0.8f, 0.0f }, { 0.0f, 0.15f, 0.6f, 1.0f });
		arcane.Sizes = { 0.6f, 1.0f, 0.9f, 0.4f };
		arcane.Physics = { 0.6f, 1.8f, 3.0f, 1.5f };
		arcane.Motion.x = 2.5f;

		ParticleMaterial& poison = define(ParticleKind::Poison, ParticleShape::Glow, ParticleAlign::Billboard, 0.5f, 0.7f);
		SetColors(poison, { 0.9f, 2.6f, 0.4f, 0.9f }, { 0.6f, 1.9f, 0.25f, 0.8f }, { 0.3f, 0.9f, 0.1f, 0.5f }, { 0.1f, 0.3f, 0.05f, 0.0f }, { 0.0f, 0.4f, 0.8f, 1.0f });
		poison.Sizes = { 0.8f, 1.0f, 1.0f, 0.6f };
		poison.Physics = { -6.0f, 0.8f, 0.0f, 0.0f };
		poison.Collision = { 0.05f, 0.9f, 1.0f, 0.25f };

		ParticleMaterial& heal = define(ParticleKind::Heal, ParticleShape::Star, ParticleAlign::Billboard, 1.0f, 1.1f);
		SetColors(heal, { 0.6f, 2.4f, 1.0f, 0.0f }, { 0.5f, 2.2f, 0.9f, 1.0f }, { 0.3f, 1.4f, 0.6f, 0.6f }, { 0.1f, 0.5f, 0.2f, 0.0f }, { 0.0f, 0.2f, 0.65f, 1.0f });
		heal.Sizes = { 0.5f, 1.0f, 0.9f, 0.4f };
		heal.Physics = { 2.2f, 1.2f, 1.2f, 1.0f };
		heal.Motion.x = 1.5f;

		ParticleMaterial& dust = define(ParticleKind::Dust, ParticleShape::Smoke, ParticleAlign::Billboard, 0.0f, 0.0f);
		SetColors(dust, { 0.5f, 0.45f, 0.38f, 0.0f }, { 0.47f, 0.42f, 0.36f, 0.6f }, { 0.42f, 0.38f, 0.33f, 0.35f }, { 0.4f, 0.36f, 0.31f, 0.0f }, { 0.0f, 0.15f, 0.6f, 1.0f });
		dust.Sizes = { 0.35f, 0.8f, 1.2f, 1.5f };
		dust.Physics = { 0.25f, 3.0f, 0.8f, 0.8f };
		dust.Motion.x = 0.6f;
		dust.Motion.y = 0.5f;

		ParticleMaterial& bone = define(ParticleKind::Bone, ParticleShape::Debris, ParticleAlign::Billboard, 0.0f, 0.0f);
		SetColors(bone, { 0.62f, 0.58f, 0.5f, 1.0f }, { 0.6f, 0.56f, 0.48f, 1.0f }, { 0.55f, 0.5f, 0.43f, 0.9f }, { 0.5f, 0.46f, 0.4f, 0.0f }, { 0.0f, 0.7f, 0.9f, 1.0f });
		bone.Physics = { -13.0f, 0.3f, 0.0f, 0.0f };
		bone.Collision = { 0.25f, 0.75f, 1.0f, 0.05f };
		bone.Motion.x = 9.0f;
		bone.Motion.y = 0.05f;

		ParticleMaterial& shockwave = define(ParticleKind::Shockwave, ParticleShape::Ring, ParticleAlign::Ground, 1.0f, 1.2f);
		SetColors(shockwave, { 2.5f, 2.0f, 1.6f, 1.0f }, { 1.6f, 1.0f, 0.6f, 0.7f }, { 0.6f, 0.3f, 0.15f, 0.3f }, { 0.2f, 0.1f, 0.05f, 0.0f }, { 0.0f, 0.35f, 0.7f, 1.0f });
		shockwave.Sizes = { 0.15f, 0.7f, 0.95f, 1.0f };
		shockwave.SizeTimes = { 0.0f, 0.3f, 0.7f, 1.0f };
		shockwave.Motion.y = 0.1f;

		ParticleMaterial& flash = define(ParticleKind::Flash, ParticleShape::Glow, ParticleAlign::Billboard, 1.0f, 2.5f);
		SetColors(flash, { 2.2f, 1.7f, 1.2f, 1.0f }, { 1.4f, 0.9f, 0.5f, 0.6f }, { 0.5f, 0.25f, 0.1f, 0.2f }, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.3f, 0.6f, 1.0f });
		flash.Sizes = { 0.6f, 1.0f, 1.0f, 0.9f };
		flash.Motion.y = 0.5f;

		ParticleMaterial& snow = define(ParticleKind::Snow, ParticleShape::Glow, ParticleAlign::Billboard, 0.0f, 0.0f);
		SetColors(snow, { 0.9f, 0.94f, 1.0f, 0.0f }, { 0.9f, 0.94f, 1.0f, 0.85f }, { 0.88f, 0.92f, 0.98f, 0.8f }, { 0.85f, 0.9f, 0.96f, 0.0f }, { 0.0f, 0.1f, 0.85f, 1.0f });
		snow.Physics = { -0.5f, 0.8f, 0.8f, 0.35f };

		ParticleMaterial& gold = define(ParticleKind::Gold, ParticleShape::Star, ParticleAlign::Billboard, 1.0f, 1.3f);
		SetColors(gold, { 3.5f, 2.6f, 0.9f, 0.0f }, { 3.2f, 2.3f, 0.7f, 1.0f }, { 1.8f, 1.1f, 0.25f, 0.6f }, { 0.6f, 0.3f, 0.05f, 0.0f }, { 0.0f, 0.2f, 0.6f, 1.0f });
		gold.Sizes = { 0.5f, 1.0f, 0.8f, 0.3f };
		gold.Physics = { 0.9f, 1.5f, 1.0f, 1.0f };
		gold.Motion.x = 1.0f;
		return materials;
	}

	std::optional<Fx> TrailFor(AbilityId ability)
	{
		switch (ability)
		{
		case AbilityId::Fireball:
		case AbilityId::EmberBolt:
			return Fx::FireTrail;
		case AbilityId::Orb:
			return Fx::FrostTrail;
		case AbilityId::Bolt:
		case AbilityId::ChainLightning:
		case AbilityId::ArcLightning:
			return Fx::BoltTrail;
		case AbilityId::IceSpear:
		case AbilityId::Frostbite:
			return Fx::IceTrail;
		case AbilityId::Swap:
			return Fx::ArcaneTrail;
		case AbilityId::Arrow:
		case AbilityId::Volley:
		case AbilityId::Quarrel:
		case AbilityId::PiercingShot:
			return Fx::ArrowTrail;
		case AbilityId::Spit:
			return Fx::PoisonTrail;
		case AbilityId::TomeVolley:
		case AbilityId::InkSpiral:
			return Fx::ArcaneTrail;
		case AbilityId::SplittingBolt:
			return Fx::ArcaneTrail;
		case AbilityId::BoltShard:
			return Fx::ShardTrail;
		case AbilityId::ShardBurst:
			return Fx::ShardTrail;
		default:
			return std::nullopt;
		}
	}

	std::optional<Fx> ProjectileImpactFor(AbilityId ability)
	{
		switch (ability)
		{
		case AbilityId::Bolt:
		case AbilityId::ChainLightning:
		case AbilityId::ArcLightning:
			return Fx::LightningBurst;
		case AbilityId::Orb:
		case AbilityId::IceSpear:
		case AbilityId::ShardBurst:
		case AbilityId::Frostbite:
			return Fx::FrostBurst;
		case AbilityId::EmberBolt:
			return Fx::FireBurst;
		case AbilityId::Swap:
			return Fx::ArcaneBurst;
		case AbilityId::Arrow:
		case AbilityId::Volley:
		case AbilityId::Quarrel:
		case AbilityId::PiercingShot:
			return Fx::ArrowImpact;
		case AbilityId::Spit:
			return Fx::PoisonSplash;
		case AbilityId::SplittingBolt:
		case AbilityId::BoltShard:
		case AbilityId::TomeVolley:
		case AbilityId::InkSpiral:
			return Fx::ArcaneBurst;
		default:
			return std::nullopt;
		}
	}

	std::optional<Fx> EffectBurstFor(AbilityId ability)
	{
		switch (ability)
		{
		case AbilityId::Firebomb:
		case AbilityId::FirebombSkip:
		case AbilityId::FirebombSkip2:
			return Fx::BombBurst;
		case AbilityId::Fireball:
			return Fx::Explosion;
		case AbilityId::Meteor:
		case AbilityId::Comet:
			return Fx::MeteorImpact;
		case AbilityId::FrostNova:
			return Fx::FrostNova;
		case AbilityId::Gust:
			return Fx::Cyclone;
		case AbilityId::StormStrike:
		case AbilityId::StormJump:
			return Fx::HitLightning;
		case AbilityId::Shockwave:
			return Fx::LightningBurst;
		case AbilityId::GravityWell:
		case AbilityId::MirrorWard:
			return Fx::ArcaneBurst;
		case AbilityId::Warp:
		case AbilityId::Swap:
			return Fx::Blink;
		case AbilityId::FirebombFlame:
		case AbilityId::Firestrike:
			return Fx::FireBurst;
		case AbilityId::Slam:
			return Fx::Slam;
		case AbilityId::GroundSlam:
			return Fx::GroundSlam;
		case AbilityId::Cyclone:
			return Fx::Cyclone;
		case AbilityId::Blink:
			return Fx::Blink;
		case AbilityId::Rejuvenate:
			return Fx::Heal;
		case AbilityId::RainOfArrows:
		case AbilityId::TailSweep:
			return Fx::Debris;
		case AbilityId::PageStorm:
		case AbilityId::ArcaneBurst:
		case AbilityId::ArcaneRing:
			return Fx::ArcaneBurst;
		case AbilityId::HookAnchor:
			return Fx::ArrowImpact;
		case AbilityId::ShelfCrush:
		case AbilityId::Eruption:
			return Fx::Debris;
		case AbilityId::CrystalSpike:
		case AbilityId::CrystalRain:
			return Fx::CrystalBurst;
		default:
			return std::nullopt;
		}
	}

	std::optional<Fx> CastFor(AbilityId ability)
	{
		const AbilityDef& def = GetAbility(ability);
		if (!def.Spell)
			return std::nullopt;
		switch (def.Damages)
		{
		case Element::Fire:
			return Fx::CastFire;
		case Element::Cold:
			return Fx::CastFrost;
		case Element::Lightning:
			return Fx::CastLightning;
		default:
			return def.Kind == AbilityKind::Heal ? Fx::CastNature : Fx::CastArcane;
		}
	}

	Fx HitFor(Element element)
	{
		switch (element)
		{
		case Element::Fire:
			return Fx::HitFire;
		case Element::Cold:
			return Fx::HitCold;
		case Element::Lightning:
			return Fx::HitLightning;
		case Element::Poison:
			return Fx::HitPoison;
		default:
			return Fx::HitPhysical;
		}
	}

	void EffectSystem::Burst(Fx fx, const glm::vec3& position, const glm::vec3& direction, float scale, const glm::vec3& tint)
	{
		if (size_t(fx) >= size_t(Fx::Count))
			return;
		for (const FxLayer& layer : FxTable()[size_t(fx)])
		{
			ParticleSpawn spawn = MakeSpawn(layer, position, direction, scale, tint);
			spawn.Count = uint32_t(std::lround(layer.Count * std::clamp(scale, 0.5f, 3.0f)));
			m_Particles.Spawn(spawn);
		}
	}

	void EffectSystem::Trail(Fx fx, uint64_t key, const glm::vec3& position, float dt, float scale, const glm::vec3& tint)
	{
		if (size_t(fx) >= size_t(Fx::Count))
			return;
		const FxLayers& layers = FxTable()[size_t(fx)];
		const uint64_t id = (key << 8) | uint64_t(fx);
		const auto [it, inserted] = m_Trails.try_emplace(id);
		TrailState& state = it->second;
		if (inserted)
		{
			state.Last = position;
			state.Carry.assign(layers.size(), 0.0f);
		}
		state.Touched = true;
		const glm::vec3 travel = position - state.Last;
		const float distance = glm::length(travel);
		const glm::vec3 velocity = dt > 1e-4f && distance < 4.0f ? travel / dt : glm::vec3(0.0f);
		for (size_t i = 0; i < layers.size(); i++)
		{
			const FxLayer& layer = layers[i];
			state.Carry[i] += layer.Count * dt + layer.Density * std::min(distance, 4.0f);
			const uint32_t count = uint32_t(state.Carry[i]);
			if (count == 0)
				continue;
			state.Carry[i] -= float(count);
			ParticleSpawn spawn = MakeSpawn(layer, distance < 4.0f ? state.Last : position, glm::length(velocity) > 0.1f ? -glm::normalize(velocity) : Up, scale, tint);
			if (layer.Shape == SpawnShape::Segment)
				spawn.Segment = distance < 4.0f ? travel : glm::vec3(0.0f);
			spawn.Velocity = velocity * layer.Inherit;
			spawn.Count = count;
			m_Particles.Spawn(spawn);
		}
		state.Last = position;
	}

	void EffectSystem::EndFrame()
	{
		std::erase_if(m_Trails, [](auto& item) { return !std::exchange(item.second.Touched, false); });
	}

	void EffectSystem::Clear()
	{
		m_Trails.clear();
		m_Marks.clear();
		m_Spikes.clear();
	}

	void EffectSystem::Mark(const GroundDecal& decal, float life, float fadeIn, float startScale, float endScale, float growTime, float cooling)
	{
		if (m_Marks.size() < 192)
			m_Marks.push_back({ .Decal = decal, .Life = life, .FadeIn = fadeIn, .StartScale = startScale, .EndScale = endScale, .GrowTime = growTime, .Cooling = cooling });
	}

	void EffectSystem::Spike(const glm::vec3& base, float height, float radius, float yaw, const glm::vec3& color, float life)
	{
		if (m_Spikes.size() < 256)
			m_Spikes.push_back({ .Base = base, .Height = height, .Radius = radius, .Yaw = yaw, .Color = color, .Life = life });
	}

	void EffectSystem::Update(float dt)
	{
		for (DecalMark& mark : m_Marks)
			mark.Age += dt;
		for (SpikeMark& spike : m_Spikes)
			spike.Age += dt;
		std::erase_if(m_Marks, [](const DecalMark& mark) { return mark.Age >= mark.Life; });
		std::erase_if(m_Spikes, [](const SpikeMark& spike) { return spike.Age >= spike.Life; });
	}

	void EffectSystem::Draw(WorldRenderer& renderer) const
	{
		const auto smooth = [](float edge0, float edge1, float x)
		{
			const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-4f), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		};
		for (const DecalMark& mark : m_Marks)
		{
			const float fade = std::clamp(mark.Age / std::max(mark.FadeIn, 1e-3f), 0.0f, 1.0f) * (1.0f - smooth(mark.Life * 0.55f, mark.Life, mark.Age));
			const float grow = 1.0f - std::pow(1.0f - std::clamp(mark.Age / std::max(mark.GrowTime, 1e-3f), 0.0f, 1.0f), 3.0f);
			GroundDecal decal = mark.Decal;
			decal.Extent *= mark.StartScale + (mark.EndScale - mark.StartScale) * grow;
			decal.Color.a *= fade;
			decal.Tint.a *= fade;
			if (mark.Cooling > 0.0f)
				decal.Params.x *= std::exp(-mark.Age * mark.Cooling);
			renderer.DrawDecal(decal);
		}
		for (const SpikeMark& spike : m_Spikes)
		{
			const float height = spike.Height * smooth(0.0f, 0.12f, spike.Age) * (1.0f - smooth(spike.Life - 0.45f, spike.Life, spike.Age));
			if (height > 0.01f)
				renderer.Draw(MeshKind::Cone, spike.Base + glm::vec3(0.0f, height * 0.5f, 0.0f), { spike.Radius, height * 0.5f, spike.Radius }, spike.Color, spike.Yaw, 0.5f, spike.Base.y, { .Roughness = 0.15f });
		}
	}

	void EffectSystem::Impact(AbilityId ability, const glm::vec3& ground, float facing, float scale, const glm::vec3& color, uint32_t seed, const HeightAt& heightAt)
	{
		const AbilityDef& def = GetAbility(ability);
		const float radius = def.ImpactRadius * scale;
		const float reach = def.Reach * scale;
		const glm::vec3 forward = Heading(facing);
		const auto random = [seed](uint32_t index)
		{
			uint32_t x = seed * 0x9E3779B1u + index * 0x85EBCA77u + 0x165667B1u;
			x ^= x >> 15;
			x *= 0x2C1B3C6Du;
			x ^= x >> 12;
			x *= 0x297A2D39u;
			x ^= x >> 15;
			return float(x & 0xFFFFu) / 65535.0f;
		};
		const auto decal = [&](DecalShape shape, const glm::vec3& center, const glm::vec2& extent)
		{
			GroundDecal result;
			result.Center = center;
			result.Extent = extent;
			result.Depth = 0.75f;
			result.Shape = shape;
			result.Seed = seed;
			return result;
		};
		const auto runes = [&](const glm::vec3& center, float size, float life, float inner, float startScale, float endScale, float intensity = 1.0f)
		{
			GroundDecal result = decal(DecalShape::Runes, center, glm::vec2(size));
			result.Color = glm::vec4(color * intensity, 1.0f);
			result.Params = glm::vec4(-1.0f, 1.0f, inner, 0.9f);
			Mark(result, life, 0.03f, startScale, endScale, 0.25f);
		};
		const auto shock = [&](float size, float life, float intensity = 1.2f)
		{
			const float extent = RingExtent(size, 0.09f);
			GroundDecal result = decal(DecalShape::Ring, ground, glm::vec2(extent));
			result.Color = glm::vec4(color * intensity, 1.0f);
			result.Params = glm::vec4(0.09f, 0.0f, 0.0f, size / extent);
			Mark(result, life, 0.01f, 0.2f, 1.0f, life * 0.8f);
		};
		const auto scorch = [&](const glm::vec3& center, float size, float life, float cooling)
		{
			GroundDecal result = decal(DecalShape::Scorch, center, glm::vec2(size));
			result.Color = glm::vec4(1.7f, 0.6f, 0.16f, 1.0f);
			result.Tint = glm::vec4(0.025f, 0.02f, 0.018f, 0.8f);
			result.Roughness = 0.95f;
			result.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			Mark(result, life, 0.05f, 0.85f, 1.0f, 0.2f, cooling);
		};
		const auto frost = [&](const glm::vec3& center, float size, float life)
		{
			GroundDecal result = decal(DecalShape::Frost, center, glm::vec2(size));
			result.Color = glm::vec4(0.35f, 0.6f, 0.95f, 1.0f);
			result.Tint = glm::vec4(0.7f, 0.85f, 1.0f, 0.28f);
			result.Roughness = 0.3f;
			result.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			Mark(result, life, 0.08f, 0.5f, 1.0f, 0.25f);
		};
		const auto cracks = [&](const glm::vec3& center, float size, float life, float heat)
		{
			GroundDecal result = decal(DecalShape::Cracks, center, glm::vec2(size));
			result.Color = glm::vec4(1.6f, 0.5f, 0.13f, 1.0f);
			result.Tint = glm::vec4(0.03f, 0.025f, 0.02f, 0.85f);
			result.Roughness = 0.9f;
			result.Params = glm::vec4(heat, 0.0f, 0.0f, 0.0f);
			Mark(result, life, 0.02f, 0.6f, 1.0f, 0.12f, heat > 0.0f ? 0.8f : 0.0f);
		};
		const auto flash = [&](DecalShape shape, const glm::vec3& center, const glm::vec2& extent, float yaw, float halfArc, float life)
		{
			GroundDecal result = decal(shape, center, extent);
			result.Yaw = yaw;
			result.Color = glm::vec4(color * 1.0f, 1.0f);
			result.Params = glm::vec4(1.0f, 1.0f, halfArc, 0.0f);
			Mark(result, life, 0.02f, 1.0f, 1.0f, 0.1f);
		};
		const glm::vec3 ice(0.42f, 0.68f, 0.95f);
		const auto spike = [&](const glm::vec2& point, float height, uint32_t index, float life)
		{
			Spike({ point.x, heightAt(point), point.y }, height, 0.1f + 0.07f * random(index * 4u + 3u), random(index * 4u + 2u) * 6.2832f, ice, life);
		};
		const auto spikeRing = [&](uint32_t count, float inner, float outer, float low, float high, float life)
		{
			for (uint32_t i = 0; i < count; i++)
			{
				const float angle = (float(i) + random(i * 4u)) / float(count) * 6.2832f;
				const glm::vec2 point = glm::vec2(ground.x, ground.z) + glm::vec2(std::cos(angle), std::sin(angle)) * (inner + (outer - inner) * random(i * 4u + 1u));
				spike(point, low + (high - low) * random(i * 4u + 2u), i, life);
			}
		};
		switch (ability)
		{
		case AbilityId::Fireball:
			scorch(ground, radius * 0.75f, 6.0f, 2.0f);
			shock(radius, 0.35f);
			break;
		case AbilityId::Firebomb:
		case AbilityId::FirebombSkip:
		case AbilityId::FirebombSkip2:
			scorch(ground, radius * 0.5f, 4.0f, 0.5f);
			shock(radius, 0.3f);
			break;
		case AbilityId::FirebombFlame:
			scorch(ground, radius * 0.5f, 2.5f, 1.0f);
			break;
		case AbilityId::StormStrike:
			scorch(ground, radius * 0.55f, 4.0f, 0.8f);
			cracks(ground, radius * 0.95f, 3.0f, 0.4f);
			shock(radius * 1.15f, 0.3f, 0.6f);
			break;
		case AbilityId::HookAnchor:
			cracks(ground, 0.5f, 1.4f, 0.0f);
			break;
		case AbilityId::Meteor:
		case AbilityId::Comet:
			scorch(ground, radius * 0.85f, 9.0f, 0.8f);
			cracks(ground, radius * 1.05f, 8.0f, 1.0f);
			shock(radius * 1.1f, 0.45f);
			runes(ground, radius, 0.7f, 0.0f, 1.0f, 1.15f);
			break;
		case AbilityId::Shockwave:
			shock(reach, 0.4f);
			shock(reach * 0.6f, 0.3f);
			runes(ground, reach, 0.6f, 0.55f, 0.5f, 1.05f);
			break;
		case AbilityId::GravityWell:
			runes(ground, radius, 0.9f, 0.3f, 1.15f, 0.25f);
			shock(radius * 0.5f, 0.35f);
			break;
		case AbilityId::Gust:
			flash(DecalShape::Sector, ground, glm::vec2(reach), facing, def.HalfArc, 0.4f);
			break;
		case AbilityId::Warp:
		case AbilityId::Swap:
		case AbilityId::MirrorWard:
			runes(ground, 1.1f, 0.6f, 0.0f, 0.6f, 1.0f, 0.55f);
			break;
		case AbilityId::Firestrike:
			scorch(ground, radius * 0.8f, 5.0f, 1.5f);
			shock(radius, 0.35f);
			break;
		case AbilityId::Eruption:
			scorch(ground, radius * 0.7f, 7.0f, 1.0f);
			cracks(ground, radius, 6.0f, 0.9f);
			break;
		case AbilityId::FrostNova:
			frost(ground, reach * 0.9f, 3.5f);
			runes(ground, reach, 0.9f, 0.0f, 0.4f, 1.0f);
			spikeRing(16, reach * 0.5f, reach * 0.95f, 0.5f, 1.1f, 1.6f);
			break;
		case AbilityId::CrystalRain:
			frost(ground, radius, 3.0f);
			runes(ground, radius, 0.6f, 0.0f, 0.8f, 1.0f);
			spikeRing(7, 0.0f, radius * 0.85f, 0.3f, 0.7f, 1.2f);
			break;
		case AbilityId::CrystalSpike:
		{
			const float width = def.Width * scale;
			flash(DecalShape::Lane, ground + forward * (reach * 0.5f), { reach * 0.5f, width * 0.5f }, facing, 0.0f, 0.5f);
			const glm::vec3 side = Heading(facing + 1.5708f);
			const uint32_t count = std::max(2u, uint32_t(reach / 0.6f));
			for (uint32_t i = 0; i < count; i++)
			{
				const glm::vec3 point = ground + forward * (reach * (float(i) + 0.5f) / float(count)) + side * ((random(i * 4u) - 0.5f) * width * 0.6f);
				spike({ point.x, point.z }, 0.4f + 0.5f * random(i * 4u + 1u), i, 1.2f);
			}
			break;
		}
		case AbilityId::ShelfCrush:
			flash(DecalShape::Lane, ground + forward * (reach * 0.5f), { reach * 0.5f, def.Width * scale * 0.5f }, facing, 0.0f, 0.45f);
			break;
		case AbilityId::TailSweep:
			flash(DecalShape::Sector, ground, glm::vec2(reach), facing, def.HalfArc, 0.45f);
			break;
		case AbilityId::Slam:
			cracks(ground, reach, 3.5f, 0.0f);
			shock(reach, 0.3f);
			break;
		case AbilityId::GroundSlam:
			cracks(ground + forward * (reach * 0.45f), reach * 0.65f, 3.5f, 0.0f);
			flash(DecalShape::Sector, ground, glm::vec2(reach), facing, def.HalfArc, 0.4f);
			break;
		case AbilityId::RainOfArrows:
			break;
		case AbilityId::PageStorm:
		case AbilityId::ArcaneBurst:
			runes(ground, radius, 0.8f, 0.0f, 0.85f, 1.05f);
			shock(radius, 0.35f);
			break;
		case AbilityId::ArcaneRing:
			runes(ground, radius, 0.8f, def.InnerRadius / std::max(def.ImpactRadius, 0.01f), 0.95f, 1.0f);
			break;
		case AbilityId::Blink:
			runes(ground, 1.1f, 0.6f, 0.0f, 0.6f, 1.0f);
			break;
		case AbilityId::Rejuvenate:
			runes(ground, 1.3f, 1.1f, 0.0f, 0.7f, 1.0f);
			break;
		default:
			break;
		}
	}
}
