#include "EmberIslandArt.h"
#include "Rendering/EmberSun.h"
#include "Rendering/LevelRenderer.h"
#include "Rendering/ModelLibrary.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/Sun.h"
#include "Rendering/WorldRenderer.h"
#include <GameShared/Content/EmberIsland.h>
#include <GameShared/Content/Props.h>
#include <GameShared/World/Level.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace Game
{
	namespace
	{
		constexpr std::string_view HeartPrefix = "EmberHeart/";
		constexpr std::string_view FlatParts[] = { "Slabs", "Bed", "Brass", "Inlay", "Lights", "Glass" };
		constexpr float ViewSlope = 0.65f;
		constexpr float GlassBottom = -0.3f;
		constexpr glm::vec3 StarGlow{ 1.0f, 0.6f, 0.26f };
		constexpr float StarLightRadius = 12.0f;
		constexpr float StarLightGain = 42.0f;
		constexpr float FillRadius = 7.5f;
		constexpr float FillReach = 4.6f;
		constexpr float FillGain = 20.0f;
		constexpr int FillLights = 6;
		constexpr float DeepGain = 14.0f;
		constexpr float PoolHeight = 0.9f;
		constexpr float PoolRadius = 7.0f;
		constexpr float PoolGain = 2.4f;
		constexpr float AnchorGain = 4.0f;
		constexpr float ImpactLightRadius = 4.5f;
		constexpr float ImpactGain = 30.0f;
		constexpr float CullRadius = 10.0f;

		constexpr std::string_view DriftPrefix = "Drift";
		constexpr std::string_view MonolithPrefix = "Monolith";
		constexpr std::string_view SpoutPrefix = "Spout";
		constexpr std::string_view ShadowlessParts[] = { "Drift", "Geode", "Fringe", "Monolith", "Footing", "Spout" };
		constexpr float WallRuneLift = 0.22f;
		constexpr float WallDistance = 7.0f;
		constexpr float WallRuneHalf = 0.42f;
		constexpr float WallRuneChance = 0.5f;
		constexpr float WallWarnArc = 0.9f;
		constexpr float WallWarnLead = 2.4f;
		constexpr float SpoutLip = 0.38f;
		constexpr float SpoutTop = 0.17f;
		constexpr float FallLightGain = 7.0f;
		constexpr float FallLightRadius = 4.5f;
		constexpr float DriftDrainGlow = 1.5f;
		constexpr uint32_t RuneGlyphs = 8;
		constexpr float RuneHalf = 0.31f;
		constexpr float RuneLift = 0.02f;
		constexpr float RuneChance = 0.4f;
		constexpr float RuneCool = 2.5f;
		constexpr float RuneEmission = 1.0f;
		constexpr float RuneWaveSpeed = 6.5f;
		constexpr float RuneWaveWidth = 1.6f;
		constexpr float RuneBeatReveal = 0.22f;
		constexpr float RuneBeatShare = 0.35f;
		constexpr float RuneImpactReveal = 0.9f;
		constexpr float RuneWarnArc = 0.6f;
		constexpr float RuneWarnLead = 1.6f;
		constexpr float RuneWarnSweep = 0.6f;
		constexpr float RuneLightRadius = 1.6f;
		constexpr float RuneLightGain = 3.0f;
		constexpr int RuneEmbers = 5;

		uint32_t Mix(uint32_t a, uint32_t b)
		{
			uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
			h ^= h >> 16;
			h *= 0x85EBCA6Bu;
			h ^= h >> 13;
			h *= 0xC2B2AE35u;
			h ^= h >> 16;
			return h;
		}

		float Hash(uint32_t a, uint32_t b)
		{
			return float(Mix(a, b) & 0xFFFFFFu) / float(0x1000000u);
		}

		float Smooth(float low, float high, float x)
		{
			const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		struct RuneState
		{
			float Progress = -1.0f;
			float Cooling = 10.0f;
			float Heat = 0.0f;

			void Merge(const RuneState& other)
			{
				Progress = std::max(Progress, other.Progress);
				Cooling = std::min(Cooling, other.Cooling);
				Heat = std::max(Heat, other.Heat);
			}
		};

		RuneState RuneTimeline(float t, float write, float hold)
		{
			if (t < 0.0f)
				return {};
			const float cooling = std::max(t - write - hold, 0.0f) / RuneCool;
			if (cooling > 2.0f)
				return {};
			return RuneState{
				.Progress = t / write,
				.Cooling = cooling,
				.Heat = Smooth(0.0f, 0.3f, t / write) * (1.0f - Smooth(0.0f, 1.0f, cooling)),
			};
		}

		glm::mat4 Turn(float yaw)
		{
			const float c = std::cos(yaw);
			const float s = std::sin(yaw);
			glm::mat4 m(1.0f);
			m[0] = glm::vec4(c, 0.0f, s, 0.0f);
			m[2] = glm::vec4(-s, 0.0f, c, 0.0f);
			return m;
		}
	}

	void EmberIslandArt::Clear()
	{
		m_Parts.clear();
		m_Drifts.clear();
		m_WallRunes.clear();
		m_Falls.clear();
	}

	void EmberIslandArt::Build(WorldRenderer& renderer, const Level& level)
	{
		Clear();
		if (level.Kind != MapKind::Hub)
			return;
		m_Place = glm::translate(glm::mat4(1.0f), glm::vec3(EmberCenter.x, 0.0f, EmberCenter.y)) * Turn(Pi);
		m_Heart = glm::vec3(EmberCenter.x, -EmberHeartDepth, EmberCenter.y);
		const std::vector<ModelMesh>& meshes = renderer.GetModels().GetData().Meshes;
		for (uint32_t mesh = 0; mesh < uint32_t(meshes.size()); mesh++)
		{
			const std::string_view name = meshes[mesh].Name;
			if (!name.starts_with(HeartPrefix))
				continue;
			const std::string_view part = name.substr(HeartPrefix.size());
			if (std::any_of(std::begin(FlatParts), std::end(FlatParts), [&](std::string_view flat) { return part.starts_with(flat); }))
				renderer.AddMeshFlags(mesh, MeshGround);
			renderer.AddMeshFlags(mesh, MeshDeep | MeshHeartbeat);
			if (std::any_of(std::begin(ShadowlessParts), std::end(ShadowlessParts), [&](std::string_view shadowless) { return part.starts_with(shadowless); }))
				renderer.AddMeshFlags(mesh, MeshGround);
			if (part.starts_with(DriftPrefix))
			{
				const int index = std::atoi(std::string(part.substr(DriftPrefix.size())).c_str());
				if (index >= 0 && index < EmberDrifts)
					m_Drifts.push_back({ index, mesh });
				continue;
			}
			glm::vec3 low(1e9f);
			glm::vec3 high(-1e9f);
			for (int corner = 0; corner < 8; corner++)
			{
				const glm::vec3 local((corner & 1) ? meshes[mesh].Max.x : meshes[mesh].Min.x, (corner & 2) ? meshes[mesh].Max.y : meshes[mesh].Min.y, (corner & 4) ? meshes[mesh].Max.z : meshes[mesh].Min.z);
				const glm::vec3 world(m_Place * glm::vec4(local, 1.0f));
				low = glm::min(low, world);
				high = glm::max(high, world);
			}
			m_Parts.push_back({ mesh, low, high });
			const glm::vec3 center = 0.5f * (low + high);
			const glm::vec2 radial(center.x - m_Heart.x, center.z - m_Heart.z);
			const float reach = glm::length(radial);
			if (reach < 1e-3f)
				continue;
			const glm::vec3 out(radial.x / reach, 0.0f, radial.y / reach);
			if (part.starts_with(MonolithPrefix))
				m_WallRunes.push_back({ m_Heart + out * (reach - 0.5f * EmberMonolithThickness) + glm::vec3(0.0f, center.y - m_Heart.y + WallRuneLift, 0.0f), out, std::atan2(out.z, out.x) });
			else if (part.starts_with(SpoutPrefix) && m_Falls.size() < size_t(EmberFallSlots))
				m_Falls.push_back(glm::vec4(center - out * SpoutLip + glm::vec3(0.0f, SpoutTop, 0.0f), m_Falls.empty() ? 1.0f : 0.7f));
		}
		std::sort(m_Falls.begin(), m_Falls.end(), [&](const glm::vec4& a, const glm::vec4& b) { return a.z > b.z; });
		for (size_t k = 0; k < m_Falls.size(); k++)
			m_Falls[k].w = k == 0 ? 1.0f : 0.7f;
		for (size_t k = 0; k < m_WallRunes.size(); k++)
			m_WallRunes[k].Glyph = uint8_t(Mix(uint32_t(k) + 8111u, 29u) % (2u * RuneGlyphs));
		for (int slot = 0; slot < RuneSlots; slot++)
		{
			uint32_t code = Mix(uint32_t(slot) + 4099u, 17u) % (2u * RuneGlyphs);
			if (slot > 0 && code % RuneGlyphs == m_Glyphs[size_t(slot - 1)] % RuneGlyphs)
				code = (code + 1u) % (2u * RuneGlyphs);
			m_Glyphs[size_t(slot)] = uint8_t(code);
		}
	}

	void EmberIslandArt::DrawRunes(WorldRenderer& renderer, float time, const EmberSunPulse& pulse) const
	{
		const float radius = 0.5f * (EmberRuneInner + EmberRuneOuter);
		const float spacing = 2.0f * Pi / float(RuneSlots);
		const float tileDistance = std::sqrt(radius * radius + EmberHeartDepth * EmberHeartDepth);
		const float beatFront = pulse.BeatAge >= 0.0f ? (pulse.BeatAge * RuneWaveSpeed - tileDistance) / RuneWaveWidth : 1e3f;
		const float beatReveal = RuneBeatReveal * std::exp(-beatFront * beatFront) * std::exp(-std::max(pulse.BeatAge, 0.0f) * 0.35f);
		const float impactAzimuth = std::atan2(pulse.Impact.z - m_Heart.z, pulse.Impact.x - m_Heart.x);
		const float impactGlow = pulse.ImpactAge >= 0.0f ? pulse.ImpactStrength * std::exp(-pulse.ImpactAge * 2.5f) : 0.0f;
		std::array<EmberEruptionPlan, 3 * EmberEruptionLanes> plans;
		for (int lane = 0; lane < EmberEruptionLanes; lane++)
			for (int offset = -1; offset <= 1; offset++)
				plans[size_t(lane * 3 + offset + 1)] = PlanEmberEruption(time, lane, offset);
		SunRenderer& sun = renderer.GetSun();
		for (int slot = 0; slot < RuneSlots; slot++)
		{
			const float angle = (float(slot) + 0.5f) * spacing;
			RuneState state;
			const float period = 12.0f + 8.0f * Hash(uint32_t(slot), 1u);
			const float shifted = time + period * Hash(uint32_t(slot), 2u);
			const float cycle = std::floor(shifted / period);
			const uint32_t key = Mix(uint32_t(slot) + 77u, uint32_t(int32_t(cycle) + 100000));
			if (Hash(key, 3u) < RuneChance)
				state.Merge(RuneTimeline(shifted - cycle * period - 0.5f * Hash(key, 4u), 1.3f + 0.7f * Hash(key, 5u), 1.2f + 1.6f * Hash(key, 6u)));
			for (const EmberEruptionPlan& plan : plans)
			{
				const float delta = std::abs(std::remainder(angle - std::atan2(plan.Site.z, plan.Site.x), 2.0f * Pi));
				if (delta < RuneWarnArc)
					state.Merge(RuneTimeline(time - (plan.Start - RuneWarnLead + RuneWarnSweep * delta / RuneWarnArc), 0.8f, 1.4f + 1.2f * plan.Power));
			}
			float reveal = Hash(uint32_t(slot) + 911u, uint32_t(int32_t(std::floor((time - pulse.BeatAge) * 4.0f)) + 100000)) < RuneBeatShare ? beatReveal : 0.0f;
			if (impactGlow > 0.0f)
				reveal += RuneImpactReveal * impactGlow * std::pow(std::max(std::cos(angle - impactAzimuth), 0.0f), 4.0f);
			const glm::vec3 center(EmberCenter.x + std::cos(angle) * radius, RuneLift, EmberCenter.y + std::sin(angle) * radius);
			renderer.DrawDecal(GroundDecal{
				.Center = center,
				.Yaw = std::atan2(std::cos(angle), -std::sin(angle)),
				.Extent = glm::vec2(RuneHalf),
				.Depth = 0.3f,
				.Shape = DecalShape::HeartRune,
				.Color = glm::vec4(1.0f, 1.0f, 1.0f, RuneEmission),
				.Tint = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
				.Roughness = -1.0f,
				.Params = glm::vec4(state.Progress, state.Cooling, reveal, float(m_Glyphs[size_t(slot)])),
				.Seed = uint32_t(slot) * 7919u + 13u,
			});
			Smoulder(renderer, sun, time, center, glm::vec3(1.0f, 0.0f, 0.0f) * RuneHalf, glm::vec3(0.0f, 0.0f, 1.0f) * RuneHalf, center + glm::vec3(0.0f, 0.3f, 0.0f), state.Heat, uint32_t(slot) * 31u);
		}
		const float wallReveal = RuneBeatReveal * std::exp(-std::pow((pulse.BeatAge * RuneWaveSpeed - WallDistance) / RuneWaveWidth, 2.0f)) * std::exp(-std::max(pulse.BeatAge, 0.0f) * 0.35f) * (pulse.BeatAge >= 0.0f ? 1.0f : 0.0f);
		for (size_t index = 0; index < m_WallRunes.size(); index++)
		{
			const WallRune& rune = m_WallRunes[index];
			const uint32_t slot = uint32_t(index) + 1000u;
			RuneState state;
			const float period = 14.0f + 8.0f * Hash(slot, 1u);
			const float shifted = time + period * Hash(slot, 2u);
			const float cycle = std::floor(shifted / period);
			const uint32_t key = Mix(slot + 77u, uint32_t(int32_t(cycle) + 100000));
			if (Hash(key, 3u) < WallRuneChance)
				state.Merge(RuneTimeline(shifted - cycle * period - 0.5f * Hash(key, 4u), 1.8f + 0.8f * Hash(key, 5u), 2.0f + 2.0f * Hash(key, 6u)));
			for (const EmberEruptionPlan& plan : plans)
			{
				const float delta = std::abs(std::remainder(rune.Azimuth - std::atan2(plan.Site.z, plan.Site.x), 2.0f * Pi));
				if (delta < WallWarnArc)
					state.Merge(RuneTimeline(time - (plan.Start - WallWarnLead + RuneWarnSweep * delta / WallWarnArc), 1.0f, 1.8f + 1.2f * plan.Power));
			}
			float reveal = 0.6f * wallReveal;
			if (impactGlow > 0.0f)
				reveal += RuneImpactReveal * impactGlow * std::pow(std::max(std::cos(rune.Azimuth - impactAzimuth), 0.0f), 4.0f);
			renderer.DrawDecal(GroundDecal{
				.Center = rune.Center,
				.Yaw = std::atan2(std::cos(rune.Azimuth), -std::sin(rune.Azimuth)),
				.Extent = glm::vec2(WallRuneHalf, 0.35f),
				.Depth = WallRuneHalf,
				.Shape = DecalShape::HeartRune,
				.Color = glm::vec4(1.0f, 1.0f, 1.0f, RuneEmission),
				.Tint = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
				.Roughness = -1.0f,
				.Params = glm::vec4(state.Progress, state.Cooling, reveal, float(rune.Glyph)),
				.Seed = slot * 7919u + 13u,
				.Wall = true,
			});
			const glm::vec3 tangent(-rune.Out.z, 0.0f, rune.Out.x);
			Smoulder(renderer, sun, time, rune.Center - rune.Out * 0.04f, tangent * WallRuneHalf, glm::vec3(0.0f, WallRuneHalf, 0.0f), rune.Center - rune.Out * 0.6f, state.Heat, slot * 31u);
		}
	}

	void EmberIslandArt::Smoulder(WorldRenderer& renderer, SunRenderer& sun, float time, const glm::vec3& center, const glm::vec3& spanA, const glm::vec3& spanB, const glm::vec3& lamp, float heat, uint32_t seed) const
	{
		if (heat <= 0.05f)
			return;
		renderer.DrawLight({ lamp, RuneLightRadius, glm::vec3(1.0f, 0.42f, 0.1f) * (RuneLightGain * heat), false });
		for (int k = 0; k < RuneEmbers; k++)
		{
			const uint32_t ember = seed + uint32_t(k);
			const float life = 1.4f + 1.0f * Hash(ember, 7u);
			const float local = time + life * Hash(ember, 8u);
			const float t = local - std::floor(local / life) * life;
			const uint32_t emberKey = Mix(ember, uint32_t(int32_t(std::floor(local / life)) + 100000));
			const float spin = 2.0f * Pi * Hash(emberKey, 1u);
			const float reach = 0.75f * std::sqrt(Hash(emberKey, 2u));
			const float rise = 0.25f + 0.35f * Hash(emberKey, 3u);
			const glm::vec3 origin = center + (spanA * std::cos(spin) + spanB * std::sin(spin)) * reach;
			const auto at = [&](float s) { return origin + glm::vec3(0.06f * std::sin(s * 3.1f + spin), rise * s, 0.06f * std::cos(s * 2.7f + spin)); };
			const float alpha = heat * Smooth(0.0f, 0.2f, t) * (1.0f - Smooth(life * 0.5f, life, t));
			if (alpha <= 0.01f)
				continue;
			const std::array<SunKnot, 2> knots{ SunKnot{ at(std::max(t - 0.12f, 0.0f)), 0.012f, 0.5f, alpha }, SunKnot{ at(t), 0.02f, 0.75f, alpha } };
			sun.Draw(knots, SunRibbon{ .Look = SunRibbonLook::Spark, .Intensity = 1.0f, .Opacity = 0.0f, .Flow = 1.0f, .Seed = emberKey });
		}
	}

	void EmberIslandArt::Draw(WorldRenderer& renderer, const LevelView& view) const
	{
		if (m_Parts.empty())
			return;
		const glm::vec2 heart(m_Heart.x, m_Heart.z);
		const bool heartVisible = !(heart.x + CullRadius < view.Min.x || heart.x - CullRadius > view.Max.x || heart.y + CullRadius < view.Min.y - ViewSlope * EmberHeartDepth || heart.y - CullRadius > view.Max.y);
		EmberSunPulse pulse;
		std::array<glm::mat4, EmberDrifts> drifts{};
		if (heartVisible)
		{
			EmberSunLook look{ .Center = m_Heart, .Radius = EmberHeartRadius, .Ceiling = GlassBottom, .Time = view.Time };
			for (int k = 0; k < EmberDrifts; k++)
			{
				drifts[size_t(k)] = EmberDriftTransform(m_Heart, k, view.Time);
				look.Sources[size_t(k)] = glm::vec3(drifts[size_t(k)][3]);
			}
			look.SourceCount = m_Drifts.empty() ? 0 : EmberDrifts;
			for (const glm::vec4& fall : m_Falls)
				look.Falls[size_t(look.FallCount++)] = fall;
			pulse = DrawEmberSun(renderer.GetSun(), look);
		}
		const float impactGlow = pulse.ImpactAge >= 0.0f ? pulse.ImpactStrength * std::exp(-pulse.ImpactAge * 2.0f) : 0.0f;
		const glm::vec4 beat(pulse.BeatAge, pulse.Swell, std::min(pulse.Flash + impactGlow, 1.5f), 0.0f);
		for (const Part& part : m_Parts)
		{
			const float south = part.Min.z + ViewSlope * std::min(part.Min.y, 0.0f);
			const float north = part.Max.z + ViewSlope * std::max(part.Max.y, 0.0f);
			if (part.Max.x < view.Min.x || part.Min.x > view.Max.x || north < view.Min.y || south > view.Max.y)
				continue;
			renderer.DrawModel(part.Mesh, m_Place, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, 0.0f, beat);
		}
		if (!heartVisible)
			return;
		for (const auto& [index, mesh] : m_Drifts)
		{
			const float drain = pulse.Drain[size_t(index)];
			const float distance = glm::length(glm::vec3(drifts[size_t(index)][3]) - m_Heart);
			renderer.DrawModel(mesh, drifts[size_t(index)], NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, 0.0f, glm::vec4(beat.x, beat.y, beat.z + DriftDrainGlow * drain, distance));
		}
		DrawRunes(renderer, view.Time, pulse);
		for (const glm::vec4& fall : m_Falls)
		{
			const glm::vec3 source(fall);
			const glm::vec3 inward = glm::normalize(glm::vec3(m_Heart.x - source.x, 0.0f, m_Heart.z - source.z));
			const float flicker = 0.85f + 0.15f * std::sin(view.Time * 3.7f + source.x);
			renderer.DrawLight({ source + inward * 0.9f - glm::vec3(0.0f, 1.6f, 0.0f), FallLightRadius, glm::vec3(1.0f, 0.45f, 0.12f) * (FallLightGain * fall.w * flicker), false });
			renderer.DrawLight({ source + inward * 1.5f - glm::vec3(0.0f, 4.5f, 0.0f), FallLightRadius, glm::vec3(1.0f, 0.38f, 0.1f) * (FallLightGain * 0.7f * fall.w * flicker), false });
		}
		const float strength = pulse.Flicker * (1.0f + 0.7f * pulse.Flash + 0.18f * pulse.Swell);
		if (pulse.ImpactAge >= 0.0f)
		{
			const float glow = pulse.ImpactStrength * std::exp(-pulse.ImpactAge * 3.5f);
			renderer.DrawLight({ m_Heart + (pulse.Impact - m_Heart) * 1.08f, ImpactLightRadius, glm::vec3(1.0f, 0.72f, 0.4f) * (ImpactGain * glow), false });
			if (pulse.ImpactAge < 0.5f)
				DrawAirShockwave(renderer, pulse.Impact, 0.2f + 1.8f * pulse.ImpactAge, 0.22f, 0.05f * pulse.ImpactStrength * (1.0f - pulse.ImpactAge / 0.5f));
		}
		if (pulse.BeatAge >= 0.0f && pulse.BeatAge < 0.7f)
			DrawAirShockwave(renderer, m_Heart, pulse.Radius * (1.05f + 1.6f * pulse.BeatAge), 0.3f, 0.035f * (1.0f - pulse.BeatAge / 0.7f));
		DrawHeatHaze(renderer, m_Heart, pulse.Radius * 1.9f, 0.016f, 1.6f / pulse.Radius, 1.6f, 0xE3B5u);
		const glm::vec3 color = glm::mix(StarGlow, glm::vec3(1.0f, 0.85f, 0.6f), 0.4f * pulse.Flash);
		renderer.DrawLight({ m_Heart, StarLightRadius, color * (StarLightGain * strength), false });
		for (int i = 0; i < FillLights; i++)
		{
			const float angle = (float(i) + 0.5f) * 2.0f * Pi / float(FillLights);
			const float lift = (i % 2 == 0 ? 1.2f : -1.4f);
			renderer.DrawLight({ m_Heart + glm::vec3(std::cos(angle) * FillReach, lift, std::sin(angle) * FillReach), FillRadius, color * (FillGain * strength), false });
		}
		renderer.DrawLight({ m_Heart + glm::vec3(0.0f, -5.0f, 0.0f), FillRadius + 1.5f, StarGlow * (DeepGain * strength), false });
		renderer.DrawLight({ glm::vec3(m_Heart.x, PoolHeight, m_Heart.z), PoolRadius, StarGlow * (PoolGain * strength) });
		for (const float azimuth : EmberChainAzimuths)
		{
			const float a = azimuth * Pi / 180.0f;
			const float reach = EmberCavernRadii[2] - 0.9f;
			renderer.DrawLight({ glm::vec3(m_Heart.x + std::cos(a) * reach, -EmberChainDepth, m_Heart.z + std::sin(a) * reach), 1.8f, glm::vec3(1.0f, 0.45f, 0.14f) * (AnchorGain * pulse.Flicker), false });
		}
	}
}
