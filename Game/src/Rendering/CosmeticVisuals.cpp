#include "CosmeticVisuals.h"
#include "Rendering/Effects.h"
#include "Rendering/ModelLibrary.h"
#include "Rendering/Models.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/Trails.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace Game
{
	namespace
	{
		constexpr glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
		constexpr float Tau = 6.2831853f;

		constexpr float StrideLength = 0.6f;
		constexpr float StrideMinSpeed = 0.6f;

		constexpr glm::vec3 EmberLight{ 1.0f, 0.42f, 0.12f };
		constexpr glm::vec3 AstralViolet{ 0.55f, 0.32f, 1.1f };
		constexpr glm::vec3 AstralCyan{ 0.3f, 0.75f, 1.2f };
		constexpr glm::vec3 CrownGlow{ 0.52f, 0.4f, 1.0f };
		constexpr float SigilLife = 1.6f;
		constexpr float SigilSize = 0.2f;
		constexpr float SigilDesign = 0.7f;

		constexpr glm::vec3 FrostLight{ 0.42f, 0.72f, 1.15f };
		constexpr glm::vec3 FrostGlow{ 0.45f, 0.8f, 1.1f };
		constexpr float FrostSigilSize = 0.6f;
		constexpr float AuraBoost = 1.25f;

		constexpr float CrownLift = 0.5f;
		constexpr float CrownSize = 1.45f;
		constexpr float CrownFollow = 30.0f;
		constexpr float CrownMaxLag = 0.03f;
		constexpr float CrownLean = 0.035f;
		constexpr float CrownMaxLean = 0.16f;
		constexpr float CrownBob = 0.012f;
		constexpr float CrownPieceDrift = 0.006f;
		constexpr float CrownShardLift = 0.035f;
		constexpr float CrownLight = 0.6f;
		constexpr float CrownLightRadius = 1.4f;

		struct CrownStyle
		{
			glm::vec3 Glow;
			Fx Mist;
		};

		CrownStyle CrownStyleOf(Headpiece headpiece)
		{
			if (headpiece == Headpiece::FrostCrown)
				return { FrostGlow, Fx::AuraFrostCrownMist };
			return { CrownGlow, Fx::AuraCrownMist };
		}

		struct Orbit
		{
			float Radius;
			float Height;
			float Tilt;
			float Speed;
			float Phase;
			float Node;
		};

		constexpr std::array<Orbit, 3> AstralOrbits = { {
			{ 0.62f, 1.02f, 0.38f, 2.1f, 0.0f, 0.0f },
			{ 0.56f, 0.72f, -0.55f, -1.7f, 2.1f, 1.9f },
			{ 0.68f, 1.3f, 0.95f, 1.45f, 4.2f, 3.8f },
		} };

		glm::vec3 ModelDirection(float degrees)
		{
			const float angle = glm::radians(degrees);
			return { std::sin(angle), 0.0f, std::cos(angle) };
		}

		ParticleSprite GlowSprite(const glm::vec3& position, float size, const glm::vec3& color, ParticleShape shape = ParticleShape::Glow)
		{
			ParticleSprite sprite;
			sprite.Shape = shape;
			sprite.Position = position;
			sprite.Size = size;
			sprite.Color = glm::vec4(color, 1.0f);
			return sprite;
		}

		TrailStyle EmberRibbon()
		{
			TrailStyle style;
			style.Look = TrailLook::Flame;
			style.Head = { 1.3f, 0.5f, 0.14f };
			style.Tail = { 0.55f, 0.11f, 0.03f };
			style.Core = { 1.5f, 0.95f, 0.42f, 1.0f };
			style.Width = 0.085f;
			style.WidthCurve = { 1.0f, 0.85f, 0.5f, 0.0f };
			style.AlphaCurve = { 0.85f, 0.65f, 0.3f, 0.0f };
			style.Life = 0.3f;
			style.Emission = 0.35f * AuraBoost;
			style.Scroll = 1.5f;
			style.Taper = 0.2f;
			style.Spacing = 0.05f;
			return style;
		}

		TrailStyle AstralRibbon()
		{
			TrailStyle style;
			style.Look = TrailLook::Wisp;
			style.Head = AstralViolet * 1.1f;
			style.Tail = AstralCyan * 0.6f;
			style.Core = { 0.95f, 0.85f, 1.35f, 1.0f };
			style.Width = 0.06f;
			style.WidthCurve = { 1.0f, 0.7f, 0.4f, 0.0f };
			style.AlphaCurve = { 0.9f, 0.6f, 0.3f, 0.0f };
			style.Life = 0.7f;
			style.Emission = 0.35f * AuraBoost;
			style.Scroll = 0.6f;
			style.Taper = 0.08f;
			style.Spacing = 0.05f;
			return style;
		}

		TrailStyle FrostRibbon()
		{
			TrailStyle style;
			style.Look = TrailLook::Frost;
			style.Head = FrostLight;
			style.Tail = { 0.3f, 0.5f, 0.9f };
			style.Core = { 0.9f, 0.97f, 1.15f, 1.0f };
			style.Width = 0.07f;
			style.WidthCurve = { 1.0f, 0.75f, 0.4f, 0.0f };
			style.AlphaCurve = { 0.85f, 0.6f, 0.3f, 0.0f };
			style.Life = 0.4f;
			style.Emission = 0.3f * AuraBoost;
			style.Scroll = 0.8f;
			style.Taper = 0.15f;
			style.Spacing = 0.05f;
			return style;
		}

		uint64_t TrailKey(uint32_t wearer, uint32_t slot)
		{
			return uint64_t(wearer) | (uint64_t(40u + slot) << 40);
		}
	}

	CosmeticVisuals::CosmeticVisuals(const ModelLibrary& models)
	{
		const auto load = [&](Headpiece kind, const std::string& prefix, std::initializer_list<const char*> groups, const char* core)
		{
			CrownModel& model = m_Crowns[size_t(kind)];
			for (const char* group : groups)
				for (uint32_t i = 0;; i++)
				{
					const std::optional<uint32_t> mesh = models.FindMesh(prefix + group + std::to_string(i));
					if (!mesh)
						break;
					const ModelMesh& data = models.GetMesh(*mesh);
					const glm::vec3 center = (data.Min + data.Max) * 0.5f;
					model.Pieces.push_back({ *mesh, glm::degrees(std::atan2(center.x, center.z)), float(model.Pieces.size()) * 1.1f });
				}
			model.Core = models.FindMesh(prefix + core);
			if (model.Core)
			{
				const ModelMesh& data = models.GetMesh(*model.Core);
				model.CoreCenter = 0.5f * (data.Min.y + data.Max.y);
			}
		};
		load(Headpiece::ShatteredCrown, "Crown/Crown_", { "Blade", "Band" }, "Shard");
		load(Headpiece::FrostCrown, "FrostCrown/FrostCrown_", { "Spike", "Band" }, "Core");
	}

	void CosmeticVisuals::Draw(WorldRenderer& renderer, EffectSystem& effects, const CosmeticWearer& wearer, float time)
	{
		if (!wearer.Pose || !wearer.Rig || (wearer.Style.Glow == Aura::None && wearer.Style.Crown == Headpiece::None))
			return;
		Wearer& state = m_Wearers[wearer.Key];
		const bool fresh = state.Seen < 0.0f || time - state.Seen > 0.5f || glm::distance(state.Last, wearer.Base) > 3.0f;
		const float dt = fresh ? 0.0f : std::clamp(time - state.Seen, 0.0f, 0.1f);
		if (fresh)
		{
			state = {};
			state.Last = wearer.Base;
		}
		else if (dt > 0.0f)
		{
			const glm::vec3 velocity = (wearer.Base - state.Last) / dt;
			state.Velocity += (velocity - state.Velocity) * std::min(1.0f, dt * 8.0f);
		}
		state.Seen = time;
		if (wearer.Style.Glow == Aura::Ember)
			DrawEmber(renderer, effects, wearer, state, time, dt);
		else if (wearer.Style.Glow == Aura::Astral)
			DrawAstral(renderer, effects, wearer, state, time, dt);
		else if (wearer.Style.Glow == Aura::Frost)
			DrawFrost(renderer, effects, wearer, state, time, dt);
		if (wearer.Style.Crown != Headpiece::None)
			DrawCrown(renderer, effects, wearer, state, time, dt);
		state.Last = wearer.Base;
	}

	void CosmeticVisuals::Forget(float time)
	{
		std::erase_if(m_Wearers, [time](const auto& entry) { return time - entry.second.Seen > 2.0f; });
	}

	std::optional<glm::vec3> CosmeticVisuals::Step(const CosmeticWearer& wearer, Wearer& state) const
	{
		const glm::vec2 moved(wearer.Base.x - state.Last.x, wearer.Base.z - state.Last.z);
		const float speed = glm::length(glm::vec2(state.Velocity.x, state.Velocity.z));
		if (!wearer.Grounded || speed < StrideMinSpeed)
		{
			state.Stride = std::min(state.Stride, StrideLength * wearer.Scale * 0.5f);
			return std::nullopt;
		}
		state.Stride += glm::length(moved);
		if (state.Stride < StrideLength * wearer.Scale)
			return std::nullopt;
		state.Stride = 0.0f;
		const uint32_t side = state.Steps++ & 1u;
		glm::vec3 foot = wearer.Base;
		if (wearer.Rig->HasLegs)
			foot = wearer.Pose->JointPosition(wearer.Rig->Legs[side].Foot);
		return glm::vec3(foot.x, wearer.Base.y, foot.z);
	}

	void CosmeticVisuals::DrawEmber(WorldRenderer& renderer, EffectSystem& effects, const CosmeticWearer& wearer, Wearer& state, float time, float dt) const
	{
		const CharacterPose& pose = *wearer.Pose;
		const CharacterRig& rig = *wearer.Rig;
		const float s = wearer.Scale;
		const float flicker = 0.85f + 0.1f * std::sin(time * 13.0f + float(wearer.Key)) + 0.05f * std::sin(time * 29.0f);
		const std::array<glm::vec3, 2> hands = { pose.JointPosition(rig.RightHand), pose.JointPosition(rig.LeftHand) };
		for (uint32_t i = 0; i < 2; i++)
		{
			effects.Trail(Fx::AuraEmberHands, TrailKey(wearer.Key, 10u + i), hands[i], dt, s);
			renderer.DrawTrail(TrailKey(wearer.Key, i), hands[i], EmberRibbon());
			renderer.DrawSprite(GlowSprite(hands[i], 0.16f * s, EmberLight * (0.35f * AuraBoost * flicker)));
		}
		const glm::vec3 chest = rig.Chest ? pose.JointPosition(*rig.Chest) : wearer.Base + Up * (1.2f * s);
		effects.Trail(Fx::AuraEmberRise, TrailKey(wearer.Key, 12u), chest - Heading(wearer.Facing) * (0.12f * s), dt, s);
		const glm::vec3 collar = rig.Neck ? pose.JointPosition(*rig.Neck) - Up * (0.1f * s) : chest + Up * (0.12f * s);
		const glm::vec3 side = glm::cross(Heading(wearer.Facing), Up);
		for (uint32_t i = 0; i < 2; i++)
			effects.Trail(Fx::AuraEmberMantle, TrailKey(wearer.Key, 14u + i), collar + side * ((i == 0 ? 0.25f : -0.25f) * s), dt, s);
		renderer.DrawLight({ .Position = chest, .Radius = 2.2f, .Color = EmberLight * (0.5f * AuraBoost * flicker) });
		if (const std::optional<glm::vec3> foot = Step(wearer, state))
		{
			GroundDecal print;
			print.Center = *foot + Heading(wearer.Facing) * (0.04f * s);
			print.Yaw = wearer.Facing;
			print.Extent = glm::vec2(0.17f, 0.1f) * s;
			print.Depth = 0.35f;
			print.Shape = DecalShape::Scorch;
			print.Color = glm::vec4(2.2f, 0.78f, 0.2f, 1.0f);
			print.Tint = glm::vec4(0.03f, 0.022f, 0.018f, 0.7f);
			print.Roughness = 0.95f;
			print.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			print.Seed = wearer.Key * 977u + state.Steps;
			effects.Mark(print, 2.6f, 0.03f, 0.8f, 1.0f, 0.12f, 1.3f);
			effects.Burst(Fx::LavaSplash, *foot, Up, 0.25f * s);
		}
	}

	void CosmeticVisuals::DrawAstral(WorldRenderer& renderer, EffectSystem& effects, const CosmeticWearer& wearer, Wearer& state, float time, float dt) const
	{
		const float s = wearer.Scale;
		for (uint32_t i = 0; i < AstralOrbits.size(); i++)
		{
			const Orbit& orbit = AstralOrbits[i];
			const float node = orbit.Node + time * 0.35f;
			const glm::vec3 across(std::cos(node), 0.0f, std::sin(node));
			const glm::vec3 level(-std::sin(node), 0.0f, std::cos(node));
			const glm::vec3 lifted = level * std::cos(orbit.Tilt) + Up * std::sin(orbit.Tilt);
			const float angle = orbit.Phase + time * orbit.Speed;
			const glm::vec3 center = wearer.Base + Up * (orbit.Height * s);
			const glm::vec3 star = center + (across * std::cos(angle) + lifted * std::sin(angle)) * (orbit.Radius * s);
			const float twinkle = 0.85f + 0.15f * std::sin(time * 7.0f + float(i) * 2.3f);
			renderer.DrawTrail(TrailKey(wearer.Key, 2u + i), star, AstralRibbon());
			effects.Trail(Fx::AuraAstralDust, TrailKey(wearer.Key, 13u + i), star, dt, s);
			ParticleSprite halo = GlowSprite(star, 0.2f * s, AstralViolet * (0.32f * AuraBoost * twinkle));
			halo.Emission = 0.5f;
			renderer.DrawSprite(halo);
			ParticleSprite core = GlowSprite(star, 0.065f * s, glm::vec3(1.1f, 1.05f, 1.4f) * (AuraBoost * twinkle));
			core.Softness = 0.15f;
			renderer.DrawSprite(core);
			ParticleSprite glint = GlowSprite(star, 0.15f * s, glm::mix(AstralCyan, glm::vec3(1.0f), 0.4f) * (0.55f * AuraBoost * twinkle), ParticleShape::Star);
			glint.Rotation = time * 1.5f + float(i);
			glint.Emission = 0.4f;
			renderer.DrawSprite(glint);
		}
		renderer.DrawLight({ .Position = wearer.Base + Up * (1.0f * s), .Radius = 2.0f, .Color = AstralViolet * (0.4f * AuraBoost) });
		if (const std::optional<glm::vec3> foot = Step(wearer, state))
			state.Prints[state.Steps % state.Prints.size()] = { .Position = *foot + Up * 0.03f, .Born = time, .Seed = wearer.Key * 131u + state.Steps };
		for (const Footprint& print : state.Prints)
		{
			const float age = time - print.Born;
			if (age < 0.0f || age > SigilLife)
				continue;
			const float fade = 1.0f - glm::smoothstep(SigilLife * 0.45f, SigilLife, age);
			ParticleSprite sigil = GlowSprite(print.Position, SigilSize * s * (0.85f + 0.15f * std::min(age / 0.3f, 1.0f)), glm::mix(AstralViolet, AstralCyan, float(print.Seed & 1u) * 0.6f) * (0.9f * AuraBoost * fade), ParticleShape::Rune);
			sigil.Align = ParticleAlign::Ground;
			sigil.Rotation = float(print.Seed % 628u) * 0.01f;
			sigil.Params = glm::vec4(-1.0f, std::min(age / 0.35f, 1.0f), 0.6f, SigilDesign);
			sigil.Seed = print.Seed;
			sigil.Emission = 0.6f;
			renderer.DrawSprite(sigil);
		}
	}

	void CosmeticVisuals::DrawCrown(WorldRenderer& renderer, EffectSystem& effects, const CosmeticWearer& wearer, Wearer& state, float time, float dt) const
	{
		const float s = wearer.Scale;
		const glm::vec3 head = wearer.Pose->JointPosition(wearer.Rig->Head);
		const glm::vec3 target = head + Up * (CrownLift * s + CrownBob * std::sin(time * 1.6f + float(wearer.Key)));
		if (!state.Placed)
		{
			state.Crown = target;
			state.Placed = true;
		}
		else
			state.Crown += (target - state.Crown) * (1.0f - std::exp(-dt * CrownFollow));
		if (const glm::vec3 lag = target - state.Crown; glm::length(lag) > CrownMaxLag * s)
			state.Crown = target - glm::normalize(lag) * (CrownMaxLag * s);
		glm::mat4 frame = glm::translate(glm::mat4(1.0f), state.Crown);
		const glm::vec3 motion(state.Velocity.x, 0.0f, state.Velocity.z);
		if (const float speed = glm::length(motion); speed > 0.05f)
			frame = glm::rotate(frame, std::min(speed * CrownLean, CrownMaxLean), glm::normalize(glm::cross(Up, -motion)));
		frame = frame * ModelTransform(glm::vec3(0.0f), wearer.Facing + 0.05f * std::sin(time * 0.7f), s * CrownSize);
		const CrownModel& model = m_Crowns[size_t(wearer.Style.Crown)];
		const glm::vec4 tint(1.0f, 1.0f, 1.0f, 0.0f);
		for (const CrownPiece& piece : model.Pieces)
		{
			const float breathe = CrownPieceDrift * (1.0f + std::sin(time * 1.7f + piece.Phase));
			const glm::vec3 offset = ModelDirection(piece.Degrees) * breathe + Up * (0.5f * CrownPieceDrift * std::sin(time * 2.1f + piece.Phase * 1.3f));
			renderer.DrawModel(piece.Mesh, frame * glm::translate(glm::mat4(1.0f), offset), NoPalette, tint);
		}
		const float bob = CrownShardLift + 0.006f * std::sin(time * 2.4f);
		const glm::vec3 core = glm::vec3(frame * glm::vec4(0.0f, bob, 0.0f, 1.0f));
		if (model.Core)
		{
			const glm::mat4 spin = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, bob - model.CoreCenter, 0.0f)), time * 1.4f, Up);
			renderer.DrawModel(*model.Core, frame * spin, NoPalette, tint);
		}
		const CrownStyle style = CrownStyleOf(wearer.Style.Crown);
		const float pulse = 0.85f + 0.15f * std::sin(time * 2.4f);
		effects.Trail(style.Mist, TrailKey(wearer.Key, 16u), core, dt, s);
		renderer.DrawLight({ .Position = core + Up * 0.1f, .Radius = CrownLightRadius, .Color = style.Glow * (CrownLight * pulse) });
	}

	void CosmeticVisuals::DrawFrost(WorldRenderer& renderer, EffectSystem& effects, const CosmeticWearer& wearer, Wearer& state, float time, float dt) const
	{
		const CharacterPose& pose = *wearer.Pose;
		const CharacterRig& rig = *wearer.Rig;
		const float s = wearer.Scale;
		const float shimmer = 0.85f + 0.15f * std::sin(time * 3.1f + float(wearer.Key));
		const std::array<glm::vec3, 2> hands = { pose.JointPosition(rig.RightHand), pose.JointPosition(rig.LeftHand) };
		for (uint32_t i = 0; i < 2; i++)
		{
			effects.Trail(Fx::AuraFrostHands, TrailKey(wearer.Key, 20u + i), hands[i], dt, s);
			renderer.DrawTrail(TrailKey(wearer.Key, 5u + i), hands[i], FrostRibbon());
			renderer.DrawSprite(GlowSprite(hands[i], 0.12f * s, FrostLight * (0.25f * AuraBoost * shimmer)));
		}
		ParticleSprite sigil = GlowSprite(wearer.Base + Up * 0.03f, FrostSigilSize * s, FrostLight * (0.9f * AuraBoost * shimmer), ParticleShape::Snowflake);
		sigil.Align = ParticleAlign::Ground;
		sigil.Rotation = time * 0.15f;
		sigil.Emission = 0.5f;
		renderer.DrawSprite(sigil);
		effects.Trail(Fx::AuraFrostMist, TrailKey(wearer.Key, 22u), wearer.Base + Up * (0.05f * s), dt, s);
		effects.Trail(Fx::AuraFrostFlurry, TrailKey(wearer.Key, 23u), wearer.Base + Up * (1.9f * s), dt, s);
		const glm::vec3 chest = rig.Chest ? pose.JointPosition(*rig.Chest) : wearer.Base + Up * (1.2f * s);
		renderer.DrawLight({ .Position = chest, .Radius = 2.0f, .Color = FrostLight * (0.4f * AuraBoost * shimmer) });
		if (const std::optional<glm::vec3> foot = Step(wearer, state))
		{
			GroundDecal print;
			print.Center = *foot + Heading(wearer.Facing) * (0.04f * s);
			print.Yaw = wearer.Facing;
			print.Extent = glm::vec2(0.2f, 0.13f) * s;
			print.Depth = 0.35f;
			print.Shape = DecalShape::Frost;
			print.Color = glm::vec4(0.35f, 0.6f, 0.95f, 1.0f);
			print.Tint = glm::vec4(0.78f, 0.88f, 1.0f, 0.75f);
			print.Roughness = 0.3f;
			print.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			print.Seed = wearer.Key * 613u + state.Steps;
			effects.Mark(print, 3.0f, 0.08f, 0.6f, 1.0f, 0.2f, 1.2f);
		}
	}
}
