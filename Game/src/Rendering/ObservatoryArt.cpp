#include "Rendering/ObservatoryArt.h"
#include "Rendering/LevelRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace Game
{
	namespace
	{
		constexpr float TwoPi = 6.2831853f;
		constexpr float FloorY = 0.02f;
		constexpr int32_t HallSpokes = 240;
		constexpr int32_t CliffTiers = 9;
		constexpr float WallHeight = 7.6f;
		constexpr float FriezeBottom = 6.05f;
		constexpr uint32_t TagGrid = 0;
		constexpr uint32_t TagOuter = 1;

		enum class Kind : uint8_t
		{
			Floor,
			Blue,
			Gold,
			Stone,
			Rock,
			Bronze,
			Mural,
		};

		uint32_t HashOf(int32_t x, int32_t y, uint32_t salt)
		{
			uint32_t h = uint32_t(x) * 0x8DA6B343u ^ uint32_t(y) * 0xD8163841u ^ salt * 0xCB1AB31Fu;
			h ^= h >> 15;
			h *= 0x2C1B3C6Du;
			h ^= h >> 12;
			h *= 0x297A2D39u;
			return h ^ (h >> 15);
		}

		float Unit(int32_t x, int32_t y, uint32_t salt)
		{
			return float(HashOf(x, y, salt) & 0xFFFFu) / 65535.0f;
		}

		float EaseStep(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float Noise2(Vec2 p, uint32_t salt)
		{
			const float fx = std::floor(p.x);
			const float fy = std::floor(p.y);
			const int32_t x = int32_t(fx);
			const int32_t y = int32_t(fy);
			const float u = EaseStep(p.x - fx);
			const float v = EaseStep(p.y - fy);
			return std::lerp(std::lerp(Unit(x, y, salt), Unit(x + 1, y, salt), u), std::lerp(Unit(x, y + 1, salt), Unit(x + 1, y + 1, salt), u), v);
		}

		float Fbm2(Vec2 p, uint32_t salt, int32_t octaves)
		{
			float total = 0.0f;
			float amplitude = 0.5f;
			float weight = 0.0f;
			for (int32_t i = 0; i < octaves; i++)
			{
				total += amplitude * Noise2(p, salt + uint32_t(i));
				weight += amplitude;
				amplitude *= 0.5f;
				p = p * 2.03f;
			}
			return total / weight;
		}

		float Noise3(const glm::vec3& p, uint32_t salt)
		{
			const glm::vec3 f = glm::floor(p);
			const int32_t x = int32_t(f.x);
			const int32_t y = int32_t(f.y);
			const int32_t z = int32_t(f.z);
			const float u = EaseStep(p.x - f.x);
			const float v = EaseStep(p.y - f.y);
			const float w = EaseStep(p.z - f.z);
			const auto corner = [&](int32_t dx, int32_t dy, int32_t dz) { return Unit(x + dx + dz * 57, y + dy, salt + uint32_t(z + dz) * 131u); };
			const float a = std::lerp(std::lerp(corner(0, 0, 0), corner(1, 0, 0), u), std::lerp(corner(0, 1, 0), corner(1, 1, 0), u), v);
			const float b = std::lerp(std::lerp(corner(0, 0, 1), corner(1, 0, 1), u), std::lerp(corner(0, 1, 1), corner(1, 1, 1), u), v);
			return std::lerp(a, b, w);
		}

		uint32_t Unorm(float value)
		{
			return uint32_t(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
		}

		uint32_t Splat(float dirt, float main, float accent, float shade)
		{
			return Unorm(dirt) | (Unorm(main) << 8) | (Unorm(accent) << 16) | (Unorm(shade) << 24);
		}

		uint32_t TangentOf(const glm::vec3& tangent, float sign)
		{
			const auto snorm = [](float value) { return uint32_t(std::lround(std::clamp(value, -1.0f, 1.0f) * 127.0f)) & 0xFFu; };
			return snorm(tangent.x) | (snorm(tangent.y) << 8) | (snorm(tangent.z) << 16) | (snorm(sign) << 24);
		}

		struct ClipPoint
		{
			Vec2 Position;
			uint32_t Tag = TagGrid;
		};

		template<typename Field>
		void Clip(std::vector<ClipPoint>& polygon, std::vector<ClipPoint>& scratch, uint32_t tag, Field&& field)
		{
			scratch.clear();
			const size_t count = polygon.size();
			for (size_t i = 0; i < count; i++)
			{
				const ClipPoint& a = polygon[i];
				const ClipPoint& b = polygon[(i + 1) % count];
				const float da = field(a.Position);
				const float db = field(b.Position);
				const bool inA = da >= 0.0f;
				const bool inB = db >= 0.0f;
				if (inA)
					scratch.push_back(a);
				if (inA == inB)
					continue;
				scratch.push_back({ Lerp(a.Position, b.Position, da / (da - db)), inA ? tag : a.Tag });
			}
			polygon.swap(scratch);
		}

		struct Mason
		{
			std::vector<ModelVertex>& Vertices;
			std::vector<uint32_t>& Target;

			static float Moss(const glm::vec3& p, float amount)
			{
				const float patch = Noise3(p * 0.55f, 1201u) * 0.65f + Noise3(p * 1.7f, 1213u) * 0.35f;
				return std::clamp(amount * EaseStep((patch - 0.5f) / 0.14f), 0.0f, 1.0f);
			}

			void Add(const glm::vec3& p, const glm::vec3& normal, const glm::vec3& tangent, float u, float v, float accent, float moss, float shade)
			{
				ModelVertex vertex;
				vertex.Position = p;
				vertex.Normal = normal;
				vertex.U = u;
				vertex.V = v;
				vertex.Weights = 255;
				vertex.Tangent = TangentOf(tangent, glm::dot(glm::cross(normal, tangent), glm::vec3(0.0f, 1.0f, 0.0f)) >= 0.0f ? 1.0f : -1.0f);
				const float green = Moss(p, moss);
				vertex.Color = Splat(0.0f, (1.0f - green) * (1.0f - accent), (1.0f - green) * accent, shade);
				Vertices.push_back(vertex);
			}

			void Quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& facing, float accent, float moss, float shade)
			{
				glm::vec3 normal = glm::cross(b - a, c - a);
				if (glm::length(normal) < 1e-7f)
					normal = facing;
				normal = glm::normalize(normal);
				const bool flip = glm::dot(normal, facing) < 0.0f;
				if (flip)
					normal = -normal;
				const bool top = std::abs(normal.y) > 0.7f;
				const bool xFacing = std::abs(normal.x) > std::abs(normal.z);
				const glm::vec3 flat = top || !xFacing ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
				const glm::vec3 tangent = glm::normalize(flat - normal * glm::dot(normal, flat) + glm::vec3(1e-4f));
				const auto uv = [&](const glm::vec3& p) { return top ? glm::vec2(p.x, p.z) : glm::vec2(xFacing ? p.z : p.x, -p.y); };
				const uint32_t base = uint32_t(Vertices.size());
				for (const glm::vec3* p : { &a, &b, &c, &d })
				{
					const glm::vec2 coords = uv(*p);
					Add(*p, normal, tangent, coords.x, coords.y, accent, moss, shade);
				}
				if (flip)
					Target.insert(Target.end(), { base, base + 2, base + 1, base, base + 3, base + 2 });
				else
					Target.insert(Target.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
			}

			void Block(const glm::vec3& center, const glm::vec3& half, const glm::vec3& right, const glm::vec3& forward, float accent, float moss, float shade, float chamfer = 0.035f)
			{
				const glm::vec3 up(0.0f, 1.0f, 0.0f);
				const float hx = std::max(half.x - chamfer, 0.01f);
				const float hz = std::max(half.z - chamfer, 0.01f);
				const glm::vec3 rx = right * half.x;
				const glm::vec3 rz = forward * half.z;
				const glm::vec3 ix = right * hx;
				const glm::vec3 iz = forward * hz;
				const glm::vec3 low = center - up * half.y;
				const glm::vec3 high = center + up * half.y;
				const glm::vec3 rim = high - up * chamfer;
				const std::array<glm::vec3, 4> ring = { -rx - rz, rx - rz, rx + rz, -rx + rz };
				const std::array<glm::vec3, 4> inner = { -ix - iz, ix - iz, ix + iz, -ix + iz };
				const std::array<glm::vec3, 4> facing = { -forward, right, forward, -right };
				for (size_t k = 0; k < 4; k++)
				{
					const size_t n = (k + 1) % 4;
					Quad(low + ring[k], low + ring[n], rim + ring[n], rim + ring[k], facing[k], accent, moss * 0.6f, shade * 0.92f);
					Quad(rim + ring[k], rim + ring[n], high + inner[n], high + inner[k], glm::normalize(facing[k] + up), accent, moss, shade);
				}
				Quad(high + inner[0], high + inner[1], high + inner[2], high + inner[3], up, accent, moss * 0.8f, shade * 1.04f);
			}

			void Bar(const glm::vec3& from, const glm::vec3& to, float width, float depth, const glm::vec3& forward, float moss, float shade)
			{
				const glm::vec3 along = to - from;
				const float length = glm::length(along);
				if (length < 1e-4f)
					return;
				const glm::vec3 axis = along / length;
				const glm::vec3 side = glm::normalize(glm::cross(forward, axis));
				const glm::vec3 w = side * (0.5f * width);
				const glm::vec3 d = forward * (0.5f * depth);
				Quad(from - w + d, to - w + d, to + w + d, from + w + d, forward, 0.0f, moss, shade);
				Quad(from - w - d, to - w - d, to - w + d, from - w + d, -side, 0.0f, moss, shade * 0.84f);
				Quad(from + w + d, to + w + d, to + w - d, from + w - d, side, 0.0f, moss, shade * 0.84f);
			}

			void Beam(const glm::vec3& from, const glm::vec3& to, float width, float shade)
			{
				const glm::vec3 axis = glm::normalize(to - from);
				glm::vec3 side = glm::cross(axis, glm::vec3(0.0f, 1.0f, 0.0f));
				side = glm::length(side) > 1e-3f ? glm::normalize(side) : glm::vec3(1.0f, 0.0f, 0.0f);
				const glm::vec3 up = glm::cross(side, axis);
				const glm::vec3 w = side * (0.5f * width);
				const glm::vec3 h = up * (0.5f * width);
				const std::array<glm::vec3, 4> ring = { -w - h, w - h, w + h, -w + h };
				const std::array<glm::vec3, 4> facing = { -up, side, up, -side };
				for (size_t k = 0; k < 4; k++)
				{
					const size_t n = (k + 1) % 4;
					const glm::vec3 a = from + ring[k];
					const glm::vec3 b = from + ring[n];
					const glm::vec3 c = to + ring[n];
					const glm::vec3 d = to + ring[k];
					const glm::vec3 tangent = glm::normalize(b - a);
					const uint32_t base = uint32_t(Vertices.size());
					const float u0 = glm::dot(a, axis);
					const float u1 = glm::dot(c, axis);
					Add(a, facing[k], tangent, 0.0f, u0, 0.0f, 0.0f, shade);
					Add(b, facing[k], tangent, width, u0, 0.0f, 0.0f, shade);
					Add(c, facing[k], tangent, width, u1, 0.0f, 0.0f, shade);
					Add(d, facing[k], tangent, 0.0f, u1, 0.0f, 0.0f, shade);
					if (glm::dot(glm::cross(b - a, c - a), facing[k]) >= 0.0f)
						Target.insert(Target.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
					else
						Target.insert(Target.end(), { base, base + 2, base + 1, base, base + 3, base + 2 });
				}
			}

			void Lathe(Vec2 center, const std::vector<glm::vec2>& profile, int32_t sides, float accent, float moss, float shade, float phase = 0.0f)
			{
				for (int32_t k = 0; k < sides; k++)
				{
					const float a0 = phase + TwoPi * float(k) / float(sides);
					const float a1 = phase + TwoPi * float(k + 1) / float(sides);
					const Vec2 d0 = FromAngle(a0);
					const Vec2 d1 = FromAngle(a1);
					for (size_t r = 0; r + 1 < profile.size(); r++)
					{
						const glm::vec3 p0(center.x + d0.x * profile[r].x, profile[r].y, center.y + d0.y * profile[r].x);
						const glm::vec3 p1(center.x + d1.x * profile[r].x, profile[r].y, center.y + d1.y * profile[r].x);
						const glm::vec3 p2(center.x + d1.x * profile[r + 1].x, profile[r + 1].y, center.y + d1.y * profile[r + 1].x);
						const glm::vec3 p3(center.x + d0.x * profile[r + 1].x, profile[r + 1].y, center.y + d0.y * profile[r + 1].x);
						const Vec2 middle = FromAngle(0.5f * (a0 + a1));
						const float dr = profile[r + 1].x - profile[r].x;
						const float dy = profile[r + 1].y - profile[r].y;
						Quad(p0, p1, p2, p3, glm::normalize(glm::vec3(middle.x * dy, -dr, middle.y * dy) * (dy >= 0.0f ? 1.0f : -1.0f) + glm::vec3(middle.x, std::abs(dr) > std::abs(dy) ? 1.0f : 0.0f, middle.y) * 0.01f), accent, moss, shade);
					}
				}
			}
		};

		struct Frame
		{
			glm::vec3 Origin{ 0.0f };
			glm::vec3 U{ 1.0f, 0.0f, 0.0f };
			glm::vec3 N{ 0.0f, 0.0f, -1.0f };

			glm::vec3 At(float u, float h, float out = 0.0f) const
			{
				return Origin + U * u + N * out + glm::vec3(0.0f, h, 0.0f);
			}
		};

		Frame MakeFrame(Vec2 center, float angle, float radius, bool outside = false)
		{
			const Vec2 direction = FromAngle(angle);
			Frame frame;
			const Vec2 middle = center + direction * radius;
			frame.Origin = { middle.x, 0.0f, middle.y };
			frame.N = outside ? glm::vec3(direction.x, 0.0f, direction.y) : glm::vec3(-direction.x, 0.0f, -direction.y);
			frame.U = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), frame.N);
			return frame;
		}

		struct ArchShape
		{
			float U = 0.0f;
			float Half = 1.0f;
			float Bottom = 0.0f;
			float Spring = 2.0f;

			float Field(Vec2 p) const
			{
				const float across = Half - std::abs(p.x - U);
				if (p.y < Spring)
					return std::min(across, p.y - Bottom);
				const float reach = 2.0f * Half;
				const float dx = std::abs(p.x - U) + Half;
				return reach - std::sqrt(dx * dx + (p.y - Spring) * (p.y - Spring));
			}

			std::vector<Vec2> Outline(int32_t steps) const
			{
				std::vector<Vec2> points;
				points.push_back({ U - Half, Bottom });
				points.push_back({ U - Half, Spring });
				const float reach = 2.0f * Half;
				for (int32_t s = 1; s <= steps; s++)
				{
					const float angle = (float(s) / float(steps)) * 1.0471976f;
					points.push_back({ U + Half - reach * std::cos(angle), Spring + reach * std::sin(angle) });
				}
				for (int32_t s = steps - 1; s >= 0; s--)
				{
					const float angle = (float(s) / float(steps)) * 1.0471976f;
					points.push_back({ U - Half + reach * std::cos(angle), Spring + reach * std::sin(angle) });
				}
				points.push_back({ U + Half, Bottom });
				return points;
			}
		};

		template<typename Mask>
		void WallPanel(Mason& mason, const Frame& frame, float u0, float u1, float h0, float h1, float out, float accent, float moss, float shade, Mask&& mask)
		{
			constexpr float Cell = 0.36f;
			std::vector<ClipPoint> polygon;
			std::vector<ClipPoint> scratch;
			for (float h = h0; h < h1 - 1e-3f; h += Cell)
				for (float u = u0; u < u1 - 1e-3f; u += Cell)
				{
					const float ue = std::min(u + Cell, u1);
					const float he = std::min(h + Cell, h1);
					polygon = { { Vec2(u, h) }, { Vec2(ue, h) }, { Vec2(ue, he) }, { Vec2(u, he) } };
					Clip(polygon, scratch, TagGrid, mask);
					if (polygon.size() < 3)
						continue;
					const uint32_t base = uint32_t(mason.Vertices.size());
					for (const ClipPoint& point : polygon)
					{
						const glm::vec3 p = frame.At(point.Position.x, point.Position.y, out);
						const float low = 1.0f - EaseStep(point.Position.y / 2.0f);
						mason.Add(p, frame.N, frame.U, point.Position.x, -point.Position.y, accent, moss * (0.25f + 0.9f * low), shade);
					}
					const uint32_t count = uint32_t(polygon.size());
					if (count <= 4)
					{
						for (uint32_t k = 1; k + 1 < count; k++)
							mason.Target.insert(mason.Target.end(), { base, base + k, base + k + 1 });
					}
					else
					{
						Vec2 middle;
						for (const ClipPoint& point : polygon)
							middle += point.Position;
						middle = middle * (1.0f / float(count));
						const glm::vec3 p = frame.At(middle.x, middle.y, out);
						mason.Add(p, frame.N, frame.U, middle.x, -middle.y, accent, moss * 0.25f, shade);
						for (uint32_t k = 0; k < count; k++)
							mason.Target.insert(mason.Target.end(), { base + count, base + k, base + (k + 1) % count });
					}
				}
		}

		void ArchReveal(Mason& mason, const Frame& frame, const ArchShape& arch, float depth, float shade)
		{
			const std::vector<Vec2> points = arch.Outline(7);
			const Vec2 middle(arch.U, arch.Bottom + 0.5f * (arch.Spring - arch.Bottom) + arch.Half);
			for (size_t k = 0; k + 1 < points.size(); k++)
			{
				const glm::vec3 a = frame.At(points[k].x, points[k].y);
				const glm::vec3 b = frame.At(points[k + 1].x, points[k + 1].y);
				const glm::vec3 facing = frame.At(middle.x, middle.y) - 0.5f * (a + b);
				mason.Quad(a, b, b - frame.N * depth, a - frame.N * depth, glm::normalize(facing), 0.0f, 0.1f, shade);
			}
		}

		void ArchFrame(Mason& mason, const Frame& frame, const ArchShape& arch, float width, float proud, float shade)
		{
			const float half = arch.Half + width * 0.5f;
			const ArchShape around{ arch.U, half, arch.Bottom, arch.Spring };
			const std::vector<Vec2> points = around.Outline(7);
			for (size_t k = 0; k + 1 < points.size(); k++)
				mason.Bar(frame.At(points[k].x, points[k].y, proud * 0.5f), frame.At(points[k + 1].x, points[k + 1].y, proud * 0.5f), width, proud, frame.N, 0.25f, shade);
		}

		float WrapNear(float angle, float reference)
		{
			return reference + std::remainder(angle - reference, TwoPi);
		}
	}

	void ObservatoryArt::Clear()
	{
		m_Level = nullptr;
		m_Items.clear();
		m_Armillary.clear();
		m_Fragments.clear();
	}

	void ObservatoryArt::Build(WorldRenderer& renderer, const Level& level)
	{
		Clear();
		m_Level = &level;
		BuildGeometry(renderer, level);
		Dress(renderer, level);
	}

	void ObservatoryArt::BuildGeometry(WorldRenderer& renderer, const Level& level)
	{
		const Rotunda& hall = level.Hall;
		const Plateau& plate = hall.Outline;
		const Vec2 center = hall.Center;
		const float R = hall.Radius;
		constexpr int32_t fixed = 200;
		const uint32_t plain = renderer.GetPlainMaterial();
		const auto slot = [&](std::string_view name) { return renderer.FindMaterialSlot(name).value_or(plain); };
		const uint32_t moss = slot("CitadelMoss");
		const uint32_t dirt = slot("GroundDirt");
		const uint32_t floor = slot("ObservatoryFloor");
		const uint32_t block = slot("ObservatoryBlock");
		const uint32_t ashlar = slot("ObservatoryAshlar");
		const uint32_t cliff = slot("GroundCliff");
		const uint32_t blue = slot("ObservatoryBlue");
		const uint32_t gold = slot("ObservatoryGold");
		const uint32_t bronze = slot("ObservatoryBronze");
		const uint32_t mural = slot("ObservatoryMural");

		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		std::map<std::pair<int32_t, Kind>, std::vector<uint32_t>> lists;
		const auto list = [&](int32_t chunk, Kind kind) -> std::vector<uint32_t>& { return lists[{ chunk, kind }]; };

		const auto renderRim = [&](Vec2 p) { return plate.RimDistance(p) + 0.14f * (Fbm2(p * 1.1f, 919u, 2) - 0.5f); };
		const auto polarVertex = [&](Vec2 p, float reference, float perAngle, Kind kind)
		{
			const Vec2 offset = p - center;
			const float radius = Length(offset);
			const float angle = WrapNear(AngleOf(offset), reference);
			ModelVertex vertex;
			vertex.Position = { p.x, FloorY, p.y };
			vertex.U = angle * perAngle;
			vertex.V = radius;
			vertex.Normal = { 0.0f, 1.0f, 0.0f };
			vertex.Weights = 255;
			const Vec2 tangent = FromAngle(reference + Pi * 0.5f);
			vertex.Tangent = TangentOf({ tangent.x, 0.0f, tangent.y }, -1.0f);
			if (kind == Kind::Floor)
			{
				const float rim = plate.RimDistance(p);
				const float brink = 1.0f - EaseStep(rim / 1.2f);
				const float patch = Fbm2(p * 0.34f, 931u, 4);
				const float joints = 0.05f + 0.2f * EaseStep((Fbm2(p * 0.5f, 937u, 3) - 0.46f) / 0.18f);
				const float overgrown = 0.5f * EaseStep((patch + 0.1f * brink - 0.62f) / 0.1f);
				const float cover = std::clamp(std::max(joints + 0.1f * brink, overgrown), 0.0f, 1.0f);
				const float accent = 0.22f * EaseStep((Fbm2(p * 0.17f, 947u, 3) - 0.44f) / 0.18f) * (1.0f - cover);
				const float soil = 0.35f * EaseStep((Fbm2(p * 0.6f, 953u, 2) - 0.62f) / 0.1f) * (1.0f - cover);
				const float shade = 0.84f + 0.24f * (Fbm2(p * 0.23f, 959u, 3) - 0.35f);
				vertex.Color = Splat(soil, std::max(1.0f - cover - accent - soil, 0.0f), accent, shade);
			}
			return vertex;
		};
		const auto emitPolygon = [&](const std::vector<ClipPoint>& polygon, std::vector<uint32_t>& target, const auto& make)
		{
			if (polygon.size() < 3)
				return;
			const uint32_t base = uint32_t(vertices.size());
			for (const ClipPoint& point : polygon)
				vertices.push_back(make(point.Position));
			const uint32_t count = uint32_t(polygon.size());
			if (count <= 4)
			{
				for (uint32_t k = 1; k + 1 < count; k++)
					target.insert(target.end(), { base, base + k + 1, base + k });
				return;
			}
			Vec2 middle;
			for (const ClipPoint& point : polygon)
				middle += point.Position;
			vertices.push_back(make(middle * (1.0f / float(count))));
			for (uint32_t k = 0; k < count; k++)
				target.insert(target.end(), { base + count, base + (k + 1) % count, base + k });
		};

		struct Segment
		{
			Vec2 A;
			Vec2 B;
			int32_t Chunk = 0;
		};
		std::vector<Segment> holeWalls;
		std::vector<std::vector<Segment>> balconyWalls(hall.Balconies.size());
		std::vector<ClipPoint> polygon;
		std::vector<ClipPoint> scratch;
		const auto sectorOf = [&](Vec2 p) { return std::clamp(int32_t((AngleOf(p - center) + Pi) / TwoPi * 16.0f), 0, 15); };

		struct Band
		{
			float From = 0.0f;
			float To = 0.0f;
			Kind Type = Kind::Floor;
		};
		const Band bands[] = {
			{ 2.8f, 6.1f, Kind::Floor }, { 6.1f, 6.2f, Kind::Gold }, { 6.2f, 6.72f, Kind::Blue }, { 6.72f, 6.82f, Kind::Gold },
			{ 6.82f, 10.1f, Kind::Floor }, { 10.1f, 10.22f, Kind::Gold }, { 10.22f, 10.94f, Kind::Blue }, { 10.94f, 11.06f, Kind::Gold },
			{ 11.06f, R, Kind::Floor },
		};
		constexpr float PlateTile = 4.4f;
		for (const Band& band : bands)
		{
			const float radialStep = band.Type == Kind::Floor ? 0.5f : band.To - band.From;
			const float middleRadius = 0.5f * (band.From + band.To);
			const float tiles = band.Type == Kind::Floor ? std::round(TwoPi * middleRadius / PlateTile) * PlateTile : TwoPi * middleRadius;
			const float perAngle = tiles / TwoPi;
			for (float r0 = band.From; r0 < band.To - 1e-3f; r0 += radialStep)
			{
				const float r1 = std::min(r0 + radialStep, band.To);
				for (int32_t k = 0; k < HallSpokes; k++)
				{
					const float a0 = TwoPi * float(k) / float(HallSpokes);
					const float a1 = TwoPi * float(k + 1) / float(HallSpokes);
					polygon = { { center + FromAngle(a0) * r0, TagGrid }, { center + FromAngle(a0) * r1, TagGrid }, { center + FromAngle(a1) * r1, TagGrid }, { center + FromAngle(a1) * r0, TagGrid } };
					Clip(polygon, scratch, TagOuter, [&](Vec2 p) { return renderRim(p); });
					if (polygon.size() < 3)
						continue;
					const int32_t chunk = sectorOf(center + FromAngle(0.5f * (a0 + a1)) * r0);
					for (size_t n = 0; n < polygon.size(); n++)
					{
						const ClipPoint& a = polygon[n];
						const ClipPoint& b = polygon[(n + 1) % polygon.size()];
						if (a.Tag == TagOuter && LengthSquared(a.Position - b.Position) > 1e-6f && Length(a.Position - center) < 9.0f)
							holeWalls.push_back({ a.Position, b.Position, chunk });
					}
					const float reference = std::remainder(0.5f * (a0 + a1), TwoPi);
					emitPolygon(polygon, list(chunk, band.Type), [&](Vec2 p) { return polarVertex(p, reference, perAngle, band.Type); });
				}
			}
		}

		const auto planarVertex = [&](Vec2 p)
		{
			ModelVertex vertex = polarVertex(p, 0.0f, 1.0f, Kind::Floor);
			vertex.U = p.x;
			vertex.V = p.y;
			vertex.Tangent = TangentOf({ 1.0f, 0.0f, 0.0f }, -1.0f);
			return vertex;
		};
		for (size_t index = 0; index < hall.Balconies.size(); index++)
		{
			const Rotunda::Balcony& balcony = hall.Balconies[index];
			const int32_t chunk = 100 + int32_t(index);
			const Vec2 direction = FromAngle(balcony.Angle);
			const Vec2 side = Perpendicular(direction);
			constexpr int32_t Sides = 40;
			for (float r0 = 0.0f; r0 < balcony.Radius - 1e-3f; r0 += 0.55f)
				for (int32_t k = 0; k < Sides; k++)
				{
					const float r1 = std::min(r0 + 0.55f, balcony.Radius);
					const float a0 = TwoPi * float(k) / float(Sides);
					const float a1 = TwoPi * float(k + 1) / float(Sides);
					polygon = { { balcony.Center + FromAngle(a0) * std::max(r0, 0.001f), TagGrid }, { balcony.Center + FromAngle(a0) * r1, TagGrid }, { balcony.Center + FromAngle(a1) * r1, TagGrid }, { balcony.Center + FromAngle(a1) * std::max(r0, 0.001f), TagGrid } };
					Clip(polygon, scratch, TagGrid, [&](Vec2 p) { return Length(p - center) - R - RotundaWallDepth; });
					if (polygon.size() < 3)
						continue;
					if (r1 >= balcony.Radius - 1e-3f)
					{
						const Vec2 a = balcony.Center + FromAngle(a0) * r1;
						const Vec2 b = balcony.Center + FromAngle(a1) * r1;
						if (Length((a + b) * 0.5f - center) > R + RotundaWallDepth + 0.3f && Dot((a + b) * 0.5f - balcony.Center, direction) > -balcony.Radius * 0.75f)
							balconyWalls[index].push_back({ a, b, chunk });
					}
					emitPolygon(polygon, list(chunk, Kind::Floor), planarVertex);
				}
			const float length = RotundaWallDepth + 0.9f;
			for (float along = 0.0f; along < length - 1e-3f; along += 0.45f)
				for (float across = -RotundaDoorHalf; across < RotundaDoorHalf - 1e-3f; across += 0.48f)
				{
					const Vec2 origin = center + direction * (R + along) + side * across;
					const Vec2 stepA = direction * 0.45f;
					const Vec2 stepB = side * 0.48f;
					polygon = { { origin, TagGrid }, { origin + stepA, TagGrid }, { origin + stepA + stepB, TagGrid }, { origin + stepB, TagGrid } };
					Clip(polygon, scratch, TagGrid, [&](Vec2 p) { return Length(p - center) - R; });
					if (polygon.size() >= 3)
						Clip(polygon, scratch, TagGrid, [&](Vec2 p) { return Length(p - balcony.Center) - balcony.Radius; });
					if (polygon.size() >= 3)
						Clip(polygon, scratch, TagGrid, [&](Vec2 p) { return Length(p - center) - R - RotundaWallDepth - 2.0f < 0.0f ? 1.0f : -1.0f; });
					emitPolygon(polygon, list(chunk, Kind::Floor), planarVertex);
				}
		}

		for (int32_t c = 0; c < 11; c++)
		{
			const float baseAngle = TwoPi * (float(c) + 0.6f * Unit(c, 1, 1231u)) / 11.0f;
			const bool outer = c % 2 == 0;
			const float baseRadius = outer ? 12.2f : 8.45f;
			const int32_t count = 4 + int32_t(3.0f * Unit(c, 2, 1231u));
			Vec2 previous;
			float angle = baseAngle;
			for (int32_t s = 0; s < count; s++)
			{
				const float radius = baseRadius + (outer ? 0.7f : 1.25f) * (2.0f * Unit(c, 10 + s, 1231u) - 1.0f);
				angle += 1.0f / baseRadius * (0.8f + 0.8f * Unit(c, 30 + s, 1231u));
				const Vec2 star = center + FromAngle(angle) * radius;
				if (plate.RimDistance(star) < 0.3f)
				{
					previous = {};
					continue;
				}
				Mason mason{ vertices, list(sectorOf(star), Kind::Gold) };
				const float size = 0.05f + 0.04f * Unit(c, 50 + s, 1231u);
				const glm::vec3 up(0.0f, 1.0f, 0.0f);
				const float y = FloorY + 0.012f;
				const float top = y + 0.005f;
				mason.Quad({ star.x - size, top, star.y }, { star.x, top, star.y + size }, { star.x + size, top, star.y }, { star.x, top, star.y - size }, up, 0.0f, 0.0f, 1.0f);
				if (LengthSquared(previous) > 0.0f)
				{
					const Vec2 along = Normalize(star - previous);
					const Vec2 off = Perpendicular(along) * 0.012f;
					mason.Quad({ previous.x - off.x, y, previous.y - off.y }, { star.x - off.x, y, star.y - off.y }, { star.x + off.x, y, star.y + off.y }, { previous.x + off.x, y, previous.y + off.y }, up, 0.0f, 0.0f, 1.0f);
				}
				previous = star;
			}
		}

		const auto holeAt = [&](Vec2 base, float t)
		{
			const Vec2 away = Normalize(base - center);
			const float depth = (6.5f + 3.5f * Noise2(base * 0.45f, 1001u)) * t;
			const float inward = 1.5f * std::pow(t, 1.6f);
			const Vec2 flat = base - away * inward;
			const glm::vec3 sample(flat.x, -depth, flat.y);
			const float grow = EaseStep((t - 0.04f) / 0.16f);
			const float blocks = Noise3(sample * glm::vec3(0.8f, 0.42f, 0.8f), 811u) - 0.5f;
			const float chips = Noise3(sample * glm::vec3(2.3f, 1.3f, 2.3f), 823u) - 0.5f;
			const float strata = std::sin(depth * 2.4f + 3.0f * Noise2(flat * 0.3f, 829u)) * 0.07f;
			const float push = (blocks * 0.8f + chips * 0.2f + strata) * grow;
			return glm::vec3(flat.x + away.x * push, FloorY - depth + chips * 0.12f * grow, flat.y + away.y * push);
		};
		const auto balconyAt = [&](const Rotunda::Balcony& balcony, Vec2 base, float t)
		{
			const Vec2 flat = Lerp(base, balcony.Center, std::pow(t, 1.25f) * 0.97f);
			return glm::vec3(flat.x, FloorY - 3.4f * t, flat.y);
		};
		const auto cliffStrip = [&](const Segment& wall, std::vector<uint32_t>& target, int32_t tiers, bool dressed, const auto& surface)
		{
			const Vec2 along = Normalize(wall.B - wall.A);
			const Vec2 outward(along.y, -along.x);
			const bool xFacing = std::abs(outward.x) > std::abs(outward.y);
			const uint32_t base = uint32_t(vertices.size());
			for (int32_t c = 0; c < 2; c++)
			{
				const Vec2 foot = c == 0 ? wall.A : wall.B;
				for (int32_t r = 0; r <= tiers; r++)
				{
					const float t = std::pow(float(r) / float(tiers), 1.35f);
					const glm::vec3 position = surface(foot, t);
					const glm::vec3 alongDelta = surface(foot + along * 0.3f, t) - surface(foot - along * 0.3f, t);
					const glm::vec3 downDelta = surface(foot, std::min(t + 0.035f, 1.0f)) - surface(foot, std::max(t - 0.035f, 0.0f));
					glm::vec3 normal = glm::cross(alongDelta, downDelta);
					normal = glm::length(normal) > 1e-6f ? glm::normalize(normal) : glm::vec3(outward.x, 0.0f, outward.y);
					if (glm::dot(normal, glm::vec3(outward.x, 0.0f, outward.y)) < 0.0f && t < 0.9f)
						normal = -normal;
					ModelVertex vertex;
					vertex.Position = position;
					vertex.Normal = normal;
					vertex.U = xFacing ? position.z : position.x;
					vertex.V = -position.y;
					vertex.Weights = 255;
					const glm::vec3 flat = xFacing ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
					const glm::vec3 tangent = glm::normalize(flat - normal * glm::dot(normal, flat) + glm::vec3(1e-4f));
					vertex.Tangent = TangentOf(tangent, glm::dot(glm::cross(normal, tangent), glm::vec3(0.0f, 1.0f, 0.0f)) >= 0.0f ? 1.0f : -1.0f);
					const float depth = FloorY - position.y;
					const float masonry = dressed ? 1.0f : 1.0f - EaseStep((depth - 0.24f - 0.28f * Noise2(foot * 1.1f, 971u)) / 0.22f);
					const float hanging = (1.0f - EaseStep((depth - 0.9f) / (2.0f * Noise2(foot * 0.8f, 977u) + 0.6f))) * (1.0f - masonry) * EaseStep((Fbm2(foot * 0.5f, 983u, 2) - 0.46f) / 0.14f);
					const float ledge = EaseStep((normal.y - 0.3f) / 0.35f) * 0.6f;
					const float green = dressed ? 0.0f : std::clamp(std::max(hanging, ledge), 0.0f, 1.0f);
					const float cavity = 0.78f + 0.3f * Noise3(position * 0.6f, 991u);
					vertex.Color = Splat(0.0f, (1.0f - masonry) * (1.0f - green), masonry, cavity * (masonry > 0.5f ? 0.92f : 1.0f));
					vertices.push_back(vertex);
				}
			}
			for (uint32_t r = 0; r < uint32_t(tiers); r++)
			{
				const uint32_t a0 = base + r;
				const uint32_t b0 = base + uint32_t(tiers + 1) + r;
				target.insert(target.end(), { a0, b0, b0 + 1, a0, b0 + 1, a0 + 1 });
			}
		};
		for (const Segment& wall : holeWalls)
			cliffStrip(wall, list(wall.Chunk, Kind::Rock), CliffTiers, false, holeAt);
		for (size_t index = 0; index < hall.Balconies.size(); index++)
			for (const Segment& wall : balconyWalls[index])
				cliffStrip(wall, list(wall.Chunk, Kind::Rock), 6, true, [&](Vec2 base, float t) { return balconyAt(hall.Balconies[index], base, t); });

		{
			Mason ring{ vertices, list(fixed, Kind::Rock) };
			constexpr int32_t Sides = 150;
			constexpr int32_t Tiers = 10;
			const auto outerPoint = [&](float angle, float t)
			{
				const Vec2 direction = FromAngle(angle);
				const float depth = 0.09f + 21.0f * t;
				const float taper = 8.5f * std::pow(t, 1.8f);
				const glm::vec3 sample(direction.x * 15.0f, -depth * 0.8f, direction.y * 15.0f);
				const float grow = EaseStep((t - 0.03f) / 0.16f);
				const float bump = (Noise3(sample * 0.42f, 1103u) - 0.5f) * 2.0f + (Noise3(sample * 1.2f, 1109u) - 0.5f) * 0.5f;
				const float radius = R + RotundaWallDepth + 0.02f - taper + bump * grow;
				return glm::vec3(center.x + direction.x * radius, FloorY - depth, center.y + direction.y * radius);
			};
			for (int32_t k = 0; k < Sides; k++)
			{
				const float a0 = TwoPi * float(k) / float(Sides);
				const float a1 = TwoPi * float(k + 1) / float(Sides);
				const Vec2 middle = FromAngle(0.5f * (a0 + a1));
				for (int32_t r = 0; r < Tiers; r++)
				{
					const float t0 = std::pow(float(r) / float(Tiers), 1.3f);
					const float t1 = std::pow(float(r + 1) / float(Tiers), 1.3f);
					const float masonry = r == 0 ? 1.0f : 0.0f;
					const uint32_t base = uint32_t(vertices.size());
					const glm::vec3 p0 = outerPoint(a0, t0);
					const glm::vec3 p1 = outerPoint(a1, t0);
					const glm::vec3 p2 = outerPoint(a1, t1);
					const glm::vec3 p3 = outerPoint(a0, t1);
					const glm::vec3 raw = glm::cross(p1 - p0, p3 - p0);
					const glm::vec3 outward(middle.x, 0.0f, middle.y);
					const glm::vec3 normal = glm::normalize(glm::dot(raw, outward) >= 0.0f ? raw : -raw);
					const auto vertex = [&](const glm::vec3& p, float u)
					{
						ModelVertex out;
						out.Position = p;
						out.Normal = normal;
						out.U = u;
						out.V = -p.y;
						out.Weights = 255;
						out.Tangent = TangentOf(glm::normalize(p1 - p0), 1.0f);
						const float green = EaseStep((Noise3(p * 0.5f, 1117u) - 0.56f) / 0.1f) * (r <= 3 ? 0.8f : 0.2f) * (1.0f - masonry);
						out.Color = Splat(0.0f, (1.0f - masonry) * (1.0f - green), masonry, 0.84f + 0.2f * Noise3(p * 0.7f, 1123u));
						return out;
					};
					const float u0 = a0 * (R + 1.0f);
					const float u1 = a1 * (R + 1.0f);
					vertices.push_back(vertex(p0, u0));
					vertices.push_back(vertex(p1, u1));
					vertices.push_back(vertex(p2, u1));
					vertices.push_back(vertex(p3, u0));
					if (glm::dot(glm::cross(p1 - p0, p2 - p0), outward) >= 0.0f)
						ring.Target.insert(ring.Target.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
					else
						ring.Target.insert(ring.Target.end(), { base, base + 2, base + 1, base, base + 3, base + 2 });
				}
				const glm::vec3 inner0(center.x + FromAngle(a0).x * R, FloorY - 0.09f, center.y + FromAngle(a0).y * R);
				const glm::vec3 inner1(center.x + FromAngle(a1).x * R, FloorY - 0.09f, center.y + FromAngle(a1).y * R);
				for (int32_t r = 0; r < 4; r++)
				{
					const float d0 = 2.4f * float(r);
					const float d1 = 2.4f * float(r + 1);
					const Vec2 bump0 = FromAngle(a0) * ((Noise2(Vec2(a0 * 7.0f, float(r)), 1129u) - 0.5f) * (r == 0 ? 0.0f : 0.5f));
					const Vec2 bump1 = FromAngle(a1) * ((Noise2(Vec2(a1 * 7.0f, float(r)), 1129u) - 0.5f) * (r == 0 ? 0.0f : 0.5f));
					const Vec2 bump2 = FromAngle(a1) * ((Noise2(Vec2(a1 * 7.0f, float(r + 1)), 1129u) - 0.5f) * 0.5f);
					const Vec2 bump3 = FromAngle(a0) * ((Noise2(Vec2(a0 * 7.0f, float(r + 1)), 1129u) - 0.5f) * 0.5f);
					ring.Quad(inner0 + glm::vec3(bump0.x, -d0, bump0.y), inner1 + glm::vec3(bump1.x, -d0, bump1.y), inner1 + glm::vec3(bump2.x, -d1, bump2.y), inner0 + glm::vec3(bump3.x, -d1, bump3.y), -glm::vec3(middle.x, 0.0f, middle.y), r == 0 ? 1.0f : 0.0f, 0.0f, 0.82f);
				}
			}
			Mason top{ vertices, list(fixed, Kind::Stone) };
			for (int32_t k = 0; k < Sides; k++)
			{
				const float a0 = TwoPi * float(k) / float(Sides);
				const float a1 = TwoPi * float(k + 1) / float(Sides);
				const Vec2 d0 = FromAngle(a0);
				const Vec2 d1 = FromAngle(a1);
				const float y = FloorY - 0.09f;
				top.Quad({ center.x + d0.x * (R - 0.02f), y, center.y + d0.y * (R - 0.02f) }, { center.x + d1.x * (R - 0.02f), y, center.y + d1.y * (R - 0.02f) }, { center.x + d1.x * (R + RotundaWallDepth + 0.02f), y, center.y + d1.y * (R + RotundaWallDepth + 0.02f) }, { center.x + d0.x * (R + RotundaWallDepth + 0.02f), y, center.y + d0.y * (R + RotundaWallDepth + 0.02f) }, { 0.0f, 1.0f, 0.0f }, 1.0f, 0.35f, 0.82f);
			}
		}

		const float halfBay = RotundaBayDegrees * 0.5f * Pi / 180.0f;
		const float chordHalf = R * std::tan(halfBay);
		Mason stone{ vertices, list(fixed, Kind::Stone) };
		Mason metal{ vertices, list(fixed, Kind::Bronze) };
		Mason fresco{ vertices, list(fixed, Kind::Mural) };
		Mason goldTrim{ vertices, list(fixed, Kind::Gold) };
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		for (const float side : { -1.0f, 1.0f })
			for (int32_t bay = 0; bay < RotundaBaysPerSide; bay++)
			{
				const float angle = RotundaBayAngle(bay, side);
				const Frame frame = MakeFrame(center, angle, R);
				const Frame back = MakeFrame(center, angle, R + RotundaWallDepth, true);
				const float span = chordHalf + 0.02f;
				if (bay == RotundaStepBay)
				{
					const float inward = side;
					for (int32_t row = 0; row < 19; row++)
					{
						const float h0 = FloorY - 0.2f + float(row) * 0.4f;
						float cursor = -span;
						for (int32_t k = 0; cursor < span - 0.02f; k++)
						{
							const float width = 0.6f + 0.4f * Unit(row * 37 + int32_t(side), k, 1433u);
							const float end = std::min(cursor + width, span);
							const float middle = 0.5f * (cursor + end);
							const float rising = 0.5f + 0.5f * (middle * inward / span);
							const float limit = (0.7f + 6.2f * rising * rising) * (0.84f + 0.3f * Noise2(Vec2(middle * 1.1f + side * 9.0f, 3.0f), 1439u));
							if (h0 + 0.4f <= limit)
								stone.Block(frame.At(middle, h0 + 0.2f, -0.5f), { 0.5f * (end - cursor) - 0.006f, 0.195f, 0.5f - 0.02f * Unit(row, k, 1447u) }, frame.U, -frame.N, 0.0f, h0 + 0.9f > limit ? 0.55f : 0.15f, 0.76f + 0.2f * Unit(row, k + 100, 1447u));
							cursor = end;
						}
					}
					continue;
				}
				std::optional<ArchShape> opening;
				bool through = false;
				bool painted = false;
				bool niche = false;
				if (bay == RotundaDoorBay)
				{
					opening = ArchShape{ 0.0f, RotundaDoorHalf, -0.3f, 2.5f };
					through = true;
				}
				else if (bay == RotundaNicheBayA || bay == RotundaNicheBayB)
				{
					opening = ArchShape{ 0.0f, 0.98f, 0.05f, 3.3f };
					niche = true;
				}
				else if (bay == 2)
				{
					opening = ArchShape{ 0.0f, 1.5f, 1.0f, 3.2f };
					painted = true;
				}
				else
				{
					opening = ArchShape{ 0.0f, 0.88f, 1.15f, 3.55f };
					through = true;
				}
				const auto field = [&](Vec2 p) { return opening ? -opening->Field(p) : 1.0f; };
				WallPanel(stone, frame, -span, span, FloorY - 0.3f, FriezeBottom, 0.0f, 1.0f, 0.55f, 0.84f, field);
				const float backSpan = (R + RotundaWallDepth) * std::tan(halfBay) + 0.02f;
				WallPanel(stone, back, -backSpan, backSpan, FloorY - 0.09f, WallHeight, 0.0f, 1.0f, 0.3f, 0.72f, [&](Vec2 p) { return opening && through ? -opening->Field(Vec2(-p.x, p.y)) : 1.0f; });
				if (opening)
				{
					ArchReveal(stone, frame, *opening, niche ? 0.42f : painted ? 0.14f : RotundaWallDepth, 0.7f);
					ArchFrame(stone, frame, *opening, 0.2f, 0.1f, 0.96f);
					if (opening->Bottom > 0.02f)
					{
						const float sillDepth = niche ? 0.42f : painted ? 0.14f : RotundaWallDepth;
						stone.Block(frame.At(0.0f, opening->Bottom - 0.05f, 0.04f - 0.5f * sillDepth), { opening->Half + 0.16f, 0.07f, 0.5f * sillDepth + 0.1f }, frame.U, frame.N, 0.0f, 0.2f, 0.95f, 0.025f);
					}
				}
				if (niche)
				{
					const Frame recess = MakeFrame(center, angle, R + 0.42f);
					WallPanel(stone, recess, -1.0f, 1.0f, 0.05f, 6.0f, 0.0f, 1.0f, 0.1f, 0.42f, [&](Vec2 p) { return opening->Field(p) + 0.02f; });
				}
				if (painted)
				{
					const Frame recess = MakeFrame(center, angle, R + 0.14f);
					std::vector<ClipPoint> shape;
					std::vector<ClipPoint> extra;
					for (float h = 1.0f; h < 6.0f; h += 0.36f)
						for (float u = -1.6f; u < 1.6f; u += 0.36f)
						{
							shape = { { Vec2(u, h) }, { Vec2(u + 0.36f, h) }, { Vec2(u + 0.36f, h + 0.36f) }, { Vec2(u, h + 0.36f) } };
							Clip(shape, extra, TagGrid, [&](Vec2 p) { return opening->Field(p) + 0.02f; });
							if (shape.size() < 3)
								continue;
							const uint32_t first = uint32_t(vertices.size());
							for (const ClipPoint& point : shape)
								fresco.Add(recess.At(point.Position.x, point.Position.y), recess.N, recess.U, point.Position.x + 2.1f + side * 0.9f, 5.5f - point.Position.y, 0.0f, 0.0f, 0.82f);
							for (uint32_t k = 1; k + 1 < uint32_t(shape.size()); k++)
								fresco.Target.insert(fresco.Target.end(), { first, first + k, first + k + 1 });
						}
				}
				if (through && bay != RotundaDoorBay)
				{
					const ArchShape& arch = *opening;
					const auto bar = [&](float u0, float h0, float u1, float h1, float width) { stone.Bar(frame.At(u0, h0, -0.35f), frame.At(u1, h1, -0.35f), width, 0.18f, frame.N, 0.15f, 0.92f); };
					const float tip = arch.Spring + 1.732f * arch.Half;
					bar(0.0f, arch.Bottom, 0.0f, tip - 0.08f, 0.09f);
					bar(-arch.Half * 0.5f, arch.Bottom, -arch.Half * 0.5f, arch.Spring + 0.5f * arch.Half, 0.06f);
					bar(arch.Half * 0.5f, arch.Bottom, arch.Half * 0.5f, arch.Spring + 0.5f * arch.Half, 0.06f);
					for (float h = arch.Bottom + 0.8f; h < arch.Spring + 0.2f; h += 0.8f)
						bar(-arch.Half, h, arch.Half, h, 0.05f);
					bar(-arch.Half * 0.6f, arch.Spring + 0.75f, 0.0f, arch.Spring + 1.25f, 0.05f);
					bar(arch.Half * 0.6f, arch.Spring + 0.75f, 0.0f, arch.Spring + 1.25f, 0.05f);
				}
				WallPanel(metal, frame, -span, span, FriezeBottom, WallHeight - 0.5f, 0.0f, 0.0f, 0.0f, 0.95f, [](Vec2) { return 1.0f; });
				for (int32_t star = 0; star < 3; star++)
				{
					const float u = (float(star) - 1.0f) * 1.2f;
					const float h = 6.55f + 0.14f * std::sin(float(star) * 2.3f);
					const glm::vec3 c = frame.At(u, h, 0.015f);
					const glm::vec3 x = frame.U * 0.13f;
					const glm::vec3 y(0.0f, 0.13f, 0.0f);
					goldTrim.Quad(c - x, c + y, c + x, c - y, frame.N, 0.0f, 0.0f, 1.0f);
				}
				for (int32_t row = 0; row < 2; row++)
				{
					const float h = row == 0 ? FriezeBottom - 0.08f : WallHeight - 0.32f;
					stone.Block(frame.At(0.0f, h + (bay % 2 == 0 ? 0.015f : 0.0f), 0.12f), { span + 0.08f, row == 0 ? 0.1f : 0.2f, 0.16f }, frame.U, frame.N, 0.0f, 0.3f, 0.92f, 0.03f);
				}
				const float capCount = std::round(span * 2.0f / 0.95f);
				for (int32_t k = 0; k < int32_t(capCount); k++)
				{
					const float u = -span + (float(k) + 0.5f) * (2.0f * span / capCount);
					stone.Block(frame.At(u, WallHeight + 0.12f + 0.012f * float((k + bay) % 3), -0.48f + 0.012f * float((k + bay) % 3)), { span / capCount + 0.03f, 0.24f, 0.62f }, frame.U, frame.N, 0.0f, 0.7f, 0.8f + 0.16f * Unit(bay * 11 + int32_t(side), k, 1453u), 0.04f);
				}

				if (bay == RotundaStepBay - 1)
				{
					const Vec2 endDirection = FromAngle(angle - side * halfBay);
					const Vec2 inner = center + endDirection * (R / std::cos(halfBay));
					const Vec2 outer = center + endDirection * ((R + RotundaWallDepth) / std::cos(halfBay));
					const Vec2 facing = Perpendicular(endDirection) * -side;
					stone.Quad({ inner.x, FloorY - 0.09f, inner.y }, { outer.x, FloorY - 0.09f, outer.y }, { outer.x, WallHeight, outer.y }, { inner.x, WallHeight, inner.y }, { facing.x, 0.0f, facing.y }, 1.0f, 0.2f, 0.74f);
				}
				if (bay > 0)
				{
					const Frame edge = MakeFrame(center, angle + side * halfBay, R + 0.1f);
					const float pierHalf = 0.4f;
					const float proud = 0.34f;
					stone.Block(edge.At(0.0f, 0.2f, proud * 0.5f - 0.1f), { pierHalf + 0.06f, 0.2f, proud * 0.5f + 0.16f }, edge.U, edge.N, 0.0f, 0.5f, 0.84f);
					stone.Block(edge.At(0.0f, 2.5f, proud * 0.5f - 0.15f), { pierHalf, 2.2f, proud * 0.5f + 0.1f }, edge.U, edge.N, 0.0f, 0.3f, 0.92f, 0.05f);
				}
				{
					const float rib = 0.13f;
					const float baseH = 4.55f;
					const float tipH = 6.0f;
					for (const float dir : { -1.0f, 1.0f })
					{
						const float u0 = dir * chordHalf;
						for (const float fan : { 0.25f, 0.55f, 0.86f })
						{
							const float u1 = dir * chordHalf * (1.0f - fan);
							const float h1 = baseH + (tipH - baseH) * (0.55f + 0.45f * std::sin(fan * Pi * 0.5f));
							const float lift = (fan < 0.3f ? 0.0f : fan < 0.6f ? 0.014f : 0.028f) + (dir > 0.0f ? 0.007f : 0.0f);
							const glm::vec3 a = frame.At(u0, baseH, 0.1f + lift);
							const glm::vec3 m = frame.At(0.5f * (u0 + u1) + 0.2f * dir * chordHalf * (fan - 0.5f), baseH + 0.62f * (h1 - baseH), 0.09f + lift);
							const glm::vec3 b = frame.At(u1, h1, 0.08f + lift);
							stone.Bar(a, m, rib, 0.14f, frame.N, 0.15f, 0.97f);
							stone.Bar(m, b, rib * 0.8f, 0.12f, frame.N, 0.15f, 0.97f);
						}
					}
				}
				if (bay < RotundaDoorBay + 1)
				{
					const Vec2 direction = FromAngle(angle + side * halfBay);
					const int32_t ribSteps = 7;
					const float reach = 2.8f + 3.4f * Unit(bay, int32_t(side), 1459u);
					const auto ribPoint = [&](Vec2 d, float t, float extra)
					{
						const float r = R + 0.1f - (reach + extra) * t;
						const float h = WallHeight + 0.3f + 3.8f * std::sin(t * Pi * 0.5f);
						return glm::vec3(center.x + d.x * r, h, center.y + d.y * r);
					};
					for (int32_t k = 0; k < ribSteps; k++)
						metal.Beam(ribPoint(direction, float(k) / float(ribSteps), 0.0f), ribPoint(direction, float(k + 1) / float(ribSteps), 0.0f), 0.2f, 0.9f);
					if (bay + 1 < RotundaDoorBay + 1)
					{
						const Vec2 nextDirection = FromAngle(angle - side * halfBay);
						for (const float t : { 0.32f, 0.62f })
						{
							if (Unit(bay, int32_t(t * 10.0f) + int32_t(side) * 5, 1471u) < 0.28f)
								continue;
							metal.Beam(ribPoint(direction, t, 0.0f), ribPoint(nextDirection, t, 0.0f), 0.09f, 0.9f);
						}
					}
				}
			}

		{
			const Vec2 dais = center + Vec2(0.0f, R - 0.3f);
			Mason steps{ vertices, list(fixed, Kind::Stone) };
			steps.Lathe(dais, { { 0.8f, -9.0f }, { 2.45f, -1.2f }, { 2.6f, FloorY }, { 2.6f, 0.24f }, { 2.25f, 0.24f }, { 2.25f, 0.46f }, { 1.9f, 0.46f }, { 1.9f, 0.68f }, { 0.0f, 0.68f } }, 36, 0.0f, 0.25f, 0.9f);
		}

		const auto balustrade = [&](int32_t part, Vec2 middle, float radius, float from, float to, int32_t salt)
		{
			Mason mason{ vertices, list(part, Kind::Stone) };
			const float arc = (to - from) * radius;
			const float pieces = std::max(std::round(arc / 0.65f), 1.0f);
			for (int32_t k = 0; k < int32_t(pieces); k++)
			{
				const float a = from + (float(k) + 0.5f) * (to - from) / pieces;
				const Vec2 direction = FromAngle(a);
				const Vec2 tangent = Perpendicular(direction);
				const Vec2 foot = middle + direction * radius;
				const glm::vec3 right(tangent.x, 0.0f, tangent.y);
				const glm::vec3 forward(direction.x, 0.0f, direction.y);
				const float half = 0.5f * arc / pieces;
				mason.Block({ foot.x, FloorY + 0.01f + 0.01f * float(k % 2), foot.y }, { half + 0.01f, 0.14f, 0.17f }, right, forward, 0.0f, 0.3f, 0.86f, 0.02f);
				const float broken = Fbm2(Vec2(a * radius * 0.5f, float(salt)), 1483u, 2);
				if (broken < 0.42f)
				{
					if (Unit(salt, k, 1487u) < 0.5f)
						mason.Block({ foot.x, FloorY + 0.26f, foot.y }, { 0.14f + 0.12f * Unit(salt, k + 40, 1487u), 0.12f, 0.13f }, right, forward, 0.0f, 0.5f, 0.82f);
					continue;
				}
				for (int32_t b = 0; b < 2; b++)
				{
					const Vec2 at = foot + tangent * ((float(b) - 0.5f) * half);
					mason.Lathe(at, { { 0.055f, FloorY + 0.14f }, { 0.085f, FloorY + 0.22f }, { 0.05f, FloorY + 0.36f }, { 0.075f, FloorY + 0.46f }, { 0.04f, FloorY + 0.56f } }, 6, 0.0f, 0.0f, 0.9f, a);
				}
				if (broken > 0.5f)
					mason.Block({ foot.x, FloorY + 0.62f + 0.01f * float(k % 2), foot.y }, { half + 0.015f, 0.06f, 0.14f }, right, forward, 0.0f, 0.4f, 0.94f, 0.025f);
			}
		};
		const float southStart = RotundaBayAngle(RotundaBaysPerSide - 1, 1.0f) - halfBay;
		const float southEnd = RotundaBayAngle(RotundaBaysPerSide - 1, -1.0f) + halfBay - TwoPi;
		std::vector<std::pair<float, float>> skips;
		for (const Rotunda::Balcony& balcony : hall.Balconies)
		{
			const float a = std::remainder(balcony.Angle + Pi * 0.5f, TwoPi) - Pi * 0.5f;
			skips.push_back({ a - 0.1f, a + 0.1f });
		}
		std::sort(skips.begin(), skips.end());
		float cursor = southEnd;
		for (const auto& [low, high] : skips)
		{
			if (high < southEnd || low > southStart)
				continue;
			if (low > cursor)
				balustrade(fixed, center, R + 0.4f, cursor, low, int32_t(low * 100.0f));
			cursor = std::max(cursor, high);
		}
		if (cursor < southStart)
			balustrade(fixed, center, R + 0.4f, cursor, southStart, 77);
		int32_t balconyIndex = 0;
		for (const Rotunda::Balcony& balcony : hall.Balconies)
		{
			const float toHall = balcony.Angle + Pi;
			const float opening = (RotundaDoorHalf + 0.25f) / balcony.Radius;
			balustrade(100 + balconyIndex, balcony.Center, balcony.Radius + 0.04f, toHall + opening, toHall + TwoPi - opening, 200 + balconyIndex);
			balconyIndex++;
		}

		const auto layer = [&](uint32_t first, uint32_t second) { return first | (second << 16); };
		std::vector<LevelMeshDesc> meshes;
		for (auto& [key, target] : lists)
		{
			if (target.empty())
				continue;
			const Kind kind = key.second;
			LevelMeshDesc desc;
			desc.FirstIndex = uint32_t(indices.size());
			desc.IndexCount = uint32_t(target.size());
			switch (kind)
			{
			case Kind::Floor:
				desc.Material = layer(moss, dirt);
				desc.Layer = layer(floor, block);
				desc.Flags = MeshSplat;
				break;
			case Kind::Stone:
				desc.Material = layer(moss, dirt);
				desc.Layer = layer(ashlar, block);
				desc.Flags = MeshSplat;
				break;
			case Kind::Rock:
				desc.Material = layer(moss, dirt);
				desc.Layer = layer(cliff, block);
				desc.Flags = MeshSplat;
				break;
			case Kind::Blue:
				desc.Material = blue;
				desc.Layer = blue;
				break;
			case Kind::Gold:
				desc.Material = gold;
				desc.Layer = gold;
				break;
			case Kind::Bronze:
				desc.Material = bronze;
				desc.Layer = bronze;
				break;
			case Kind::Mural:
				desc.Material = mural;
				desc.Layer = mural;
				break;
			}
			indices.insert(indices.end(), target.begin(), target.end());
			meshes.push_back(desc);
		}
		const uint32_t base = renderer.AddLevelGeometry(vertices, indices, meshes);
		for (uint32_t index = 0; index < uint32_t(meshes.size()); index++)
		{
			glm::vec3 low(1e9f);
			glm::vec3 high(-1e9f);
			const LevelMeshDesc& desc = meshes[index];
			for (uint32_t i = desc.FirstIndex; i < desc.FirstIndex + desc.IndexCount; i++)
			{
				low = glm::min(low, vertices[indices[i]].Position);
				high = glm::max(high, vertices[indices[i]].Position);
			}
			Item item;
			item.Mesh = base + index;
			item.Instance = renderer.MakeModelInstance(item.Mesh, glm::mat4(1.0f));
			item.Center = { 0.5f * (low.x + high.x), 0.5f * (low.z + high.z) };
			item.Reach = 0.5f * glm::length(glm::vec2(high.x - low.x, high.z - low.z)) + 0.5f * (high.y - low.y);
			m_Items.push_back(item);
		}
	}

	void ObservatoryArt::AddItem(const WorldRenderer& renderer, uint32_t mesh, const glm::mat4& transform, const glm::vec4& tint)
	{
		const ModelMesh& info = renderer.GetModels().GetMesh(mesh);
		Item item;
		item.Mesh = mesh;
		item.Instance = renderer.MakeModelInstance(mesh, transform, NoPalette, tint);
		const glm::vec3 middle = glm::vec3(transform * glm::vec4(0.5f * (info.Min + info.Max), 1.0f));
		item.Center = { middle.x, middle.z };
		const glm::vec3 scale(glm::length(glm::vec3(transform[0])), glm::length(glm::vec3(transform[1])), glm::length(glm::vec3(transform[2])));
		item.Reach = 0.5f * glm::length((info.Max - info.Min) * scale);
		m_Items.push_back(item);
	}

	void ObservatoryArt::Dress(WorldRenderer& renderer, const Level& level)
	{
		const Rotunda& hall = level.Hall;
		const Vec2 center = hall.Center;
		const float R = hall.Radius;
		const ModelLibrary& models = renderer.GetModels();
		const auto group = [&](const char* name)
		{
			std::vector<uint32_t> found;
			if (const std::optional<uint32_t> mesh = models.FindMesh(name))
			{
				found.push_back(*mesh);
				for (uint32_t extra = 1; extra < 4; extra++)
					if (const std::optional<uint32_t> part = models.FindMesh(std::string(name) + "." + std::to_string(extra)))
						found.push_back(*part);
					else
						break;
			}
			return found;
		};
		m_Armillary = { group("Citadel/Armillary_A"), group("Citadel/Armillary_B"), group("Citadel/Armillary_C"), group("Citadel/Armillary_Core") };
		const std::vector<std::vector<uint32_t>> vines = { group("Citadel/Vine_A"), group("Citadel/Vine_B") };
		const std::vector<std::vector<uint32_t>> ivy = { group("Citadel/Ivy_A"), group("Citadel/Ivy_B") };
		const std::vector<std::vector<uint32_t>> carpet = { group("Citadel/GroundIvy_A"), group("Citadel/GroundIvy_B") };
		const std::vector<std::vector<uint32_t>> grass = { group("Nature/Grass_A"), group("Nature/Grass_B"), group("Nature/Grass_C") };
		const std::vector<std::vector<uint32_t>> flowers = { group("Nature/Flowers_A"), group("Nature/Flowers_B") };
		const std::vector<std::vector<uint32_t>> roots = { group("Citadel/Roots_A"), group("Citadel/Roots_B") };
		const auto place = [&](const std::vector<uint32_t>& meshes, const glm::mat4& transform)
		{
			for (const uint32_t mesh : meshes)
				AddItem(renderer, mesh, transform);
		};
		const glm::vec4 leaf(0.66f, 0.72f, 0.54f, 0.0f);
		const auto green = [&](const std::vector<uint32_t>& meshes, const glm::mat4& transform)
		{
			for (const uint32_t mesh : meshes)
				AddItem(renderer, mesh, transform, leaf);
		};
		const auto placed = [](Vec2 p, float y, float yaw, float scale)
		{
			return glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(p.x, y, p.y)), yaw, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(scale));
		};

		const Vec2 dais = center + Vec2(0.0f, R - 0.3f);
		place(group("Citadel/Telescope"), placed(dais + Vec2(0.0f, 0.2f), 0.68f, Pi, 1.25f));

		const auto clearFloor = [&](Vec2 p, float margin)
		{
			if (hall.Outline.RimDistance(p) < margin)
				return false;
			for (const PlacedProp& prop : level.Props)
			{
				const float reach = prop.Id == PropId::ObservatoryDais ? 3.0f : prop.Id == PropId::ObservatoryWall || prop.Id == PropId::ObservatoryJamb ? 0.0f : GetProp(prop.Id).Colliders.empty() ? 0.0f : 1.2f;
				if (reach > 0.0f && LengthSquared(prop.Position - p) < reach * reach)
					return false;
			}
			return true;
		};
		{
			Vec2 spot;
			bool found = false;
			for (int32_t n = 0; n < 40 && !found; n++)
			{
				spot = center + FromAngle(TwoPi * Unit(n, 1, 1489u) + float(level.Seed)) * (8.0f + 1.6f * Unit(n, 2, 1489u));
				found = clearFloor(spot, 2.2f);
				for (const PlacedProp& prop : level.Props)
					found = found && (GetProp(prop.Id).Colliders.empty() || LengthSquared(prop.Position - spot) > 3.0f * 3.0f);
			}
			if (found)
				place(group("Citadel/FallenRing"), placed(spot, FloorY - 0.01f, Unit(1, 3, 1489u) * TwoPi, 0.9f));
		}

		for (int32_t n = 0; n < 320; n++)
		{
			const float angle = TwoPi * Unit(n, 1, 1301u);
			const float radius = 4.4f + (R + 0.05f - 4.4f) * std::sqrt(Unit(n, 2, 1301u));
			const Vec2 p = center + FromAngle(angle) * radius;
			if (!clearFloor(p, 0.3f))
				continue;
			const float nearWall = EaseStep((radius - 10.8f) / 2.5f);
			const float lush = Fbm2(p * 0.34f, 931u, 4) + 0.18f * nearWall;
			const float pick = Unit(n, 3, 1301u);
			const float yaw = TwoPi * Unit(n, 4, 1301u);
			const float scale = 0.6f + 0.5f * Unit(n, 5, 1301u);
			if (lush > 0.64f && pick < 0.32f)
				green(carpet[HashOf(n, 6, 1301u) % carpet.size()], placed(p, FloorY, yaw, scale));
			else if (lush > 0.57f && pick < 0.5f)
				green(grass[HashOf(n, 7, 1301u) % grass.size()], placed(p, FloorY, yaw, scale * 0.7f));
			else if (lush > 0.6f && pick < 0.6f)
				place(flowers[HashOf(n, 8, 1301u) % flowers.size()], placed(p, FloorY, yaw, scale * 0.7f));
		}
		for (const float side : { -1.0f, 1.0f })
			for (int32_t bay = 0; bay < RotundaStepBay; bay++)
			{
				if (bay == 0)
					continue;
				const float angle = RotundaBayAngle(bay, side) + side * RotundaBayDegrees * 0.5f * Pi / 180.0f;
				const Vec2 direction = FromAngle(angle);
				const float yaw = std::atan2(-direction.x, -direction.y);
				const int32_t i = bay * 2 + int32_t(side);
				if (Unit(i, 1, 1503u) < 0.8f)
					green(ivy[HashOf(i, 2, 1503u) % ivy.size()], placed(center + direction * (R - 0.08f), FloorY - 0.1f, yaw, 0.6f + 0.4f * Unit(i, 3, 1503u)));
				if (Unit(i, 4, 1503u) < 0.45f)
				{
					const float offset = RotundaBayDegrees * Pi / 180.0f * (Unit(i, 5, 1503u) - 0.5f);
					const Vec2 at = FromAngle(angle + offset);
					green(vines[HashOf(i, 6, 1503u) % vines.size()], placed(center + at * (R - 0.2f), WallHeight - 0.5f, std::atan2(-at.x, -at.y), 0.65f + 0.3f * Unit(i, 7, 1503u)));
				}
			}
		for (int32_t n = 0; n < 40; n++)
		{
			const float angle = TwoPi * Unit(n, 1, 1511u);
			const Vec2 direction = FromAngle(angle);
			green(vines[size_t(n) % vines.size()], placed(center + direction * (R + 1.02f), FloorY - 0.15f, std::atan2(direction.x, direction.y), 0.8f + 0.7f * Unit(n, 2, 1511u)));
		}
		for (const Rotunda::Balcony& balcony : hall.Balconies)
		{
			const int32_t i = int32_t(&balcony - hall.Balconies.data());
			green(carpet[size_t(i) % carpet.size()], placed(balcony.Center + FromAngle(Unit(i, 1, 1523u) * TwoPi) * balcony.Radius * 0.65f, FloorY, Unit(i, 2, 1523u) * TwoPi, 0.55f));
			for (int32_t k = 0; k < 3; k++)
			{
				const Vec2 direction = FromAngle(balcony.Angle + (float(k) - 1.0f) * 1.0f);
				green(vines[size_t(k) % vines.size()], placed(balcony.Center + direction * (balcony.Radius + 0.1f), FloorY - 0.1f, std::atan2(direction.x, direction.y), 0.55f + 0.2f * Unit(i, k + 5, 1523u)));
			}
		}
		for (int32_t n = 0; n < 7; n++)
		{
			const float angle = TwoPi * (float(n) + 0.5f * Unit(n, 1, 1531u)) / 7.0f;
			const Vec2 direction = FromAngle(angle);
			const Vec2 p = center + direction * (R - 0.75f);
			if (clearFloor(p, 0.2f))
				place(roots[size_t(n) % roots.size()], placed(p, FloorY - 0.02f, std::atan2(direction.x, direction.y) + (n % 2 == 0 ? 1.0f : 2.2f), 0.55f));
		}

		const std::vector<uint32_t> rocks[] = { group("Nature/Rock_A"), group("Nature/Rock_B"), group("Nature/Rock_C"), group("Nature/Rock_D") };
		for (int32_t n = 0; n < 16; n++)
		{
			const std::vector<uint32_t>& meshes = rocks[size_t(n) % std::size(rocks)];
			if (meshes.empty())
				continue;
			const float angle = TwoPi * Unit(n, 1, 1543u);
			const Vec2 direction = FromAngle(angle);
			Fragment fragment;
			fragment.Mesh = meshes[0];
			const float distance = R + 5.5f + 12.0f * Unit(n, 2, 1543u);
			fragment.Position = { center.x + direction.x * distance, -5.0f + 7.5f * Unit(n, 3, 1543u), center.y + direction.y * distance };
			fragment.Axis = glm::normalize(glm::vec3(Unit(n, 4, 1543u) - 0.5f, 0.6f, Unit(n, 5, 1543u) - 0.5f));
			fragment.Scale = 0.5f + 1.5f * Unit(n, 6, 1543u) * Unit(n, 7, 1543u);
			fragment.Phase = TwoPi * Unit(n, 8, 1543u);
			m_Fragments.push_back(fragment);
		}

	}

	void ObservatoryArt::DrawArmillary(WorldRenderer& renderer, const LevelView& view) const
	{
		if (m_Armillary.size() < 4)
			return;
		const Vec2 center = m_Level->Hall.Center;
		const float bob = 1.25f + 0.1f * std::sin(view.Time * 0.6f);
		const glm::mat4 base = glm::translate(glm::mat4(1.0f), glm::vec3(center.x, bob, center.y));
		const float time = view.Time;
		const glm::mat4 rings[3] = {
			base * glm::rotate(glm::mat4(1.0f), time * 0.13f, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), 1.15f, glm::vec3(1.0f, 0.0f, 0.0f)),
			base * glm::rotate(glm::mat4(1.0f), -time * 0.21f, glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f))) * glm::rotate(glm::mat4(1.0f), 0.3f, glm::vec3(0.0f, 0.0f, 1.0f)),
			base * glm::rotate(glm::mat4(1.0f), time * 0.34f, glm::normalize(glm::vec3(1.0f, 0.4f, -0.3f))) * glm::rotate(glm::mat4(1.0f), 1.9f, glm::vec3(0.0f, 1.0f, 0.0f)),
		};
		for (size_t k = 0; k < 3; k++)
			for (const uint32_t mesh : m_Armillary[k])
				renderer.DrawModel(mesh, rings[k]);
		const glm::mat4 core = base * glm::rotate(glm::mat4(1.0f), time * 0.4f, glm::vec3(0.0f, 1.0f, 0.0f));
		for (const uint32_t mesh : m_Armillary[3])
			renderer.DrawModel(mesh, core);
		SpellLight light;
		light.Position = { center.x, bob, center.y };
		light.Radius = 7.5f;
		light.Color = glm::vec3(0.18f, 0.75f, 0.62f) * (0.8f + 0.2f * std::sin(time * 1.3f));
		renderer.DrawLight(light);
	}

	void ObservatoryArt::Draw(WorldRenderer& renderer, const LevelView& view) const
	{
		for (const Item& item : m_Items)
		{
			if (item.Center.x + item.Reach < view.Min.x || item.Center.x - item.Reach > view.Max.x || item.Center.y + item.Reach < view.Min.y || item.Center.y - item.Reach > view.Max.y)
				continue;
			renderer.SubmitModel(item.Mesh, item.Instance);
		}
		DrawArmillary(renderer, view);
		for (const Fragment& fragment : m_Fragments)
		{
			const glm::vec3 position = fragment.Position + glm::vec3(0.0f, 0.35f * std::sin(view.Time * 0.4f + fragment.Phase), 0.0f);
			renderer.DrawModel(fragment.Mesh, glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), position), view.Time * 0.08f + fragment.Phase, fragment.Axis), glm::vec3(fragment.Scale)));
		}
	}

	float ObservatoryArt::GroundHeight() const
	{
		return FloorY;
	}

	bool ObservatoryArt::IsOverSky(Vec2 position) const
	{
		return m_Level->Hall.Outline.RimDistance(position) < 0.0f;
	}
}
