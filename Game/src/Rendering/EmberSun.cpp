#include "EmberSun.h"
#include "Rendering/Sun.h"
#include <GameShared/Content/EmberIsland.h>
#include <algorithm>
#include <iterator>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace Game
{
	namespace
	{
		constexpr int Prominences = 6;
		constexpr int Knots = 26;
		constexpr int Spicules = 30;
		constexpr int Embers = 55;
		constexpr int Droplets = 28;
		constexpr int Splashes = 9;
		constexpr int SplashKnots = 12;
		constexpr int Siphons = 3;
		constexpr int SiphonSparks = 22;
		constexpr int FallKnots = 24;
		constexpr int FallStrands = 6;
		constexpr int FallSparks = 10;
		constexpr float FallDrop = 9.0f;
		constexpr float FallLip = 0.25f;
		constexpr float FallInward = 0.9f;
		constexpr float FallHug = 0.3f;
		constexpr int Drips = 18;
		constexpr float DripGrow = 0.9f;
		constexpr float EmberDripRadius = 4.66f;
		constexpr float EmberDripDrop = 0.95f;
		constexpr float EruptionPeriod = 10.0f;
		constexpr float EruptionLength = 3.4f;
		constexpr float ImpactTime = 0.32f;
		constexpr float Spin = 0.05f;
		constexpr float WardScale = 2.0f;
		constexpr float WardInset = 0.965f;
		constexpr float BeatPeriod = 2.8f;
		constexpr float BeatSwell = 0.022f;
		constexpr glm::vec3 TowardCamera{ 0.0f, 0.84f, -0.54f };
		constexpr glm::vec3 Gravity{ 0.0f, -5.5f, 0.0f };

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

		glm::vec3 Unit(uint32_t key)
		{
			const float z = 2.0f * Hash(key, 1) - 1.0f;
			const float a = 2.0f * Pi * Hash(key, 2);
			const float r = std::sqrt(std::max(1.0f - z * z, 0.0f));
			return { r * std::cos(a), z, r * std::sin(a) };
		}

		glm::vec3 Perpendicular(const glm::vec3& n, uint32_t key)
		{
			glm::vec3 g = glm::cross(n, Unit(key));
			if (glm::dot(g, g) < 1e-6f)
				g = glm::cross(n, glm::vec3(1.0f, 0.0f, 0.0f));
			return glm::normalize(g);
		}

		float Smooth(float low, float high, float x)
		{
			const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		glm::vec3 Slerp(const glm::vec3& a, const glm::vec3& b, float t)
		{
			const float c = std::clamp(glm::dot(a, b), -1.0f, 1.0f);
			const float angle = std::acos(c);
			if (angle < 1e-4f)
				return a;
			return (a * std::sin((1.0f - t) * angle) + b * std::sin(t * angle)) / std::sin(angle);
		}

		struct Frame
		{
			glm::vec3 Center{ 0.0f };
			float Radius = 1.0f;
			float Ward = 2.0f;
			float Time = 0.0f;

			glm::vec3 Contain(const glm::vec3& p) const
			{
				const glm::vec3 d = p - Center;
				const float length = glm::length(d);
				const float limit = Ward * WardInset;
				return length > limit ? Center + d * (limit / length) : p;
			}
		};

		void DrawSpark(SunRenderer& sun, const glm::vec3& head, const glm::vec3& tail, float width, float heat, float alpha, uint32_t seed)
		{
			if (alpha <= 0.002f)
				return;
			const std::array<SunKnot, 2> knots{ SunKnot{ tail, width * 0.6f, heat * 0.7f, alpha }, SunKnot{ head, width, heat, alpha } };
			sun.Draw(knots, SunRibbon{ .Look = SunRibbonLook::Spark, .Intensity = 1.0f, .Opacity = 0.0f, .Flow = 1.0f, .Seed = seed });
		}

		struct Loop
		{
			glm::vec3 Axis{ 0.0f };
			glm::vec3 Along{ 0.0f };
			float Separation = 0.4f;
			float Height = 0.5f;
			float Lean = 0.0f;
			float Width = 0.1f;
			float Heat = 0.6f;
			float Alpha = 1.0f;
			float Breakup = 0.0f;
			float Intensity = 1.0f;
			int Filaments = 8;
			int Flows = 3;
			uint32_t Seed = 0;
		};

		struct Path
		{
			const Frame& F;
			const Loop& L;
			glm::vec3 Normal;
			glm::vec3 First;
			glm::vec3 Second;
			float Phase;

			Path(const Frame& frame, const Loop& loop)
				: F(frame), L(loop)
			{
				Normal = glm::normalize(glm::cross(loop.Axis, loop.Along));
				First = glm::normalize(loop.Axis * std::cos(0.5f * loop.Separation) + loop.Along * std::sin(0.5f * loop.Separation));
				Second = glm::normalize(loop.Axis * std::cos(0.5f * loop.Separation) - loop.Along * std::sin(0.5f * loop.Separation));
				Phase = float(loop.Seed % 1000u) * 0.0959f;
			}

			glm::vec3 At(float s) const
			{
				const float arch = std::sin(Pi * s);
				const glm::vec3 base = Slerp(First, Second, s);
				const glm::vec3 up = glm::normalize(base + Normal * (L.Lean * arch));
				const float lift = L.Height * std::pow(arch, 0.8f) * (1.0f + 0.1f * std::sin(3.0f * Pi * s + Phase));
				const glm::vec3 writhe = Normal * (0.1f * L.Height * arch * std::sin(F.Time * 0.8f + s * 4.0f + Phase))
					+ L.Along * (0.06f * L.Height * std::sin(2.0f * Pi * s) * std::sin(F.Time * 0.6f + Phase * 2.0f));
				return F.Center + base * (F.Radius * 0.985f) + up * lift + writhe;
			}
		};

		void DrawLoop(SunRenderer& sun, const Frame& f, const Loop& loop)
		{
			if (loop.Alpha <= 0.002f)
				return;
			const Path path(f, loop);
			std::array<glm::vec3, Knots> spine;
			std::array<glm::vec3, Knots> across;
			std::array<glm::vec3, Knots> lateral;
			for (int k = 0; k < Knots; k++)
				spine[k] = path.At(float(k) / float(Knots - 1));
			for (int k = 0; k < Knots; k++)
			{
				const glm::vec3 tangent = glm::normalize(spine[std::min(k + 1, Knots - 1)] - spine[std::max(k - 1, 0)] + glm::vec3(1e-6f));
				glm::vec3 side = glm::cross(tangent, glm::normalize(spine[k] - f.Center));
				side = glm::dot(side, side) > 1e-6f ? glm::normalize(side) : loop.Along;
				across[k] = side;
				lateral[k] = glm::cross(tangent, side);
			}
			std::array<SunKnot, Knots> knots;
			for (int strand = 0; strand < loop.Filaments; strand++)
			{
				const uint32_t key = Mix(loop.Seed, uint32_t(strand) + 1u);
				const float angle = 2.0f * Pi * Hash(key, 1);
				const float reach = std::sqrt(Hash(key, 2));
				const float twist = 2.0f * (Hash(key, 3) - 0.5f);
				const float width = loop.Width * (0.16f + 0.12f * Hash(key, 4));
				const float sway = 0.35f * std::sin(f.Time * (0.5f + 0.6f * Hash(key, 5)) + angle * 3.0f);
				const float phase = path.Phase * 3.0f + float(strand) * 2.3f;
				for (int k = 0; k < Knots; k++)
				{
					const float s = float(k) / float(Knots - 1);
					const float arch = std::sin(Pi * s);
					const float spread = loop.Width * 0.8f * reach * (0.2f + 0.8f * std::sqrt(arch));
					const float a = angle + twist * Pi * s + sway * arch;
					float alpha = loop.Alpha;
					if (loop.Breakup > 0.0f)
					{
						const float gap = 0.5f + 0.5f * std::sin(s * 23.0f + phase);
						alpha *= 1.0f - Smooth(1.0f - loop.Breakup, 1.05f - loop.Breakup * 0.6f, gap);
					}
					knots[k] = {
						.Position = f.Contain(spine[k] + (across[k] * std::cos(a) + lateral[k] * std::sin(a)) * spread),
						.Width = width * (0.6f + 0.6f * arch),
						.Heat = std::clamp(loop.Heat + 0.4f * (1.0f - arch), 0.0f, 1.0f),
						.Alpha = alpha * (0.4f + 0.6f * Smooth(0.0f, 0.08f, s) * Smooth(0.0f, 0.08f, 1.0f - s)),
					};
				}
				sun.Draw(knots, SunRibbon{ .Look = SunRibbonLook::Plasma, .Intensity = 1.1f * loop.Intensity, .Opacity = 0.16f, .Flow = 0.5f + 0.6f * Hash(key, 6), .Seed = key });
			}
			for (int i = 0; i < loop.Flows; i++)
			{
				const uint32_t key = Mix(loop.Seed, 500u + uint32_t(i));
				const float rate = 0.22f + 0.3f * Hash(key, 1);
				float s = std::fmod(f.Time * rate + Hash(key, 2), 1.0f);
				const float back = Hash(key, 3) < 0.5f ? -0.06f : 0.06f;
				if (back > 0.0f)
					s = 1.0f - s;
				const float alpha = loop.Alpha * std::sin(Pi * s) * (1.0f - loop.Breakup);
				DrawSpark(sun, f.Contain(path.At(s)), f.Contain(path.At(std::clamp(s + back, 0.0f, 1.0f))), 0.045f, 0.95f, alpha * 1.2f, key);
			}
			if (loop.Breakup > 0.05f)
				for (int k = 1; k < Knots - 1; k += 2)
				{
					const uint32_t key = Mix(loop.Seed, uint32_t(k) + 900u);
					const glm::vec3 out = glm::normalize(spine[k] - f.Center);
					const float drift = loop.Breakup * (0.3f + 0.6f * Hash(key, 1));
					const glm::vec3 head = f.Contain(spine[k] + (out + Unit(key) * 0.5f) * drift);
					DrawSpark(sun, head, f.Contain(head - out * (0.06f + 0.1f * loop.Breakup)), 0.045f, 0.85f, loop.Alpha * Smooth(0.0f, 0.2f, loop.Breakup) * 1.3f, key);
				}
		}

		void DrawProminences(SunRenderer& sun, const Frame& f)
		{
			const float room = f.Ward * WardInset - f.Radius;
			for (int slot = 0; slot < Prominences; slot++)
			{
				const float period = 10.0f + 6.0f * Hash(uint32_t(slot), 11);
				const float shifted = f.Time + period * Hash(uint32_t(slot), 12);
				const float cycle = std::floor(shifted / period);
				const float tau = shifted - cycle * period;
				const uint32_t key = Mix(uint32_t(slot) + 17u, uint32_t(int32_t(cycle) + 100000));
				glm::vec3 axis = Unit(key);
				axis.y *= 0.65f;
				axis = glm::normalize(axis);
				const bool erupts = Hash(key, 9) < 0.3f;
				Loop loop;
				loop.Axis = axis;
				loop.Along = Perpendicular(axis, key + 3u);
				loop.Separation = 0.28f + 0.35f * Hash(key, 4);
				loop.Height = f.Radius * (0.3f + 0.45f * Hash(key, 5)) * (slot == 0 ? 1.2f : 1.0f);
				loop.Lean = 0.8f * (Hash(key, 6) - 0.5f);
				loop.Width = f.Radius * (0.14f + 0.08f * Hash(key, 7));
				loop.Filaments = 7 + int(Hash(key, 8) * 5.0f);
				loop.Seed = key;
				loop.Heat = 0.45f;
				const float grow = Smooth(0.0f, 2.2f, tau);
				loop.Height *= 0.25f + 0.75f * grow;
				loop.Alpha = grow;
				if (erupts)
				{
					const float launch = std::max(tau - (period - 3.0f), 0.0f);
					loop.Height = std::min(loop.Height + f.Radius * (1.2f * launch + 0.8f * launch * launch), room * 1.1f);
					loop.Alpha *= 1.0f - Smooth(0.4f, 2.8f, launch);
					loop.Breakup = Smooth(0.2f, 2.4f, launch);
					loop.Heat += 0.25f * Smooth(0.0f, 0.4f, launch);
				}
				else
				{
					const float fade = Smooth(period - 2.4f, period, tau);
					loop.Height *= 1.0f - 0.35f * fade;
					loop.Alpha *= 1.0f - fade;
				}
				DrawLoop(sun, f, loop);
			}
		}

		void DrawSpicules(SunRenderer& sun, const Frame& f)
		{
			for (int i = 0; i < Spicules; i++)
			{
				const float period = 2.5f + 2.5f * Hash(uint32_t(i), 21);
				const float shifted = f.Time + period * Hash(uint32_t(i), 22);
				const float cycle = std::floor(shifted / period);
				const float t = shifted - cycle * period;
				const uint32_t key = Mix(uint32_t(i) + 401u, uint32_t(int32_t(cycle) + 100000));
				const float life = 1.2f + 1.2f * Hash(key, 3);
				if (t > life)
					continue;
				const glm::vec3 n = Unit(key);
				const glm::vec3 drift = Perpendicular(n, key + 5u) * (0.3f * (Hash(key, 6) - 0.5f));
				const float speed = 0.6f + 0.9f * Hash(key, 4);
				const float gravity = 2.0f * speed / life;
				const auto at = [&](float s) { return f.Contain(f.Center + n * f.Radius + (n * speed + drift) * s - n * (0.5f * gravity * s * s)); };
				const float alpha = Smooth(0.0f, 0.15f, t) * (1.0f - Smooth(life * 0.6f, life, t));
				DrawSpark(sun, at(t), at(std::max(t - 0.09f, 0.0f)), 0.035f, 0.8f, alpha * 0.8f, key);
			}
		}

		void DrawEmbers(SunRenderer& sun, const Frame& f, float ceiling)
		{
			for (int i = 0; i < Embers; i++)
			{
				const float period = 7.0f + 6.0f * Hash(uint32_t(i), 31);
				const float shifted = f.Time + period * Hash(uint32_t(i), 32);
				const float cycle = std::floor(shifted / period);
				const float t = shifted - cycle * period;
				const uint32_t key = Mix(uint32_t(i) + 907u, uint32_t(int32_t(cycle) + 100000));
				const float reach = 1.6f + 5.0f * std::sqrt(Hash(key, 1));
				const float angle = 2.0f * Pi * Hash(key, 2);
				const float start = f.Center.y - 1.5f - 6.5f * Hash(key, 3);
				const float rise = 0.45f + 0.8f * Hash(key, 4);
				const float swirl = 0.12f + 0.12f * Hash(key, 5);
				const auto at = [&](float s)
				{
					const float a = angle + swirl * s;
					const float r = reach + 0.25f * std::sin(s * 0.9f + angle * 3.0f);
					return glm::vec3(f.Center.x + r * std::cos(a), start + rise * s + 0.15f * std::sin(s * 1.7f + angle), f.Center.z + r * std::sin(a));
				};
				const glm::vec3 head = at(t);
				if (head.y > ceiling - 0.15f)
					continue;
				const float alpha = Smooth(0.0f, 1.0f, t) * (1.0f - Smooth(period - 1.5f, period, t)) * (1.0f - Smooth(ceiling - 1.4f, ceiling - 0.15f, head.y));
				DrawSpark(sun, head, at(std::max(t - 0.09f, 0.0f)), 0.025f + 0.02f * Hash(key, 6), 0.35f + 0.45f * Hash(key, 7), alpha * 0.5f, key);
			}
		}

		void DrawSiphons(SunRenderer& sun, const Frame& f, const EmberSunLook& look, std::array<float, EmberDrifts>& drain)
		{
			if (look.SourceCount <= 0)
				return;
			std::array<SunKnot, Knots> knots;
			for (int i = 0; i < Siphons; i++)
			{
				const float period = 8.0f + 4.0f * Hash(uint32_t(i), 41);
				const float shifted = f.Time + period * Hash(uint32_t(i), 42);
				const float cycle = std::floor(shifted / period);
				const float t = shifted - cycle * period;
				const uint32_t key = Mix(uint32_t(i) + 1301u, uint32_t(int32_t(cycle) + 100000));
				const float life = period * 0.85f;
				if (t > life)
					continue;
				const int source = int(Mix(key, 7u) % uint32_t(look.SourceCount));
				const glm::vec3 from = look.Sources[size_t(source)] - f.Center;
				const float azimuth = std::atan2(from.z, from.x);
				const float lift = from.y;
				const float reach = glm::length(glm::vec2(from.x, from.z));
				const float swirl = (Hash(key, 4) < 0.5f ? -1.0f : 1.0f) * (1.2f + 0.8f * Hash(key, 5));
				const float head = Smooth(0.0f, 2.4f, t);
				const float tail = Smooth(life - 2.4f, life, t);
				const float fade = Smooth(0.0f, 0.6f, t) * (1.0f - Smooth(life - 0.5f, life, t));
				drain[size_t(source)] = std::max(drain[size_t(source)], fade);
				const auto at = [&](float s, float offset, float angleOffset)
				{
					const float r = reach + (f.Radius * 1.02f - reach) * std::pow(s, 0.85f) + offset;
					const float a = azimuth + swirl * std::pow(s, 1.3f) + angleOffset;
					const float y = lift + (lift * 0.25f - lift) * s - 0.35f * std::sin(Pi * s);
					return f.Center + glm::vec3(std::cos(a) * r, y, std::sin(a) * r);
				};
				for (int k = 0; k < Knots; k++)
				{
					const float u = float(k) / float(Knots - 1);
					const float s = tail + (head - tail) * u;
					knots[k] = {
						.Position = at(s, 0.0f, 0.0f),
						.Width = 0.1f + 0.08f * s,
						.Heat = 0.25f + 0.45f * s,
						.Alpha = fade * (0.2f + 0.8f * s) * Smooth(0.0f, 0.3f, s) * Smooth(0.0f, 0.1f, u) * Smooth(0.0f, 0.06f, 1.0f - u),
					};
				}
				sun.Draw(knots, SunRibbon{ .Look = SunRibbonLook::Plasma, .Intensity = 0.3f, .Opacity = 0.04f, .Flow = 1.6f, .Seed = key });
				for (int k = 0; k < SiphonSparks; k++)
				{
					const uint32_t sparkKey = Mix(key, uint32_t(k) + 90u);
					const float s = tail + (head - tail) * std::fmod(f.Time * (0.22f + 0.12f * Hash(sparkKey, 2)) + Hash(sparkKey, 1), 1.0f);
					const float offset = 0.14f * (Hash(sparkKey, 3) - 0.5f) * (1.0f - 0.7f * s);
					const float angle = 0.05f * (Hash(sparkKey, 4) - 0.5f);
					DrawSpark(sun, at(s, offset, angle), at(std::max(s - 0.025f, 0.0f), offset, angle), 0.025f + 0.02f * Hash(sparkKey, 5), 0.4f + 0.5f * s, fade * Smooth(0.0f, 0.2f, s) * (0.6f + 0.4f * Hash(sparkKey, 6)), sparkKey);
				}
			}
		}

		float CavernWall(float depth, float angle)
		{
			constexpr size_t count = std::size(EmberCavernDepths);
			depth = std::clamp(depth, EmberCavernDepths[0], EmberCavernDepths[count - 1]);
			size_t i = 0;
			while (i + 2 < count && depth > EmberCavernDepths[i + 1])
				i++;
			const float t = (depth - EmberCavernDepths[i]) / (EmberCavernDepths[i + 1] - EmberCavernDepths[i]);
			float shape = 1.0f;
			for (size_t k = 0; k + 2 < std::size(EmberCavernLobes); k += 3)
				shape += EmberCavernLobes[k + 1] * std::cos(EmberCavernLobes[k] * angle) + EmberCavernLobes[k + 2] * std::sin(EmberCavernLobes[k] * angle);
			return (EmberCavernRadii[i] + (EmberCavernRadii[i + 1] - EmberCavernRadii[i]) * t) * shape;
		}

		void DrawLavaFalls(SunRenderer& sun, const Frame& f, const EmberSunLook& look)
		{
			std::array<SunKnot, FallKnots> knots;
			for (int i = 0; i < look.FallCount; i++)
			{
				const glm::vec3 source = glm::vec3(look.Falls[size_t(i)]);
				const float power = look.Falls[size_t(i)].w;
				const glm::vec3 rel = source - f.Center;
				const float angle = std::atan2(rel.z, rel.x);
				const glm::vec3 out(std::cos(angle), 0.0f, std::sin(angle));
				const glm::vec3 side(-out.z, 0.0f, out.x);
				const float reach = glm::length(glm::vec2(rel.x, rel.z));
				const float drop = FallDrop * (0.75f + 0.25f * power);
				const auto at = [&](float s, float lateral)
				{
					if (s < 0.0f)
						return source + out * (-FallLip * s) + side * lateral;
					const float h = drop * s * s;
					const float t = std::sqrt(2.0f * h / 9.8f);
					const float depth = f.Center.y + EmberHeartDepth - (source.y - h);
					const float r = std::min(reach - FallInward * t, CavernWall(depth, angle) - FallHug);
					return f.Center + out * r + side * (lateral * (1.0f + 0.8f * s)) + glm::vec3(0.0f, source.y - h - f.Center.y, 0.0f);
				};
				for (int strand = 0; strand < FallStrands; strand++)
				{
					const uint32_t key = Mix(uint32_t(i) * 17u + 2001u, uint32_t(strand));
					const float lateral = power * 0.34f * (float(strand) / float(FallStrands - 1) - 0.5f) + 0.06f * (Hash(key, 4) - 0.5f);
					const float width = (0.12f + 0.12f * Hash(key, 1)) * (0.7f + 0.3f * power);
					const float speed = 6.0f + 2.5f * Hash(key, 5);
					for (int k = 0; k < FallKnots; k++)
					{
						const float u = float(k) / float(FallKnots - 1);
						const float s = -0.15f + 1.15f * u;
						const float down = std::max(s, 0.0f);
						const float wobble = (0.03f + 0.12f * down) * std::sin(f.Time * (2.2f + Hash(key, 2)) + s * 9.0f + float(strand) * 1.7f);
						const float clumps = 0.5f + 0.5f * std::sin(s * 17.0f - f.Time * speed + float(strand) * 2.1f) * std::sin(s * 6.3f - f.Time * speed * 0.45f + float(strand));
						knots[k] = {
							.Position = at(s, lateral * (1.0f + 1.2f * down) + wobble),
							.Width = width * (1.0f + 0.7f * down),
							.Heat = 0.62f - 0.35f * down,
							.Alpha = Smooth(0.0f, 0.08f, u) * (1.0f - Smooth(0.55f, 1.0f, u)) * (1.0f - down * 0.75f * (1.0f - Smooth(0.25f, 0.75f, clumps))),
						};
					}
					sun.Draw(knots, SunRibbon{ .Look = SunRibbonLook::Plasma, .Intensity = 0.75f, .Opacity = 0.22f, .Flow = speed, .Seed = key });
				}
				for (int k = 0; k < FallKnots; k++)
				{
					const float u = float(k) / float(FallKnots - 1);
					const float s = -0.1f + 1.1f * u;
					knots[k] = { .Position = at(s, 0.0f), .Width = 0.6f * power, .Heat = 0.5f, .Alpha = (1.0f - Smooth(0.5f, 1.0f, u)) * Smooth(0.0f, 0.1f, u) };
				}
				sun.Draw(knots, SunRibbon{ .Look = SunRibbonLook::Plasma, .Intensity = 0.35f, .Opacity = 0.06f, .Flow = 4.0f, .Seed = uint32_t(i) + 777u });
				for (int k = 0; k < FallSparks; k++)
				{
					const uint32_t key = Mix(uint32_t(i) * 31u + 5003u, uint32_t(k));
					const float life = 0.9f + 0.8f * Hash(key, 1);
					const float local = f.Time + life * Hash(key, 2);
					const float t = local - std::floor(local / life) * life;
					const uint32_t spark = Mix(key, uint32_t(int32_t(std::floor(local / life)) + 100000));
					const float s0 = 0.15f + 0.6f * Hash(spark, 1);
					const glm::vec3 base = at(s0, power * 0.3f * (Hash(spark, 2) - 0.5f));
					const glm::vec3 kick = side * (1.4f * (Hash(spark, 3) - 0.5f)) - out * (0.4f + 0.6f * Hash(spark, 4));
					const auto fly = [&](float x) { return base + kick * x + glm::vec3(0.0f, -4.9f * x * x, 0.0f); };
					DrawSpark(sun, fly(t), fly(std::max(t - 0.05f, 0.0f)), 0.03f, 0.75f - 0.35f * t / life, power * (1.0f - Smooth(life * 0.4f, life, t)), spark);
				}
			}
		}

		void DrawDrips(SunRenderer& sun, const Frame& f, float ceiling)
		{
			for (int i = 0; i < Drips; i++)
			{
				const uint32_t site = uint32_t(i) + 6007u;
				const float period = 2.6f + 2.8f * Hash(site, 1);
				const float local = f.Time + period * Hash(site, 2);
				const float t = local - std::floor(local / period) * period;
				const float angle = 2.0f * Pi * Hash(site, 3);
				const float radius = EmberDripRadius + 0.22f * Hash(site, 4);
				const glm::vec3 tip(f.Center.x + std::cos(angle) * radius, ceiling - EmberDripDrop - 0.3f * Hash(site, 5), f.Center.z + std::sin(angle) * radius);
				if (t < DripGrow)
				{
					const float grow = t / DripGrow;
					DrawSpark(sun, tip - glm::vec3(0.0f, 0.04f * grow, 0.0f), tip, 0.02f + 0.025f * grow, 0.8f, Smooth(0.0f, 0.3f, grow), site);
					continue;
				}
				const float fall = t - DripGrow;
				const auto at = [&](float x) { return tip - glm::vec3(0.0f, 4.9f * x * x, 0.0f); };
				DrawSpark(sun, at(fall), at(std::max(fall - 0.05f, 0.0f)), 0.035f, 0.85f - 0.3f * std::min(fall, 1.0f), 1.0f - Smooth(0.7f, 1.4f, fall), site);
			}
		}

		struct Impact
		{
			glm::vec3 Direction{ 0.0f, 1.0f, 0.0f };
			float Age = -1.0f;
			float Strength = 0.0f;
			float Flash = 0.0f;
		};

		Impact DrawEruption(SunRenderer& sun, const Frame& f, SunStar& star, int lane)
		{
			const EmberEruptionPlan plan = PlanEmberEruption(f.Time, lane);
			const float power = plan.Power;
			const float tau = f.Time - plan.Start;
			if (tau < 0.0f || tau > EruptionLength)
				return {};
			const uint32_t key = Mix(uint32_t(int32_t(std::floor(plan.Start * 16.0f)) + 100000), 91u + uint32_t(lane));
			const glm::vec3 site = plan.Site;
			star.Flares[size_t(lane)] = glm::vec4(site, power * 1.1f * std::exp(-tau * 2.2f) * Smooth(0.0f, 0.08f, tau));
			for (int w = 0; w < 2; w++)
			{
				const float s = tau - 0.05f - 0.35f * float(w);
				if (s <= 0.0f || s > 1.4f)
					continue;
				const float fade = 1.0f - s / 1.4f;
				sun.Draw(SunWave{
					.Origin = f.Center + site * f.Radius,
					.Outward = site,
					.Radius = f.Radius * (0.15f + 1.05f * std::pow(s, 0.7f)),
					.Width = 0.08f + 0.08f * s,
					.Intensity = power * fade * fade * (w == 0 ? 1.4f : 0.7f),
					.Spread = 1.15f,
					.Seed = key + uint32_t(w),
				});
			}
			const float shell = f.Ward * WardInset;
			const glm::vec3 bend = Perpendicular(site, key + 9u);
			Impact impact;
			impact.Direction = glm::normalize(site + bend * (0.35f * (Hash(key, 10) - 0.5f)));
			const float head = std::pow(std::min(tau / ImpactTime, 1.0f), 0.8f);
			const float tail = Smooth(ImpactTime + 0.05f, ImpactTime + 1.0f, tau);
			const float jetAlpha = power * Smooth(0.0f, 0.06f, tau) * (1.0f - Smooth(ImpactTime + 0.5f, ImpactTime + 1.1f, tau));
			if (jetAlpha > 0.002f && head > tail)
			{
				const int jets = lane == 0 ? 14 : 9;
				std::array<SunKnot, Knots> knots;
				for (int j = 0; j < jets; j++)
				{
					const uint32_t jetKey = Mix(key, 200u + uint32_t(j));
					const glm::vec3 foot = glm::normalize(site + Perpendicular(site, jetKey + 1u) * (0.14f * Hash(jetKey, 2))) * (f.Radius * 0.98f);
					const glm::vec3 hit = glm::normalize(impact.Direction + Perpendicular(impact.Direction, jetKey + 3u) * (0.25f * Hash(jetKey, 4))) * shell;
					const glm::vec3 curl = Perpendicular(site, jetKey + 5u) * (0.35f * (Hash(jetKey, 6) - 0.5f));
					const glm::vec3 control = glm::normalize(site + curl) * (f.Radius + 0.6f * (shell - f.Radius));
					const glm::vec3 wobbleAxis = Perpendicular(site, jetKey + 7u);
					const float phase = 2.0f * Pi * Hash(jetKey, 8);
					for (int k = 0; k < Knots; k++)
					{
						const float u = float(k) / float(Knots - 1);
						const float s = tail + (head - tail) * u;
						const float r = 1.0f - s;
						const glm::vec3 point = foot * (r * r) + control * (2.0f * r * s) + hit * (s * s);
						const float wobble = 0.07f * std::sin(Pi * s) * std::sin(f.Time * 7.0f + s * 9.0f + phase);
						knots[k] = {
							.Position = f.Contain(f.Center + point + wobbleAxis * wobble),
							.Width = (0.035f + 0.04f * Hash(jetKey, 9)) * (1.3f - 0.6f * s),
							.Heat = 0.95f - 0.3f * s,
							.Alpha = jetAlpha * Smooth(0.0f, 0.12f, u) * Smooth(0.0f, 0.12f, 1.0f - u + 0.05f),
						};
					}
					sun.Draw(knots, SunRibbon{ .Look = SunRibbonLook::Plasma, .Intensity = 1.4f, .Opacity = 0.16f, .Flow = 2.2f, .Seed = jetKey });
				}
			}
			impact.Age = tau - ImpactTime;
			impact.Strength = impact.Age >= 0.0f ? power : 0.0f;
			impact.Flash = power * std::exp(-tau * 1.8f) * Smooth(0.0f, 0.1f, tau);
			if (impact.Age < 0.0f)
				return impact;
			const float age = impact.Age;
			const glm::vec3 u = Perpendicular(impact.Direction, key + 21u);
			const glm::vec3 v = glm::cross(impact.Direction, u);
			std::array<SunKnot, SplashKnots> splash;
			for (int j = 0; j < Splashes; j++)
			{
				const uint32_t splashKey = Mix(key, 300u + uint32_t(j));
				const float angle = 2.0f * Pi * (float(j) + 0.6f * Hash(splashKey, 1)) / float(Splashes);
				const glm::vec3 heading = u * std::cos(angle) + v * std::sin(angle);
				const float reach = (0.35f + 0.7f * Hash(splashKey, 2)) * power;
				const float front = reach * (1.0f - std::exp(-age * 2.4f));
				const float back = front * (0.15f + 0.75f * Smooth(0.2f, 1.5f, age));
				const float alpha = power * std::exp(-age * 1.4f) * Smooth(0.0f, 0.05f, age);
				if (alpha <= 0.01f)
					continue;
				for (int k = 0; k < SplashKnots; k++)
				{
					const float u01 = float(k) / float(SplashKnots - 1);
					const float theta = back + (front - back) * u01;
					splash[k] = {
						.Position = f.Center + (impact.Direction * std::cos(theta) + heading * std::sin(theta)) * shell,
						.Width = 0.05f * (0.4f + 0.6f * u01),
						.Heat = 0.55f + 0.35f * u01,
						.Alpha = alpha * Smooth(0.0f, 0.2f, u01),
					};
				}
				sun.Draw(splash, SunRibbon{ .Look = SunRibbonLook::Plasma, .Intensity = 1.4f, .Opacity = 0.12f, .Flow = 1.2f, .Seed = splashKey });
			}
			const int droplets = int(float(Droplets) * power);
			for (int j = 0; j < droplets; j++)
			{
				const uint32_t dropKey = Mix(key, 700u + uint32_t(j));
				const float t = age - 0.12f * Hash(dropKey, 1);
				const float life = 0.9f + 1.3f * Hash(dropKey, 2);
				if (t <= 0.0f || t > life)
					continue;
				const glm::vec3 velocity = Perpendicular(impact.Direction, dropKey + 3u) * (1.2f + 2.6f * Hash(dropKey, 4)) - impact.Direction * (0.4f + 1.4f * Hash(dropKey, 5));
				const glm::vec3 origin = f.Center + impact.Direction * (shell * 0.99f);
				const auto at = [&](float s) { return f.Contain(origin + velocity * s + Gravity * (0.5f * s * s)); };
				const glm::vec3 head = at(t);
				if (glm::length(head - f.Center) < f.Radius * 1.03f)
					continue;
				DrawSpark(sun, head, at(std::max(t - 0.06f, 0.0f)), 0.03f + 0.025f * Hash(dropKey, 6), 1.0f - 0.6f * t / life, 1.0f - Smooth(life * 0.55f, life, t), dropKey);
			}
			return impact;
		}

		void ActiveRegions(SunStar& star, float time)
		{
			const float period = 23.0f;
			const float shifted = time + period * 0.74f;
			const float cycle = std::floor(shifted / period);
			const float t = (shifted - cycle * period) / period;
			const uint32_t key = Mix(57u, uint32_t(int32_t(cycle) + 100000));
			const float envelope = std::sin(Pi * t);
			star.Flares[2] = glm::vec4(Unit(key), 0.22f * envelope * envelope);
		}

		struct Beat
		{
			float Swell = 0.0f;
			float Age = -1.0f;
			glm::vec3 Site{ 0.0f, 1.0f, 0.0f };
		};

		Beat HeartBeat(float time)
		{
			const float cycle = std::floor(time / BeatPeriod);
			const float t = time - cycle * BeatPeriod;
			const auto thump = [](float x)
			{
				if (x <= 0.0f)
					return 0.0f;
				const float k = x / 0.085f;
				return k * std::exp(1.0f - k);
			};
			const uint32_t key = Mix(uint32_t(int32_t(cycle) + 100000), 5u);
			return Beat{
				.Swell = thump(t) + 0.6f * thump(t - 0.3f),
				.Age = t,
				.Site = glm::normalize(glm::normalize(TowardCamera) + Unit(key) * 0.7f),
			};
		}
	}

	EmberEruptionPlan PlanEmberEruption(float time, int lane, int cycleOffset)
	{
		const float offset = lane == 0 ? 0.0f : 0.5f * EruptionPeriod;
		const float cycle = std::floor((time - offset) / EruptionPeriod) + float(cycleOffset);
		const uint32_t key = Mix(uint32_t(int32_t(cycle) + 100000), 77u + uint32_t(lane) * 13u);
		const glm::vec3 toward = glm::normalize(TowardCamera);
		glm::vec3 site = Unit(key + 2u);
		for (uint32_t attempt = 0; attempt < 12; attempt++)
		{
			const float facing = glm::dot(site, toward);
			if (facing > -0.15f && facing < 0.55f && std::abs(site.y) < 0.75f)
				break;
			site = Unit(key + 3u + attempt);
		}
		return EmberEruptionPlan{
			.Start = offset + cycle * EruptionPeriod + 3.0f * Hash(key, 1),
			.Site = site,
			.Power = lane == 0 ? 1.0f : 0.55f,
		};
	}

	EmberSunPulse DrawEmberSun(SunRenderer& sun, const EmberSunLook& look)
	{
		const Beat beat = HeartBeat(look.Time);
		const Frame frame{ .Center = look.Center, .Radius = look.Radius * (1.0f + BeatSwell * beat.Swell), .Ward = look.Radius * WardScale, .Time = look.Time };
		SunStar star{
			.Center = look.Center,
			.Radius = frame.Radius,
			.Spin = look.Time * Spin,
			.Seed = 7u,
		};
		star.Swell = beat.Swell;
		star.Beat = glm::vec4(beat.Site, beat.Age);
		ActiveRegions(star, look.Time);
		const Impact major = DrawEruption(sun, frame, star, 0);
		const Impact minor = DrawEruption(sun, frame, star, 1);
		const float flash = major.Flash + minor.Flash;
		star.Activity = flash;
		star.Brightness = 1.0f + 0.12f * flash + 0.04f * beat.Swell;
		star.Corona = 1.0f + 0.5f * flash + 0.2f * beat.Swell;
		DrawProminences(sun, frame);
		DrawSpicules(sun, frame);
		DrawEmbers(sun, frame, look.Ceiling);
		std::array<float, EmberDrifts> drain{};
		DrawSiphons(sun, frame, look, drain);
		DrawLavaFalls(sun, frame, look);
		DrawDrips(sun, frame, look.Ceiling);
		sun.Draw(star);
		SunWard ward{
			.Center = look.Center,
			.Radius = frame.Ward,
			.StarRadius = look.Radius,
			.Glow = 0.006f,
			.Pulse = 0.08f * beat.Swell,
		};
		ward.Impacts[0] = glm::vec4(major.Direction, major.Age);
		ward.Impacts[1] = glm::vec4(minor.Direction, minor.Age);
		ward.Strength = { major.Strength, minor.Strength };
		const auto ringing = [](const Impact& impact) { return impact.Strength > 0.0f && impact.Age < 2.8f; };
		if (ringing(major) || ringing(minor) || beat.Swell > 0.05f)
			sun.Draw(ward);
		sun.Draw(SunHaze{
			.Center = look.Center,
			.Radius = look.Radius,
			.Top = look.Ceiling,
			.DarkStart = -look.Center.y + 2.5f,
			.DarkEnd = -look.Center.y + 10.0f,
			.Darkness = 0.88f,
			.Color = glm::vec3(1.0f, 0.5f, 0.17f),
			.Power = 0.075f * (1.0f + 0.8f * flash + 0.3f * beat.Swell),
			.Shafts = 0.65f,
			.Extent = 10.0f,
		});
		const float flicker = 1.0f + 0.035f * std::sin(look.Time * 2.3f) + 0.025f * std::sin(look.Time * 5.1f + 1.3f) + 0.015f * std::sin(look.Time * 11.7f);
		const Impact& loud = major.Strength * std::exp(-std::max(major.Age, 0.0f) * 3.0f) >= minor.Strength * std::exp(-std::max(minor.Age, 0.0f) * 3.0f) ? major : minor;
		return EmberSunPulse{
			.Flash = flash,
			.Flicker = flicker,
			.Swell = beat.Swell,
			.BeatAge = beat.Age,
			.Radius = frame.Radius,
			.Ward = frame.Ward,
			.Impact = look.Center + loud.Direction * frame.Ward,
			.ImpactAge = loud.Strength > 0.0f ? loud.Age : -1.0f,
			.ImpactStrength = loud.Strength,
			.Drain = drain,
		};
	}

	glm::mat4 EmberDriftTransform(const glm::vec3& heart, int index, float time)
	{
		const uint32_t key = uint32_t(index) + 3301u;
		const float reach = 3.4f + 1.1f * Hash(key, 1);
		const float speed = 0.07f + 0.05f * Hash(key, 2);
		const float angle = 2.0f * Pi * (float(index) + 0.4f * Hash(key, 3)) / float(EmberDrifts) + speed * time;
		const float height = -1.6f + 1.3f * Hash(key, 4) + 0.16f * std::sin(time * (0.4f + 0.2f * Hash(key, 5)) + float(index) * 1.7f);
		const glm::vec3 position = heart + glm::vec3(std::cos(angle) * reach, height, std::sin(angle) * reach);
		const float tumble = time * (0.12f + 0.18f * Hash(key, 6)) + 2.0f * Pi * Hash(key, 7);
		return glm::rotate(glm::translate(glm::mat4(1.0f), position), tumble, Unit(key + 11u));
	}
}
