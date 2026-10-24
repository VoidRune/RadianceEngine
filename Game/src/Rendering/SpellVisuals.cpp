#include "SpellVisuals.h"
#include "Rendering/Models.h"
#include "Rendering/WorldRenderer.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr glm::vec3 Up{ 0.0f, 1.0f, 0.0f };

		ParticleSprite Sprite(ParticleShape shape, const glm::vec3& position, float size, const glm::vec3& color)
		{
			ParticleSprite sprite;
			sprite.Shape = shape;
			sprite.Position = position;
			sprite.Size = size;
			sprite.Color = glm::vec4(color, 1.0f);
			return sprite;
		}

		ParticleSprite Stretched(ParticleShape shape, const glm::vec3& from, const glm::vec3& to, float width, const glm::vec3& color)
		{
			const glm::vec3 span = from - to;
			const float length = glm::length(span);
			ParticleSprite sprite = Sprite(shape, (from + to) * 0.5f, width, color);
			sprite.Align = ParticleAlign::Velocity;
			sprite.Axis = length > 1e-4f ? span / length : Up;
			sprite.Stretch = std::max(length * 0.5f - width, 0.0f);
			return sprite;
		}

		ParticleSprite RingSprite(const glm::vec3& center, float radius, float thickness, const glm::vec3& color)
		{
			const float size = RingExtent(radius, thickness);
			ParticleSprite sprite = Sprite(ParticleShape::Ring, center, size, color);
			sprite.Params = glm::vec4(thickness / size, radius / size, 0.0f, 0.0f);
			return sprite;
		}

		uint32_t Mix(uint32_t a, uint32_t b)
		{
			uint32_t x = a * 0x9E3779B1u ^ (b * 0x85EBCA77u + 0x7F4A7C15u);
			x ^= x >> 16;
			x *= 0x7FEB352Du;
			x ^= x >> 15;
			x *= 0x846CA68Bu;
			x ^= x >> 16;
			return x;
		}

		float Unit(uint32_t x)
		{
			return float(x & 0xFFFFu) / 65535.0f;
		}
	}

	float RingExtent(float radius, float thickness)
	{
		return (radius + 9.0f * thickness) / 0.975f;
	}

	void DrawRuneCircle(WorldRenderer& renderer, const RuneCircle& circle)
	{
		GroundDecal decal;
		decal.Center = circle.Center;
		decal.Extent = glm::vec2(circle.Radius);
		decal.Depth = 0.75f;
		decal.Shape = DecalShape::Runes;
		decal.Color = glm::vec4(circle.Color * circle.Intensity, 1.0f);
		decal.Params = glm::vec4(circle.Progress, circle.Appear, circle.Inner, circle.Spin);
		decal.Seed = circle.Seed;
		renderer.DrawDecal(decal);
	}

	void DrawGroundRing(WorldRenderer& renderer, const glm::vec3& center, float radius, const glm::vec3& color, float intensity, float thickness, float dashes, float spin, uint32_t pattern)
	{
		GroundDecal decal;
		decal.Seed = pattern;
		decal.Center = center;
		const float extent = RingExtent(radius, thickness);
		decal.Extent = glm::vec2(extent);
		decal.Depth = 0.5f;
		decal.Shape = DecalShape::Ring;
		decal.Color = glm::vec4(color * intensity, 1.0f);
		decal.Params = glm::vec4(thickness, dashes, spin, radius / extent);
		renderer.DrawDecal(decal);
	}

	void DrawGroundGlow(WorldRenderer& renderer, const glm::vec3& center, float radius, const glm::vec3& color, float intensity)
	{
		GroundDecal decal;
		decal.Center = center;
		decal.Extent = glm::vec2(radius);
		decal.Depth = 0.5f;
		decal.Shape = DecalShape::Glow;
		decal.Color = glm::vec4(color * intensity, 1.0f);
		renderer.DrawDecal(decal);
	}

	void DrawSectorTelegraph(WorldRenderer& renderer, const glm::vec3& apex, float facing, float reach, float halfArc, const glm::vec3& color, float intensity, float progress, float appear)
	{
		GroundDecal decal;
		decal.Center = apex;
		decal.Yaw = facing;
		decal.Extent = glm::vec2(reach);
		decal.Depth = 0.75f;
		decal.Shape = DecalShape::Sector;
		decal.Color = glm::vec4(color * intensity, 1.0f);
		decal.Params = glm::vec4(progress, appear, halfArc, 0.0f);
		renderer.DrawDecal(decal);
	}

	void DrawLaneTelegraph(WorldRenderer& renderer, const glm::vec3& start, float facing, float length, float width, const glm::vec3& color, float intensity, float progress, float appear)
	{
		GroundDecal decal;
		decal.Center = start + Heading(facing) * (length * 0.5f);
		decal.Yaw = facing;
		decal.Extent = glm::vec2(length * 0.5f, width * 0.5f);
		decal.Depth = 0.75f;
		decal.Shape = DecalShape::Lane;
		decal.Color = glm::vec4(color * intensity, 1.0f);
		decal.Params = glm::vec4(progress, appear, 0.0f, 0.0f);
		renderer.DrawDecal(decal);
	}

	void DrawGlow(WorldRenderer& renderer, const glm::vec3& position, float size, const glm::vec3& color, float softness)
	{
		ParticleSprite sprite = Sprite(ParticleShape::Glow, position, size, color);
		sprite.Softness = softness;
		renderer.DrawSprite(sprite);
	}

	void DrawStar(WorldRenderer& renderer, const glm::vec3& position, float size, const glm::vec3& color, float rotation)
	{
		ParticleSprite sprite = Sprite(ParticleShape::Star, position, size, color);
		sprite.Rotation = rotation;
		renderer.DrawSprite(sprite);
	}

	void DrawReticle(WorldRenderer& renderer, const glm::vec3& position, float radius, const glm::vec3& color)
	{
		ParticleSprite ring = RingSprite(position, radius, radius * 0.08f, color);
		ring.Emission = 0.2f;
		ring.Softness = 0.05f;
		renderer.DrawSprite(ring);
		ParticleSprite core = Sprite(ParticleShape::Glow, position, radius * 0.35f, color * 1.3f);
		core.Emission = 0.2f;
		core.Softness = 0.05f;
		renderer.DrawSprite(core);
	}

	void DrawFloatingRing(WorldRenderer& renderer, const glm::vec3& center, float radius, const glm::vec3& color, float thickness)
	{
		ParticleSprite sprite = RingSprite(center, radius, thickness, color);
		sprite.Align = ParticleAlign::Ground;
		sprite.Softness = 0.05f;
		renderer.DrawSprite(sprite);
	}

	void DrawBeam(WorldRenderer& renderer, const glm::vec3& base, float height, float width, const glm::vec3& color, float emission)
	{
		ParticleSprite sprite = Stretched(ParticleShape::Beam, base + Up * height, base, width, color);
		sprite.Emission = emission;
		renderer.DrawSprite(sprite);
	}

	void DrawStreak(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, float width, const glm::vec3& color)
	{
		renderer.DrawSprite(Stretched(ParticleShape::Beam, from, to, width, color));
	}

	void DrawSlash(WorldRenderer& renderer, const glm::vec3& center, float facing, float reach, float halfArc, float sweep, const glm::vec3& color, bool mirrored, float inner)
	{
		ParticleSprite sprite = Sprite(ParticleShape::Slash, center, reach, color);
		sprite.Align = ParticleAlign::Ground;
		sprite.Rotation = facing;
		sprite.Params = glm::vec4(halfArc, sweep, inner, mirrored ? -0.9f : 0.9f);
		sprite.Softness = 0.05f;
		renderer.DrawSprite(sprite);
	}

	void DrawProjectileVisual(WorldRenderer& renderer, AbilityId ability, const glm::vec3& center, float facing, const glm::vec3& color, float time, uint32_t seed, const glm::vec3& owner)
	{
		const glm::vec3 forward = Heading(facing);
		const bool marked = owner != glm::vec3(0.0f);
		const glm::vec3 heart = glm::mix(owner, glm::vec3(1.0f), 0.3f) * 1.7f;
		switch (ability)
		{
		case AbilityId::Swap:
		{
			DrawGlow(renderer, center, 0.6f, { 0.24f, 0.1f, 0.38f });
			ParticleSprite core = Sprite(ParticleShape::Orb, center, 0.2f, { 1.3f, 0.65f, 1.8f });
			core.Seed = seed;
			renderer.DrawSprite(core);
			for (uint32_t i = 0; i < 2; i++)
			{
				const float angle = time * 7.0f + float(i) * 3.1416f;
				DrawStar(renderer, center + Heading(angle) * 0.32f + Up * (0.08f * std::sin(time * 9.0f + float(i))), 0.12f, { 1.5f, 0.85f, 2.0f }, angle);
			}
			renderer.DrawSprite(RingSprite(center, 0.3f, 0.03f, marked ? owner * 1.3f : glm::vec3(0.85f, 0.42f, 1.25f)));
			if (marked)
				DrawGlow(renderer, center, 0.1f, heart);
			break;
		}
		case AbilityId::EmberBolt:
		case AbilityId::Fireball:
		{
			DrawGlow(renderer, center, 0.6f, { 0.5f, 0.18f, 0.05f });
			renderer.DrawSprite(Stretched(ParticleShape::Beam, center - forward * 0.75f, center, 0.2f, { 1.2f, 0.5f, 0.12f }));
			ParticleSprite core = Sprite(ParticleShape::Orb, center, 0.26f, { 1.4f, 0.65f, 0.18f });
			core.Seed = seed;
			renderer.DrawSprite(core);
			DrawGlow(renderer, center, marked ? 0.13f : 0.08f, marked ? heart : glm::vec3(1.2f, 1.0f, 0.7f));
			break;
		}
		case AbilityId::Orb:
		{
			DrawGlow(renderer, center, 0.55f, { 0.06f, 0.14f, 0.32f });
			ParticleSprite flake = Sprite(ParticleShape::Snowflake, center, 0.46f, { 0.45f, 0.8f, 1.3f });
			flake.Rotation = time * 1.4f;
			renderer.DrawSprite(flake);
			ParticleSprite core = Sprite(ParticleShape::Orb, center, 0.15f, { 0.6f, 0.95f, 1.4f });
			core.Seed = seed;
			renderer.DrawSprite(core);
			for (uint32_t i = 0; i < 3; i++)
			{
				const float angle = time * 4.5f + float(i) * 2.0944f;
				ParticleSprite shard = Sprite(ParticleShape::Shard, center + Heading(angle) * 0.38f + Up * (0.1f * std::sin(angle * 2.0f + float(i))), 0.1f, { 0.6f, 1.0f, 1.5f });
				shard.Rotation = angle;
				shard.Additive = 0.6f;
				renderer.DrawSprite(shard);
			}
			break;
		}
		case AbilityId::Bolt:
		case AbilityId::ChainLightning:
		case AbilityId::ArcLightning:
		{
			const float scale = ability == AbilityId::Bolt ? 1.0f : 1.35f;
			DrawGlow(renderer, center, 0.42f * scale, { 0.1f, 0.15f, 0.45f });
			const uint32_t frame = uint32_t(time * 30.0f);
			for (uint32_t i = 0; i < 3; i++)
			{
				ParticleSprite arc = Sprite(ParticleShape::Lightning, center, 0.5f * scale, { 0.9f, 1.15f, 2.0f });
				arc.Rotation = Unit(Mix(seed + i * 977u, frame)) * 6.2832f;
				arc.Seed = Mix(seed * 31u + i, frame);
				renderer.DrawSprite(arc);
			}
			ParticleSprite core = Sprite(ParticleShape::Orb, center, 0.13f * scale, { 1.2f, 1.4f, 2.1f });
			core.Seed = seed;
			renderer.DrawSprite(core);
			if (marked)
				DrawGlow(renderer, center, 0.1f * scale, heart);
			break;
		}
		case AbilityId::IceSpear:
		case AbilityId::ShardBurst:
		case AbilityId::Frostbite:
		{
			const float length = ability == AbilityId::ShardBurst ? 0.3f : 0.42f;
			DrawGlow(renderer, center, 0.4f, { 0.25f, 0.5f, 1.0f });
			ParticleSprite body = Stretched(ParticleShape::Shard, center + forward * length, center - forward * length, 0.1f, { 1.2f, 1.8f, 2.6f });
			body.Additive = 0.45f;
			body.Emission = 0.7f;
			body.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			renderer.DrawSprite(body);
			DrawStar(renderer, center + forward * (length + 0.05f), 0.16f, { 2.0f, 2.6f, 3.4f });
			if (marked)
				DrawGlow(renderer, center, 0.12f, heart);
			break;
		}
		case AbilityId::TomeVolley:
		{
			DrawGlow(renderer, center, 0.5f, { 0.15f, 0.08f, 0.32f });
			ParticleSprite rune = Sprite(ParticleShape::Rune, center, 0.44f, { 0.75f, 0.42f, 1.35f });
			rune.Params = glm::vec4(-1.0f, 1.0f, 2.5f, 0.44f);
			rune.Seed = seed;
			renderer.DrawSprite(rune);
			DrawGlow(renderer, center, 0.06f, { 0.8f, 0.65f, 1.1f });
			break;
		}
		case AbilityId::Spit:
		{
			DrawGlow(renderer, center, 0.42f, { 0.3f, 0.8f, 0.12f });
			renderer.DrawSprite(Stretched(ParticleShape::Beam, center - forward * 0.45f, center, 0.09f, { 0.6f, 1.6f, 0.25f }));
			ParticleSprite blob = Sprite(ParticleShape::Orb, center, 0.2f, { 0.8f, 2.0f, 0.3f });
			blob.Additive = 0.55f;
			blob.Seed = seed;
			renderer.DrawSprite(blob);
			break;
		}
		case AbilityId::InkSpiral:
		{
			DrawGlow(renderer, center, 0.45f, { 0.12f, 0.03f, 0.2f });
			ParticleSprite ink = Sprite(ParticleShape::Orb, center, 0.27f, { 0.18f, 0.04f, 0.26f });
			ink.Additive = 0.1f;
			ink.Emission = 0.2f;
			ink.Seed = seed;
			renderer.DrawSprite(ink);
			renderer.DrawSprite(RingSprite(center, 0.255f, 0.036f, { 0.6f, 0.22f, 0.95f }));
			break;
		}
		case AbilityId::PiercingShot:
			DrawStreak(renderer, center - forward * 1.1f, center, 0.07f, { 0.6f, 1.8f, 0.5f });
			DrawGlow(renderer, center, 0.25f, { 0.4f, 1.2f, 0.35f });
			break;
		case AbilityId::SplittingBolt:
		{
			DrawGlow(renderer, center, 0.75f, { 0.45f, 0.06f, 0.16f });
			renderer.DrawSprite(Stretched(ParticleShape::Beam, center - forward * 0.9f, center, 0.26f, { 1.05f, 0.2f, 0.4f }));
			ParticleSprite core = Sprite(ParticleShape::Orb, center, 0.32f, { 1.3f, 0.28f, 0.52f });
			core.Seed = seed;
			renderer.DrawSprite(core);
			for (uint32_t i = 0; i < 3; i++)
			{
				const float angle = time * 5.5f + float(i) * 2.0944f;
				DrawStar(renderer, center + Heading(angle) * 0.42f + Up * (0.08f * std::sin(time * 6.0f + float(i) * 2.0f)), 0.09f, { 1.0f, 0.28f, 0.52f }, angle);
			}
			DrawGlow(renderer, center, marked ? 0.12f : 0.09f, marked ? heart : glm::vec3(1.2f, 0.8f, 0.9f));
			break;
		}
		case AbilityId::BoltShard:
		{
			DrawGlow(renderer, center, 0.4f, { 0.42f, 0.05f, 0.14f });
			ParticleSprite body = Stretched(ParticleShape::Shard, center + forward * 0.3f, center - forward * 0.3f, 0.09f, { 2.2f, 0.5f, 0.9f });
			body.Additive = 0.5f;
			body.Emission = 0.8f;
			body.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			renderer.DrawSprite(body);
			DrawStar(renderer, center + forward * 0.34f, 0.13f, { 2.6f, 1.1f, 1.5f });
			break;
		}
		case AbilityId::ChainHook:
		{
			DrawGlow(renderer, center, 0.5f, { 0.32f, 0.18f, 0.07f });
			ParticleSprite tip = Stretched(ParticleShape::Shard, center + forward * 0.36f, center - forward * 0.14f, 0.12f, { 1.7f, 1.25f, 0.8f });
			tip.Additive = 0.25f;
			tip.Emission = 0.45f;
			tip.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			renderer.DrawSprite(tip);
			for (const float side : { -1.0f, 1.0f })
			{
				const glm::vec3 across = Heading(facing + 1.5708f * side);
				ParticleSprite barb = Stretched(ParticleShape::Shard, center - forward * 0.06f + across * 0.18f, center + forward * 0.12f, 0.06f, { 1.5f, 1.05f, 0.65f });
				barb.Additive = 0.25f;
				barb.Emission = 0.45f;
				barb.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
				renderer.DrawSprite(barb);
			}
			DrawStar(renderer, center + forward * 0.38f, 0.15f, marked ? heart : glm::vec3(2.3f, 2.0f, 1.5f), time * 4.0f);
			break;
		}
		case AbilityId::Arrow:
		case AbilityId::Volley:
		case AbilityId::Quarrel:
			break;
		default:
		{
			DrawGlow(renderer, center, 0.45f, color * 0.6f);
			ParticleSprite core = Sprite(ParticleShape::Orb, center, 0.18f, color * 2.5f);
			core.Seed = seed;
			renderer.DrawSprite(core);
			break;
		}
		}
	}

	void DrawChain(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, float sag, float glow)
	{
		constexpr float LinkSpacing = 0.2f;
		const float length = glm::length(to - from);
		const uint32_t links = std::max(2u, uint32_t(length / LinkSpacing));
		const auto at = [&](float t) { return glm::mix(from, to, t) - Up * (sag * 4.0f * t * (1.0f - t)); };
		for (uint32_t i = 0; i < links; i++)
		{
			const float t0 = float(i) / float(links);
			const float t1 = float(i + 1) / float(links);
			const glm::vec3 a = at(t0);
			const glm::vec3 b = at(t1);
			ParticleSprite link = (i % 2 == 0) ? RingSprite((a + b) * 0.5f, 0.065f, 0.024f, { 0.36f, 0.27f, 0.17f }) : Stretched(ParticleShape::Beam, a, b, 0.045f, { 0.3f, 0.22f, 0.14f });
			link.Additive = 0.0f;
			link.Emission = 0.12f + 0.3f * glow;
			link.Softness = 0.05f;
			renderer.DrawSprite(link);
			if (i % 4 == 1)
				DrawGlow(renderer, (a + b) * 0.5f + Up * 0.02f, 0.07f, glm::vec3(0.9f, 0.7f, 0.45f) * (0.35f + 1.2f * glow), 0.1f);
		}
	}

	void DrawBolt(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, uint32_t seed, float jitter, float width, const glm::vec3& color, float emission)
	{
		const auto beam = [&](const glm::vec3& a, const glm::vec3& b, float size, const glm::vec3& tint)
		{
			ParticleSprite sprite = Stretched(ParticleShape::Beam, a, b, size, tint);
			sprite.Emission = emission;
			renderer.DrawSprite(sprite);
		};
		const glm::vec3 span = to - from;
		const float length = glm::length(span);
		if (length < 1e-3f)
			return;
		const glm::vec3 axis = span / length;
		const glm::vec3 side = glm::normalize(std::abs(axis.y) > 0.9f ? glm::cross(axis, glm::vec3(1.0f, 0.0f, 0.0f)) : glm::cross(axis, Up));
		const glm::vec3 lift = glm::cross(side, axis);
		const uint32_t segments = std::clamp(uint32_t(length / 0.6f), 3u, 14u);
		glm::vec3 previous = from;
		for (uint32_t i = 1; i <= segments; i++)
		{
			const float t = float(i) / float(segments);
			const float swing = i == segments ? 0.0f : jitter * std::sin(t * 3.1416f);
			const glm::vec3 point = from + span * t + side * ((Unit(Mix(seed, i * 2u)) - 0.5f) * 2.0f * swing) + lift * ((Unit(Mix(seed, i * 2u + 1u)) - 0.5f) * swing);
			beam(previous, point, width * 2.2f, color * 0.2f);
			beam(previous, point, width, color);
			if (i % 3 == 1)
			{
				const glm::vec3 fork = point + side * ((Unit(Mix(seed, i * 7u)) - 0.5f) * jitter * 2.0f) - axis * (0.15f * length / float(segments));
				beam(point, fork, width * 0.6f, color * 0.7f);
			}
			previous = point;
		}
	}
}
