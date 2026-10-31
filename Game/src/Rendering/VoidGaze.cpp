#include "VoidGaze.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float Tau = 6.28318531f;
		constexpr glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
		constexpr glm::vec3 VoidBody{ 0.022f, 0.004f, 0.055f };
		constexpr glm::vec3 VoidWhite{ 1.25f, 1.05f, 1.75f };
		constexpr uint32_t TendrilCount = 6;
		constexpr uint32_t TendrilKnots = 28;
		constexpr uint32_t StreamCount = 7;
		constexpr uint32_t StreamKnots = 24;
		constexpr uint32_t MoteCount = 110;
		constexpr uint32_t StreakCount = 12;
		constexpr uint32_t ArcCount = 7;
		constexpr float MotePeriod = 1.1f;
		constexpr float ConsumeSeconds = 0.42f;

		float Clamp01(float x)
		{
			return std::clamp(x, 0.0f, 1.0f);
		}

		float Smooth(float a, float b, float x)
		{
			const float t = Clamp01((x - a) / (b - a));
			return t * t * (3.0f - 2.0f * t);
		}

		float EaseOut(float t)
		{
			t = Clamp01(t);
			return 1.0f - (1.0f - t) * (1.0f - t);
		}

		float EaseIn(float t)
		{
			t = Clamp01(t);
			return t * t;
		}

		float BackOut(float t, float overshoot)
		{
			t = Clamp01(t) - 1.0f;
			return 1.0f + t * t * ((overshoot + 1.0f) * t + overshoot);
		}

		uint32_t Hash(uint32_t x)
		{
			x ^= x >> 16;
			x *= 0x7FEB352Du;
			x ^= x >> 15;
			x *= 0x846CA68Bu;
			x ^= x >> 16;
			return x;
		}

		float Unit(uint32_t seed, uint32_t index)
		{
			return float(Hash(seed * 0x9E3779B9u + index * 0x85EBCA6Bu + 0x632BE5ABu) & 0xFFFFFFu) / 16777215.0f;
		}

		glm::vec3 Turn(const glm::vec3& v, const glm::vec3& axis, float angle)
		{
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			return v * c + glm::cross(axis, v) * s + axis * (glm::dot(axis, v) * (1.0f - c));
		}

		struct VoidState
		{
			float Pre = 0.0f;
			float Post = -1.0f;
			float Form = 0.0f;
			float Grow = 0.0f;
			float Open = 0.0f;
			float Seam = 0.0f;
			float Pupil = 0.25f;
			float Iris = 0.0f;
			float Flash = 0.0f;
			float Glow = 0.5f;
			float Point = 0.0f;
			float Collapse = 0.0f;
			float Consume = 0.0f;
			float Ring = 0.0f;
			float Spin = 0.0f;
			float Distort = 0.0f;
			float Swirl = 0.35f;
			float Blast = -1.0f;
		};

		float SuctionAt(float age, float pullAge, float tempo)
		{
			if (pullAge < 0.0f || age <= pullAge)
				return 0.45f * age;
			const float p = (age - pullAge) / tempo;
			return 0.45f * pullAge + 1.4f * p + 10.0f * p * p;
		}

		VoidState Evaluate(const VoidGazeLook& look)
		{
			VoidState state;
			const bool pulled = look.Pull >= 0.0f;
			const float pre = pulled && look.PullAge >= 0.0f ? std::min(look.Age, look.PullAge) : look.Age;
			const float p = pulled ? look.Pull / look.Tempo : -1.0f;
			state.Pre = pre;
			state.Post = p;
			state.Form = EaseOut(pre / 0.22f);
			state.Grow = BackOut((pre - 0.02f) / 0.14f, 1.4f);
			state.Open = BackOut((pre - VoidOpenStart) / (VoidOpenEnd - VoidOpenStart), 0.9f);
			state.Seam = Smooth(VoidSeamStart, VoidOpenStart + 0.01f, pre) * (1.0f - Smooth(VoidOpenStart + 0.02f, VoidOpenEnd, pre));
			state.Pupil = 0.2f + 0.24f * Smooth(VoidOpenEnd, 0.52f, pre) + 0.03f * std::sin(look.Time * 3.1f);
			state.Glow = 0.55f + 0.45f * std::min(state.Open, 1.0f);
			state.Ring = state.Form * (0.75f + 0.35f * std::min(state.Open, 1.0f));
			state.Spin = 3.0f * pre + 3.6f * std::max(pre - VoidOpenStart, 0.0f);
			state.Distort = 0.6f * state.Form + 0.18f * std::min(state.Open, 1.0f);
			state.Swirl = 0.3f + 0.15f * std::min(state.Open, 1.0f);
			if (!pulled)
				return state;
			state.Flash = std::exp(-p * 16.0f) + 0.45f * std::exp(-std::abs(p - VoidCloseEnd) * 30.0f);
			state.Pupil = glm::mix(state.Pupil, 0.03f, Smooth(0.0f, 0.06f, p));
			state.Iris = Smooth(0.0f, 0.3f, p);
			state.Open = std::min(state.Open, 1.0f) * (1.0f - EaseIn((p - VoidCloseStart) / (VoidCloseEnd - VoidCloseStart)));
			state.Seam = p >= VoidCloseEnd - 0.02f ? 1.0f - EaseIn((p - VoidCloseEnd) / (VoidSlitEnd - VoidCloseEnd)) : 0.0f;
			state.Collapse = std::pow(EaseIn((p - VoidCollapseStart) / (VoidCollapseEnd - VoidCollapseStart)), 1.3f);
			state.Point = Smooth(0.45f, 0.6f, p);
			state.Consume = EaseIn(p / ConsumeSeconds);
			state.Glow = 1.0f + 0.6f * state.Iris + 1.5f * state.Point;
			state.Ring = (1.0f + 0.8f * Smooth(0.0f, 0.15f, p)) * (1.0f - Smooth(0.55f, VoidDetonate, p));
			state.Spin += 5.0f * p + 22.0f * p * p;
			state.Distort = (1.0f + 0.3f * Smooth(0.0f, 0.1f, p) + 0.6f * Smooth(0.45f, 0.62f, p)) * (1.0f - Smooth(0.64f, VoidDetonate + 0.01f, p));
			state.Swirl = 0.35f + 0.3f * Smooth(0.0f, 0.12f, p);
			state.Blast = p >= VoidDetonate ? (p - VoidDetonate) * look.Tempo : -1.0f;
			return state;
		}

		float Funnel(float r, float core, float field, float ground, float top)
		{
			const float t = 1.0f - Clamp01((r - core) / std::max(field * 0.95f - core, 1e-3f));
			return ground + 0.12f + (top - ground - 0.12f) * std::pow(t, 2.4f);
		}

		glm::vec3 Around(const glm::vec3& core, float r, float angle, float height)
		{
			return glm::vec3(core.x + std::cos(angle) * r, height, core.z + std::sin(angle) * r);
		}

		void DrawTendrils(WorldRenderer& renderer, const VoidGazeLook& look, const VoidState& state, const glm::vec3& core, float coreNow, float field, float fade)
		{
			if (state.Consume >= 0.999f || state.Blast >= 0.0f)
				return;
			const float head = Smooth(0.04f, 0.26f, state.Pre);
			const float appear = Smooth(0.03f, 0.14f, state.Pre) * fade * (1.0f - Smooth(0.75f, 1.0f, state.Consume));
			const float scale = std::sqrt(look.Scale);
			std::array<VoidKnot, TendrilKnots> knots;
			for (uint32_t i = 0; i < TendrilCount; i++)
			{
				const uint32_t key = look.Seed + i * 101u;
				const float base = float(i) * Tau / float(TendrilCount) + 0.5f * (Unit(key, 1) - 0.5f);
				const float dir = i == 4 ? -1.0f : 1.0f;
				const float speed = (i == 4 ? 0.4f : 0.55f) + 0.25f * Unit(key, 2);
				const float twist = 1.1f + 0.5f * Unit(key, 3) + 2.8f * state.Consume;
				const float outer = glm::mix(field * (0.72f + 0.22f * Unit(key, 4)), coreNow * 1.05f, state.Consume);
				const float inner = coreNow * 1.02f;
				const float width = (0.2f + 0.16f * Unit(key, 5)) * scale * (1.0f - 0.5f * state.Consume);
				const float bob = 0.36f * (Unit(key, 6) - 0.5f);
				const float phase = base + dir * state.Spin * speed;
				for (uint32_t k = 0; k < TendrilKnots; k++)
				{
					const float s = float(k) / float(TendrilKnots - 1) * head;
					const float r = glm::mix(outer, inner, std::pow(s, 0.85f));
					const float angle = phase + dir * twist * std::log(std::max(outer, 1e-3f) / std::max(r, 1e-3f));
					const float swell = 0.7f + 0.6f * (0.5f + 0.5f * std::sin(s * 9.0f + float(i) * 1.7f + look.Time * 2.3f));
					const float height = Funnel(r, coreNow, field, look.Ground.y, core.y) + bob * std::sin(angle * 1.3f + float(i)) * Clamp01(r / std::max(outer, 1e-3f));
					knots[k] = { Around(core, r, angle, height), width * std::pow(std::sin(3.14159f * std::clamp(s, 0.02f, 0.98f)), 0.6f) * swell * std::pow(r / std::max(outer, 1e-3f), 0.3f), Smooth(0.0f, 0.15f, s / std::max(head, 1e-3f)) * (1.0f - Smooth(0.86f, 1.0f, s)) * Smooth(coreNow * 1.05f, coreNow * 1.9f, r) };
				}
				renderer.GetVoid().Draw(knots, { .Look = VoidRibbonLook::Tendril, .Body = VoidBody, .Glow = glm::mix(VoidViolet, VoidLavender, 0.25f * Unit(key, 7)) * (0.5f + 0.5f * state.Iris), .Opacity = 0.9f * appear, .Intensity = appear, .Flow = 1.0f + 3.0f * state.Iris, .Seed = key });
			}
		}

		void DrawStreams(WorldRenderer& renderer, const VoidGazeLook& look, const VoidState& state, const glm::vec3& core, float coreNow, float field, float fade)
		{
			if (state.Consume >= 0.999f || state.Blast >= 0.0f)
				return;
			const float head = Smooth(0.07f, 0.3f, state.Pre);
			const float appear = Smooth(0.05f, 0.18f, state.Pre) * fade * (1.0f - Smooth(0.7f, 1.0f, state.Consume));
			const float scale = std::sqrt(look.Scale);
			std::array<VoidKnot, StreamKnots> knots;
			std::array<VoidKnot, StreamKnots> branch;
			for (uint32_t j = 0; j < StreamCount; j++)
			{
				const uint32_t key = look.Seed + 7919u + j * 211u;
				const float base = float(j) * Tau / float(StreamCount) + Unit(key, 1) * 0.9f;
				const float dir = j == 2 ? -1.0f : 1.0f;
				const float speed = (j == 2 ? 0.7f : 0.8f) + 0.4f * Unit(key, 2);
				const float twist = 1.8f + 0.8f * Unit(key, 3) + 3.2f * state.Consume;
				const float outer = glm::mix(field * (0.5f + 0.4f * Unit(key, 4)), coreNow * 1.05f, state.Consume);
				const float inner = coreNow * 1.04f;
				const float width = (0.05f + 0.035f * Unit(key, 5)) * scale;
				const float bob = 0.6f * (Unit(key, 6) - 0.5f);
				const float phase = base + dir * state.Spin * speed;
				const float flicker = (0.75f + 0.35f * std::sin(look.Time * 11.0f + float(j) * 2.1f)) * (1.0f + 0.5f * state.Iris);
				const auto knotAt = [&](float s)
				{
					const float r = glm::mix(outer, inner, std::pow(s, 0.8f));
					const float angle = phase + dir * twist * std::log(std::max(outer, 1e-3f) / std::max(r, 1e-3f));
					const float height = Funnel(r, coreNow, field, look.Ground.y, core.y) + bob * std::sin(angle + float(j) * 0.7f) * Clamp01(r / std::max(outer, 1e-3f));
					return Around(core, r, angle, height);
				};
				for (uint32_t k = 0; k < StreamKnots; k++)
				{
					const float s = float(k) / float(StreamKnots - 1) * head;
					knots[k] = { knotAt(s), width * (0.4f + 0.6f * std::sin(3.14159f * std::clamp(s, 0.05f, 0.95f))), Smooth(0.0f, 0.12f, s / std::max(head, 1e-3f)) * (1.0f - Smooth(0.88f, 1.0f, s)) * Smooth(coreNow * 1.05f, coreNow * 1.8f, glm::mix(outer, inner, std::pow(s, 0.8f))) };
				}
				const glm::vec3 glow = glm::mix(VoidViolet, VoidLavender, 0.35f + 0.65f * Unit(key, 7)) * flicker;
				const VoidRibbon stream{ .Look = VoidRibbonLook::Stream, .Body = VoidBody, .Glow = glow, .Opacity = 0.25f * appear, .Intensity = 0.55f * appear, .Flow = 1.2f + 2.5f * state.Iris, .Seed = key };
				renderer.GetVoid().Draw(knots, stream);
				if (Unit(key, 8) < 0.45f)
					continue;
				const float from = 0.2f + 0.35f * Unit(key, 9);
				const float span = 0.22f + 0.1f * Unit(key, 10);
				for (float side : { -1.0f, 1.0f })
				{
					uint32_t count = 0;
					for (uint32_t k = 0; k < StreamKnots && count < branch.size(); k++)
					{
						const float s = from + span * float(k) / float(StreamKnots - 1);
						if (s > head)
							break;
						const glm::vec3 a = knotAt(s);
						const glm::vec3 b = knotAt(std::min(s + 0.01f, 1.0f));
						glm::vec3 across = glm::cross(b - a, Up);
						across = glm::dot(across, across) > 1e-8f ? glm::normalize(across) : glm::vec3(1.0f, 0.0f, 0.0f);
						const float bulge = std::sin(3.14159f * float(k) / float(StreamKnots - 1));
						branch[count++] = { a + across * (side * 0.16f * scale * bulge) + Up * (0.05f * side * bulge), width * 0.45f, bulge };
					}
					if (count >= 2)
						renderer.GetVoid().Draw(std::span<const VoidKnot>(branch.data(), count), { .Look = VoidRibbonLook::Stream, .Body = VoidBody, .Glow = glow * 0.8f, .Opacity = 0.0f, .Intensity = appear, .Flow = 1.6f, .Seed = key + uint32_t(side + 2.0f) });
				}
			}
		}

		void DrawMotes(WorldRenderer& renderer, const VoidGazeLook& look, const VoidState& state, const glm::vec3& core, float coreNow, float field, float fade)
		{
			if (state.Blast >= 0.0f || state.Pre < 0.03f)
				return;
			const float pullAge = look.PullAge;
			const float scale = std::sqrt(look.Scale);
			std::array<VoidKnot, 3> knots;
			for (uint32_t m = 0; m < MoteCount; m++)
			{
				const float offset = Unit(look.Seed + 31u, m) * MotePeriod;
				if (look.Age < offset)
					continue;
				const float cycle = std::floor((look.Age - offset) / MotePeriod);
				const float born = offset + cycle * MotePeriod;
				if (pullAge >= 0.0f && born > pullAge + 0.15f)
					continue;
				const uint32_t key = Hash(look.Seed + m * 977u + uint32_t(cycle) * 131u);
				const float start = field * (0.35f + 0.6f * Unit(key, 1));
				const float angle0 = Unit(key, 2) * Tau;
				const float rate = 0.7f + 0.6f * Unit(key, 3);
				const float wind = 0.35f + 0.5f * Unit(key, 4);
				const float orbit = 0.4f + 0.9f * Unit(key, 5);
				const float dir = Unit(key, 9) < 0.12f ? -1.0f : 1.0f;
				const float size = (0.025f + 0.035f * Unit(key, 6)) * scale;
				const float bright = 0.5f + 0.9f * Unit(key, 7);
				const bool dark = Unit(key, 8) < 0.22f;
				const float bob = 0.3f * (Unit(key, 10) - 0.5f);
				const float fromSuction = SuctionAt(born, pullAge, look.Tempo);
				bool consumed = false;
				float nearCore = 1.0f;
				for (uint32_t k = 0; k < knots.size(); k++)
				{
					const float at = std::max(look.Age - 0.03f * float(k), born);
					const float r = start * std::exp(-rate * (SuctionAt(at, pullAge, look.Tempo) - fromSuction));
					if (k == 0)
					{
						consumed = r < coreNow * 0.92f;
						nearCore = Smooth(coreNow * 0.92f, coreNow * 1.5f, r);
					}
					const float angle = angle0 + dir * (wind * (std::pow(start / std::max(r, 1e-3f), 1.5f) - 1.0f) + orbit * (at - born));
					const float height = Funnel(r, coreNow, field, look.Ground.y, core.y) + bob * Clamp01(r / start);
					knots[k] = { Around(core, r, angle, height), size * (1.0f - 0.3f * float(k)), 1.0f - 0.5f * float(k) };
				}
				if (consumed)
					continue;
				const float alpha = Smooth(0.0f, 0.12f, look.Age - born) * nearCore * fade;
				for (VoidKnot& knot : knots)
					knot.Alpha *= alpha;
				knots[2].Alpha = 0.0f;
				if (dark)
					renderer.GetVoid().Draw(knots, { .Look = VoidRibbonLook::Stream, .Body = VoidBody, .Glow = VoidViolet * 0.2f, .Opacity = 0.75f, .Intensity = 1.0f, .Flow = 0.0f, .Seed = key });
				else
					renderer.GetVoid().Draw(knots, { .Look = VoidRibbonLook::Mote, .Glow = glm::mix(VoidViolet, VoidLavender, Unit(key, 11)) * (bright * (1.0f + 0.8f * state.Iris)), .Intensity = 1.0f, .Seed = key });
			}
		}

		void DrawCrackle(WorldRenderer& renderer, const VoidGazeLook& look, const VoidState& state, const glm::vec3& core, const glm::vec3& normal, float coreBase)
		{
			if (state.Post >= VoidCollapseStart || std::min(state.Open, 1.0f) < 0.4f)
				return;
			const float slots = look.Time * 7.0f + float(look.Seed % 13u);
			const float slot = std::floor(slots);
			const float within = slots - slot;
			const uint32_t key = Hash(look.Seed + uint32_t(int32_t(slot)) * 7u);
			if (Unit(key, 1) < (state.Post >= 0.0f ? 0.2f : 0.5f) || within > 0.45f)
				return;
			glm::vec3 right = glm::cross(normal, Up);
			right = glm::dot(right, right) > 1e-6f ? glm::normalize(right) : glm::vec3(1.0f, 0.0f, 0.0f);
			const glm::vec3 forward = glm::cross(right, normal);
			const float a = Unit(key, 2) * Tau;
			const float b = a + 0.7f + 0.8f * Unit(key, 3);
			const float r = coreBase * (1.35f + 0.5f * Unit(key, 4));
			LightningShape arc;
			arc.From = core + (right * std::cos(a) + forward * std::sin(a)) * r;
			arc.To = core + (right * std::cos(b) + forward * std::sin(b)) * (r * 1.1f);
			arc.Seed = key;
			arc.Detail = uint32_t(look.Time * 40.0f);
			arc.Core = 0.018f;
			arc.Glow = 0.2f;
			arc.Jag = 0.24f;
			arc.Branches = 0.6f;
			arc.Depth = 1;
			arc.Intensity = (1.0f - within / 0.45f) * 0.9f;
			arc.CoreColor = VoidWhite;
			arc.GlowColor = VoidViolet;
			renderer.DrawLightning(arc);
		}

		void DrawBlast(WorldRenderer& renderer, const VoidGazeLook& look, const VoidState& state, const glm::vec3& core, float coreBase, float field, float lightGain)
		{
			const float t = state.Blast;
			if (t < 0.0f || t > VoidBlastSeconds)
				return;
			const float scale = std::sqrt(look.Scale);
			const float flash = std::exp(-t * 18.0f);
			DrawGlow(renderer, core, coreBase * (0.9f + 3.0f * t), VoidWhite * (0.6f * std::exp(-t * 26.0f)), 1.0f);
			DrawGlow(renderer, core, coreBase * (1.8f + 4.0f * t), VoidViolet * (0.35f * std::exp(-t * 8.0f)), 1.0f);
			renderer.DrawLight({ .Position = core, .Radius = field * 1.3f, .Color = glm::pow(VoidViolet, glm::vec3(2.2f)) * (lightGain * (0.22f * flash + 0.06f * std::exp(-t * 5.0f))) });

			const float fade = 1.0f - Smooth(0.25f, 0.55f, t);
			renderer.GetVoid().Draw(VoidShock{ .Center = look.Ground + Up * 0.1f, .Radius = 0.3f + field * 1.05f * EaseOut(t / 0.42f), .Width = glm::mix(0.7f, 0.35f, Clamp01(t / 0.5f)) * scale, .Intensity = fade, .Dark = 0.75f, .Spin = Unit(look.Seed, 40) * Tau, .Seed = look.Seed });
			renderer.GetVoid().Draw(VoidShock{ .Center = core, .Radius = 0.2f + field * 0.6f * EaseOut(t / 0.25f), .Width = 0.5f * scale, .Intensity = 1.0f - Smooth(0.08f, 0.3f, t), .Dark = 0.4f, .Spin = Unit(look.Seed, 41) * Tau, .Seed = look.Seed + 5u });
			if (t < 0.4f)
			{
				DrawShockwave(renderer, look.Ground, 0.3f + field * 1.05f * EaseOut(t / 0.42f), 0.3f, 0.12f * (1.0f - t / 0.4f));
				DrawAirShockwave(renderer, core, 0.2f + field * 0.6f * EaseOut(t / 0.25f), 0.25f, 0.09f * (1.0f - t / 0.4f));
			}

			for (uint32_t i = 0; i < ArcCount && t < 0.26f; i++)
			{
				const uint32_t key = look.Seed + 4001u + i * 37u;
				const float angle = float(i) * Tau / float(ArcCount) + 0.6f * Unit(key, 1);
				const float reach = field * (0.35f + 0.35f * Unit(key, 2));
				LightningShape arc;
				arc.From = core;
				arc.To = glm::vec3(core.x + std::cos(angle) * reach, look.Ground.y + 0.25f + 0.5f * Unit(key, 3), core.z + std::sin(angle) * reach);
				arc.Seed = key;
				arc.Detail = uint32_t(look.Time * 30.0f);
				arc.Core = 0.03f;
				arc.Glow = 0.32f;
				arc.Jag = 0.2f;
				arc.Branches = 1.1f;
				arc.BranchLength = 0.3f;
				arc.Depth = 1;
				arc.Flatten = 0.5f;
				arc.Taper = 0.6f;
				arc.Reach = EaseOut(t / 0.07f);
				arc.Intensity = 1.0f - Smooth(0.06f, 0.26f, t);
				arc.CoreColor = VoidWhite;
				arc.GlowColor = VoidViolet;
				renderer.DrawLightning(arc);
			}

			std::array<VoidKnot, 8> knots;
			const float streakFade = 1.0f - Smooth(0.12f, 0.42f, t);
			for (uint32_t j = 0; j < StreakCount && streakFade > 0.0f; j++)
			{
				const uint32_t key = look.Seed + 6007u + j * 53u;
				const float angle = float(j) * Tau / float(StreakCount) + 0.45f * Unit(key, 1);
				const float speed = (5.0f + 4.5f * Unit(key, 2)) * scale;
				const float bend = 2.4f * (Unit(key, 3) - 0.5f);
				const auto radiusAt = [&](float time)
				{
					const float clamped = std::clamp(time, 0.0f, 0.8f);
					return coreBase * 0.3f + speed * clamped * (1.0f - 0.6f * clamped);
				};
				const float head = radiusAt(t);
				const float tail = radiusAt(t - 0.1f);
				for (uint32_t k = 0; k < knots.size(); k++)
				{
					const float s = float(k) / float(knots.size() - 1);
					const float r = glm::mix(tail, head, s);
					const float a = angle + bend * std::log(1.0f + r);
					const float height = glm::mix(core.y, look.Ground.y + 0.3f, Clamp01(r / (field * 0.8f)));
					knots[k] = { Around(core, r, a, height), (0.02f + 0.05f * s) * scale, s * streakFade };
				}
				renderer.GetVoid().Draw(knots, { .Look = VoidRibbonLook::Stream, .Body = VoidBody, .Glow = glm::mix(VoidLavender, VoidWhite, 0.4f * Unit(key, 4)), .Opacity = 0.0f, .Intensity = 1.2f, .Flow = 3.0f, .Seed = key });
			}
		}
	}

	glm::vec3 VoidCore(const VoidGazeLook& look)
	{
		return look.Ground + Up * (VoidCoreHeight * std::sqrt(look.Scale));
	}

	float VoidCoreSize(float scale)
	{
		return VoidCoreRadius * std::sqrt(scale);
	}

	float VoidLifetime(float tempo)
	{
		return VoidDetonate * tempo + VoidBlastSeconds;
	}

	TrailStyle VoidWispTrail()
	{
		TrailStyle style;
		style.Look = TrailLook::Wisp;
		style.Head = VoidViolet * 0.55f;
		style.Tail = glm::vec3(0.06f, 0.01f, 0.14f);
		style.Core = glm::vec4(VoidLavender * 0.7f, 1.0f);
		style.Width = 0.34f;
		style.WidthCurve = { 0.6f, 1.0f, 0.8f, 0.3f };
		style.AlphaCurve = { 0.7f, 0.5f, 0.25f, 0.0f };
		style.Life = 0.3f;
		style.Emission = 0.35f;
		style.Additive = 0.6f;
		style.Tiling = 1.2f;
		style.Scroll = -0.8f;
		style.Taper = 0.3f;
		return style;
	}

	void DrawVoidGaze(WorldRenderer& renderer, const VoidGazeLook& look, float lightGain)
	{
		const VoidState state = Evaluate(look);
		const float fade = Clamp01(look.Fade);
		const glm::vec3 core = VoidCore(look) + Up * (0.04f * std::sin(look.Time * 2.1f) * (1.0f - state.Collapse));
		const float coreBase = VoidCoreSize(look.Scale) * state.Grow;
		const float tremble = 1.0f + 0.22f * state.Point * std::sin(look.Time * 61.0f);
		const float coreNow = std::max(coreBase * glm::mix(1.0f, 0.07f, state.Collapse) * tremble, 0.01f);
		const float field = look.Radius;
		const float fieldSpin = 0.55f * state.Spin;
		const float fieldGlow = (state.Post < 0.0f ? 0.35f + 0.3f * std::min(state.Open, 1.0f) : 0.8f * (1.0f - Smooth(0.45f, 0.75f, state.Post))) * fade;
		const float fieldDark = fade * (state.Blast >= 0.0f ? 1.0f - Smooth(0.0f, 0.3f, state.Blast) : 1.0f);
		if (fieldDark > 0.0f)
		{
			GroundDecal decal;
			decal.Center = look.Ground;
			decal.Extent = glm::vec2(field * VoidFieldBox);
			decal.Depth = 1.0f;
			decal.Shape = DecalShape::Void;
			decal.Color = glm::vec4(VoidViolet, std::max(fieldGlow, 0.0f));
			decal.Tint = glm::vec4(0.02f, 0.006f, 0.04f, fieldDark);
			decal.Params = glm::vec4(state.Form, 1.0f, fieldSpin, 0.0f);
			decal.Seed = look.Seed;
			renderer.DrawDecal(decal);
		}

		const glm::vec3 view = glm::normalize(look.Eye - core);
		glm::vec3 flat(view.x, 0.0f, view.z);
		flat = glm::dot(flat, flat) > 1e-6f ? glm::normalize(flat) : glm::vec3(0.0f, 0.0f, 1.0f);
		const float tilt = std::clamp(std::asin(std::clamp(view.y, -1.0f, 1.0f)) - 0.5236f, 0.0f, 1.0f);
		const float roll = (Unit(look.Seed, 90) - 0.5f) * 0.5f + 0.05f * std::sin(look.Time * 0.9f + float(look.Seed % 7u));
		const glm::vec3 normal = Turn(Up * std::cos(tilt) - flat * std::sin(tilt), view, roll);

		if (state.Blast < 0.0f)
		{
			VoidSingularity singularity;
			singularity.Center = core;
			singularity.Radius = coreNow;
			singularity.Open = state.Open;
			singularity.Seam = state.Seam;
			singularity.Pupil = state.Pupil;
			singularity.Iris = state.Iris;
			singularity.Flash = state.Flash;
			singularity.Glow = state.Glow * fade;
			singularity.Point = state.Point;
			singularity.Tilt = 0.24f * (Unit(look.Seed, 91) - 0.5f) + 0.04f * std::sin(look.Time * 1.3f);
			singularity.Gaze = look.Gaze;
			singularity.Gazing = look.Gazing * std::min(state.Open, 1.0f) * (1.0f - state.Iris);
			singularity.Seed = look.Seed;
			renderer.GetVoid().Draw(singularity);

			if (state.Ring > 0.0f)
			{
				VoidDisk disk;
				disk.Center = core;
				disk.Normal = normal;
				disk.Inner = coreBase * 1.2f * glm::mix(1.0f, 0.12f, state.Collapse);
				disk.Outer = coreBase * 2.15f * glm::mix(1.0f, 0.2f, state.Collapse);
				disk.Core = std::max(disk.Inner / 1.2f, 0.01f);
				disk.Spin = state.Spin;
				disk.Brightness = 0.85f * state.Ring * fade;
				disk.Dust = 0.85f;
				disk.Thin = state.Collapse;
				disk.Reveal = Smooth(0.04f, 0.22f, state.Pre);
				disk.Seed = look.Seed;
				renderer.GetVoid().Draw(disk);
			}

			if (state.Distort > 0.0f)
			{
				Distorter vortex;
				vortex.Center = core;
				vortex.Shape = DistortionShape::Vortex;
				vortex.Extent = glm::vec2(std::max(field * 0.9f, coreNow * 4.5f));
				vortex.Yaw = state.Spin * 0.6f;
				vortex.Strength = 0.22f * state.Distort * fade;
				vortex.Soft = 0.25f;
				vortex.Params = glm::vec4(std::max(coreNow, coreBase * 0.25f), coreBase * (0.5f + 0.15f * std::min(state.Open, 1.0f)), state.Swirl, 0.3f + 0.3f * state.Iris);
				vortex.Seed = look.Seed;
				renderer.DrawDistortion(vortex);
			}

			const float ringLight = 0.05f * state.Ring + 0.035f * std::min(state.Open, 1.0f) + 0.22f * state.Flash + 0.25f * state.Point;
			renderer.DrawLight({ .Position = core, .Radius = field * 1.1f, .Color = glm::pow(VoidViolet, glm::vec3(2.2f)) * (lightGain * ringLight * fade) });

			if (state.Post >= 0.0f && state.Post < 0.3f)
			{
				const float p = state.Post;
				const float reach = p < 0.1f ? EaseOut(p / 0.1f) : 1.0f - EaseIn((p - 0.1f) / 0.2f);
				renderer.GetVoid().Draw(VoidShock{ .Center = look.Ground + Up * 0.08f, .Radius = 0.2f + field * 0.55f * reach, .Width = 0.6f * std::sqrt(look.Scale), .Intensity = 0.7f * (1.0f - Smooth(0.24f, 0.3f, p)), .Dark = 0.9f, .Spin = -state.Spin * 0.3f, .Seed = look.Seed + 9u });
			}
		}

		DrawTendrils(renderer, look, state, core, coreNow, field, fade);
		DrawStreams(renderer, look, state, core, coreNow, field, fade);
		DrawMotes(renderer, look, state, core, coreNow, field, fade);
		DrawCrackle(renderer, look, state, core, normal, coreBase);
		DrawBlast(renderer, look, state, VoidCore(look), VoidCoreSize(look.Scale), field, lightGain);
	}
}
