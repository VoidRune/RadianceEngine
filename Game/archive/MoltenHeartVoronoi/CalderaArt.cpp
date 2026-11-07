#include "CalderaArt.h"
#include "Rendering/LevelRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float WallBase = 21.3f;
		constexpr float WallWobble = 0.6f;
		constexpr float SegmentWidth = 0.65f;
		constexpr int32_t Sectors = 24;
		constexpr float ChainScale = 7.0f;
		constexpr float ChainPitch = 0.11f * ChainScale;
		constexpr float ChainSnapSeconds = 1.6f;
		constexpr float PieceSinkSeconds = 1.4f;
		constexpr float CrystalGlow = 0.16f;
		constexpr float VentScale = 1.35f;
		constexpr float SpireScale = 1.25f;
		constexpr float CrustLift = 0.03f;
		constexpr float CrustGrow = 0.15f;
		constexpr float CrustFrost = 1.2f;
		constexpr float SpireStretch = 1.9f;
		constexpr float SpireSink = -0.75f;
		constexpr glm::vec3 SulfurYellow{ 0.95f, 0.74f, 0.1f };
		constexpr glm::vec3 VentLight{ 0.18f, 0.55f, 1.7f };
		constexpr glm::vec3 ConeLight{ 1.8f, 0.6f, 0.15f };
		constexpr glm::vec3 FallLight{ 2.0f, 0.7f, 0.16f };
		constexpr int32_t LightGrid = 128;
		constexpr float LightOrigin = -8.0f;
		constexpr float LightSpan = 64.0f;
		constexpr int32_t LightBlur = 3;

		struct Row
		{
			float Out = 0.0f;
			float Y = 0.0f;
			float Seam = 0.0f;
			int32_t Tier = 0;
		};

		constexpr Row Profile[] = {
			{ -0.6f, -1.2f, 0.0f, 0 },
			{ 0.0f, LavaSurface + 0.02f, 0.5f, 0 },
			{ 0.12f, 1.4f, 0.0f, 0 },
			{ 0.3f, 3.1f, 0.0f, 0 },
			{ 0.62f, 3.24f, 1.0f, 1 },
			{ 1.25f, 3.42f, 0.0f, 1 },
			{ 1.4f, 6.0f, 0.0f, 1 },
			{ 1.6f, 8.4f, 0.0f, 1 },
			{ 3.4f, 8.9f, 0.0f, 2 },
			{ 8.0f, 11.2f, 0.0f, 2 },
			{ 16.0f, 12.8f, 0.0f, 2 },
		};

		float Unit(int32_t x, int32_t y, uint32_t salt)
		{
			return CollapseNoise(x, y, salt);
		}

		uint32_t Pack(const glm::vec3& color, float alpha)
		{
			const glm::uvec3 c(glm::clamp(color, 0.0f, 1.0f) * 255.0f + 0.5f);
			return c.r | (c.g << 8) | (c.b << 16) | (uint32_t(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f) << 24);
		}

		struct Builder
		{
			std::vector<ModelVertex> Vertices;
			std::vector<uint32_t> Indices;
			std::vector<LevelMeshDesc> Meshes;
			uint32_t First = 0;
			float Glass = 0.0f;

			void Tri(const glm::vec3& a, glm::vec3 b, glm::vec3 c, const glm::vec3& facing, const glm::vec3& color, float sa = 0.0f, float sb = 0.0f, float sc = 0.0f)
			{
				glm::vec3 normal = glm::cross(b - a, c - a);
				const float length = glm::length(normal);
				if (length < 1e-9f)
					return;
				if (glm::dot(normal, facing) < 0.0f)
				{
					std::swap(b, c);
					std::swap(sb, sc);
					normal = -normal;
				}
				normal /= length;
				for (const auto& [point, seam] : { std::pair(a, sa), std::pair(b, sb), std::pair(c, sc) })
				{
					Indices.push_back(uint32_t(Vertices.size()));
					Vertices.push_back({ .Position = point, .U = Glass, .Normal = normal, .Color = Pack(color, seam) });
				}
			}

			void Quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& facing, const glm::vec3& color, float sa = 0.0f, float sb = 0.0f, float sc = 0.0f, float sd = 0.0f)
			{
				Tri(a, b, c, facing, color, sa, sb, sc);
				Tri(a, c, d, facing, color, sa, sc, sd);
			}

			uint32_t Close(uint32_t material, uint32_t flags)
			{
				const uint32_t index = uint32_t(Meshes.size());
				Meshes.push_back({ .FirstIndex = First, .IndexCount = uint32_t(Indices.size()) - First, .Material = material, .Layer = material, .Flags = flags });
				First = uint32_t(Indices.size());
				return index;
			}

			uint32_t Upload(WorldRenderer& renderer)
			{
				return renderer.AddLevelGeometry(Vertices, Indices, Meshes);
			}

			void Prism(const glm::vec3& base, const glm::vec3& axis, float radius, float length, int32_t sides, float taper, float tip, float twist, const glm::vec3& color, float seamLow = 0.0f, float seamHigh = 0.0f)
			{
				const glm::vec3 up = glm::normalize(axis);
				glm::vec3 side = std::abs(up.y) < 0.95f ? glm::normalize(glm::cross(up, glm::vec3(0.0f, 1.0f, 0.0f))) : glm::vec3(1.0f, 0.0f, 0.0f);
				const glm::vec3 other = glm::cross(up, side);
				std::vector<glm::vec3> low(static_cast<size_t>(sides)), high(static_cast<size_t>(sides));
				for (int32_t k = 0; k < sides; k++)
				{
					const float angle = twist + 6.2832f * float(k) / float(sides);
					const glm::vec3 direction = side * std::cos(angle) + other * std::sin(angle);
					low[size_t(k)] = base + direction * radius;
					high[size_t(k)] = base + up * (length * (1.0f - tip)) + direction * (radius * taper);
				}
				const glm::vec3 apex = base + up * length;
				for (int32_t k = 0; k < sides; k++)
				{
					const size_t a = size_t(k);
					const size_t b = size_t((k + 1) % sides);
					const glm::vec3 out = glm::normalize((low[a] + low[b]) * 0.5f - base);
					Quad(low[a], low[b], high[b], high[a], out, color, seamLow, seamLow, seamHigh, seamHigh);
					if (tip > 0.0f)
						Tri(high[a], high[b], apex, out + up * 0.5f, color, seamHigh, seamHigh, seamHigh);
					else
						Tri(high[a], high[b], base + up * length, up, color, seamHigh, seamHigh, seamHigh);
					Tri(low[a], low[b], base, -up, color);
				}
			}

			void Ring(const glm::vec3& center, const glm::vec3& axis, float major, float minor, int32_t steps, int32_t sides, const glm::vec3& color)
			{
				const glm::vec3 n = glm::normalize(axis);
				const glm::vec3 u = std::abs(n.y) < 0.95f ? glm::normalize(glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f))) : glm::vec3(1.0f, 0.0f, 0.0f);
				const glm::vec3 v = glm::cross(n, u);
				const auto point = [&](int32_t i, int32_t j)
				{
					const float a = 6.2832f * float(i) / float(steps);
					const float b = 6.2832f * float(j) / float(sides);
					const glm::vec3 radial = u * std::cos(a) + v * std::sin(a);
					return center + radial * (major + minor * std::cos(b)) + n * (minor * std::sin(b));
				};
				for (int32_t i = 0; i < steps; i++)
					for (int32_t j = 0; j < sides; j++)
					{
						const glm::vec3 p00 = point(i, j), p10 = point(i + 1, j), p11 = point(i + 1, j + 1), p01 = point(i, j + 1);
						const float a = 6.2832f * (float(i) + 0.5f) / float(steps);
						const float b = 6.2832f * (float(j) + 0.5f) / float(sides);
						const glm::vec3 radial = u * std::cos(a) + v * std::sin(a);
						Quad(p00, p10, p11, p01, radial * std::cos(b) + n * std::sin(b), color);
					}
			}
		};

		glm::mat4 Place(const glm::vec3& position, float yaw, float scale = 1.0f, float stretch = 1.0f)
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
			transform = glm::rotate(transform, -yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			return glm::scale(transform, glm::vec3(scale, scale * stretch, scale));
		}
	}

	float CalderaArt::WallRadius(float angle) const
	{
		const float phase = 6.2832f * Unit(1, 2, m_Seed);
		return WallBase + WallWobble * (0.5f + 0.3f * std::sin(3.0f * angle + phase) + 0.2f * std::sin(7.0f * angle + 2.1f * phase));
	}

	void CalderaArt::Clear()
	{
		m_Built = false;
		m_Wall.clear();
		m_Falls.clear();
		m_FallFeet.clear();
		m_Pieces.clear();
		m_Anchors.clear();
		m_VentMeshes.clear();
		m_AnchorMeshes.clear();
		m_Obsidian.clear();
		m_VentState.clear();
		m_ChainState.clear();
		m_LightChunks.clear();
		m_LightKey = 0;
	}

	void CalderaArt::Build(WorldRenderer& renderer, const Level& level)
	{
		Clear();
		m_Center = Vec2(float(level.Map.GetWidth()) * 0.5f, float(level.Map.GetHeight()) * 0.5f);
		m_Seed = level.Seed;
		m_ChainLink = renderer.GetModels().FindMesh("Emberhold/ChainLink");
		for (const char* name : { "ObsidianCluster/ObsidianCluster_A", "ObsidianCluster/ObsidianCluster_B", "ObsidianCluster/ObsidianCluster_C" })
			if (const std::optional<uint32_t> mesh = renderer.GetModels().FindMesh(name))
				m_Obsidian.push_back(*mesh);
		BuildWall(renderer);
		BuildFalls(renderer);

		const uint32_t basalt = renderer.FindMaterialSlot("GroundBasalt").value_or(renderer.GetPlainMaterial());
		const uint32_t bronze = renderer.FindMaterialSlot("ObservatoryBronze").value_or(renderer.GetPlainMaterial());
		const uint32_t plain = renderer.GetPlainMaterial();
		const uint32_t rockFlags = MeshVertexTint | MeshCliff | MeshSeam | MeshLavaGlow;
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		Builder pieces;
		for (uint32_t variant = 0; variant < 2; variant++)
		{
			const uint32_t salt = 0xC300u + variant * 97u;
			const int32_t blocks = 7 + int32_t(variant);
			for (int32_t i = 0; i < blocks; i++)
			{
				const float angle = 6.2832f * (float(i) + 0.4f * Unit(i, 1, salt)) / float(blocks);
				const Vec2 direction = FromAngle(angle);
				const float radius = 0.44f + 0.14f * Unit(i, 2, salt);
				const float height = 0.26f + 0.3f * Unit(i, 3, salt);
				const glm::vec3 base(direction.x * radius, -0.12f, direction.y * radius);
				const glm::vec3 axis = glm::normalize(up + glm::vec3(direction.x, 0.0f, direction.y) * (0.25f + 0.25f * Unit(i, 4, salt)));
				pieces.Prism(base, axis, 0.17f + 0.08f * Unit(i, 5, salt), height + 0.12f, 5 + int32_t(Unit(i, 6, salt) * 2.0f), 0.78f, 0.0f, 6.2832f * Unit(i, 7, salt), glm::vec3(0.72f + 0.25f * Unit(i, 8, salt)), 0.0f, 0.05f);
			}
			for (int32_t k = 0; k < 7; k++)
			{
				const float a = 6.2832f * float(k) / 7.0f;
				const float b = 6.2832f * float(k + 1) / 7.0f;
				const glm::vec3 pa(0.36f * std::cos(a), 0.06f, 0.36f * std::sin(a));
				const glm::vec3 pb(0.36f * std::cos(b), 0.06f, 0.36f * std::sin(b));
				pieces.Tri(glm::vec3(0.0f, -0.05f, 0.0f), pa, pb, up, glm::vec3(0.3f), 1.0f, 0.7f, 0.7f);
			}
			m_VentMeshes.push_back(pieces.Close(basalt, rockFlags));
			for (int32_t c = 0; c < 16; c++)
			{
				const float angle = 6.2832f * Unit(c, 11, salt);
				const Vec2 direction = FromAngle(angle);
				const float reach = 0.3f + 0.4f * Unit(c, 12, salt);
				const glm::vec3 base(direction.x * reach, 0.04f + 0.3f * Unit(c, 13, salt) * (1.0f - reach), direction.y * reach);
				const glm::vec3 axis = glm::normalize(up * (0.7f + 0.5f * Unit(c, 14, salt)) + glm::vec3(direction.x, 0.0f, direction.y) * 0.6f + glm::vec3(Unit(c, 15, salt) - 0.5f, 0.0f, Unit(c, 16, salt) - 0.5f) * 0.5f);
				const float tone = 0.8f + 0.25f * Unit(c, 17, salt);
				pieces.Prism(base, axis, 0.025f + 0.03f * Unit(c, 18, salt), 0.12f + 0.2f * Unit(c, 19, salt), 6, 1.0f, 0.35f, 6.2832f * Unit(c, 20, salt), SulfurYellow * tone);
			}
			m_VentMeshes.push_back(pieces.Close(plain, MeshVertexTint));
		}
		for (int32_t k = 0; k < 8; k++)
		{
			const float a = 6.2832f * (float(k) + 0.5f) / 8.0f;
			const float b = 6.2832f * (float(k) + 1.5f) / 8.0f;
			const glm::vec3 la(0.5f * std::cos(a), -0.2f, 0.5f * std::sin(a)), lb(0.5f * std::cos(b), -0.2f, 0.5f * std::sin(b));
			const glm::vec3 ha(0.44f * std::cos(a), 0.42f, 0.44f * std::sin(a)), hb(0.44f * std::cos(b), 0.42f, 0.44f * std::sin(b));
			const glm::vec3 out(std::cos((a + b) * 0.5f), 0.0f, std::sin((a + b) * 0.5f));
			pieces.Quad(la, lb, hb, ha, out, glm::vec3(0.55f));
			pieces.Tri(glm::vec3(0.0f, 0.42f, 0.0f), ha, hb, up, glm::vec3(0.5f));
		}
		pieces.Prism(glm::vec3(0.0f, 0.4f, 0.0f), up, 0.1f, 0.42f, 4, 0.85f, 0.0f, 0.785f, glm::vec3(0.4f));
		m_AnchorMeshes.push_back(pieces.Close(basalt, rockFlags));
		for (int32_t k = 0; k < 8; k++)
		{
			const float a = 6.2832f * (float(k) + 0.5f) / 8.0f;
			const float b = 6.2832f * (float(k) + 1.5f) / 8.0f;
			const glm::vec3 out(std::cos((a + b) * 0.5f), 0.0f, std::sin((a + b) * 0.5f));
			pieces.Quad(glm::vec3(0.52f * std::cos(a), 0.18f, 0.52f * std::sin(a)), glm::vec3(0.52f * std::cos(b), 0.18f, 0.52f * std::sin(b)), glm::vec3(0.5f * std::cos(b), 0.3f, 0.5f * std::sin(b)), glm::vec3(0.5f * std::cos(a), 0.3f, 0.5f * std::sin(a)), out, glm::vec3(1.0f));
		}
		pieces.Ring(glm::vec3(0.12f, 0.98f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.2f, 0.055f, 14, 6, glm::vec3(1.0f));
		m_AnchorMeshes.push_back(pieces.Close(bronze, MeshVertexTint));
		for (int32_t k = 0; k < 12; k++)
		{
			const float a = 6.2832f * float(k) / 12.0f;
			const float b = 6.2832f * float(k + 1) / 12.0f;
			const glm::vec3 pa(0.0f, 1.0f * std::sin(a), 1.0f * std::cos(a)), pb(0.0f, 1.0f * std::sin(b), 1.0f * std::cos(b));
			pieces.Quad(pa - glm::vec3(0.6f, 0.0f, 0.0f), pb - glm::vec3(0.6f, 0.0f, 0.0f), pb + glm::vec3(0.4f, 0.0f, 0.0f), pa + glm::vec3(0.4f, 0.0f, 0.0f), glm::vec3(0.0f, std::sin((a + b) * 0.5f), std::cos((a + b) * 0.5f)), glm::vec3(0.5f));
			pieces.Tri(glm::vec3(0.4f, 0.0f, 0.0f), pa + glm::vec3(0.4f, 0.0f, 0.0f), pb + glm::vec3(0.4f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.45f));
		}
		m_WallPlate = pieces.Close(basalt, rockFlags);
		pieces.Ring(glm::vec3(0.42f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 0.8f, 0.09f, 20, 6, glm::vec3(1.0f));
		pieces.Ring(glm::vec3(0.62f, -0.05f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.24f, 0.07f, 14, 6, glm::vec3(1.0f));
		const uint32_t plateRing = pieces.Close(bronze, MeshVertexTint);

		{
			constexpr Row Cone[] = {
				{ 3.0f, -1.0f, 0.2f, 0 },
				{ 2.75f, -0.35f, 0.3f, 0 },
				{ 2.3f, 0.7f, 0.0f, 0 },
				{ 1.85f, 1.85f, 0.0f, 0 },
				{ 1.45f, 2.85f, 0.0f, 0 },
				{ 1.12f, 3.38f, 0.1f, 0 },
				{ 0.96f, 3.5f, 0.3f, 1 },
				{ 0.78f, 3.15f, 0.8f, 1 },
				{ 0.6f, 2.3f, 1.0f, 1 },
				{ 0.42f, 1.4f, 1.0f, 1 },
			};
			constexpr int32_t Segments = 22;
			for (int32_t s = 0; s < Segments; s++)
			{
				const float a = 6.2832f * float(s) / float(Segments);
				const float b = 6.2832f * float(s + 1) / float(Segments);
				const float jut = 1.0f + 0.12f * (Unit(s, 1, 0xC0E0u) - 0.5f);
				const bool rivulet = Unit(s, 2, 0xC0E0u) < 0.28f;
				const glm::vec3 da(std::cos(a), 0.0f, std::sin(a)), db(std::cos(b), 0.0f, std::sin(b));
				for (size_t k = 0; k + 1 < std::size(Cone); k++)
				{
					const Row& r0 = Cone[k];
					const Row& r1 = Cone[k + 1];
					const float j0 = r0.Tier == 0 ? jut : 1.0f;
					const float j1 = r1.Tier == 0 ? jut : 1.0f;
					const float s0 = rivulet && k >= 1 && k <= 4 ? 0.65f : r0.Seam;
					const float s1 = rivulet && k + 1 >= 1 && k + 1 <= 4 ? 0.65f : r1.Seam;
					const glm::vec3 facing = (r1.Tier == 1 && r0.Tier == 1) ? -(da + db) + glm::vec3(0.0f, 1.0f, 0.0f) : (da + db) + glm::vec3(0.0f, 0.6f, 0.0f);
					pieces.Quad(da * (r0.Out * j0) + glm::vec3(0.0f, r0.Y, 0.0f), db * (r0.Out * j0) + glm::vec3(0.0f, r0.Y, 0.0f), db * (r1.Out * j1) + glm::vec3(0.0f, r1.Y, 0.0f), da * (r1.Out * j1) + glm::vec3(0.0f, r1.Y, 0.0f), facing, glm::vec3(0.62f), s0, s0, s1, s1);
				}
				pieces.Tri(glm::vec3(0.0f, 1.3f, 0.0f), da * 0.42f + glm::vec3(0.0f, 1.4f, 0.0f), db * 0.42f + glm::vec3(0.0f, 1.4f, 0.0f), up, glm::vec3(0.3f), 1.0f, 1.0f, 1.0f);
			}
			m_ConeMesh = pieces.Close(basalt, rockFlags);
		}
		{
			constexpr int32_t Rim = 28;
			std::array<glm::vec3, Rim> edge;
			for (int32_t k = 0; k < Rim; k++)
			{
				const float a = 6.2832f * float(k) / float(Rim);
				const float r = 0.8f + 0.28f * Unit(k, 1, 0xC7A5u);
				edge[size_t(k)] = glm::vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
			}
			for (int32_t k = 0; k < Rim; k++)
			{
				const glm::vec3& a = edge[size_t(k)];
				const glm::vec3& b = edge[size_t((k + 1) % Rim)];
				const glm::vec3 tone(0.14f + 0.06f * Unit(k, 2, 0xC7A5u));
				pieces.Tri(glm::vec3(0.0f, 0.02f, 0.0f), a, b, up, tone, 0.0f, 0.15f, 0.15f);
				const glm::vec3 out = glm::normalize(a + b);
				pieces.Quad(a, b, b * 1.08f - glm::vec3(0.0f, 0.07f, 0.0f), a * 1.08f - glm::vec3(0.0f, 0.07f, 0.0f), out + up, tone * 0.8f, 0.15f, 0.15f, 0.9f, 0.9f);
			}
			m_CrustMesh = pieces.Close(basalt, rockFlags);
		}
		for (uint32_t variant = 0; variant < 2; variant++)
		{
			const uint32_t salt = 0xC5A0u + variant * 131u;
			pieces.Glass = 1.0f;
			const int32_t shards = 6 + int32_t(variant);
			for (int32_t i = 0; i < shards; i++)
			{
				const bool main = i == 0;
				const float angle = 6.2832f * (float(i) + 0.5f * Unit(i, 1, salt)) / float(shards - 1);
				const Vec2 direction = FromAngle(angle);
				const float reach = main ? 0.0f : 0.28f + 0.3f * Unit(i, 2, salt);
				const float length = main ? 3.1f + 0.5f * float(variant) : 0.7f + 1.6f * Unit(i, 3, salt) * Unit(i, 3, salt) + 0.3f;
				const float lean = main ? 0.08f : 0.25f + 0.35f * Unit(i, 4, salt);
				const glm::vec3 base(direction.x * reach, -0.2f, direction.y * reach);
				const glm::vec3 axis = glm::normalize(up + glm::vec3(direction.x, 0.0f, direction.y) * lean + glm::vec3(Unit(i, 5, salt) - 0.5f, 0.0f, Unit(i, 6, salt) - 0.5f) * 0.12f);
				pieces.Prism(base, axis, (main ? 0.34f : 0.11f + 0.11f * Unit(i, 7, salt)), length, 4 + int32_t(Unit(i, 8, salt) * 2.0f), 0.62f, 0.32f, 6.2832f * Unit(i, 9, salt), glm::vec3(0.4f), 0.15f, 0.0f);
			}
			pieces.Glass = 0.0f;
			m_Spires.push_back(pieces.Close(basalt, rockFlags | MeshObsidian));
		}
		const uint32_t base = pieces.Upload(renderer);
		for (uint32_t& mesh : m_VentMeshes)
			mesh += base;
		for (uint32_t& mesh : m_AnchorMeshes)
			mesh += base;
		m_WallPlate += base;
		m_ConeMesh += base;
		m_CrustMesh += base;
		for (uint32_t& mesh : m_Spires)
			mesh += base;
		m_AnchorMeshes.push_back(plateRing + base);

		for (const PlacedProp& prop : level.Props)
		{
			switch (prop.Id)
			{
			case PropId::SulfurVent:
			{
				const size_t variant = size_t(m_VentState.size() % 2);
				m_Pieces.push_back({ prop.Id, prop.Position, prop.Yaw, { m_VentMeshes[variant * 2], m_VentMeshes[variant * 2 + 1] }, VentScale });
				m_VentState.push_back({ prop.Position, glm::vec3(prop.Position.x, 0.4f, prop.Position.y), true });
				break;
			}
			case PropId::ObsidianCluster:
				if (!m_Spires.empty())
					m_Pieces.push_back({ prop.Id, prop.Position, prop.Yaw, { m_Spires[m_Pieces.size() % m_Spires.size()] }, 1.0f, 1.0f });
				break;
			case PropId::MoltenAnchor:
			{
				m_Pieces.push_back({ prop.Id, prop.Position, prop.Yaw, { m_AnchorMeshes[0], m_AnchorMeshes[1] } });
				const Vec2 outward = Normalize(prop.Position - m_Center);
				const float angle = std::atan2(outward.y, outward.x);
				const float reach = WallRadius(angle) + 1.05f;
				const Vec2 wall = m_Center + outward * reach;
				m_Anchors.push_back({ prop.Position, glm::vec3(0.0f), glm::vec3(wall.x, 6.6f, wall.y), -outward });
				m_ChainState.push_back({ glm::vec3(wall.x, 6.6f, wall.y), glm::vec3(prop.Position.x, 0.9f, prop.Position.y), -1.0f });
				break;
			}
			case PropId::MoltenCone:
				m_Cone = prop.Position;
				m_ConeMouth = glm::vec3(prop.Position.x, 3.4f, prop.Position.y);
				break;
			default:
				break;
			}
		}
		m_Built = true;
	}

	void CalderaArt::BuildWall(WorldRenderer& renderer)
	{
		const uint32_t basalt = renderer.FindMaterialSlot("GroundBasalt").value_or(renderer.GetPlainMaterial());
		const uint32_t flags = MeshVertexTint | MeshCliff | MeshSeam | MeshLavaGlow;
		Builder wall;
		const int32_t segments = int32_t(std::round(6.2832f * WallBase / SegmentWidth));
		const auto position = [&](float angle, const Row& row, float jut)
		{
			const Vec2 direction = FromAngle(angle);
			const float radius = WallRadius(angle) + row.Out - (row.Tier < 2 ? jut : 0.0f);
			return glm::vec3(m_Center.x + direction.x * radius, row.Y, m_Center.y + direction.y * radius);
		};
		const auto jutOf = [&](int32_t segment, int32_t tier)
		{
			const int32_t s = (segment % segments + segments) % segments;
			if (tier >= 2)
				return 0.0f;
			return (tier == 0 ? 0.36f : 0.42f) * Unit(s, tier, m_Seed ^ 0xC1A5u);
		};
		for (int32_t sector = 0; sector < Sectors; sector++)
		{
			Patch patch;
			for (int32_t s = sector * segments / Sectors; s < (sector + 1) * segments / Sectors; s++)
			{
				const float a = 6.2832f * float(s) / float(segments);
				const float b = 6.2832f * float(s + 1) / float(segments);
				const Vec2 middle = FromAngle((a + b) * 0.5f);
				const glm::vec3 inward(-middle.x, 0.0f, -middle.y);
				const glm::vec3 tangent(-middle.y, 0.0f, middle.x);
				const float tone = 0.48f + 0.2f * Unit(s, 9, m_Seed);
				for (size_t k = 0; k + 1 < std::size(Profile); k++)
				{
					const Row& r0 = Profile[k];
					const Row& r1 = Profile[k + 1];
					const float j0 = jutOf(s, r0.Tier);
					const float j1 = jutOf(s, r1.Tier);
					const glm::vec3 facing = inward + glm::vec3(0.0f, (r1.Out - r0.Out) / std::max(r1.Y - r0.Y, 0.05f) * 0.5f + 0.15f, 0.0f);
					wall.Quad(position(a, r0, j0), position(b, r0, j0), position(b, r1, j1), position(a, r1, j1), facing, glm::vec3(tone), r0.Seam, r0.Seam, r1.Seam, r1.Seam);
					const float n0 = jutOf(s + 1, r0.Tier);
					const float n1 = jutOf(s + 1, r1.Tier);
					if (r0.Tier < 2 && std::abs(j0 - n0) + std::abs(j1 - n1) > 0.01f)
						wall.Quad(position(b, r0, j0), position(b, r1, j1), position(b, r1, n1), position(b, r0, n0), j0 > n0 ? tangent : -tangent, glm::vec3(tone * 0.85f), r0.Seam, r1.Seam, r1.Seam, r0.Seam);
				}
				for (const float angle : { a, b })
				{
					const Vec2 d = FromAngle(angle);
					const float far = WallRadius(angle) + Profile[std::size(Profile) - 1].Out;
					patch.Min = { std::min(patch.Min.x, m_Center.x + d.x * (WallBase - 1.0f)), std::min(patch.Min.y, m_Center.y + d.y * (WallBase - 1.0f)) };
					patch.Max = { std::max(patch.Max.x, m_Center.x + d.x * (WallBase - 1.0f)), std::max(patch.Max.y, m_Center.y + d.y * (WallBase - 1.0f)) };
					patch.Min = { std::min(patch.Min.x, m_Center.x + d.x * far), std::min(patch.Min.y, m_Center.y + d.y * far) };
					patch.Max = { std::max(patch.Max.x, m_Center.x + d.x * far), std::max(patch.Max.y, m_Center.y + d.y * far) };
				}
			}
			patch.Mesh = wall.Close(basalt, flags);
			m_Wall.push_back(patch);
		}
		const uint32_t base = wall.Upload(renderer);
		for (Patch& patch : m_Wall)
			patch.Mesh += base;
	}

	void CalderaArt::BuildFalls(WorldRenderer& renderer)
	{
		Builder falls;
		const float offsets[] = { -0.78f, 0.6f, 1.45f };
		for (const float offset : offsets)
		{
			const float center = 0.5f * 3.14159f + offset + 0.12f * (Unit(int32_t(offset * 10.0f), 3, m_Seed) - 0.5f);
			const float width = (0.75f + 0.35f * Unit(int32_t(offset * 10.0f), 4, m_Seed)) / WallBase;
			constexpr int32_t Across = 4;
			constexpr float Shares[Across + 1] = { 0.0f, 0.65f, 1.0f, 0.65f, 0.0f };
			Patch patch;
			for (size_t k = 1; k + 1 < std::size(Profile) - 1; k++)
			{
				const Row& r0 = Profile[k];
				const Row& r1 = Profile[k + 1];
				for (int32_t i = 0; i < Across; i++)
				{
					const float a = center - width + 2.0f * width * float(i) / float(Across);
					const float b = center - width + 2.0f * width * float(i + 1) / float(Across);
					const auto point = [&](float angle, const Row& row, float share)
					{
						const Vec2 d = FromAngle(angle);
						const float bulge = 0.08f + 0.06f * share;
						const float radius = WallRadius(angle) + row.Out - (row.Tier < 2 ? 0.46f + bulge : bulge * 0.5f);
						return glm::vec3(m_Center.x + d.x * radius, row.Y + 0.03f, m_Center.y + d.y * radius);
					};
					const Vec2 d = FromAngle((a + b) * 0.5f);
					falls.Quad(point(a, r0, Shares[i]), point(b, r0, Shares[i + 1]), point(b, r1, Shares[i + 1]), point(a, r1, Shares[i]), glm::vec3(-d.x, 0.3f, -d.y), glm::vec3(1.0f), Shares[i], Shares[i + 1], Shares[i + 1], Shares[i]);
					for (const glm::vec3& p : { point(a, r0, 0.0f), point(b, r1, 0.0f) })
					{
						patch.Min = { std::min(patch.Min.x, p.x), std::min(patch.Min.y, p.z) };
						patch.Max = { std::max(patch.Max.x, p.x), std::max(patch.Max.y, p.z) };
					}
				}
			}
			patch.Mesh = falls.Close(renderer.GetPlainMaterial(), MeshVertexTint | MeshFlow);
			m_Falls.push_back(patch);
			const Vec2 foot = m_Center + FromAngle(center) * (WallRadius(center) - 0.7f);
			m_FallFeet.push_back(glm::vec3(foot.x, LavaSurface + 0.05f, foot.y));
		}
		const uint32_t base = falls.Upload(renderer);
		for (Patch& patch : m_Falls)
			patch.Mesh += base;
	}

	void CalderaArt::UpdateLavaLight(WorldRenderer& renderer, const IslandArt& island) const
	{
		const float step = LightSpan / float(LightGrid);
		if (m_LightChunks.empty())
		{
			m_LightChunks.resize(size_t(LightGrid) * LightGrid);
			for (int32_t y = 0; y < LightGrid; y++)
				for (int32_t x = 0; x < LightGrid; x++)
				{
					const Vec2 p(LightOrigin + (float(x) + 0.5f) * step, LightOrigin + (float(y) + 0.5f) * step);
					const Vec2 offset = p - m_Center;
					const bool rock = Length(offset) > WallRadius(std::atan2(offset.y, offset.x)) - 0.2f || LengthSquared(p - m_Cone) < 2.4f * 2.4f;
					m_LightChunks[size_t(y) * LightGrid + size_t(x)] = rock ? -2 : island.ChunkIndexAt(p);
				}
		}
		const uint64_t key = island.SolidKey();
		if (key == m_LightKey)
			return;
		m_LightKey = key;
		std::vector<float> lava(m_LightChunks.size());
		for (size_t i = 0; i < lava.size(); i++)
			lava[i] = m_LightChunks[i] == -2 ? 0.0f : m_LightChunks[i] < 0 ? 1.0f : island.IsChunkSolid(m_LightChunks[i]) ? 0.0f : 1.0f;
		std::vector<float> blurred(lava.size());
		for (int32_t pass = 0; pass < 2; pass++)
		{
			for (int32_t y = 0; y < LightGrid; y++)
				for (int32_t x = 0; x < LightGrid; x++)
				{
					float sum = 0.0f;
					for (int32_t k = -LightBlur; k <= LightBlur; k++)
						sum += pass == 0 ? lava[size_t(y) * LightGrid + size_t(std::clamp(x + k, 0, LightGrid - 1))] : lava[size_t(std::clamp(y + k, 0, LightGrid - 1)) * LightGrid + size_t(x)];
					blurred[size_t(y) * LightGrid + size_t(x)] = sum / float(2 * LightBlur + 1);
				}
			lava.swap(blurred);
		}
		std::vector<uint8_t> coverage(lava.size());
		for (size_t i = 0; i < lava.size(); i++)
			coverage[i] = uint8_t(std::clamp(lava[i], 0.0f, 1.0f) * 255.0f + 0.5f);
		renderer.SetLavaLight(uint32_t(LightGrid), uint32_t(LightGrid), coverage, glm::vec2(LightOrigin), glm::vec2(LightSpan));
	}

	void CalderaArt::DrawChain(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, float sag, float scale) const
	{
		if (!m_ChainLink)
			return;
		const auto along = [&](float s) { return glm::mix(from, to, s) - glm::vec3(0.0f, 4.0f * sag * s * (1.0f - s), 0.0f); };
		float length = 0.0f;
		for (int32_t i = 0; i < 16; i++)
			length += glm::length(along(float(i + 1) / 16.0f) - along(float(i) / 16.0f));
		const int32_t count = std::max(2, int32_t(std::ceil(length / (ChainPitch * scale / ChainScale))));
		for (int32_t i = 0; i < count; i++)
		{
			const float s = (float(i) + 0.5f) / float(count);
			const glm::vec3 point = along(s);
			if (point.y < LavaSurface - 0.4f)
				continue;
			const glm::vec3 tangent = glm::normalize(along(std::min(s + 0.01f, 1.0f)) - along(std::max(s - 0.01f, 0.0f)));
			glm::vec3 side = glm::cross(tangent, glm::vec3(0.0f, 1.0f, 0.0f));
			side = glm::length(side) > 1e-4f ? glm::normalize(side) : glm::vec3(1.0f, 0.0f, 0.0f);
			const glm::vec3 up = glm::cross(side, tangent);
			const bool turned = (i & 1) != 0;
			glm::mat4 transform(1.0f);
			transform[0] = glm::vec4(tangent * scale, 0.0f);
			transform[1] = glm::vec4((turned ? side : up) * scale, 0.0f);
			transform[2] = glm::vec4((turned ? -up : side) * scale, 0.0f);
			transform[3] = glm::vec4(point, 1.0f);
			const float heat = std::clamp(1.0f - (point.y - LavaSurface) / 1.2f, 0.0f, 1.0f);
			renderer.DrawModel(*m_ChainLink, transform, NoPalette, glm::vec4(0.8f, 0.7f, 0.64f, 0.0f), 0.25f * heat * heat, point.y);
		}
	}

	void CalderaArt::Draw(WorldRenderer& renderer, const LevelView& view, const IslandArt& island) const
	{
		if (!m_Built)
			return;
		UpdateLavaLight(renderer, island);
		const auto visible = [&](const Patch& patch, float margin)
		{
			return !(patch.Max.x < view.Min.x - margin || patch.Min.x > view.Max.x + margin || patch.Max.y < view.Min.y - margin || patch.Min.y > view.Max.y + margin);
		};
		for (const Patch& patch : m_Wall)
			if (visible(patch, 6.0f))
				renderer.DrawModel(patch.Mesh, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, 0.0f);
		for (size_t i = 0; i < m_Falls.size(); i++)
		{
			if (!visible(m_Falls[i], 6.0f))
				continue;
			ModelInstance instance = renderer.MakeModelInstance(m_Falls[i].Mesh, glm::mat4(1.0f));
			instance.Shading.z = view.Time;
			instance.Shading.w = 0.0f;
			renderer.SubmitModel(m_Falls[i].Mesh, instance);
			renderer.DrawLight({ .Position = m_FallFeet[i] + glm::vec3(0.0f, 1.2f, 0.0f), .Radius = 7.0f, .Color = FallLight });
		}
		if (m_Cone.x != 0.0f || m_Cone.y != 0.0f)
		{
			renderer.DrawModel(m_ConeMesh, Place(glm::vec3(m_Cone.x, 0.0f, m_Cone.y), 0.0f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, 0.0f);
			const float fury = view.Brawl ? CalderaFury(view.Brawl->Plan, view.Brawl->Clock) : 0.2f;
			renderer.DrawLight({ .Position = m_ConeMouth + glm::vec3(0.0f, 1.5f, 0.0f), .Radius = 9.0f + 5.0f * fury, .Color = ConeLight * (0.8f + 0.8f * fury) });
		}
		if (view.Brawl)
			for (const Crust& crust : CrustsOf(view.Brawl->Plan))
			{
				const float age = view.Brawl->Clock - crust.Time;
				if (age < 0.0f || age >= CrustSeconds)
					continue;
				const float grow = std::min(age / CrustGrow, 1.0f);
				const float melt = std::clamp((age - (CrustSeconds - CrustMelt)) / CrustMelt, 0.0f, 1.0f);
				const float frost = std::max(1.0f - age / CrustFrost, 0.0f);
				const glm::vec3 center(crust.Center.x, LavaSurface + CrustLift - 0.08f * melt * melt, crust.Center.y);
				const float yaw = 6.2832f * Unit(int32_t(crust.Center.x * 13.0f), int32_t(crust.Center.y * 13.0f), 0xC7A6u);
				renderer.DrawModel(m_CrustMesh, Place(center, yaw, crust.Radius * (0.6f + 0.4f * grow)), NoPalette, glm::vec4(0.6f, 0.82f, 1.0f, 0.55f * frost), 0.3f * frost + 0.15f * melt, center.y);
			}
		std::vector<glm::vec3> rings(m_Anchors.size(), glm::vec3(0.0f));
		std::vector<bool> holding(m_Anchors.size(), false);
		size_t vent = 0;
		size_t anchor = 0;
		for (const Piece& piece : m_Pieces)
		{
			const std::optional<IslandCarry> carry = island.CarryAt(view, piece.Position);
			const bool isVent = piece.Id == PropId::SulfurVent;
			const bool isAnchor = piece.Id == PropId::MoltenAnchor;
			if (isVent)
				m_VentState[vent].Alive = carry.has_value() && carry->Fall <= 0.0f;
			if (carry)
			{
				const float lift = -0.04f;
				const glm::mat4 transform = carry->Transform * Place(glm::vec3(piece.Position.x, carry->Ground + lift, piece.Position.y), piece.Yaw, piece.Scale, piece.Stretch);
				for (size_t m = 0; m < piece.Meshes.size(); m++)
				{
					const float glow = isVent && m == 1 ? CrystalGlow : carry->Heat;
					renderer.DrawModel(piece.Meshes[m], transform, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), glow, carry->Ground);
				}
				if (isVent)
				{
					m_VentState[vent].Mouth = glm::vec3(transform * glm::vec4(0.0f, 0.05f, 0.0f, 1.0f)) + glm::vec3(0.0f, 0.0f, 0.0f);
					if (m_VentState[vent].Alive)
						renderer.DrawLight({ .Position = m_VentState[vent].Mouth + glm::vec3(0.0f, 0.55f, 0.0f), .Radius = 4.6f, .Color = VentLight });
				}
				if (isAnchor)
				{
					rings[anchor] = glm::vec3(transform * glm::vec4(0.32f, 0.98f, 0.0f, 1.0f));
					holding[anchor] = carry->Fall <= 0.0f;
				}
			}
			if (isAnchor && !carry)
				rings[anchor] = glm::vec3(piece.Position.x, LavaSurface - 1.0f, piece.Position.y);
			vent += isVent ? 1 : 0;
			anchor += isAnchor ? 1 : 0;
		}
		for (size_t i = 0; i < m_Anchors.size(); i++)
		{
			const Anchor& a = m_Anchors[i];
			const Vec2 out = -a.Inward;
			const float yaw = std::atan2(out.y, out.x);
			renderer.DrawModel(m_WallPlate, Place(a.Wall, yaw + 3.14159f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, a.Wall.y);
			renderer.DrawModel(m_AnchorMeshes[2], Place(a.Wall, yaw + 3.14159f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, a.Wall.y);
			const glm::vec3 hook = a.Wall + glm::vec3(-out.x * 0.62f, -0.05f, -out.y * 0.62f);
			CalderaChain& state = m_ChainState[i];
			if (holding[i])
			{
				state.Snapped = -1.0f;
				state.Anchor = rings[i];
				DrawChain(renderer, rings[i], hook, 0.7f, ChainScale);
				continue;
			}
			if (state.Snapped < 0.0f)
				state.Snapped = view.Time;
			const float t = std::clamp((view.Time - state.Snapped) / ChainSnapSeconds, 0.0f, 1.0f);
			const glm::vec3 below = hook + glm::vec3(-out.x * 1.4f, 0.0f, -out.y * 1.4f);
			const glm::vec3 rest(below.x, LavaSurface - 1.2f, below.z);
			const glm::vec3 start = state.Anchor.y > -50.0f && LengthSquared(Vec2(state.Anchor.x, state.Anchor.z)) > 0.0f ? state.Anchor : rest;
			const float ease = t * t * (3.0f - 2.0f * t);
			const glm::vec3 end = glm::mix(start, rest, ease) - glm::vec3(0.0f, 2.5f * std::sin(ease * 3.14159f) * (1.0f - ease), 0.0f);
			DrawChain(renderer, end, hook, 0.7f + 1.6f * ease, ChainScale);
		}
	}
}
