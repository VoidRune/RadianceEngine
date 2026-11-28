#include "SpellVisuals.h"
#include "Rendering/Models.h"
#include "Rendering/WorldRenderer.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace Game
{
	namespace
	{
		constexpr glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
		constexpr glm::vec3 SpikeEmber{ 0.9f, 0.26f, 0.06f };
		constexpr glm::vec3 SpikeDust{ 0.05f, 0.04f, 0.03f };
		constexpr glm::vec4 SpikeModelTint{ 1.0f, 1.0f, 1.0f, 0.0f };
		constexpr glm::vec4 SpikeModelRim{ 0.16f, 0.12f, 0.08f, 0.3f };
		constexpr glm::vec3 MeltEdge{ 0.9f, 2.4f, 4.2f };
		constexpr glm::vec3 WardBurn{ 2.6f, 2.2f, 1.4f };
		constexpr glm::vec3 SpellIceAbsorption{ 1.4f, 0.5f, 0.16f };

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

		struct CrystalRing
		{
			float X = 0.0f;
			float Radius = 0.0f;
			float Twist = 0.0f;
		};

		uint32_t PackColor(const glm::vec3& color)
		{
			const glm::uvec3 bytes(glm::clamp(color, 0.0f, 1.0f) * 255.0f + 0.5f);
			return bytes.x | bytes.y << 8 | bytes.z << 16 | 0xFF000000u;
		}

		void AppendCrystal(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, std::span<const CrystalRing> rings, uint32_t sides, float flatten, uint32_t seed, const glm::vec3& root, const glm::vec3& tip)
		{
			const auto point = [&](size_t ring, uint32_t side)
			{
				const CrystalRing& r = rings[ring];
				if (r.Radius <= 0.0f)
					return glm::vec3(r.X, 0.0f, 0.0f);
				const float angle = (float(side) + 0.5f * float(ring % 2)) / float(sides) * 6.2832f + r.Twist;
				const float radius = r.Radius * (0.86f + 0.28f * Unit(Mix(seed + uint32_t(ring) * 131u, side)));
				return glm::vec3(r.X, radius * std::sin(angle) * flatten, radius * std::cos(angle));
			};
			const float first = rings.front().X;
			const float last = rings.back().X;
			const auto face = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, float along, uint32_t key)
			{
				glm::vec3 normal = glm::cross(b - a, c - a);
				const float area = glm::length(normal);
				if (area < 1e-9f)
					return;
				normal /= area;
				const glm::vec3 middle = (a + b + c) / 3.0f;
				if (glm::dot(normal, glm::vec3(0.0f, middle.y, middle.z)) < 0.0f)
				{
					std::swap(b, c);
					normal = -normal;
				}
				const float shade = 0.82f + 0.18f * Unit(Mix(seed ^ 0x5BD1E995u, key));
				const glm::vec3 color = glm::mix(root, tip, std::pow(std::clamp(along, 0.0f, 1.0f), 1.6f)) * shade;
				const uint32_t base = uint32_t(vertices.size());
				for (const glm::vec3& corner : { a, b, c })
				{
					ModelVertex vertex;
					vertex.Position = corner;
					vertex.Normal = normal;
					vertex.Color = PackColor(color);
					vertices.push_back(vertex);
				}
				indices.insert(indices.end(), { base, base + 1, base + 2 });
			};
			for (size_t ring = 0; ring + 1 < rings.size(); ring++)
				for (uint32_t side = 0; side < sides; side++)
				{
					const uint32_t next = (side + 1) % sides;
					const glm::vec3 a = point(ring, side);
					const glm::vec3 b = point(ring, next);
					const glm::vec3 c = point(ring + 1, next);
					const glm::vec3 d = point(ring + 1, side);
					const float along = ((rings[ring].X + rings[ring + 1].X) * 0.5f - first) / (last - first);
					const uint32_t key = uint32_t(ring) * 64u + side;
					if (rings[ring].Radius > 0.0f)
						face(a, b, c, along, key * 2u);
					if (rings[ring + 1].Radius > 0.0f)
						face(a, c, d, along, key * 2u + 1u);
				}
		}

		struct RockRing
		{
			float Y = 0.0f;
			float Radius = 0.0f;
			float X = 0.0f;
			float Z = 0.0f;
		};

		uint32_t PackTangent(const glm::vec3& tangent, float sign)
		{
			const auto snorm = [](float value) { return uint32_t(int32_t(std::lround(std::clamp(value, -1.0f, 1.0f) * 511.0f))) & 0x3FFu; };
			return snorm(tangent.x) | (snorm(tangent.y) << 10) | (snorm(tangent.z) << 20) | (sign < 0.0f ? 1u << 30 : 0u);
		}

		void AppendRock(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, std::span<const RockRing> rings, uint32_t sides, const glm::vec3& origin, const glm::vec3& axis, float size, uint32_t seed)
		{
			const glm::vec3 lean = glm::normalize(axis);
			const glm::vec3 hinge = glm::cross(Up, lean);
			const float sine = glm::length(hinge);
			const glm::vec3 pivot = sine > 1e-5f ? hinge / sine : glm::vec3(1.0f, 0.0f, 0.0f);
			const float cosine = glm::dot(Up, lean);
			const auto place = [&](const glm::vec3& p)
			{
				const glm::vec3 turned = p * cosine + glm::cross(pivot, p) * sine + pivot * (glm::dot(pivot, p) * (1.0f - cosine));
				return origin + turned * size;
			};
			const auto center = [&](size_t ring) { return place(glm::vec3(rings[ring].X, rings[ring].Y, rings[ring].Z)); };
			const auto point = [&](size_t ring, uint32_t side)
			{
				const RockRing& r = rings[ring];
				glm::vec3 local(r.X, r.Y, r.Z);
				if (r.Radius > 0.0f)
				{
					const uint32_t key = Mix(seed + uint32_t(ring) * 131u, side);
					const float angle = (float(side) + 0.5f * float(ring % 2) + 0.4f * (Unit(Mix(key, 3u)) - 0.5f)) / float(sides) * 6.2832f;
					const float radius = r.Radius * (0.68f + 0.55f * Unit(key));
					local += glm::vec3(std::cos(angle) * radius * 1.12f, (Unit(Mix(key, 5u)) - 0.5f) * 0.1f * r.Radius, std::sin(angle) * radius);
				}
				return place(local);
			};
			const auto face = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, const glm::vec3& core, uint32_t key)
			{
				glm::vec3 normal = glm::cross(b - a, c - a);
				const float area = glm::length(normal);
				if (area < 1e-9f)
					return;
				normal /= area;
				const glm::vec3 middle = (a + b + c) / 3.0f;
				if (glm::dot(normal, middle - core) < 0.0f)
				{
					std::swap(b, c);
					normal = -normal;
				}
				const glm::vec3 tangent = std::abs(normal.y) < 0.97f ? glm::normalize(glm::cross(Up, normal)) : glm::vec3(1.0f, 0.0f, 0.0f);
				const glm::vec3 bitangent = glm::cross(normal, tangent);
				const float shade = 0.78f + 0.22f * Unit(Mix(seed ^ 0x5BD1E995u, key));
				const float soil = 1.0f - std::clamp((middle.y + 0.1f) / 0.6f, 0.0f, 1.0f);
				const glm::vec3 color = glm::mix(glm::vec3(1.0f, 0.93f, 0.84f) * shade, glm::vec3(0.5f, 0.39f, 0.29f) * shade, soil * soil);
				const glm::vec2 drift(Unit(Mix(key, 17u)) * 3.0f, Unit(Mix(key, 19u)) * 3.0f);
				const uint32_t base = uint32_t(vertices.size());
				for (const glm::vec3& corner : { a, b, c })
				{
					ModelVertex vertex;
					vertex.Position = corner;
					vertex.Normal = normal;
					vertex.U = glm::dot(corner, tangent) + drift.x;
					vertex.V = -glm::dot(corner, bitangent) + drift.y;
					vertex.Tangent = PackTangent(tangent, 1.0f);
					vertex.Color = PackColor(color);
					vertices.push_back(vertex);
				}
				indices.insert(indices.end(), { base, base + 1, base + 2 });
			};
			for (size_t ring = 0; ring + 1 < rings.size(); ring++)
			{
				const glm::vec3 core = (center(ring) + center(ring + 1)) * 0.5f;
				for (uint32_t side = 0; side < sides; side++)
				{
					const uint32_t next = (side + 1) % sides;
					const uint32_t key = uint32_t(ring) * 64u + side;
					if (rings[ring].Radius > 0.0f)
						face(point(ring, side), point(ring, next), point(ring + 1, next), core, key * 2u);
					if (rings[ring + 1].Radius > 0.0f)
						face(point(ring, side), point(ring + 1, next), point(ring + 1, side), core, key * 2u + 1u);
				}
			}
		}

		constexpr RockRing ShardRings[] = {
			{ -0.55f, 0.36f, 0.0f, 0.0f },
			{ 0.0f, 0.34f, 0.02f, 0.0f },
			{ 0.32f, 0.27f, 0.04f, 0.02f },
			{ 0.62f, 0.17f, 0.06f, 0.0f },
			{ 0.86f, 0.07f, 0.08f, -0.01f },
			{ 1.05f, 0.0f, 0.1f, 0.0f },
		};

		void BuildEarthSpike(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, uint32_t seed)
		{
			constexpr RockRing Main[] = {
				{ -0.9f, 0.74f, 0.0f, 0.0f },
				{ -0.3f, 0.73f, 0.02f, 0.03f },
				{ 0.15f, 0.68f, -0.02f, -0.02f },
				{ 0.6f, 0.61f, 0.03f, 0.03f },
				{ 1.05f, 0.53f, 0.0f, 0.0f },
				{ 1.5f, 0.44f, -0.03f, -0.03f },
				{ 1.95f, 0.34f, 0.02f, 0.02f },
				{ 2.4f, 0.24f, 0.0f, 0.0f },
				{ 2.8f, 0.14f, 0.03f, 0.01f },
				{ 3.15f, 0.06f, 0.02f, -0.01f },
				{ SpikeLength, 0.0f, 0.05f, -0.02f },
			};
			AppendRock(vertices, indices, Main, 8, glm::vec3(0.0f), Up, 1.0f, seed);
			for (uint32_t i = 0; i < 2; i++)
			{
				const float angle = (i == 0 ? 1.9f : -1.9f) + 0.4f * (Unit(Mix(seed, i * 7u)) - 0.5f);
				const glm::vec3 out(std::cos(angle), 0.0f, std::sin(angle));
				const float tilt = 0.45f + 0.2f * Unit(Mix(seed, i * 7u + 1u));
				AppendRock(vertices, indices, ShardRings, 6, out * 0.3f + Up * 0.1f, Up * std::cos(tilt) + out * std::sin(tilt), 0.5f + 0.15f * Unit(Mix(seed, i * 7u + 2u)), seed * 7u + i * 911u);
			}
		}

		void BuildSpikeCollar(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, uint32_t seed)
		{
			constexpr uint32_t Shards = 6;
			for (uint32_t i = 0; i < Shards; i++)
			{
				const float angle = 0.95f + (6.2832f - 1.9f) * (float(i) + 0.5f) / float(Shards) + 0.3f * (Unit(Mix(seed, i * 5u)) - 0.5f);
				const glm::vec3 out(std::cos(angle), 0.0f, std::sin(angle));
				const float tilt = 0.35f + 0.35f * Unit(Mix(seed, i * 5u + 1u));
				const float size = 0.34f + 0.36f * Unit(Mix(seed, i * 5u + 2u));
				AppendRock(vertices, indices, ShardRings, 5 + i % 2, out * (0.55f + 0.3f * Unit(Mix(seed, i * 5u + 3u))), Up * std::cos(tilt) + out * std::sin(tilt), size, seed * 13u + i * 577u);
			}
		}

		struct IceRing
		{
			float T = 0.0f;
			float Radius = 0.0f;
			float Slant = 0.0f;
		};

		void AppendIce(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, std::span<const IceRing> rings, const glm::vec3& base, const glm::vec3& axis, uint32_t sides, float flatten, float rime, uint32_t seed)
		{
			const glm::vec3 across = glm::normalize(glm::cross(axis, std::abs(axis.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f)));
			const glm::vec3 depth = glm::cross(axis, across);
			const float chisel = 6.2832f * Unit(Mix(seed, 1u));
			const glm::vec3 lean = across * std::cos(chisel) + depth * (std::sin(chisel) * flatten);
			const float length = rings.back().T;
			const glm::vec3 bottom = base + axis * rings.front().T;
			const auto point = [&](size_t ring, uint32_t side)
			{
				const IceRing& r = rings[ring];
				if (r.Radius <= 0.0f)
					return base + axis * r.T + lean * (0.3f * rings[ring - 1].Radius);
				const uint32_t key = Mix(seed + uint32_t(ring) * 131u, side);
				const float angle = (float(side) + 0.3f * (Unit(Mix(key, 3u)) - 0.5f)) / float(sides) * 6.2832f;
				const float radius = r.Radius * (0.88f + 0.24f * Unit(key));
				return base + axis * (r.T + r.Slant * std::cos(angle - chisel)) + (across * std::cos(angle) + depth * (std::sin(angle) * flatten)) * radius;
			};
			const auto face = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, uint32_t key, bool cap)
			{
				glm::vec3 normal = glm::cross(b - a, c - a);
				const float area = glm::length(normal);
				if (area < 1e-9f)
					return;
				normal /= area;
				const glm::vec3 middle = (a + b + c) / 3.0f;
				const glm::vec3 core = cap ? bottom + axis * (0.05f * length) : base + axis * glm::dot(middle - base, axis);
				if (glm::dot(normal, middle - core) < 0.0f)
				{
					std::swap(b, c);
					normal = -normal;
				}
				const float along = std::clamp(glm::dot(middle - base, axis) / length, 0.0f, 1.0f);
				const float shade = 0.8f + 0.2f * Unit(Mix(seed ^ 0x5BD1E995u, key));
				const glm::vec3 body = glm::mix(glm::vec3(0.4f, 0.66f, 1.0f), glm::vec3(0.93f, 0.98f, 1.0f), std::pow(along, 1.8f));
				const float foot = std::max(rime, 1.0f - std::clamp(along / 0.18f, 0.0f, 1.0f));
				const glm::vec3 color = glm::mix(body, glm::vec3(0.84f, 0.91f, 0.98f), foot) * shade;
				const uint32_t first = uint32_t(vertices.size());
				for (const glm::vec3& corner : { a, b, c })
				{
					ModelVertex vertex;
					vertex.Position = corner;
					vertex.Normal = normal;
					vertex.Color = PackColor(color);
					vertices.push_back(vertex);
				}
				indices.insert(indices.end(), { first, first + 1, first + 2 });
			};
			for (uint32_t side = 0; side < sides; side++)
			{
				const uint32_t next = (side + 1) % sides;
				face(bottom, point(0, next), point(0, side), side, true);
				for (size_t ring = 0; ring + 1 < rings.size(); ring++)
				{
					const uint32_t key = uint32_t(ring + 1) * 64u + side;
					face(point(ring, side), point(ring, next), point(ring + 1, next), key * 2u, false);
					if (rings[ring + 1].Radius > 0.0f)
						face(point(ring, side), point(ring + 1, next), point(ring + 1, side), key * 2u + 1u, false);
				}
			}
		}

		struct IceShoot
		{
			float X = 0.0f;
			float Z = 0.0f;
			float Azimuth = 0.0f;
			float Lean = 0.0f;
			float Length = 1.0f;
			float Radius = 0.1f;
			float Flatten = 1.0f;
			float Tip = 0.3f;
			uint32_t Sides = 6;
		};

		void AppendIceShoot(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, const IceShoot& shoot, uint32_t seed)
		{
			const glm::vec3 axis(std::sin(shoot.Lean) * std::cos(shoot.Azimuth), std::cos(shoot.Lean), std::sin(shoot.Lean) * std::sin(shoot.Azimuth));
			const float l = shoot.Length;
			const float r = shoot.Radius * 1.3f;
			const float tip = shoot.Length * shoot.Tip;
			const IceRing rings[] = {
				{ -0.2f * l, 0.9f * r, 0.0f },
				{ 0.0f, r, 0.0f },
				{ 0.5f * (l - 1.5f * tip), 0.97f * r, 0.0f },
				{ l - tip, 0.9f * r, 0.5f * tip },
				{ l, 0.0f, 0.0f },
			};
			AppendIce(vertices, indices, rings, glm::vec3(shoot.X, 0.0f, shoot.Z), axis, shoot.Sides, shoot.Flatten, 0.0f, seed);
		}

		void AppendIceMound(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, float radius, float height, uint32_t seed)
		{
			const IceRing rings[] = {
				{ -0.05f, radius, 0.0f },
				{ 0.4f * height, 0.8f * radius, 0.2f * height },
				{ 0.8f * height, 0.45f * radius, 0.1f * height },
				{ height, 0.0f, 0.0f },
			};
			AppendIce(vertices, indices, rings, glm::vec3(0.0f), Up, 7, 0.85f, 0.85f, seed);
		}

		void BuildIceCluster(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, uint32_t variant)
		{
			std::vector<IceShoot> shoots;
			float mound = 0.2f;
			switch (variant)
			{
			case 0:
				shoots = { { 0.0f, 0.0f, 0.0f, 0.08f, 1.0f, 0.12f, 0.85f, 0.3f, 6 }, { 0.05f, 0.03f, 2.2f, 0.55f, 0.5f, 0.07f, 0.9f, 0.35f, 6 }, { -0.04f, -0.05f, -2.0f, 0.7f, 0.36f, 0.06f, 1.0f, 0.4f, 5 } };
				break;
			case 1:
				shoots = { { 0.0f, 0.0f, 0.3f, 0.12f, 1.0f, 0.15f, 0.45f, 0.42f, 5 }, { -0.06f, 0.02f, 3.0f, 0.5f, 0.42f, 0.07f, 0.75f, 0.35f, 6 } };
				mound = 0.19f;
				break;
			case 2:
				shoots = { { 0.02f, 0.0f, 0.2f, 0.18f, 0.95f, 0.1f, 0.9f, 0.3f, 6 }, { -0.04f, 0.05f, 2.4f, 0.32f, 0.8f, 0.09f, 0.85f, 0.32f, 6 }, { -0.03f, -0.05f, -2.3f, 0.42f, 0.62f, 0.08f, 0.9f, 0.35f, 6 }, { 0.07f, 0.04f, 1.0f, 0.75f, 0.32f, 0.05f, 1.0f, 0.4f, 5 } };
				mound = 0.23f;
				break;
			case 3:
				shoots = { { 0.0f, 0.0f, 0.0f, 0.1f, 0.72f, 0.09f, 0.9f, 0.32f, 6 } };
				for (uint32_t i = 0; i < 5; i++)
				{
					const float azimuth = float(i) * 1.2566f + 0.4f + 0.3f * (Unit(Mix(97u, i)) - 0.5f);
					shoots.push_back({ 0.08f * std::cos(azimuth), 0.08f * std::sin(azimuth), azimuth, 0.55f + 0.3f * Unit(Mix(97u, i + 9u)), 0.35f + 0.2f * Unit(Mix(97u, i + 17u)), 0.055f + 0.015f * Unit(Mix(97u, i + 23u)), 0.9f, 0.36f, 5u + i % 2u });
				}
				mound = 0.26f;
				break;
			case 4:
				shoots = { { 0.0f, 0.0f, 0.0f, 0.06f, 1.0f, 0.11f, 0.9f, 0.28f, 6 }, { 0.07f, -0.04f, 2.6f, 0.42f, 0.88f, 0.095f, 0.8f, 0.3f, 6 } };
				mound = 0.18f;
				break;
			default:
				shoots = { { 0.0f, 0.0f, 1.2f, 0.2f, 0.62f, 0.17f, 0.8f, 0.14f, 7 }, { 0.1f, -0.06f, -0.6f, 0.6f, 0.3f, 0.065f, 1.0f, 0.4f, 6 }, { -0.1f, 0.04f, 2.8f, 0.7f, 0.26f, 0.055f, 1.0f, 0.4f, 5 } };
				mound = 0.24f;
				break;
			}
			for (size_t i = 0; i < shoots.size(); i++)
				AppendIceShoot(vertices, indices, shoots[i], variant * 7919u + uint32_t(i) * 977u + 5u);
			AppendIceMound(vertices, indices, mound, mound * 0.32f, variant * 131u + 3u);
		}

		void BuildIceFang(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, uint32_t variant)
		{
			std::vector<IceShoot> shoots;
			float mound = 0.16f;
			switch (variant)
			{
			case 0:
				shoots = { { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.085f, 0.7f, 0.5f, 5 }, { 0.05f, 0.02f, 2.5f, 0.65f, 0.38f, 0.05f, 0.85f, 0.5f, 5 }, { -0.04f, -0.03f, -2.3f, 0.8f, 0.22f, 0.035f, 1.0f, 0.5f, 4 } };
				break;
			case 1:
				shoots = { { 0.0f, 0.0f, 0.4f, 0.04f, 1.0f, 0.1f, 0.42f, 0.55f, 4 }, { -0.05f, 0.03f, 2.9f, 0.55f, 0.45f, 0.06f, 0.6f, 0.5f, 5 } };
				mound = 0.15f;
				break;
			default:
				shoots = { { 0.0f, 0.0f, 0.0f, 0.05f, 0.95f, 0.08f, 0.8f, 0.45f, 6 }, { 0.04f, -0.03f, 1.9f, 0.32f, 0.7f, 0.065f, 0.75f, 0.5f, 5 }, { -0.05f, 0.04f, -2.2f, 0.7f, 0.3f, 0.04f, 1.0f, 0.5f, 4 } };
				mound = 0.17f;
				break;
			}
			for (size_t i = 0; i < shoots.size(); i++)
				AppendIceShoot(vertices, indices, shoots[i], variant * 6151u + uint32_t(i) * 1031u + 19u);
			AppendIceMound(vertices, indices, mound, mound * 0.25f, variant * 173u + 11u);
		}

		struct BladeShape
		{
			float Tip = 0.66f;
			float Width = 0.13f;
			float Belly = 0.6f;
			float Bend = 0.0f;
			float Thick = 0.05f;
			std::array<float, 3> NotchLeft{ -1.0f, -1.0f, -1.0f };
			std::array<float, 3> NotchRight{ -1.0f, -1.0f, -1.0f };
			float Guard = 0.17f;
			float HookLeft = 0.08f;
			float HookRight = 0.08f;
			float Grip = 0.17f;
			float Vein = 0.62f;
		};

		constexpr float BladeHeel = -0.1f;
		constexpr glm::vec3 BladeBody{ 0.03f, 0.024f, 0.042f };
		constexpr glm::vec3 BladeDeep{ 0.13f, 0.045f, 0.27f };
		constexpr glm::vec3 BladeEdge{ 0.25f, 0.24f, 0.29f };
		constexpr glm::vec3 BladeTip{ 0.8f, 0.38f, 0.98f };
		constexpr glm::vec3 BladeVein{ 0.62f, 0.24f, 1.0f };
		constexpr glm::vec3 BladeGrip{ 0.06f, 0.05f, 0.07f };

		void AppendFacet(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, glm::vec3 a, glm::vec3 b, glm::vec3 c, const glm::vec3& outward, const glm::vec3& color)
		{
			glm::vec3 normal = glm::cross(b - a, c - a);
			const float area = glm::length(normal);
			if (area < 1e-9f)
				return;
			normal /= area;
			if (glm::dot(normal, outward) < 0.0f)
			{
				std::swap(b, c);
				normal = -normal;
			}
			const uint32_t base = uint32_t(vertices.size());
			for (const glm::vec3& corner : { a, b, c })
			{
				ModelVertex vertex;
				vertex.Position = corner;
				vertex.Normal = normal;
				vertex.Color = PackColor(color);
				vertices.push_back(vertex);
			}
			indices.insert(indices.end(), { base, base + 1, base + 2 });
		}

		void AppendTube(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, std::span<const glm::vec3> path, std::span<const float> radii, uint32_t sides, float flatten, uint32_t seed, const glm::vec3& color, const glm::vec3& accent)
		{
			const auto point = [&](size_t ring, uint32_t side)
			{
				const glm::vec3 along = path[std::min(ring + 1, path.size() - 1)] - path[ring > 0 ? ring - 1 : 0];
				const glm::vec3 lateral = glm::normalize(glm::vec3(-along.z, 0.0f, along.x) + glm::vec3(1e-6f, 0.0f, 0.0f));
				const float angle = (float(side) + 0.5f * float(ring % 2)) / float(sides) * 6.2832f;
				const float radius = radii[ring] * (0.88f + 0.24f * Unit(Mix(seed + uint32_t(ring) * 131u, side)));
				return path[ring] + lateral * (radius * std::cos(angle)) + Up * (radius * std::sin(angle) * flatten);
			};
			for (size_t ring = 0; ring + 1 < path.size(); ring++)
			{
				const glm::vec3 middle = 0.5f * (path[ring] + path[ring + 1]);
				for (uint32_t side = 0; side < sides; side++)
				{
					const uint32_t next = (side + 1) % sides;
					const glm::vec3 a = point(ring, side);
					const glm::vec3 b = point(ring, next);
					const glm::vec3 c = point(ring + 1, next);
					const glm::vec3 d = point(ring + 1, side);
					const uint32_t key = Mix(seed ^ 0x5BD1E995u, uint32_t(ring) * 64u + side);
					const glm::vec3 tone = (side == 0 || side == sides / 2 ? accent : color) * (0.8f + 0.4f * Unit(key));
					AppendFacet(vertices, indices, a, b, c, (a + b + c) / 3.0f - middle, tone);
					AppendFacet(vertices, indices, a, c, d, (a + c + d) / 3.0f - middle, tone);
				}
			}
		}

		float BladeWidth(const BladeShape& shape, float u)
		{
			return shape.Width * std::pow(std::max(1.0f - u, 0.0f), 0.8f) * (1.0f + shape.Belly * u);
		}

		float BladeSpine(const BladeShape& shape, float u)
		{
			return shape.Bend * shape.Width * u * u;
		}

		float BladeRidge(const BladeShape& shape, float u)
		{
			return shape.Thick * std::pow(BladeWidth(shape, u) / shape.Width, 0.7f);
		}

		void AppendVein(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, const BladeShape& shape, std::span<const glm::vec2> path, float width)
		{
			const float span = shape.Tip - BladeHeel;
			const auto place = [&](const glm::vec2& p, float sign)
			{
				const float w = BladeWidth(shape, p.x);
				const float height = BladeRidge(shape, p.x) * (1.0f - 0.92f * std::abs(p.y)) + 0.006f;
				return glm::vec3(BladeHeel + span * p.x, sign * height, BladeSpine(shape, p.x) + p.y * w * 0.78f);
			};
			for (const float sign : { 1.0f, -1.0f })
				for (size_t k = 0; k + 1 < path.size(); k++)
				{
					const glm::vec3 a = place(path[k], sign);
					const glm::vec3 b = place(path[k + 1], sign);
					const glm::vec3 run = b - a;
					const glm::vec3 across = glm::normalize(glm::vec3(-run.z, 0.0f, run.x) + glm::vec3(0.0f, 0.0f, 1e-6f));
					const float wa = width * (1.0f - 0.5f * float(k) / float(path.size()));
					const float wb = width * (1.0f - 0.5f * float(k + 1) / float(path.size()));
					const glm::vec3 outward(0.0f, sign, 0.0f);
					AppendFacet(vertices, indices, a - across * wa, a + across * wa, b + across * wb, outward, BladeVein);
					AppendFacet(vertices, indices, a - across * wa, b + across * wb, b - across * wb, outward, BladeVein);
				}
		}

		void BuildObsidianBlade(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, uint32_t variant)
		{
			BladeShape shape;
			switch (variant)
			{
			case 0:
				shape.NotchRight = { 0.34f, 0.55f, -1.0f };
				break;
			case 1:
				shape.Tip = 0.74f;
				shape.Width = 0.11f;
				shape.Belly = 0.8f;
				shape.Bend = 0.35f;
				shape.NotchLeft = { 0.46f, -1.0f, -1.0f };
				shape.HookRight = -0.06f;
				shape.Guard = 0.15f;
				shape.Vein = 0.72f;
				break;
			case 2:
				shape.Tip = 0.62f;
				shape.Width = 0.15f;
				shape.Belly = 0.3f;
				shape.NotchRight = { 0.28f, 0.46f, 0.64f };
				shape.Guard = 0.14f;
				shape.HookLeft = 0.05f;
				shape.HookRight = 0.05f;
				shape.Thick = 0.055f;
				shape.Vein = 0.55f;
				break;
			default:
				shape.Tip = 0.7f;
				shape.Width = 0.1f;
				shape.Belly = 1.0f;
				shape.Bend = -0.2f;
				shape.NotchLeft = { 0.3f, -1.0f, -1.0f };
				shape.NotchRight = { 0.52f, -1.0f, -1.0f };
				shape.HookLeft = 0.11f;
				shape.HookRight = 0.11f;
				shape.Guard = 0.18f;
				shape.Grip = 0.19f;
				break;
			}
			const uint32_t seed = 977u + variant * 7919u;
			constexpr uint32_t Stations = 14;
			const float span = shape.Tip - BladeHeel;
			for (const float side : { 1.0f, -1.0f })
			{
				std::vector<glm::vec2> knots;
				for (uint32_t k = 0; k <= Stations; k++)
					knots.push_back({ float(k) / float(Stations), k > 0 && k < Stations ? 0.9f + 0.2f * Unit(Mix(seed + (side > 0.0f ? 7u : 13u), k)) : 1.0f });
				for (const float notch : side > 0.0f ? shape.NotchLeft : shape.NotchRight)
					if (notch > 0.0f)
					{
						std::erase_if(knots, [notch](const glm::vec2& knot) { return std::abs(knot.x - notch) < 0.02f; });
						knots.push_back({ notch - 0.012f, 1.14f });
						knots.push_back({ notch + 0.004f, 0.5f });
					}
				std::sort(knots.begin(), knots.end(), [](const glm::vec2& a, const glm::vec2& b) { return a.x < b.x; });
				std::vector<glm::vec3> edge, bevel, ridge;
				std::vector<float> along;
				for (const glm::vec2& knot : knots)
				{
					const float u = knot.x;
					const float x = BladeHeel + span * u;
					const float w = BladeWidth(shape, u) * knot.y;
					const float spine = BladeSpine(shape, u);
					const float h = BladeRidge(shape, u);
					edge.push_back({ x, 0.0f, spine + side * w });
					bevel.push_back({ x, 0.28f * h, spine + side * w * 0.8f });
					ridge.push_back({ x, h, spine });
					along.push_back(u);
				}
				for (const float sign : { 1.0f, -1.0f })
				{
					const glm::vec3 flip(1.0f, sign, 1.0f);
					AppendFacet(vertices, indices, edge[0] * flip, bevel[0] * flip, ridge[0] * flip, { -1.0f, 0.0f, 0.0f }, BladeBody);
					for (size_t k = 0; k + 1 < edge.size(); k++)
					{
						const float u = 0.5f * (along[k] + along[k + 1]);
						const uint32_t key = Mix(seed + uint32_t(k) * 31u, side > 0.0f ? (sign > 0.0f ? 1u : 2u) : (sign > 0.0f ? 3u : 4u));
						const glm::vec3 edgeTone = glm::mix(BladeEdge, BladeTip, glm::smoothstep(0.72f, 1.0f, u)) * (0.85f + 0.3f * Unit(key));
						const float deep = u < 0.5f && Unit(Mix(key, 5u)) > 0.6f ? 0.35f * (1.0f - u / 0.5f) : 0.0f;
						const glm::vec3 bodyTone = glm::mix(BladeBody * (0.7f + 0.7f * Unit(Mix(key, 9u))), BladeDeep, deep);
						const glm::vec3 slope(0.0f, sign, 0.6f * side);
						AppendFacet(vertices, indices, edge[k] * flip, edge[k + 1] * flip, bevel[k + 1] * flip, slope, edgeTone);
						AppendFacet(vertices, indices, edge[k] * flip, bevel[k + 1] * flip, bevel[k] * flip, slope, edgeTone);
						AppendFacet(vertices, indices, bevel[k] * flip, bevel[k + 1] * flip, ridge[k + 1] * flip, { 0.0f, sign, 0.0f }, bodyTone);
						AppendFacet(vertices, indices, bevel[k] * flip, ridge[k + 1] * flip, ridge[k] * flip, { 0.0f, sign, 0.0f }, bodyTone);
					}
				}
			}
			std::vector<glm::vec2> vein{ { 0.04f, 0.0f } };
			std::vector<std::vector<glm::vec2>> branches;
			for (uint32_t k = 1; vein.back().x < shape.Vein; k++)
			{
				const glm::vec2 last = vein.back();
				const float lateral = std::clamp(last.y + (Unit(Mix(seed + 101u, k)) - 0.5f) * 0.75f, -0.5f, 0.5f);
				vein.push_back({ last.x + 0.07f + 0.06f * Unit(Mix(seed + 103u, k)), lateral });
				if (k % 2u == 0u && branches.size() < 2u)
				{
					const float out = lateral >= 0.0f ? 1.0f : -1.0f;
					branches.push_back({ vein.back(), vein.back() + glm::vec2(0.05f, out * 0.18f), vein.back() + glm::vec2(0.08f + 0.04f * Unit(Mix(seed, k)), out * 0.3f) });
				}
			}
			AppendVein(vertices, indices, shape, vein, 0.012f);
			for (const std::vector<glm::vec2>& branch : branches)
				AppendVein(vertices, indices, shape, branch, 0.008f);
			for (const float side : { 1.0f, -1.0f })
			{
				const float hook = side > 0.0f ? shape.HookLeft : shape.HookRight;
				const glm::vec3 horn[] = {
					{ BladeHeel + 0.015f, 0.0f, 0.0f },
					{ BladeHeel, 0.0f, side * 0.45f * shape.Guard },
					{ BladeHeel - 0.35f * hook, 0.0f, side * 0.85f * shape.Guard },
					{ BladeHeel - hook, 0.0f, side * shape.Guard },
				};
				constexpr float HornRadii[] = { 0.05f, 0.04f, 0.026f, 0.0f };
				AppendTube(vertices, indices, horn, HornRadii, 4, 0.55f, seed + (side > 0.0f ? 17u : 23u), BladeBody * 1.4f, BladeEdge);
			}
			for (const float sign : { 1.0f, -1.0f })
			{
				const glm::vec3 gem[] = { { BladeHeel - 0.035f, sign * 0.036f, 0.0f }, { BladeHeel, sign * 0.036f, 0.0f }, { BladeHeel + 0.035f, sign * 0.036f, 0.0f } };
				constexpr float GemRadii[] = { 0.0f, 0.03f, 0.0f };
				AppendTube(vertices, indices, gem, GemRadii, 4, 0.9f, seed + 29u, BladeVein, BladeTip);
			}
			std::vector<glm::vec3> grip;
			std::vector<float> wraps;
			for (float x = BladeHeel; x > BladeHeel - shape.Grip; x -= 0.028f)
			{
				grip.push_back({ x, 0.0f, 0.0f });
				wraps.push_back(grip.size() % 2u ? 0.027f : 0.021f);
			}
			grip.push_back({ BladeHeel - shape.Grip, 0.0f, 0.0f });
			wraps.push_back(0.024f);
			AppendTube(vertices, indices, grip, wraps, 5, 0.8f, seed + 31u, BladeGrip, BladeGrip * 1.6f);
			const glm::vec3 pommel[] = { { BladeHeel - shape.Grip + 0.005f, 0.0f, 0.0f }, { BladeHeel - shape.Grip - 0.04f, 0.0f, 0.0f }, { BladeHeel - shape.Grip - 0.085f, 0.0f, 0.0f } };
			constexpr float PommelRadii[] = { 0.02f, 0.042f, 0.0f };
			AppendTube(vertices, indices, pommel, PommelRadii, 4, 0.8f, seed + 37u, BladeEdge, BladeTip);
		}

		void AppendCrystalFacet(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, glm::vec3 a, glm::vec3 b, glm::vec3 c, const glm::vec3& inside, const glm::vec3& color)
		{
			glm::vec3 normal = glm::cross(b - a, c - a);
			const float area = glm::length(normal);
			if (area < 1e-10f)
				return;
			normal /= area;
			if (glm::dot(normal, (a + b + c) / 3.0f - inside) < 0.0f)
			{
				std::swap(b, c);
				normal = -normal;
			}
			const uint32_t base = uint32_t(vertices.size());
			for (const glm::vec3& corner : { a, b, c })
			{
				ModelVertex vertex;
				vertex.Position = corner;
				vertex.Normal = normal;
				vertex.Color = PackColor(color);
				vertices.push_back(vertex);
			}
			indices.insert(indices.end(), { base, base + 1, base + 2 });
		}

		void AppendGem(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, const glm::vec3& base, const glm::vec3& axis, float length, float radius, float back, const glm::vec3& color, float twist)
		{
			const glm::vec3 along = glm::normalize(axis);
			const glm::vec3 helper = std::abs(along.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
			const glm::vec3 u = glm::normalize(glm::cross(along, helper));
			const glm::vec3 v = glm::cross(along, u);
			const glm::vec3 tip = base + along * length;
			const glm::vec3 tail = base - along * (length * back);
			const glm::vec3 waist = base + along * (length * 0.22f);
			std::array<glm::vec3, 4> ring;
			for (uint32_t k = 0; k < 4; k++)
			{
				const float angle = twist + float(k) * 1.5708f;
				ring[k] = waist + (u * std::cos(angle) + v * std::sin(angle) * 0.6f) * radius;
			}
			for (uint32_t k = 0; k < 4; k++)
			{
				const glm::vec3& a = ring[k];
				const glm::vec3& b = ring[(k + 1) % 4];
				const float shade = k % 2 == 0 ? 1.0f : 0.82f;
				AppendCrystalFacet(vertices, indices, a, b, tip, waist, color * shade);
				AppendCrystalFacet(vertices, indices, b, a, tail, waist, color * (shade * 0.85f));
			}
		}

		void AppendQuartz(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, const glm::vec3& base, const glm::vec3& axis, float length, float radius, uint32_t seed, const glm::vec3& root, const glm::vec3& tip)
		{
			const glm::vec3 along = glm::normalize(axis);
			const glm::vec3 helper = std::abs(along.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
			const glm::vec3 u = glm::normalize(glm::cross(along, helper));
			const glm::vec3 v = glm::cross(along, u);
			const float twist = Unit(Mix(seed, 3u)) * 1.0472f;
			const float shoulder = 0.68f + 0.12f * Unit(Mix(seed, 5u));
			std::array<glm::vec3, 6> low;
			std::array<glm::vec3, 6> high;
			for (uint32_t k = 0; k < 6; k++)
			{
				const float angle = twist + float(k) * 1.0472f;
				const float jitter = 0.88f + 0.24f * Unit(Mix(seed, 10u + k));
				const glm::vec3 ring = (u * std::cos(angle) + v * std::sin(angle)) * (radius * jitter);
				low[k] = base - along * (0.08f * length) + ring;
				high[k] = base + along * (shoulder * length) + ring * 0.94f;
			}
			const glm::vec3 apex = base + along * length + (u * (Unit(Mix(seed, 20u)) - 0.5f) + v * (Unit(Mix(seed, 21u)) - 0.5f)) * (radius * 0.3f);
			const glm::vec3 body = base + along * (0.4f * length);
			const glm::vec3 crown = base + along * (shoulder * length * 0.98f);
			for (uint32_t k = 0; k < 6; k++)
			{
				const uint32_t next = (k + 1) % 6;
				const float shade = 0.78f + 0.3f * Unit(Mix(seed, 30u + k));
				AppendCrystalFacet(vertices, indices, low[k], low[next], high[next], body, root * shade);
				AppendCrystalFacet(vertices, indices, low[k], high[next], high[k], body, glm::mix(root, tip, 0.35f) * shade);
				AppendCrystalFacet(vertices, indices, high[k], high[next], apex, crown, tip * (shade + 0.1f));
			}
		}

		void BuildQuartzCluster(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, uint32_t seed)
		{
			const glm::vec3 root{ 0.16f, 0.5f, 0.48f };
			const glm::vec3 tip{ 0.62f, 1.0f, 0.92f };
			AppendQuartz(vertices, indices, glm::vec3(0.0f), glm::vec3(1.0f, 0.04f, 0.03f), 1.0f, 0.12f, seed, root, tip);
			const uint32_t count = 4u + seed % 3u;
			for (uint32_t i = 0; i < count; i++)
			{
				const uint32_t key = Mix(seed, 100u + i);
				const float around = (float(i) + 0.35f * Unit(key)) / float(count) * 6.2832f;
				const float lean = 0.35f + 0.5f * Unit(Mix(key, 1u));
				const glm::vec3 out(0.0f, std::cos(around), std::sin(around));
				const glm::vec3 base = out * (0.05f + 0.07f * Unit(Mix(key, 2u)));
				const glm::vec3 axis = glm::vec3(std::cos(lean), 0.0f, 0.0f) + out * std::sin(lean);
				AppendQuartz(vertices, indices, base, axis, 0.32f + 0.42f * Unit(Mix(key, 3u)), 0.05f + 0.05f * Unit(Mix(key, 4u)), key, root, tip);
			}
		}

		void BuildCrystalSword(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices)
		{
			struct Ring
			{
				float X;
				float Width;
				float Thick;
			};
			constexpr Ring Blade[] = {
				{ SwordGuard, 0.07f, 0.028f },
				{ SwordGuard + 0.07f, 0.094f, 0.034f },
				{ SwordGuard + 0.48f, 0.086f, 0.031f },
				{ SwordGuard + 0.88f, 0.072f, 0.027f },
				{ SwordGuard + 1.1f, 0.046f, 0.018f },
				{ SwordGuard + SwordBlade, 0.0f, 0.0f },
			};
			constexpr glm::vec2 Section[] = { { 0.0f, 1.0f }, { 0.55f, 0.5f }, { 1.0f, 0.0f }, { 0.55f, -0.5f }, { 0.0f, -1.0f }, { -0.55f, -0.5f }, { -1.0f, 0.0f }, { -0.55f, 0.5f } };
			constexpr size_t Sides = std::size(Section);
			const glm::vec3 deep{ 0.16f, 0.5f, 0.5f };
			const glm::vec3 pale{ 0.72f, 1.0f, 0.94f };
			const auto point = [&](const Ring& ring, size_t k)
			{
				return glm::vec3(ring.X, Section[k].x * ring.Thick, Section[k].y * ring.Width);
			};
			for (size_t i = 0; i + 1 < std::size(Blade); i++)
			{
				const Ring& a = Blade[i];
				const Ring& b = Blade[i + 1];
				const float along = ((a.X + b.X) * 0.5f - SwordGuard) / SwordBlade;
				const glm::vec3 inside((a.X + b.X) * 0.5f, 0.0f, 0.0f);
				for (size_t k = 0; k < Sides; k++)
				{
					const size_t next = (k + 1) % Sides;
					const bool edge = k == 7 || k == 0 || k == 3 || k == 4;
					const glm::vec3 color = glm::mix(deep, pale, std::pow(along, 0.8f)) * (edge ? 1.15f : 0.9f + 0.1f * float(k % 2));
					AppendCrystalFacet(vertices, indices, point(a, k), point(a, next), point(b, next), inside, color);
					if (b.Width > 0.0f)
						AppendCrystalFacet(vertices, indices, point(a, k), point(b, next), point(b, k), inside, color);
				}
			}
			const glm::vec3 heel(SwordGuard - 0.01f, 0.0f, 0.0f);
			for (size_t k = 0; k < Sides; k++)
				AppendCrystalFacet(vertices, indices, point(Blade[0], k), point(Blade[0], (k + 1) % Sides), heel, glm::vec3(SwordGuard + 0.2f, 0.0f, 0.0f), deep * 0.8f);

			const glm::vec3 guard{ 0.14f, 0.44f, 0.46f };
			for (const float side : { 1.0f, -1.0f })
			{
				AppendGem(vertices, indices, { SwordGuard - 0.005f, 0.0f, side * 0.05f }, { 0.42f, 0.0f, side }, 0.2f, 0.03f, 0.25f, guard * 1.25f, 0.4f);
				AppendGem(vertices, indices, { SwordGuard - 0.015f, 0.0f, side * 0.04f }, { -0.35f, 0.0f, side }, 0.11f, 0.022f, 0.3f, guard, 0.9f);
				AppendGem(vertices, indices, { SwordGuard + 0.02f, 0.0f, side * 0.07f }, { 1.0f, 0.0f, side * 0.55f }, 0.13f, 0.018f, 0.2f, guard * 1.4f, 0.2f);
			}
			AppendGem(vertices, indices, { SwordGuard - 0.01f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 0.05f, 0.04f, 1.0f, pale * 1.1f, 0.785f);

			constexpr float GripStart = -0.15f;
			constexpr float GripEnd = SwordGuard - 0.03f;
			constexpr uint32_t GripSides = 6;
			const glm::vec3 grip{ 0.07f, 0.2f, 0.22f };
			for (uint32_t k = 0; k < GripSides; k++)
			{
				const float a0 = float(k) / float(GripSides) * 6.2832f;
				const float a1 = float(k + 1) / float(GripSides) * 6.2832f;
				for (uint32_t band = 0; band < 3; band++)
				{
					const float x0 = GripStart + (GripEnd - GripStart) * float(band) / 3.0f;
					const float x1 = GripStart + (GripEnd - GripStart) * float(band + 1) / 3.0f;
					const float r0 = 0.021f + (band % 2 == 0 ? 0.0f : 0.003f);
					const float r1 = 0.021f + (band % 2 == 0 ? 0.003f : 0.0f);
					const glm::vec3 p00(x0, std::sin(a0) * r0, std::cos(a0) * r0);
					const glm::vec3 p01(x0, std::sin(a1) * r0, std::cos(a1) * r0);
					const glm::vec3 p10(x1, std::sin(a0) * r1, std::cos(a0) * r1);
					const glm::vec3 p11(x1, std::sin(a1) * r1, std::cos(a1) * r1);
					const glm::vec3 inside((x0 + x1) * 0.5f, 0.0f, 0.0f);
					AppendCrystalFacet(vertices, indices, p00, p01, p11, inside, grip * (0.85f + 0.15f * float(k % 2)));
					AppendCrystalFacet(vertices, indices, p00, p11, p10, inside, grip * (0.85f + 0.15f * float(k % 2)));
				}
			}
			AppendGem(vertices, indices, { GripStart, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, 0.085f, 0.034f, 0.35f, guard * 1.3f, 0.3f);
		}

		glm::mat4 Frame(const glm::vec3& origin, const glm::vec3& forward, const glm::vec3& up, float scale)
		{
			const glm::vec3 side = glm::normalize(glm::cross(forward, up));
			glm::mat4 transform(1.0f);
			transform[0] = glm::vec4(forward * scale, 0.0f);
			transform[1] = glm::vec4(glm::cross(side, forward) * scale, 0.0f);
			transform[2] = glm::vec4(side * scale, 0.0f);
			transform[3] = glm::vec4(origin, 1.0f);
			return transform;
		}
	}

	SpellMeshes RegisterSpellMeshes(WorldRenderer& renderer)
	{
		constexpr CrystalRing Spear[] = {
			{ -0.4f, 0.0f, 0.0f },
			{ -0.33f, 0.036f, 0.1f },
			{ -0.24f, 0.054f, 0.24f },
			{ -0.17f, 0.04f, 0.34f },
			{ -0.06f, 0.07f, 0.44f },
			{ 0.08f, 0.094f, 0.54f },
			{ 0.24f, 0.078f, 0.62f },
			{ 0.4f, 0.048f, 0.68f },
			{ 0.6f, 0.0f, 0.7f },
		};
		const EffectMaterial ice{ .Color = { 0.86f, 0.95f, 1.0f }, .Roughness = 0.07f, .Metallic = 0.0f, .Translucency = 0.7f, .Transmission = 1.0f, .Ior = 1.31f, .Absorption = SpellIceAbsorption };
		SpellMeshes meshes;
		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		AppendCrystal(vertices, indices, Spear, 6, 0.62f, 17u, { 0.5f, 0.74f, 1.0f }, { 0.93f, 0.98f, 1.0f });
		meshes.Spear = renderer.AddEffectMesh("IceSpear", vertices, indices, ice, MeshVertexTint);
		const std::optional<uint32_t> cliff = renderer.FindMaterialSlot("GroundCliff");
		const EffectMaterial granite{ .Color = { 0.4f, 0.38f, 0.36f }, .Roughness = 0.85f, .Metallic = 0.0f };
		for (uint32_t variant = 0; variant < meshes.Spike.size(); variant++)
		{
			vertices.clear();
			indices.clear();
			BuildEarthSpike(vertices, indices, 23u + variant * 38u);
			const std::string name = "EarthSpike" + std::to_string(variant);
			meshes.Spike[variant] = cliff ? renderer.AddEffectMesh(name, vertices, indices, *cliff, MeshVertexTint) : renderer.AddEffectMesh(name, vertices, indices, granite, MeshVertexTint);
		}
		vertices.clear();
		indices.clear();
		BuildSpikeCollar(vertices, indices, 41u);
		meshes.SpikeCollar = cliff ? renderer.AddEffectMesh("EarthSpikeCollar", vertices, indices, *cliff, MeshVertexTint) : renderer.AddEffectMesh("EarthSpikeCollar", vertices, indices, granite, MeshVertexTint);
		const ModelLibrary& models = renderer.GetModels();
		meshes.SpikeModel = true;
		for (size_t variant = 0; variant < SpikeVariants; variant++)
			for (size_t piece = 0; piece < SpikePieces; piece++)
			{
				const std::optional<uint32_t> part = models.FindMesh("EarthSpike/Spike" + std::string(1, char('A' + variant)) + std::to_string(piece));
				meshes.SpikeModel = meshes.SpikeModel && part.has_value();
				if (!part)
					continue;
				const ModelMesh& mesh = models.GetMesh(*part);
				meshes.SpikeParts[variant][piece] = *part;
				meshes.SpikePivots[variant][piece] = 0.5f * (mesh.Min + mesh.Max);
				const glm::vec3 extent = 0.5f * (mesh.Max - mesh.Min);
				meshes.SpikeRadii[variant][piece] = std::min({ extent.x, extent.y, extent.z });
			}
		if (const std::optional<uint32_t> collar = models.FindMesh("EarthSpike/Collar"); collar && meshes.SpikeModel)
		{
			meshes.SpikeCollar = *collar;
			meshes.CollarDepth = SpikeModelCollarDepth;
		}
		for (size_t i = 0; i < meshes.SpikeFrags.size(); i++)
			if (const std::optional<uint32_t> frag = models.FindMesh("EarthSpike/Frag" + std::to_string(i)))
				meshes.SpikeFrags[i] = *frag;
		const EffectMaterial clear{ .Color = { 0.9f, 0.96f, 1.0f }, .Roughness = 0.1f, .Metallic = 0.0f, .Translucency = 0.8f, .Transmission = 1.0f, .Ior = 1.31f, .Absorption = SpellIceAbsorption };
		for (uint32_t variant = 0; variant < meshes.Ice.size(); variant++)
		{
			vertices.clear();
			indices.clear();
			BuildIceCluster(vertices, indices, variant);
			meshes.Ice[variant] = renderer.AddEffectMesh("IceCluster" + std::to_string(variant), vertices, indices, clear, MeshVertexTint);
		}
		for (uint32_t variant = 0; variant < meshes.Fang.size(); variant++)
		{
			vertices.clear();
			indices.clear();
			BuildIceFang(vertices, indices, variant);
			meshes.Fang[variant] = renderer.AddEffectMesh("IceFang" + std::to_string(variant), vertices, indices, clear, MeshVertexTint);
		}
		const EffectMaterial glass{ .Color = { 1.0f, 1.0f, 1.0f }, .Roughness = 0.05f, .Metallic = 0.0f };
		for (uint32_t variant = 0; variant < meshes.Obsidian.size(); variant++)
		{
			vertices.clear();
			indices.clear();
			BuildObsidianBlade(vertices, indices, variant);
			meshes.Obsidian[variant] = renderer.AddEffectMesh("ObsidianBlade" + std::to_string(variant), vertices, indices, glass, MeshVertexTint);
		}
		vertices.clear();
		indices.clear();
		BuildCrystalSword(vertices, indices);
		const EffectMaterial crystal{ .Color = { 0.92f, 1.0f, 0.98f }, .Roughness = 0.06f, .Metallic = 0.0f, .Translucency = 0.75f, .Transmission = 1.0f, .Ior = 1.45f, .Absorption = { 1.3f, 0.22f, 0.38f } };
		meshes.Sword = renderer.AddEffectMesh("CrystalSword", vertices, indices, crystal, MeshVertexTint);
		constexpr float ShardShapes[][4] = { { 0.3f, 0.06f, 0.5f, 0.3f }, { 0.22f, 0.07f, 0.8f, 1.1f }, { 0.36f, 0.045f, 0.35f, 0.7f }, { 0.18f, 0.06f, 1.0f, 0.2f } };
		for (size_t variant = 0; variant < meshes.SwordShards.size(); variant++)
		{
			vertices.clear();
			indices.clear();
			const float* shape = ShardShapes[variant];
			AppendGem(vertices, indices, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), shape[0], shape[1], shape[2], glm::vec3(0.45f, 0.95f, 0.88f), shape[3]);
			meshes.SwordShards[variant] = renderer.AddEffectMesh("CrystalShard" + std::to_string(variant), vertices, indices, crystal, MeshVertexTint);
		}
		for (size_t variant = 0; variant < meshes.SwordClusters.size(); variant++)
		{
			vertices.clear();
			indices.clear();
			BuildQuartzCluster(vertices, indices, 71u + uint32_t(variant) * 977u);
			meshes.SwordClusters[variant] = renderer.AddEffectMesh("QuartzCluster" + std::to_string(variant), vertices, indices, crystal, MeshVertexTint);
		}
		return meshes;
	}

	float SpikeThrust(float age)
	{
		constexpr float Omega = 48.0f;
		constexpr float Damping = 0.55f;
		const float damped = Omega * std::sqrt(1.0f - Damping * Damping);
		const float t = std::max(age, 0.0f);
		return 1.0f - std::exp(-Damping * Omega * t) * (std::cos(damped * t) + Damping * Omega / damped * std::sin(damped * t));
	}

	glm::mat4 EarthSpikeFrame(const EarthSpikeLook& look)
	{
		const glm::vec3 heading = Heading(look.Facing);
		const glm::vec3 side = Heading(look.Facing + HalfPi);
		const glm::vec3 axis = glm::normalize(Up * std::cos(look.Tilt) + heading * std::sin(look.Tilt) + side * std::sin(look.Sway));
		const glm::vec3 forward = glm::normalize(heading - axis * glm::dot(heading, axis));
		return Frame(look.Base + axis * ((look.Extend - 1.0f) * (SpikeLength + 0.2f) * look.Scale), forward, axis, look.Scale);
	}

	void DrawSpikeShard(WorldRenderer& renderer, uint32_t mesh, const glm::mat4& transform, float dissolve, float ground, uint32_t seed)
	{
		const ModelDissolve crumble{ .Amount = dissolve, .Edge = SpikeDust, .Width = 0.03f, .Scale = 3.2f, .Sweep = -1.0f, .Seed = float(seed % 211u) };
		renderer.DrawModel(mesh, transform, NoPalette, SpikeModelTint, 0.0f, ground, SpikeModelRim, 0u, glm::vec4(0.0f), crumble);
	}

	void DrawEarthSpike(WorldRenderer& renderer, const SpellMeshes& meshes, const EarthSpikeLook& look)
	{
		const glm::vec3 heading = Heading(look.Facing);
		const glm::vec3 side = Heading(look.Facing + HalfPi);
		const glm::vec4 tint = meshes.SpikeModel ? SpikeModelTint : glm::vec4(1.55f, 1.45f, 1.35f, 0.0f);
		const glm::vec4 rim = meshes.SpikeModel ? SpikeModelRim : glm::vec4(0.3f, 0.2f, 0.11f, 0.35f);
		const ModelDissolve crumble{ .Amount = look.Dissolve, .Edge = meshes.SpikeModel ? SpikeDust : SpikeEmber, .Width = 0.04f, .Scale = 3.2f, .Sweep = -SpikeLength, .Seed = float(look.Seed % 211u) };
		const auto drawSpike = [&](uint32_t variant, const glm::mat4& transform, float ground)
		{
			if (meshes.SpikeModel)
			{
				for (const uint32_t part : meshes.SpikeParts[variant % SpikeVariants])
					renderer.DrawModel(part, transform, NoPalette, tint, 0.0f, ground, rim, 0u, glm::vec4(0.0f), crumble);
				return;
			}
			if (const uint32_t mesh = meshes.Spike[variant % meshes.Spike.size()]; mesh != UINT32_MAX)
				renderer.DrawModel(mesh, transform, NoPalette, tint, 0.0f, ground, rim, 0u, glm::vec4(0.0f), crumble);
		};
		if (look.Extend > 0.01f)
			drawSpike(look.Seed, EarthSpikeFrame(look), look.Base.y);
		for (size_t i = 0; i < std::size(SpikeSatellites) && look.Satellite > 0.01f; i++)
		{
			const SpikeSatellite& satellite = SpikeSatellites[i];
			const float mirror = look.Seed % 2u == 0u ? 1.0f : -1.0f;
			const float size = satellite.Size * look.Scale;
			const glm::vec3 lean = Heading(look.Facing + satellite.Yaw * mirror);
			const glm::vec3 axis = glm::normalize(Up * std::cos(look.Tilt + satellite.Lean) + lean * std::sin(look.Tilt + satellite.Lean) + side * (0.7f * std::sin(look.Sway)));
			const glm::vec3 forward = glm::normalize(lean - axis * glm::dot(lean, axis));
			const glm::vec3 origin = look.SatelliteBase[i] + axis * ((look.Satellite - 1.0f) * (SpikeLength + 0.2f) * size);
			drawSpike(look.Seed + 1u + uint32_t(i), Frame(origin, forward, axis, size), look.SatelliteBase[i].y);
		}
		if (meshes.SpikeCollar != UINT32_MAX && look.Collar > 0.01f)
			renderer.DrawModel(meshes.SpikeCollar, Frame(look.Base + Up * ((look.Collar - 1.0f) * meshes.CollarDepth * look.Scale), heading, Up, look.Scale), NoPalette, tint, 0.0f, look.Base.y, rim, 0u, glm::vec4(0.0f), crumble);
	}

	void DrawIceCluster(WorldRenderer& renderer, const SpellMeshes& meshes, const IceClusterLook& look)
	{
		const uint32_t mesh = look.Fang ? meshes.Fang[look.Variant % meshes.Fang.size()] : meshes.Ice[look.Variant % meshes.Ice.size()];
		if (mesh == UINT32_MAX || look.Scale < 0.01f)
			return;
		const glm::vec3 heading = Heading(look.Yaw);
		const glm::vec3 axis = Up * std::cos(look.Lean) + heading * std::sin(look.Lean);
		const glm::vec3 forward = glm::normalize(heading - axis * glm::dot(heading, axis));
		const glm::vec4 rim(glm::vec3(0.08f, 0.3f, 0.7f) * (0.5f + look.Glow), 1.0f);
		const ModelDissolve melt{ .Amount = look.Melt, .Edge = MeltEdge, .Width = 0.09f, .Scale = 5.0f, .Sweep = -1.2f, .Seed = float(look.Variant * 37u % 199u) };
		renderer.DrawModel(mesh, Frame(look.Base - Up * look.Sink, forward, axis, look.Scale), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, look.Frost), 0.04f + 0.14f * look.Glow, look.Base.y, rim, 0u, glm::vec4(0.0f), melt);
	}

	uint32_t NovaArmCount(uint32_t seed)
	{
		return 12u + seed % 4u;
	}

	float NovaArmAngle(uint32_t seed, uint32_t arm)
	{
		const float count = float(NovaArmCount(seed));
		return (float(arm) + 0.5f) / count * 6.2831853f + float((seed >> 8) & 255u) / 255.0f * 6.2831853f / count;
	}

	void DrawIceSpear(WorldRenderer& renderer, const SpellMeshes& meshes, const IceSpearLook& look, float time, uint32_t seed)
	{
		const float length = SpearLength * look.Size * look.Grow;
		if (length < 0.02f || meshes.Spear == UINT32_MAX)
			return;
		const glm::vec3 forward = glm::normalize(Heading(look.Facing) * std::cos(look.Pitch) + Up * std::sin(look.Pitch));
		const glm::vec3 level = glm::normalize(Up - forward * glm::dot(Up, forward));
		const glm::vec3 across = glm::cross(forward, level);
		const glm::vec3 up = level * std::cos(look.Roll) + across * std::sin(look.Roll);
		const glm::vec3 origin = look.Center - forward * (0.1f * length);
		const float pulse = 0.5f + 0.5f * std::sin(time * 9.0f + float(seed & 255u));
		const float full = look.Power >= 0.999f ? 1.0f : 0.0f;
		const float power = look.Power;
		const float whiten = std::min(0.3f * look.Flash, 0.7f);
		const float emission = look.Flying ? 0.16f : 0.06f + 0.1f * power + 0.05f * full * pulse + 0.15f * look.Flash;
		const glm::vec4 rim(glm::vec3(0.08f, 0.3f, 0.7f) * (0.6f + 0.6f * power + 0.6f * look.Flash), 1.0f);
		const glm::vec4 tint(1.0f, 1.0f, 1.0f, whiten);
		renderer.DrawModel(meshes.Spear, Frame(origin, forward, up, length), NoPalette, tint, emission, look.Ground, rim);
		const glm::vec3 tip = origin + forward * (0.6f * length);
		const glm::vec3 tail = origin - forward * (0.4f * length);
		ParticleSprite core = Stretched(ParticleShape::Beam, tail + forward * (0.12f * length), tip - forward * (0.08f * length), 0.03f * length, glm::vec3(0.1f, 0.26f, 0.55f) * (look.Flying ? 0.8f : 0.3f + 0.5f * power));
		core.Emission = 0.6f;
		renderer.DrawSprite(core);
		DrawGlow(renderer, look.Center, 0.5f * length, glm::vec3(0.02f, 0.06f, 0.14f) * (0.6f + 0.8f * power + 1.0f * look.Flash), 0.3f);
		DrawGlow(renderer, tip, (0.06f + 0.05f * power + 0.04f * full * pulse) * look.Size, glm::vec3(0.5f, 0.75f, 1.1f) * (0.4f + 0.4f * power), 0.1f);
		if (!look.Flying)
			DrawImplosion(renderer, look.Center, (0.7f + 0.5f * power) * look.Grow, 0.03f + 0.08f * power, time * (0.8f + 0.7f * power) + float(seed % 61u) * 0.37f);
		if (look.Flash > 0.0f)
			DrawStar(renderer, look.Center, 0.7f * length * look.Flash, glm::vec3(0.7f, 1.0f, 1.4f) * look.Flash, time * 0.7f);
	}

	void DrawObsidianBlade(WorldRenderer& renderer, const SpellMeshes& meshes, const ObsidianLook& look, float time, uint32_t seed)
	{
		const uint32_t mesh = meshes.Obsidian[look.Variant % meshes.Obsidian.size()];
		const float length = ObsidianLength * look.Size * look.Grow;
		if (length < 0.02f || mesh == UINT32_MAX)
			return;
		const glm::vec3 forward = glm::normalize(look.Forward);
		const glm::vec3 level = std::abs(forward.y) < 0.99f ? glm::normalize(Up - forward * glm::dot(Up, forward)) : glm::vec3(1.0f, 0.0f, 0.0f);
		const glm::vec3 across = glm::cross(forward, level);
		const glm::vec3 up = level * std::cos(look.Roll) + across * std::sin(look.Roll);
		const glm::vec3 side = glm::cross(forward, up);
		const glm::vec4 rim(ObsidianSheen * (0.12f + 0.4f * look.Glint), 1.0f);
		const ModelDissolve burn{ .Amount = look.Dissolve, .Edge = ObsidianSheen * 2.2f, .Width = 0.08f, .Scale = 6.0f, .Seed = float(seed % 97u) };
		renderer.DrawModel(mesh, Frame(look.Center, forward, up, length), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.25f * look.Flash), 0.08f + 0.35f * look.Glint, look.Ground, rim, 0u, glm::vec4(0.0f), burn);
		const glm::vec3 tip = look.Center + forward * (0.68f * length);
		const glm::vec3 heel = look.Center + forward * (BladeHeel * length);
		DrawGlow(renderer, look.Center + forward * (0.2f * length), 0.36f * length + 0.05f, ObsidianSheen * ((0.03f + 0.05f * look.Glint) * (1.0f - look.Dissolve)), 0.45f);
		if (look.Sweep >= 0.0f && look.Sweep <= 1.0f)
		{
			const float fade = std::sin(look.Sweep * Pi);
			const float back = std::max(look.Sweep - 0.4f, 0.0f);
			const glm::vec3 front = glm::mix(heel, tip, look.Sweep);
			for (const float edge : { 1.0f, -1.0f })
			{
				const glm::vec3 a = glm::mix(heel, tip, back) + side * (edge * 0.12f * length * (1.0f - back));
				const glm::vec3 b = front + side * (edge * 0.12f * length * (1.0f - look.Sweep));
				ParticleSprite line = Stretched(ParticleShape::Beam, a, b, 0.01f * look.Size, glm::vec3(0.75f, 0.5f, 1.2f) * (0.5f * fade));
				line.Emission = 0.6f;
				renderer.DrawSprite(line);
			}
			DrawStar(renderer, front, 0.08f * look.Size * fade, glm::vec3(0.8f, 0.65f, 1.2f) * (0.6f * fade), time * 3.0f + float(seed % 7u));
		}
		if (look.Flash > 0.01f)
			DrawStar(renderer, tip, 0.1f * look.Size * look.Flash, glm::vec3(0.9f, 0.75f, 1.2f) * (0.6f * look.Flash), time * 2.0f + float(seed % 13u));
	}

	void DrawBladeSlash(WorldRenderer& renderer, const BladeSlash& slash)
	{
		if (slash.Age < 0.0f || slash.Age > ObsidianSlashSeconds)
			return;
		const float reveal = glm::smoothstep(0.0f, 0.04f, slash.Age);
		const float fade = 1.0f - glm::smoothstep(0.03f, ObsidianSlashSeconds, slash.Age);
		const float half = (0.5f + 0.25f * slash.Kill) * slash.Size;
		const glm::vec3 from = slash.Center - slash.Axis * half;
		const glm::vec3 to = from + slash.Axis * (2.0f * half * reveal);
		ParticleSprite glow = Stretched(ParticleShape::Beam, from, to, 0.08f * slash.Size, ObsidianSheen * (0.45f * fade));
		glow.Emission = 0.5f;
		renderer.DrawSprite(glow);
		ParticleSprite core = Stretched(ParticleShape::Beam, from, to, (0.016f + 0.01f * slash.Kill) * slash.Size * (0.5f + 0.5f * fade), glm::vec3(0.95f, 0.8f, 1.2f) * (1.3f * fade));
		core.Emission = 0.8f;
		renderer.DrawSprite(core);
	}

	glm::vec3 SwordTip(const CrystalSwordLook& look)
	{
		return look.Grip + glm::normalize(look.Forward) * ((SwordGuard + SwordBlade) * look.Scale);
	}

	void DrawCrystalSword(WorldRenderer& renderer, const SpellMeshes& meshes, const CrystalSwordLook& look, float time, uint32_t seed)
	{
		if (meshes.Sword == UINT32_MAX || look.Form <= 0.0f || look.Fade >= 1.0f)
			return;
		const glm::vec3 forward = glm::normalize(look.Forward);
		const float shown = std::clamp(look.Form, 0.0f, 1.0f) * (1.0f - std::clamp(look.Fade, 0.0f, 1.0f));
		const float glow = std::clamp(look.Glow, 0.0f, 1.0f);
		const float shimmer = 0.5f + 0.5f * std::sin(time * 7.0f + float(seed % 41u));
		const ModelDissolve form{ .Amount = std::max(1.0f - look.Form, look.Fade), .Edge = SwordCrystal * 2.4f, .Width = 0.12f, .Scale = 7.0f, .Seed = float(seed % 89u) };
		const glm::vec4 rim(SwordCrystal * (0.3f + 0.45f * glow + 0.1f * shimmer), 1.0f);
		renderer.DrawModel(meshes.Sword, Frame(look.Grip, forward, look.Flat, look.Scale), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.1f * glow), 0.08f + 0.16f * glow, look.Ground, rim, 0u, glm::vec4(0.0f), form);
		const glm::vec3 heel = look.Grip + forward * ((SwordGuard + 0.06f) * look.Scale);
		const glm::vec3 tip = SwordTip(look);
		ParticleSprite core = Stretched(ParticleShape::Beam, heel, tip - forward * (0.12f * look.Scale), 0.03f * look.Scale, SwordCrystal * ((0.1f + 0.28f * glow) * shown));
		core.Emission = 0.7f;
		renderer.DrawSprite(core);
		DrawGlow(renderer, glm::mix(heel, tip, 0.45f), 0.55f * look.Scale, SwordCrystal * ((0.025f + 0.08f * glow) * shown), 0.5f);
		if (glow > 0.2f)
			DrawStar(renderer, tip, 0.12f * look.Scale * glow, SwordCore * (0.45f * glow * shown), time * 3.0f + float(seed % 13u));
		for (uint32_t k = 0; k < SwordGlints; k++)
		{
			const float wave = std::sin(time * (5.0f + 1.3f * float(k)) + float(k) * 2.3f + float(seed % 31u));
			const float glint = std::pow(std::max(wave, 0.0f), 12.0f) * shown;
			if (glint > 0.02f)
				DrawStar(renderer, glm::mix(heel, tip, 0.2f + 0.22f * float(k)), 0.09f * look.Scale * glint, SwordCore * (0.6f * glint), time * 2.0f + float(k));
		}
		renderer.DrawLight({ .Position = glm::mix(heel, tip, 0.5f), .Radius = 2.4f * look.Scale, .Color = SwordCrystal * ((0.35f + 0.9f * glow) * shown) });
	}

	void DrawCrystalShard(WorldRenderer& renderer, const SpellMeshes& meshes, const CrystalShardLook& look, uint32_t seed)
	{
		const uint32_t mesh = look.Cluster ? meshes.SwordClusters[look.Variant % meshes.SwordClusters.size()] : meshes.SwordShards[look.Variant % meshes.SwordShards.size()];
		if (mesh == UINT32_MAX || look.Size <= 0.001f || look.Fade >= 1.0f)
			return;
		const ModelDissolve fade{ .Amount = look.Fade, .Edge = SwordCrystal * 2.2f, .Width = 0.14f, .Scale = 9.0f, .Seed = float(seed % 97u) };
		const glm::vec4 rim(SwordCrystal * (0.35f + 0.8f * look.Glow), 1.0f);
		renderer.DrawModel(mesh, Frame(look.Center, glm::normalize(look.Axis), look.Up, look.Size), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.15f * look.Glow), 0.12f + 0.4f * look.Glow, look.Ground, rim, 0u, glm::vec4(0.0f), fade);
	}

	void DrawSwordSlash(WorldRenderer& renderer, const SwordSlashLook& look)
	{
		if (look.Age < 0.0f || look.Age > look.Life)
			return;
		const float sweep = 1.0f - std::pow(1.0f - std::clamp(look.Age / SwordSweepSeconds, 0.0f, 1.0f), 3.0f);
		const float fade = 1.0f - glm::smoothstep(SwordSweepSeconds * 0.7f, look.Life, look.Age);
		const float side = look.Backhand ? -1.0f : 1.0f;
		const float after = std::clamp((look.Age - SwordSweepSeconds) / std::max(look.Life - SwordSweepSeconds, 1e-3f), 0.0f, 1.0f);
		const float spread = 1.0f + SwordSlashSpread * (1.0f - (1.0f - after) * (1.0f - after) * (1.0f - after));
		const auto crescent = [&](float size, float lift, float thickness, float span, float lag, const glm::vec3& color, float emission)
		{
			size *= spread;
			const float head = SwordSweepLead + (1.0f - SwordSweepLead) * (1.0f - std::pow(1.0f - std::clamp((look.Age - lag) / SwordSweepSeconds, 0.0f, 1.0f), 3.0f));
			ParticleSprite sprite = Sprite(ParticleShape::Crescent, look.Center + Up * lift, size, color * fade);
			sprite.Align = ParticleAlign::Ground;
			sprite.Rotation = look.Facing;
			sprite.Params = glm::vec4(look.HalfArc, head, thickness, side * span);
			sprite.Softness = 0.02f;
			sprite.Emission = emission;
			sprite.Seed = look.Seed;
			renderer.DrawSprite(sprite);
		};
		crescent(look.Reach, 0.0f, 0.3f, 1.8f, 0.0f, SwordCrystal * 0.42f, 0.45f);
		crescent(look.Reach, 0.0f, 0.08f, 1.4f, 0.0f, SwordCore * 0.75f, 0.6f);
		crescent(look.Reach * 0.8f, -0.18f, 0.2f, 1.2f, 0.02f, SwordCrystal * 0.2f, 0.4f);
		crescent(look.Reach * 1.06f, 0.12f, 0.16f, 1.6f, 0.035f, SwordCrystal * (0.18f * (spread - 1.0f) / SwordSlashSpread), 0.4f);
		const float angle = look.Facing + side * look.HalfArc * (2.0f * sweep - 1.0f);
		const glm::vec3 front = look.Center + Heading(angle) * (look.Reach * 0.96f);
		DrawStar(renderer, front, 0.26f * (1.0f - sweep * 0.6f) * fade, SwordCore * (0.55f * fade), angle);
		renderer.DrawLight({ .Position = look.Center + Heading(look.Facing) * (look.Reach * 0.5f), .Radius = look.Reach * 1.4f, .Color = SwordCrystal * (1.3f * fade) });
	}

	void DrawSwordCleave(WorldRenderer& renderer, const SwordCleaveLook& look)
	{
		if (look.Age < 0.0f || look.Age > look.Life)
			return;
		const float reveal = 1.0f - std::pow(1.0f - std::clamp(look.Age / 0.06f, 0.0f, 1.0f), 3.0f);
		const float fade = 1.0f - glm::smoothstep(0.05f, look.Life, look.Age);
		const glm::vec3 forward = Heading(look.Facing);
		const glm::vec3 start = look.Ground + Up * 0.14f + forward * 0.35f;
		const glm::vec3 end = look.Ground + Up * 0.14f + forward * look.Reach;
		const glm::vec3 head = glm::mix(start, end, reveal);
		ParticleSprite glow = Stretched(ParticleShape::Beam, start, head, 0.45f * look.Width, SwordCrystal * (0.35f * fade));
		glow.Emission = 0.5f;
		renderer.DrawSprite(glow);
		ParticleSprite core = Stretched(ParticleShape::Beam, start, head, 0.06f * look.Width, SwordCore * (1.1f * fade));
		core.Emission = 0.8f;
		renderer.DrawSprite(core);
		const float rise = std::exp(-look.Age * 9.0f);
		ParticleSprite wall = Stretched(ParticleShape::Beam, head, head + Up * (1.6f * rise), 0.14f, SwordCore * (0.6f * fade * rise));
		wall.Emission = 0.8f;
		renderer.DrawSprite(wall);
		DrawStar(renderer, head + Up * 0.1f, 0.38f * fade, SwordCore * (0.7f * fade), look.Facing);
		renderer.DrawLight({ .Position = glm::mix(start, end, 0.6f) + Up * 0.4f, .Radius = look.Reach * 1.3f, .Color = SwordCrystal * (1.8f * fade) });
	}

	TrailStyle SwordTrail()
	{
		TrailStyle style;
		style.Look = TrailLook::Swipe;
		style.Head = SwordCore * 1.2f;
		style.Tail = SwordCrystal * 0.55f;
		style.Core = glm::vec4(SwordCore * 1.5f, 1.0f);
		style.Width = 0.3f;
		style.WidthCurve = { 1.0f, 0.8f, 0.45f, 0.1f };
		style.AlphaCurve = { 1.0f, 0.7f, 0.35f, 0.0f };
		style.Life = 0.12f;
		style.Emission = 0.6f;
		style.Tiling = 1.0f;
		style.Scroll = 0.0f;
		style.Taper = 0.04f;
		style.Spacing = 0.03f;
		return style;
	}

	float FireballViolet(float power)
	{
		const float t = std::clamp((power - 0.1f) / 0.75f, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	glm::vec3 FireballColor(float power)
	{
		return glm::mix(glm::vec3(1.0f, 0.42f, 0.12f), glm::vec3(0.74f, 0.3f, 1.0f), FireballViolet(power));
	}

	glm::vec3 FireballFlameHue(float power)
	{
		return glm::mix(glm::vec3(1.3f, 0.57f, 0.15f), glm::vec3(0.5f, 0.12f, 0.85f), FireballViolet(power));
	}

	glm::vec3 FireballFlameTint(float power)
	{
		const glm::vec3 hue = FireballFlameHue(power);
		return hue / std::max({ hue.r, hue.g, hue.b });
	}

	namespace
	{
		constexpr glm::vec3 FireArcCore{ 1.5f, 1.2f, 2.1f };
		constexpr glm::vec3 FireArcGlow{ 0.5f, 0.16f, 1.0f };

		float FireArcDrive(float power)
		{
			return glm::smoothstep(0.55f, 1.0f, power);
		}

		float BoltFlicker(float since)
		{
			if (since < 0.0f)
				return 0.0f;
			if (since < 0.045f)
				return 1.0f;
			if (since < 0.065f)
				return 0.0f;
			return since < 0.095f ? 0.65f : 0.0f;
		}

		void FireBolt(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, uint32_t seed, float intensity, float width, float branches)
		{
			if (intensity <= 0.0f)
				return;
			LightningShape bolt;
			bolt.From = from;
			bolt.To = to;
			bolt.Seed = seed;
			bolt.CoarseLevels = 2;
			bolt.Core = 0.014f * width;
			bolt.Glow = 0.13f * width;
			bolt.Jag = 0.2f;
			bolt.Step = 0.04f;
			bolt.Branches = branches;
			bolt.BranchLength = 0.35f;
			bolt.BranchSpread = 0.9f;
			bolt.Depth = 1;
			bolt.Taper = 0.6f;
			bolt.Intensity = intensity;
			bolt.CoreColor = FireArcCore;
			bolt.GlowColor = FireArcGlow;
			renderer.DrawLightning(bolt);
		}

		glm::vec3 RandomDirection(uint32_t key)
		{
			const float around = 6.2832f * Unit(key);
			const float height = 2.0f * Unit(Mix(key, 1u)) - 1.0f;
			const float ring = std::sqrt(std::max(1.0f - height * height, 0.0f));
			return { std::cos(around) * ring, height, std::sin(around) * ring };
		}

		void DrawFireballArcs(WorldRenderer& renderer, const FireballLook& look, float radius, float time, uint32_t seed)
		{
			const float drive = FireArcDrive(look.Power);
			if (drive <= 0.0f)
				return;
			constexpr float Slot = 0.11f;
			const uint32_t lanes = 1u + uint32_t(drive + 0.5f);
			for (uint32_t lane = 0; lane < lanes; lane++)
			{
				const float shifted = time / Slot + 0.37f * float(lane);
				const uint32_t slot = uint32_t(shifted);
				const uint32_t key = Mix(seed + lane * 977u, slot);
				if (Unit(key) > 0.3f + 0.55f * drive)
					continue;
				const float flicker = BoltFlicker((shifted - float(slot)) * Slot);
				if (flicker <= 0.0f)
					continue;
				const glm::vec3 start = RandomDirection(Mix(key, 2u));
				const glm::vec3 axis = glm::normalize(glm::cross(start, RandomDirection(Mix(key, 4u))) + glm::vec3(1e-4f));
				const float swing = 1.0f + 0.9f * Unit(Mix(key, 6u));
				const glm::vec3 end = glm::normalize(start * std::cos(swing) + axis * std::sin(swing));
				const glm::vec3 apex = look.Center + glm::normalize(start + end + glm::vec3(1e-4f)) * (radius * (1.45f + 0.35f * Unit(Mix(key, 7u))));
				const float strength = flicker * (0.35f + 0.3f * drive);
				FireBolt(renderer, look.Center + start * (radius * 1.02f), apex, key, strength, 0.8f, 0.6f);
				FireBolt(renderer, apex, look.Center + end * (radius * 1.02f), Mix(key, 9u), strength, 0.8f, 0.6f);
			}
			if (!look.Grounded || drive < 0.8f)
				return;
			constexpr float StrikeSlot = 0.21f;
			const float shifted = time / StrikeSlot + 0.5f;
			const uint32_t slot = uint32_t(shifted);
			const uint32_t key = Mix(seed + 4111u, slot);
			if (Unit(key) > 0.55f)
				return;
			const float flicker = BoltFlicker((shifted - float(slot)) * StrikeSlot);
			const float angle = 6.2832f * Unit(Mix(key, 1u));
			const float reach = radius * (1.1f + 0.9f * Unit(Mix(key, 2u)));
			glm::vec3 target = look.Center + glm::vec3(std::cos(angle), 0.0f, std::sin(angle)) * reach;
			if (look.Flying)
				target -= look.Forward * (radius * (0.5f + 1.0f * Unit(Mix(key, 3u))));
			target.y = look.Ground + 0.02f;
			FireBolt(renderer, look.Center, target, key, flicker * 0.55f, 0.9f, 0.8f);
		}
	}

	void DrawPurpleFireball(WorldRenderer& renderer, const FireballLook& look, float time, uint32_t seed)
	{
		const float radius = FireballBallRadius * look.Size * look.Grow;
		if (radius < 0.01f)
			return;
		const float violet = FireballViolet(look.Power);
		const float full = look.Power >= 0.999f ? 1.0f : 0.0f;
		const float pulse = 0.5f + 0.5f * std::sin(time * 7.0f + float(seed & 255u));
		const float wobble = 1.0f + 0.04f * pulse;
		const float energy = 0.8f + 0.2f * look.Power + 0.12f * full * pulse;
		const glm::vec3 halo = glm::mix(glm::vec3(0.5f, 0.15f, 0.03f), glm::vec3(0.28f, 0.05f, 0.6f), violet);
		const glm::vec3 mid = glm::mix(glm::vec3(0.8f, 0.26f, 0.04f), glm::vec3(0.55f, 0.08f, 0.95f), violet);
		const glm::vec3 hue = FireballFlameHue(look.Power);
		const glm::vec3 hot = glm::mix(glm::vec3(1.2f, 0.95f, 0.6f), glm::vec3(1.0f, 0.72f, 1.2f), violet);
		if (!look.Flying)
			DrawHeatHaze(renderer, look.Center + Up * (0.6f * radius), radius * 2.4f, 0.012f + 0.016f * look.Power, 1.6f / radius, 1.4f, seed);
		DrawImplosion(renderer, look.Center, radius * (1.8f + 1.4f * look.Power), (0.015f + 0.1f * look.Power * look.Power) * (look.Flying ? 0.5f : 1.0f), 0.0f, 0);
		if (look.Flying)
		{
			const float grow = std::pow(std::max(look.Size, 1.0f), 0.8f);
			const glm::vec3 forward = look.Forward;
			DrawGlow(renderer, look.Center + forward * (0.22f * grow), 0.7f * grow, halo * 0.4f);
			DrawFlame(renderer, { .Base = look.Center + forward * (0.16f * grow), .Axis = -forward, .Width = 0.35f * grow, .Height = 1.1f * grow, .Color = hue, .Wild = 1.0f, .Speed = 6.0f / std::sqrt(grow), .Comet = 1.0f, .Additive = 0.3f, .Emission = 0.35f, .Seed = seed });
			DrawFlame(renderer, { .Base = look.Center + forward * (0.12f * grow), .Axis = -forward, .Width = 0.23f * grow, .Height = 0.82f * grow, .Color = hue * 0.85f, .Wild = 1.0f, .Speed = 8.0f / std::sqrt(grow), .Comet = 0.6f, .Additive = 0.3f, .Emission = 0.35f, .Seed = seed * 13u + 7u });
			DrawGlow(renderer, look.Center + forward * (0.17f * grow), 0.095f * grow, hot);
			DrawFireballArcs(renderer, look, 0.3f * grow, time, seed);
			return;
		}
		ParticleSprite aura = Sprite(ParticleShape::Glow, look.Center, radius * (1.8f + 0.4f * look.Power), halo * (0.12f + 0.12f * look.Power));
		aura.Softness = 0.3f;
		aura.Emission = 0.3f;
		renderer.DrawSprite(aura);
		{
			const uint32_t wisps = 4u + uint32_t(10.0f * look.Power);
			for (uint32_t i = 0; i < wisps; i++)
			{
				const uint32_t key = Mix(seed, i + 1u);
				const float period = 0.5f + 0.25f * Unit(key) - 0.15f * look.Power;
				const float u = std::fmod(time / period + Unit(Mix(key, 3u)), 1.0f);
				const float start = 6.2832f * Unit(Mix(key, 5u));
				const float rise = (Unit(Mix(key, 7u)) - 0.5f) * 2.2f * radius;
				const auto at = [&](float s)
				{
					const float angle = start + 3.2f * s;
					const float distance = radius * (1.0f + 2.4f * std::pow(1.0f - s, 1.4f));
					return look.Center + glm::vec3(std::cos(angle), 0.0f, std::sin(angle)) * distance + Up * (rise * (1.0f - s));
				};
				const float fade = std::sin(3.1416f * u);
				ParticleSprite wisp = Stretched(ParticleShape::Beam, at(std::max(u - 0.14f, 0.0f)), at(u), 0.035f + 0.025f * look.Size, mid * (fade * (0.5f + 0.5f * look.Power)));
				wisp.Emission = 0.4f;
				renderer.DrawSprite(wisp);
			}
		}
		const auto ball = [&](float size, const glm::vec3& color, float speed, uint32_t salt)
		{
			ParticleSprite flame = Sprite(ParticleShape::FlameSheet, look.Center, size * wobble, color * energy);
			flame.Rotation = 1.5708f;
			flame.Additive = 0.3f;
			flame.Emission = 0.35f;
			flame.Softness = 0.15f;
			flame.Params = glm::vec4(1.0f, speed, 0.0f, 1.0f);
			flame.Seed = salt;
			renderer.DrawSprite(flame);
		};
		ball(radius * 1.75f, hue * 0.55f, 1.8f, seed);
		ball(radius * 1.3f, hue * 0.7f, 2.6f, seed * 7u + 3u);
		DrawGlow(renderer, look.Center, radius * 0.18f, hot * (0.45f + 0.1f * pulse));
		DrawFireballArcs(renderer, look, radius, time, seed);
		if (look.Flash > 0.0f)
		{
			ParticleSprite flare = Sprite(ParticleShape::Star, look.Center, radius * 2.0f * look.Flash, glm::vec3(0.55f, 0.3f, 0.85f) * look.Flash);
			flare.Rotation = time * 0.6f;
			flare.Emission = 0.5f;
			renderer.DrawSprite(flare);
		}
	}

	float FireballBlastFire(float power)
	{
		return 0.6f + 0.3f * std::clamp(power, 0.0f, 1.0f);
	}

	float FireballBlastSmoke(float power)
	{
		return 1.0f + 0.8f * std::clamp(power, 0.0f, 1.0f);
	}

	float FireballBlastLife(float power)
	{
		return std::max(FireballBlastFire(power), FireballBlastSmoke(power));
	}

	void DrawFireballBlast(WorldRenderer& renderer, const FireballBlastLook& look, uint32_t seed)
	{
		if (look.Age < 0.0f || look.Age >= FireballBlastLife(look.Power))
			return;
		const float t = look.Age / FireballBlastFire(look.Power);
		const float smoke = look.Age / FireballBlastSmoke(look.Power);
		const float violet = FireballViolet(look.Power);
		const glm::vec3 halo = glm::mix(glm::vec3(0.5f, 0.15f, 0.03f), glm::vec3(0.28f, 0.05f, 0.6f), violet);
		const glm::vec3 hot = glm::mix(glm::vec3(1.2f, 0.95f, 0.6f), glm::vec3(1.0f, 0.72f, 1.2f), violet);
		renderer.GetBlast().Draw({
			.Ground = look.Ground,
			.Radius = look.Radius,
			.Fire = t,
			.Smoke = smoke,
			.SmokeAmount = 0.15f + 0.85f * look.Power * look.Power,
			.Power = look.Power,
			.Violet = violet,
			.Spin = FireballBlastSpin,
			.Height = FireballBlastDome,
			.Seed = seed,
		});
		if (const float drive = FireArcDrive(look.Power); drive > 0.0f)
		{
			const float R = look.Radius;
			const auto heading = [](float angle) { return glm::vec3(std::cos(angle), 0.0f, std::sin(angle)); };
			const uint32_t forks = 2u + uint32_t(4.0f * drive);
			for (uint32_t i = 0; i < forks; i++)
			{
				const uint32_t key = Mix(seed, 400u + i);
				const float flicker = BoltFlicker(look.Age - (0.02f + 0.28f * Unit(key)));
				if (flicker <= 0.0f)
					continue;
				const glm::vec3 way = heading(6.2832f * (float(i) + Unit(Mix(key, 1u))) / float(forks));
				FireBolt(renderer, look.Ground + way * (0.15f * R) + Up * 0.35f, look.Ground + way * (R * (0.8f + 0.25f * Unit(Mix(key, 2u)))) + Up * 0.05f, key, flicker, 1.3f, 1.6f);
			}
			for (uint32_t i = 0; i < 2u; i++)
			{
				const uint32_t key = Mix(seed, 500u + i);
				const float flicker = BoltFlicker(look.Age - (0.08f + 0.3f * Unit(key)));
				if (flicker <= 0.0f)
					continue;
				const float angle = 6.2832f * Unit(Mix(key, 1u));
				FireBolt(renderer, look.Ground + heading(angle) * (0.35f * R) + Up * (0.25f * R), look.Ground + heading(angle + 0.9f) * (0.7f * R) + Up * (0.18f * R), key, flicker * 0.8f, 0.9f, 1.0f);
			}
			for (uint32_t i = 0; drive > 0.8f && i < 3u; i++)
			{
				const uint32_t key = Mix(seed, 600u + i);
				const float flicker = BoltFlicker(look.Age - (0.5f + 0.8f * Unit(key)));
				if (flicker <= 0.0f)
					continue;
				const float angle = 6.2832f * Unit(Mix(key, 1u));
				const glm::vec3 from = look.Ground + heading(angle) * (0.7f * R) + Up * (0.3f * R);
				const glm::vec3 to = look.Ground + heading(angle + 0.5f) * (0.85f * R) + Up * (0.25f * R);
				FireBolt(renderer, from, to, key, flicker * 0.6f, 0.8f, 0.8f);
				DrawGlow(renderer, 0.5f * (from + to), 0.15f * R, FireballColor(look.Power) * (0.08f * flicker));
			}
		}
		const float haze = 1.0f - glm::smoothstep(0.3f, 1.0f, smoke);
		DrawHeatHaze(renderer, look.Ground + Up * (0.5f + 0.2f * look.Radius), look.Radius * 1.15f, (0.008f + 0.014f * look.Power) * haze, 1.6f / look.Radius, 1.4f, seed);
		if (t >= 1.0f)
			return;
		const float fade = 1.0f - glm::smoothstep(0.45f, 1.0f, t);
		const float flash = std::exp(-t * 12.0f);
		DrawGroundGlow(renderer, look.Ground, look.Radius * 1.1f, halo * (0.3f * fade), 1.0f);
		DrawGroundGlow(renderer, look.Ground, look.Radius * 0.6f, hot * ((0.5f + 0.3f * look.Power) * flash), 1.0f);
		const float peak = 0.35f + (0.65f + 1.6f * look.Power) * flash;
		renderer.DrawLight({ .Position = look.Ground + Up * (0.6f * look.Radius * FireballBlastDome + 0.3f), .Radius = look.Radius * (2.2f + 1.2f * look.Power), .Color = FireballColor(look.Power) * (FireballBlastLight * peak * fade) });
	}

	void DrawComet(WorldRenderer& renderer, const CometLook& look, float time, uint32_t seed)
	{
		const float s = look.Scale;
		const glm::vec3 forward = look.Direction;
		const glm::vec3 head = look.Center - forward * (0.1f * s);
		const float flicker = 0.92f + 0.08f * std::sin(time * 23.0f + float(seed & 255u));
		DrawGlow(renderer, head, 1.8f * s, glm::vec3(0.2f, 0.06f, 0.014f) * flicker);
		DrawFlame(renderer, { .Base = head, .Axis = -forward, .Width = 0.95f * s, .Height = 4.4f * s, .Color = { 0.95f, 0.34f, 0.07f }, .Wild = 1.0f, .Speed = 4.0f, .Comet = 1.0f, .Additive = 0.25f, .Emission = 0.3f, .Seed = seed });
		DrawFlame(renderer, { .Base = head - forward * (0.1f * s), .Axis = -forward, .Width = 0.62f * s, .Height = 2.8f * s, .Color = { 1.05f, 0.44f, 0.1f }, .Wild = 1.0f, .Speed = 5.0f, .Comet = 0.8f, .Additive = 0.25f, .Emission = 0.3f, .Seed = seed * 13u + 7u });
		ParticleSprite ball = Sprite(ParticleShape::FlameSheet, look.Center - forward * (0.15f * s), 0.75f * s, glm::vec3(1.0f, 0.4f, 0.09f));
		ball.Align = ParticleAlign::Velocity;
		ball.Axis = -forward;
		ball.Additive = 0.25f;
		ball.Emission = 0.3f;
		ball.Softness = 0.15f;
		ball.Params = glm::vec4(1.0f, 3.0f, 0.0f, 0.7f);
		ball.Seed = seed * 7u + 3u;
		renderer.DrawSprite(ball);
		DrawGlow(renderer, look.Center, 0.12f * s, glm::vec3(1.1f, 0.8f, 0.45f) * flicker);
	}

	void DrawCometBlast(WorldRenderer& renderer, const CometBlastLook& look, uint32_t seed)
	{
		if (look.Age < 0.0f || look.Age >= CometBlastSmoke)
			return;
		const float t = look.Age / CometBlastFire;
		const float smoke = look.Age / CometBlastSmoke;
		const float R = look.Radius * 0.9f;
		renderer.GetBlast().Draw({
			.Ground = look.Ground,
			.Radius = R,
			.Fire = t,
			.Smoke = smoke,
			.SmokeAmount = 0.85f,
			.Power = 0.7f,
			.Violet = 0.0f,
			.Spin = 0.3f,
			.Height = 0.6f,
			.Seed = seed,
		});
		const float haze = 1.0f - glm::smoothstep(0.3f, 1.0f, smoke);
		DrawHeatHaze(renderer, look.Ground + Up * (0.5f + 0.2f * R), R * 1.2f, 0.02f * haze, 1.6f / R, 1.4f, seed);
		if (t >= 1.0f)
			return;
		const float fade = 1.0f - glm::smoothstep(0.45f, 1.0f, t);
		const float flash = std::exp(-t * 10.0f);
		DrawGroundGlow(renderer, look.Ground, R * 1.15f, glm::vec3(0.5f, 0.15f, 0.03f) * (0.35f * fade), 1.0f);
		DrawGroundGlow(renderer, look.Ground, R * 0.65f, glm::vec3(1.2f, 0.95f, 0.6f) * (0.8f * flash), 1.0f);
		renderer.DrawLight({ .Position = look.Ground + Up * (0.4f * R + 0.3f), .Radius = R * 2.8f, .Color = CometLight * (CometBlastLight * (0.35f + 2.0f * flash) * fade) });
	}

	namespace
	{
		struct ShieldFrame
		{
			glm::vec3 Center{ 0.0f };
			glm::vec3 Forward{ 1.0f, 0.0f, 0.0f };
			glm::vec3 Right{ 0.0f, 0.0f, 1.0f };
			glm::vec3 Rise{ 0.0f, 1.0f, 0.0f };
		};

		ShieldFrame ShieldFrameOf(const glm::vec3& base, float facing)
		{
			const glm::vec3 forward(std::cos(facing), 0.0f, std::sin(facing));
			ShieldFrame frame;
			frame.Center = base + Up * HexShieldLift - forward * HexShieldBack;
			frame.Forward = forward * std::cos(HexShieldTilt) + Up * std::sin(HexShieldTilt);
			frame.Rise = Up * std::cos(HexShieldTilt) - forward * std::sin(HexShieldTilt);
			frame.Right = glm::vec3(-forward.z, 0.0f, forward.x);
			return frame;
		}
	}

	float HexShieldRim(const glm::vec2& point)
	{
		return glm::length(point / glm::vec2(HexShieldHalfWidth, point.y > 0.0f ? HexShieldHalfTop : HexShieldHalfBottom));
	}

	std::span<const HexShieldCell> HexShieldCells()
	{
		static const std::vector<HexShieldCell> cells = []
		{
			std::vector<HexShieldCell> list;
			const float size = HexShieldCellSize;
			for (int32_t column = -12; column <= 12; column++)
				for (int32_t row = -12; row <= 12; row++)
				{
					const glm::vec2 point(1.5f * size * float(column), 1.7320508f * size * (float(row) + 0.5f * float(column & 1)));
					if (HexShieldRim(point) <= 1.0f && list.size() < HexShieldMaxCells)
						list.push_back({ point, HexShieldRim(point), Mix(uint32_t(column + 64), uint32_t(row + 64)) });
				}
			return list;
		}();
		return cells;
	}

	glm::vec3 HexShieldPoint(const glm::vec3& base, float facing, const glm::vec2& point, glm::vec3* normal)
	{
		const ShieldFrame frame = ShieldFrameOf(base, facing);
		const float length = glm::length(point);
		const float angle = length / HexShieldSphere;
		glm::vec3 direction = frame.Forward;
		if (length > 1e-5f)
			direction = frame.Forward * std::cos(angle) + (frame.Right * point.x + frame.Rise * point.y) * (std::sin(angle) / length);
		if (normal)
			*normal = direction;
		return frame.Center + direction * HexShieldSphere;
	}

	glm::vec2 HexShieldLocal(const glm::vec3& base, float facing, const glm::vec3& world)
	{
		const ShieldFrame frame = ShieldFrameOf(base, facing);
		const glm::vec3 offset = world - frame.Center;
		const float distance = glm::length(offset);
		if (distance < 1e-4f)
			return glm::vec2(0.0f);
		const glm::vec3 direction = offset / distance;
		const glm::vec2 side(glm::dot(direction, frame.Right), glm::dot(direction, frame.Rise));
		const float across = glm::length(side);
		if (across < 1e-5f)
			return glm::vec2(0.0f);
		return side * (std::acos(std::clamp(glm::dot(direction, frame.Forward), -1.0f, 1.0f)) * HexShieldSphere / across);
	}

	namespace
	{
		struct CellPose
		{
			glm::vec3 Center{ 0.0f };
			glm::vec3 Normal{ 0.0f };
			glm::vec3 Right{ 0.0f };
			glm::vec3 Up{ 0.0f };
		};

		CellPose HexCellPose(const HexShieldLook& look, const glm::vec2& point)
		{
			CellPose pose;
			pose.Center = HexShieldPoint(look.Base, look.Facing, point, &pose.Normal);
			const glm::vec3 ahead = HexShieldPoint(look.Base, look.Facing, point + glm::vec2(0.02f, 0.0f));
			pose.Right = glm::normalize(ahead - pose.Center - pose.Normal * glm::dot(ahead - pose.Center, pose.Normal));
			pose.Up = glm::cross(pose.Normal, pose.Right);
			return pose;
		}

		float HexCellScar(const HexShieldLook& look, const glm::vec2& point)
		{
			float scar = 0.0f;
			for (uint32_t k = 0; k < std::min<uint32_t>(look.ScarCount, uint32_t(HexShieldScarSlots)); k++)
			{
				const glm::vec2 offset = point - look.Scars[k];
				scar = std::max(scar, std::exp(-glm::dot(offset, offset) / 0.12f));
			}
			return scar;
		}

		float HexCellStress(const HexShieldLook& look, const HexShieldCell& cell)
		{
			const float damage = 1.0f - std::clamp(look.Guard, 0.0f, 1.0f);
			return damage * 1.1f - (0.25f + 0.65f * Unit(cell.Seed) - 0.35f * HexCellScar(look, cell.Point));
		}

		float HexCellForm(const HexShieldCell& cell, float raise)
		{
			const float delay = std::min(cell.Rim, 1.0f) * 0.42f + 0.16f * Unit(Mix(cell.Seed, 3u));
			return std::clamp((raise - delay) / 0.42f, 0.0f, 1.0f);
		}

		EnergyShield HexEnergy(const HexShieldLook& look)
		{
			const ShieldFrame frame = ShieldFrameOf(look.Base, look.Facing);
			EnergyShield shield;
			shield.Center = frame.Center;
			shield.Radius = HexShieldSphere;
			shield.Forward = frame.Forward;
			shield.Right = frame.Right;
			shield.Rise = frame.Rise;
			shield.Color = HexShieldColor;
			shield.Lattice = HexShieldCellSize;
			shield.Shatter = look.Shatter;
			shield.Seed = look.Seed;
			return shield;
		}
	}

	void DrawHexShield(WorldRenderer& renderer, const HexShieldLook& look, float time)
	{
		if (look.Raise <= 0.01f)
			return;
		EnergyShield shield = HexEnergy(look);
		const float weak = std::clamp((0.4f - look.Guard) / 0.4f, 0.0f, 1.0f);
		shield.Intensity = 1.0f - 0.25f * weak;
		for (size_t k = 0; k < HexShieldImpactSlots; k++)
			shield.Impacts[k] = glm::vec4(look.Impacts[k].Point, look.Impacts[k].Age, look.Impacts[k].Strength);
		const uint32_t index = renderer.AddShield(shield);
		if (index == UINT32_MAX)
			return;
		const std::span<const HexShieldCell> cells = HexShieldCells();
		for (size_t i = 0; i < cells.size(); i++)
		{
			const HexShieldCell& cell = cells[i];
			if (look.Popped[i])
				continue;
			const float form = HexCellForm(cell, look.Raise);
			if (form <= 0.0f)
				continue;
			float flare = look.Flash * (1.0f - 0.5f * std::min(cell.Rim, 1.0f));
			float ripple = 0.0f;
			for (const HexShieldImpact& impact : look.Impacts)
			{
				if (impact.Age >= 1.1f)
					continue;
				const float reach = glm::length(cell.Point - impact.Point);
				flare += impact.Strength * std::exp(-reach * reach / 0.12f) * std::exp(-impact.Age * 5.0f);
				const float front = (reach - impact.Age * 2.6f) / 0.22f;
				ripple += impact.Strength * std::exp(-front * front - impact.Age * 2.4f);
			}
			ripple = std::min(ripple, 1.1f);
			const float stress = HexCellStress(look, cell);
			const float unstable = std::clamp((stress + 0.3f) / 0.3f, 0.0f, 1.0f);
			const float crack = std::clamp(stress / 0.3f, 0.0f, 1.0f);
			const uint32_t beat = uint32_t(time * (14.0f + 8.0f * Unit(cell.Seed)) + 7.0f * Unit(Mix(cell.Seed, 5u)));
			float flicker = 1.0f - unstable * (0.25f + 0.35f * Unit(Mix(cell.Seed, beat)));
			if (Unit(Mix(cell.Seed ^ 0x5bd1u, beat)) < 0.12f * crack)
				flicker *= 0.25f;
			const float ease = 1.0f - std::pow(1.0f - form, 3.0f) + 0.12f * std::sin(form * 3.14159265f);
			const CellPose pose = HexCellPose(look, cell.Point);
			EnergyCell energy;
			energy.Center = pose.Center;
			energy.Right = pose.Right;
			energy.Up = pose.Up;
			energy.Plane = cell.Point;
			energy.Size = HexShieldCellSize * 1.04f * (0.3f + 0.7f * ease);
			energy.Lift = 0.32f * (1.0f - form) * (1.0f - form) + 0.08f * ripple;
			energy.Form = form;
			energy.Flare = std::min(flare, 1.6f);
			energy.Crack = crack;
			energy.Flicker = flicker;
			energy.Fade = 1.0f;
			energy.Shield = index;
			energy.Seed = cell.Seed ^ look.Seed;
			renderer.DrawCell(energy);
		}
	}

	void ShatterHexShield(HexShieldLook& look, std::vector<HexShieldShard>& shards)
	{
		const std::span<const HexShieldCell> cells = HexShieldCells();
		std::vector<std::pair<float, size_t>> order;
		for (size_t i = 0; i < cells.size(); i++)
			if (HexCellForm(cells[i], look.Raise) > 0.5f)
				order.push_back({ HexCellStress(look, cells[i]) + 0.15f * Unit(Mix(cells[i].Seed, look.Seed)), i });
		std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
		const size_t pops = std::min(order.size(), std::max<size_t>(10, order.size() * 2 / 5));
		for (size_t n = 0; n < pops; n++)
		{
			const HexShieldCell& cell = cells[order[n].second];
			look.Popped.set(order[n].second);
			const uint32_t key = Mix(cell.Seed ^ look.Seed, 7u);
			const CellPose pose = HexCellPose(look, cell.Point);
			const glm::vec3 scatter = glm::vec3(Unit(Mix(key, 1u)), Unit(Mix(key, 2u)), Unit(Mix(key, 3u))) * 2.0f - 1.0f;
			HexShieldShard shard;
			shard.Position = pose.Center;
			shard.Velocity = pose.Normal * (0.6f + 1.1f * Unit(Mix(key, 4u))) + Up * (0.3f + 0.9f * Unit(Mix(key, 5u))) + scatter * 0.35f;
			shard.Right = pose.Right;
			shard.Up = pose.Up;
			shard.Axis = glm::length(scatter) > 1e-3f ? glm::normalize(scatter) : Up;
			shard.Plane = cell.Point;
			shard.Spin = (3.0f + 6.0f * Unit(Mix(key, 6u))) * (Unit(Mix(key, 8u)) > 0.5f ? 1.0f : -1.0f);
			shard.Size = HexShieldCellSize * (0.85f + 0.1f * Unit(Mix(key, 9u)));
			shard.Floor = look.Base.y;
			shard.Life = 0.9f + 0.5f * Unit(Mix(key, 10u));
			shard.Seed = key;
			shards.push_back(shard);
		}
	}

	void UpdateHexShards(std::vector<HexShieldShard>& shards, float dt)
	{
		const auto turn = [](const glm::vec3& v, const glm::vec3& axis, float angle)
		{
			return v * std::cos(angle) + glm::cross(axis, v) * std::sin(angle) + axis * (glm::dot(axis, v) * (1.0f - std::cos(angle)));
		};
		for (HexShieldShard& shard : shards)
		{
			shard.Age += dt;
			shard.Velocity.y -= 9.0f * dt;
			shard.Velocity *= std::exp(-0.8f * dt);
			shard.Position += shard.Velocity * dt;
			if (shard.Position.y < shard.Floor + 0.04f && shard.Velocity.y < 0.0f)
			{
				shard.Position.y = shard.Floor + 0.04f;
				shard.Velocity = glm::vec3(shard.Velocity.x * 0.5f, -shard.Velocity.y * 0.3f, shard.Velocity.z * 0.5f);
				shard.Spin *= 0.5f;
			}
			const float angle = shard.Spin * dt;
			shard.Right = glm::normalize(turn(shard.Right, shard.Axis, angle));
			shard.Up = glm::normalize(turn(shard.Up, shard.Axis, angle));
		}
		std::erase_if(shards, [](const HexShieldShard& shard) { return shard.Age >= shard.Life; });
	}

	void DrawHexShards(WorldRenderer& renderer, std::span<const HexShieldShard> shards)
	{
		if (shards.empty())
			return;
		EnergyShield shield;
		shield.Color = HexShieldColor;
		shield.Lattice = HexShieldCellSize;
		for (glm::vec4& impact : shield.Impacts)
			impact = glm::vec4(0.0f, 0.0f, 1e3f, 0.0f);
		const uint32_t index = renderer.AddShield(shield);
		if (index == UINT32_MAX)
			return;
		for (const HexShieldShard& shard : shards)
		{
			const float t = std::clamp(shard.Age / shard.Life, 0.0f, 1.0f);
			EnergyCell cell;
			cell.Center = shard.Position;
			cell.Right = shard.Right;
			cell.Up = shard.Up;
			cell.Plane = shard.Plane;
			cell.Size = shard.Size * (1.0f - 0.3f * t * t);
			cell.Form = 1.0f;
			cell.Flare = 0.7f * std::exp(-shard.Age * 6.0f);
			cell.Flicker = 1.0f - 0.5f * t * Unit(Mix(shard.Seed, uint32_t(shard.Age * 24.0f)));
			cell.Fade = 1.0f - std::clamp((t - 0.55f) / 0.45f, 0.0f, 1.0f);
			cell.Shield = index;
			cell.Seed = shard.Seed;
			cell.Attached = false;
			renderer.DrawCell(cell);
		}
	}

	namespace
	{
		constexpr uint32_t DiamondSides = 8;
		constexpr float DiamondIor = 2.42f;
		constexpr float DiamondDispersion = 0.4f;
		constexpr glm::vec3 DiamondAbsorption{ 0.22f, 0.75f, 0.08f };
		constexpr float DiamondScatter = 0.06f;
		constexpr float DiamondCausticHeight = 1.1f;

		struct DiamondFacet
		{
			std::array<glm::vec3, 3> Corners{};
			glm::vec3 Center{ 0.0f };
			glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
			float Delay = 0.0f;
			uint32_t Seed = 0;
		};

		glm::vec3 RingPoint(float angle, float radius, float height)
		{
			return glm::vec3(std::cos(angle) * radius, height, std::sin(angle) * radius);
		}

		glm::vec3 SpinY(const glm::vec3& point, float angle)
		{
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			return glm::vec3(point.x * c - point.z * s, point.y, point.x * s + point.z * c);
		}

		glm::vec3 Turn(const glm::vec3& point, const glm::vec3& axis, float angle)
		{
			const float c = std::cos(angle);
			return point * c + glm::cross(axis, point) * std::sin(angle) + axis * (glm::dot(axis, point) * (1.0f - c));
		}

		glm::vec2 FacetPlane(const DiamondFacet& facet, const glm::vec3& point)
		{
			const glm::vec3 along = glm::normalize(facet.Corners[1] - facet.Corners[0]);
			const glm::vec3 across = glm::cross(facet.Normal, along);
			const glm::vec3 offset = point - facet.Corners[0];
			return glm::vec2(glm::dot(offset, along), glm::dot(offset, across));
		}

		float DiamondRadius(float height)
		{
			constexpr std::array<glm::vec2, 5> profile{ { { DiamondFoot, 0.0f }, { DiamondPavilion, DiamondPavilionRadius }, { DiamondGirdle, DiamondGirdleRadius }, { DiamondCrown, DiamondCrownRadius }, { DiamondApex, 0.0f } } };
			for (size_t i = 1; i < profile.size(); i++)
				if (height <= profile[i].x)
					return std::lerp(profile[i - 1].y, profile[i].y, (height - profile[i - 1].x) / (profile[i].x - profile[i - 1].x)) * std::cos(3.14159265f / float(DiamondSides)) * 1.04f;
			return 0.0f;
		}

		std::span<const DiamondFacet> DiamondFacets()
		{
			static const std::vector<DiamondFacet> facets = []
			{
				std::vector<DiamondFacet> list;
				const auto add = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c)
				{
					DiamondFacet facet;
					facet.Center = (a + b + c) / 3.0f;
					glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
					if (glm::dot(normal, glm::vec3(facet.Center.x, 0.0f, facet.Center.z)) < 0.0f)
					{
						std::swap(b, c);
						normal = -normal;
					}
					facet.Corners = { a, b, c };
					facet.Normal = normal;
					facet.Seed = Mix(uint32_t(list.size()) + 101u, 0x51ED27u);
					facet.Delay = 0.1f * (facet.Center.y / DiamondApex) + 0.06f * Unit(Mix(facet.Seed, 2u));
					list.push_back(facet);
				};
				const float step = 2.0f * 3.14159265f / float(DiamondSides);
				const glm::vec3 foot(0.0f, DiamondFoot, 0.0f);
				const glm::vec3 apex(0.0f, DiamondApex, 0.0f);
				for (uint32_t i = 0; i < DiamondSides; i++)
				{
					const float angle = step * float(i);
					const glm::vec3 g0 = RingPoint(angle, DiamondGirdleRadius, DiamondGirdle);
					const glm::vec3 g1 = RingPoint(angle + step, DiamondGirdleRadius, DiamondGirdle);
					const glm::vec3 p0 = RingPoint(angle - 0.5f * step, DiamondPavilionRadius, DiamondPavilion);
					const glm::vec3 p1 = RingPoint(angle + 0.5f * step, DiamondPavilionRadius, DiamondPavilion);
					const glm::vec3 c0 = RingPoint(angle + 0.5f * step, DiamondCrownRadius, DiamondCrown);
					const glm::vec3 c1 = RingPoint(angle + 1.5f * step, DiamondCrownRadius, DiamondCrown);
					add(foot, p0, p1);
					add(p0, g0, p1);
					add(g0, g1, p1);
					add(g0, g1, c0);
					add(c0, g1, c1);
					add(c0, c1, apex);
				}
				return list;
			}();
			return facets;
		}
	}

	glm::vec3 DiamondPoint(const glm::vec3& base, const glm::vec3& toward, float height, glm::vec3* normal)
	{
		glm::vec3 flat(toward.x - base.x, 0.0f, toward.z - base.z);
		const float length = glm::length(flat);
		flat = length > 1e-4f ? flat / length : glm::vec3(1.0f, 0.0f, 0.0f);
		const float y = std::clamp(height, DiamondFoot, DiamondApex);
		if (normal)
			*normal = glm::normalize(flat + Up * (y > DiamondGirdle ? 0.45f : -0.6f));
		return base + flat * DiamondRadius(y) + Up * y;
	}

	glm::vec3 DiamondLocal(const DiamondWardLook& look, const glm::vec3& world)
	{
		return SpinY(world - look.Base, -look.Spin);
	}

	void DrawDiamondWard(WorldRenderer& renderer, const DiamondWardLook& look, float time)
	{
		if (look.Fade <= 0.01f)
			return;
		const float damage = 1.0f - std::clamp(look.Ward, 0.0f, 1.0f);
		const float weak = std::clamp((0.5f - look.Ward) / 0.5f, 0.0f, 1.0f);
		const uint32_t beat = uint32_t(time * 14.0f);
		const float span = DiamondFormSeconds - 0.16f;
		const glm::vec3 edge = glm::mix(DiamondGold, DiamondViolet, 0.35f * weak);
		for (const DiamondFacet& facet : DiamondFacets())
		{
			const float t = std::clamp((look.Age - facet.Delay) / span, 0.0f, 1.0f);
			if (t <= 0.0f)
				continue;
			const float settle = 1.0f - std::pow(1.0f - t, 3.0f);
			const float size = (0.3f + 0.7f * settle) * (1.0f + 0.1f * std::sin(t * 3.14159265f));
			const float twist = (1.0f - settle) * (Unit(Mix(facet.Seed, 4u)) - 0.5f) * 2.2f;
			const glm::vec3 drift = facet.Normal * ((1.0f - settle) * (1.0f + 0.7f * Unit(Mix(facet.Seed, 3u)))) + Up * ((1.0f - settle) * 0.35f);
			float flash = 0.35f * look.Flash + 1.3f * (1.0f - t);
			for (const DiamondImpact& impact : look.Impacts)
				if (impact.Age < 0.8f)
				{
					const glm::vec3 offset = facet.Center - impact.Point;
					flash += impact.Strength * std::exp(-glm::dot(offset, offset) / 0.2f) * std::exp(-impact.Age * 8.0f);
				}
			float scar = 0.0f;
			glm::vec3 origin = facet.Corners[facet.Seed % 3u];
			for (uint32_t k = 0; k < std::min<uint32_t>(look.ScarCount, uint32_t(DiamondScarSlots)); k++)
			{
				const glm::vec3 offset = facet.Center - look.Scars[k];
				const float closeness = std::exp(-glm::dot(offset, offset) / 0.22f);
				if (closeness > scar)
				{
					scar = closeness;
					origin = look.Scars[k];
				}
			}
			const float crack = std::clamp((damage * 1.3f - (0.15f + 0.75f * Unit(facet.Seed)) + 0.45f * scar * damage) / 0.3f, 0.0f, 1.0f);
			const bool faint = Unit(Mix(facet.Seed, 7u)) < weak * 0.55f;
			const float flicker = faint ? 0.3f + 0.7f * Unit(Mix(facet.Seed, beat)) : 1.0f;
			std::array<glm::vec3, 3> corners;
			for (size_t k = 0; k < 3; k++)
				corners[k] = look.Base + SpinY(facet.Center + (facet.Corners[k] - facet.Center) * size + drift, look.Spin + twist);
			GlassFacet glass;
			glass.A = corners[0];
			glass.B = corners[1];
			glass.C = corners[2];
			glass.Absorption = DiamondAbsorption * (1.0f + 1.4f * weak);
			glass.Scatter = DiamondScatter * (1.0f + 0.6f * weak);
			glass.Edge = edge * flicker;
			glass.Glint = (1.0f - 0.35f * weak) * flicker;
			glass.Ior = DiamondIor;
			glass.Dispersion = DiamondDispersion;
			glass.Roughness = 0.015f + 0.06f * weak;
			glass.Floor = look.Base.y;
			glass.Crack = crack;
			glass.CrackOrigin = FacetPlane(facet, origin);
			glass.Flash = std::min(flash, 1.5f);
			glass.Fade = look.Fade * std::min(t * 3.0f, 1.0f) * (faint ? 0.55f + 0.45f * flicker : 1.0f);
			glass.Dissolve = look.Dissolve;
			glass.Burn = WardBurn;
			glass.Seed = facet.Seed ^ look.Seed;
			renderer.DrawFacet(glass);
		}
		const float formed = std::clamp((look.Age - DiamondFormSeconds * 0.6f) / (DiamondFormSeconds * 0.4f), 0.0f, 1.0f);
		if (formed <= 0.0f)
			return;
		DrawStar(renderer, look.Base + Up * DiamondApex, (0.14f + 0.05f * std::sin(time * 5.3f)) * (1.0f + 0.6f * look.Flash), edge * (0.2f * look.Fade * formed * (1.0f - look.Dissolve)), time * 0.6f);
	}

	void DrawDiamondCaustics(WorldRenderer& renderer, const DiamondWardLook& look, const glm::vec3& sunDirection)
	{
		const float strength = look.Fade * std::clamp(look.Age / DiamondFormSeconds, 0.0f, 1.0f) * (1.0f - look.Dissolve);
		if (strength <= 0.01f)
			return;
		const float damage = 1.0f - std::clamp(look.Ward, 0.0f, 1.0f);
		const float weak = std::clamp((0.5f - look.Ward) / 0.5f, 0.0f, 1.0f);
		glm::vec2 shift(0.0f);
		if (sunDirection.y < -0.2f)
			shift = glm::vec2(sunDirection.x, sunDirection.z) * (DiamondCausticHeight / -sunDirection.y);
		if (const float length = glm::length(shift); length > 0.8f)
			shift *= 0.8f / length;
		GroundDecal decal;
		decal.Center = look.Base + glm::vec3(shift.x, 0.0f, shift.y);
		decal.Extent = glm::vec2(1.15f);
		decal.Depth = 0.6f;
		decal.Shape = DecalShape::Caustic;
		decal.Color = glm::vec4(glm::mix(glm::vec3(0.95f, 0.88f, 1.0f), DiamondViolet, 0.25f + 0.45f * weak), (0.45f + 0.4f * look.Flash) * strength);
		decal.Tint = glm::vec4(0.42f, 0.38f, 0.52f, 0.28f * strength);
		decal.Params = glm::vec4(look.Spin, 0.035f, damage, 0.0f);
		decal.Seed = look.Seed;
		renderer.DrawDecal(decal);
	}

	void ShatterDiamondWard(const DiamondWardLook& look, std::vector<DiamondShard>& shards)
	{
		for (const DiamondFacet& facet : DiamondFacets())
		{
			const uint32_t key = Mix(facet.Seed, look.Seed);
			std::array<glm::vec3, 3> corners;
			for (size_t k = 0; k < 3; k++)
				corners[k] = look.Base + SpinY(facet.Corners[k], look.Spin);
			const auto jitter = [&](const glm::vec3& a, const glm::vec3& b, uint32_t salt) { return glm::mix(a, b, 0.35f + 0.3f * Unit(Mix(key, salt))); };
			const glm::vec3 ab = jitter(corners[0], corners[1], 1u);
			const glm::vec3 bc = jitter(corners[1], corners[2], 2u);
			const glm::vec3 ca = jitter(corners[2], corners[0], 3u);
			const std::array<std::array<glm::vec3, 3>, 4> pieces{ { { corners[0], ab, ca }, { ab, corners[1], bc }, { ca, bc, corners[2] }, { ab, bc, ca } } };
			const glm::vec3 normal = SpinY(facet.Normal, look.Spin);
			for (uint32_t k = 0; k < pieces.size(); k++)
			{
				const uint32_t piece = Mix(key, k + 11u);
				const std::array<glm::vec3, 3>& triangle = pieces[k];
				const glm::vec3 center = (triangle[0] + triangle[1] + triangle[2]) / 3.0f;
				const glm::vec3 scatter = glm::vec3(Unit(Mix(piece, 1u)), Unit(Mix(piece, 2u)), Unit(Mix(piece, 3u))) * 2.0f - 1.0f;
				DiamondShard shard;
				for (size_t c = 0; c < 3; c++)
					shard.Corners[c] = (triangle[c] - center) * 0.8f;
				shard.Center = center;
				shard.Velocity = normal * (1.4f + 2.2f * Unit(Mix(piece, 4u))) + Up * (0.4f + 1.5f * Unit(Mix(piece, 5u))) + scatter * 0.6f;
				shard.Axis = glm::length(scatter) > 1e-3f ? glm::normalize(scatter) : Up;
				shard.Spin = (5.0f + 9.0f * Unit(Mix(piece, 6u))) * (Unit(Mix(piece, 7u)) > 0.5f ? 1.0f : -1.0f);
				shard.Floor = look.Base.y;
				shard.Life = 0.55f + 0.45f * Unit(Mix(piece, 8u));
				shard.Seed = piece;
				shards.push_back(shard);
			}
		}
	}

	void UpdateDiamondShards(std::vector<DiamondShard>& shards, float dt)
	{
		for (DiamondShard& shard : shards)
		{
			shard.Age += dt;
			shard.Velocity.y -= 9.0f * dt;
			shard.Velocity *= std::exp(-1.1f * dt);
			shard.Center += shard.Velocity * dt;
			if (shard.Center.y < shard.Floor + 0.04f && shard.Velocity.y < 0.0f)
			{
				shard.Center.y = shard.Floor + 0.04f;
				shard.Velocity = glm::vec3(shard.Velocity.x * 0.5f, -shard.Velocity.y * 0.3f, shard.Velocity.z * 0.5f);
				shard.Spin *= 0.55f;
			}
			shard.Angle += shard.Spin * dt;
		}
		std::erase_if(shards, [](const DiamondShard& shard) { return shard.Age >= shard.Life; });
	}

	void DrawDiamondShards(WorldRenderer& renderer, std::span<const DiamondShard> shards)
	{
		for (const DiamondShard& shard : shards)
		{
			const float t = std::clamp(shard.Age / shard.Life, 0.0f, 1.0f);
			const float scale = 1.0f - 0.45f * t;
			GlassFacet glass;
			glass.A = shard.Center + Turn(shard.Corners[0], shard.Axis, shard.Angle) * scale;
			glass.B = shard.Center + Turn(shard.Corners[1], shard.Axis, shard.Angle) * scale;
			glass.C = shard.Center + Turn(shard.Corners[2], shard.Axis, shard.Angle) * scale;
			glass.Absorption = DiamondAbsorption * 3.0f;
			glass.Thickness = 0.07f;
			glass.Wedge = 0.3f + 0.45f * Unit(Mix(shard.Seed, 9u));
			glass.Edge = DiamondGold;
			glass.Ior = DiamondIor;
			glass.Dispersion = DiamondDispersion;
			glass.Roughness = 0.02f;
			glass.Floor = shard.Floor;
			glass.Flash = 1.0f * std::exp(-shard.Age * 8.0f);
			glass.Fade = 1.0f - std::clamp((t - 0.4f) / 0.6f, 0.0f, 1.0f);
			glass.Seed = shard.Seed;
			renderer.DrawFacet(glass);
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

	GroundDecal FireballCraterDecal(const glm::vec3& ground, float radius, float power, uint32_t seed)
	{
		const float violet = std::clamp(power * 1.25f, 0.0f, 1.0f);
		return CraterDecal(ground, radius * (1.05f + 0.15f * power), glm::mix(CometCraterGlow, glm::vec3(0.8f, 0.22f, 1.35f), violet), seed);
	}

	float FireballCraterCooling(float power)
	{
		return 0.9f - 0.4f * std::clamp(power, 0.0f, 1.0f);
	}

	GroundDecal CraterDecal(const glm::vec3& ground, float extent, const glm::vec3& glow, uint32_t seed)
	{
		GroundDecal crater;
		crater.Center = ground;
		crater.Extent = glm::vec2(extent);
		crater.Depth = 0.75f;
		crater.Shape = DecalShape::Crater;
		crater.Color = glm::vec4(glow, 1.0f);
		crater.Tint = glm::vec4(1.0f);
		crater.Roughness = 0.95f;
		crater.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		crater.Seed = seed;
		return crater;
	}

	GroundDecal SpellMarkDecal(const SpellMark& mark)
	{
		static constexpr DecalShape Shapes[] = { DecalShape::CometMark, DecalShape::StormMark, DecalShape::VoidMark, DecalShape::FirebombMark, DecalShape::AftershockMark, DecalShape::ResonanceMark, DecalShape::RitualMark };
		static constexpr glm::vec4 Shades[] = { { 0.03f, 0.016f, 0.01f, 1.0f }, { 0.012f, 0.016f, 0.04f, 0.85f }, { 0.018f, 0.005f, 0.04f, 1.0f }, { 0.035f, 0.02f, 0.012f, 1.0f }, { 0.035f, 0.014f, 0.008f, 1.0f }, { 0.03f, 0.024f, 0.012f, 0.9f }, { 0.01f, 0.014f, 0.035f, 0.95f } };
		const float extent = mark.Radius + 0.4f;
		GroundDecal decal;
		decal.Center = mark.Center;
		decal.Yaw = mark.Yaw;
		decal.Extent = glm::vec2(extent);
		decal.Depth = 0.9f;
		decal.Shape = Shapes[size_t(mark.Kind)];
		decal.Color = glm::vec4(mark.Color * mark.Intensity, mark.Fade);
		decal.Tint = Shades[size_t(mark.Kind)] * glm::vec4(1.0f, 1.0f, 1.0f, mark.Fade);
		decal.Params = glm::vec4(mark.Progress, mark.Appear, mark.Radius / extent, mark.Inner / extent);
		decal.Seed = mark.Cells > 0 ? (mark.Seed & 0xFFFFu) | (mark.Cells << 16) : mark.Seed;
		return decal;
	}

	void DrawSpellMark(WorldRenderer& renderer, const SpellMark& mark)
	{
		renderer.DrawDecal(SpellMarkDecal(mark));
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

	void DrawContactShadow(WorldRenderer& renderer, const glm::vec3& center, float radius, float strength)
	{
		GroundDecal decal;
		decal.Center = center;
		decal.Extent = glm::vec2(radius);
		decal.Depth = 0.3f;
		decal.Shape = DecalShape::Contact;
		decal.Color = glm::vec4(0.0f);
		decal.Tint = glm::vec4(0.0f, 0.0f, 0.0f, strength);
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

	void DrawBloodBubble(WorldRenderer& renderer, const glm::vec3& center, const glm::vec3& eye, float radius, const glm::vec3& color, float strength, float flash, uint32_t seed)
	{
		const glm::vec3 toward = eye - center;
		const float distance = glm::length(toward);
		if (distance < radius * 2.0f)
			return;
		const float lift = radius * 0.85f;
		const glm::vec3 at = center + toward / distance * lift;
		const float size = radius * (1.0f + 0.04f * flash) * (distance - lift) / distance;
		ParticleSprite film = Sprite(ParticleShape::Bubble, at, size, glm::vec3(0.16f, 0.0f, 0.02f));
		film.Additive = 0.0f;
		film.Emission = 0.0f;
		film.Softness = 0.12f;
		film.Params = glm::vec4(strength, flash, 1.0f, 0.0f);
		film.Seed = seed;
		renderer.DrawSprite(film);
		ParticleSprite glow = Sprite(ParticleShape::Bubble, at, size, color);
		glow.Softness = 0.12f;
		glow.Emission = 0.6f + 0.4f * flash;
		glow.Params = glm::vec4(strength, flash, 0.0f, 0.0f);
		glow.Seed = seed;
		renderer.DrawSprite(glow);
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
		case AbilityId::EmberBolt:
		{
			DrawGlow(renderer, center + forward * 0.22f, 0.55f, { 0.2f, 0.065f, 0.015f });
			DrawFlame(renderer, { .Base = center + forward * 0.14f, .Axis = -forward, .Width = 0.26f, .Height = 1.05f, .Wild = 1.0f, .Speed = 6.0f, .Comet = 1.0f, .Additive = 0.3f, .Emission = 0.35f, .Seed = seed });
			DrawFlame(renderer, { .Base = center + forward * 0.1f, .Axis = -forward, .Width = 0.17f, .Height = 0.78f, .Color = { 1.2f, 0.48f, 0.1f }, .Wild = 1.0f, .Speed = 8.0f, .Comet = 0.6f, .Additive = 0.3f, .Emission = 0.35f, .Seed = seed * 13u + 7u });
			DrawGlow(renderer, center + forward * 0.17f, 0.07f, { 1.2f, 0.95f, 0.6f });
			if (marked)
				DrawGlow(renderer, center, 0.13f, heart);
			break;
		}
		case AbilityId::ArcLightning:
		{
			const float scale = 1.35f;
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
		case AbilityId::ShardBurst:
		case AbilityId::FrostSpear:
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

	TrailStyle ProjectileTrail(AbilityId ability, float power, float size)
	{
		TrailStyle style;
		switch (ability)
		{
		case AbilityId::EmberBolt:
			style.Look = TrailLook::Blaze;
			style.Head = { 1.2f, 0.52f, 0.13f };
			style.Tail = { 0.8f, 0.26f, 0.05f };
			style.Additive = 0.35f;
			style.Width = 0.34f;
			style.WidthCurve = { 1.0f, 0.7f, 0.4f, 0.08f };
			style.AlphaCurve = { 1.0f, 0.8f, 0.45f, 0.0f };
			style.Life = 0.3f;
			style.Emission = 0.45f;
			style.Tiling = 1.3f;
			style.Scroll = 4.0f;
			style.Taper = 0.3f;
			style.Spacing = 0.1f;
			break;
		case AbilityId::Comet:
			style.Look = TrailLook::Blaze;
			style.Head = { 1.2f, 0.5f, 0.12f };
			style.Tail = { 0.75f, 0.22f, 0.05f };
			style.Additive = 0.35f;
			style.Width = 0.8f * size;
			style.WidthCurve = { 1.0f, 0.75f, 0.45f, 0.1f };
			style.AlphaCurve = { 1.0f, 0.8f, 0.45f, 0.0f };
			style.Life = 0.22f;
			style.Emission = 0.45f;
			style.Tiling = 0.5f;
			style.Scroll = 3.0f;
			style.Taper = 0.3f;
			style.Spacing = 0.3f;
			break;
		case AbilityId::PurpleFireball:
		{
			const float grow = std::pow(std::max(size, 1.0f), 0.8f);
			style.Look = TrailLook::Blaze;
			style.Head = FireballFlameHue(power) * 0.92f;
			style.Tail = FireballFlameHue(power) * 0.6f;
			style.Additive = 0.35f;
			style.Width = 0.43f * grow;
			style.WidthCurve = { 1.0f, 0.7f, 0.4f, 0.08f };
			style.AlphaCurve = { 1.0f, 0.8f, 0.45f, 0.0f };
			style.Life = 0.3f + 0.12f * power;
			style.Emission = 0.45f;
			style.Tiling = 1.3f / grow;
			style.Scroll = 4.0f;
			style.Taper = 0.3f * grow;
			style.Spacing = 0.1f;
			break;
		}
		case AbilityId::FrostSpear:
			style.Look = TrailLook::Frost;
			style.Head = { 0.55f, 0.85f, 1.25f };
			style.Tail = { 0.15f, 0.35f, 0.8f };
			style.Core = { 1.1f, 1.3f, 1.6f, 1.0f };
			style.Width = 0.2f * std::pow(size, 0.7f);
			style.WidthCurve = { 1.0f, 0.8f, 0.55f, 0.3f };
			style.AlphaCurve = { 0.85f, 0.6f, 0.3f, 0.0f };
			style.Life = 0.26f;
			style.Emission = 0.4f;
			style.Additive = 0.85f;
			style.Tiling = 1.0f;
			style.Scroll = 0.0f;
			style.Taper = 0.2f;
			break;
		case AbilityId::ObsidianBlades:
			style.Look = TrailLook::Obsidian;
			style.Head = { 0.07f, 0.035f, 0.11f };
			style.Tail = { 0.03f, 0.02f, 0.05f };
			style.Core = glm::vec4(ObsidianSheen * 0.6f, 1.0f);
			style.Width = 0.11f;
			style.WidthCurve = { 1.0f, 0.7f, 0.4f, 0.1f };
			style.AlphaCurve = { 0.85f, 0.5f, 0.2f, 0.0f };
			style.Life = 0.065f;
			style.Emission = 0.45f;
			style.Additive = 0.3f;
			style.Tiling = 1.5f;
			style.Scroll = 0.0f;
			style.Taper = 0.1f;
			break;
		default:
			style.Life = 0.0f;
			break;
		}
		return style;
	}

	TrailStyle LobTrail()
	{
		TrailStyle style;
		style.Look = TrailLook::Smoke;
		style.Head = { 0.22f, 0.17f, 0.14f };
		style.Tail = { 0.12f, 0.11f, 0.11f };
		style.Core = { 1.6f, 0.7f, 0.2f, 1.0f };
		style.Width = 0.28f;
		style.WidthCurve = { 0.5f, 0.85f, 1.0f, 1.1f };
		style.AlphaCurve = { 0.8f, 0.6f, 0.3f, 0.0f };
		style.Life = 0.45f;
		style.Emission = 0.4f;
		style.Additive = 0.2f;
		style.Tiling = 0.9f;
		style.Scroll = -0.6f;
		style.Taper = 0.25f;
		return style;
	}

	TrailStyle DashTrail(const glm::vec3& color)
	{
		TrailStyle style;
		style.Look = TrailLook::Wisp;
		style.Head = color * 0.7f;
		style.Tail = color * 0.25f;
		style.Core = glm::vec4(glm::mix(color, glm::vec3(1.0f), 0.3f) * 0.8f, 1.0f);
		style.Width = 0.7f;
		style.WidthCurve = { 1.0f, 0.9f, 0.7f, 0.4f };
		style.AlphaCurve = { 0.55f, 0.4f, 0.2f, 0.0f };
		style.Life = 0.3f;
		style.Emission = 0.25f;
		style.Tiling = 1.0f;
		style.Scroll = 0.0f;
		style.Taper = 0.4f;
		return style;
	}

	TrailStyle WandTrail(const glm::vec3& color)
	{
		TrailStyle style;
		style.Look = TrailLook::Swipe;
		style.Head = glm::mix(color, glm::vec3(1.0f), 0.35f) * 1.1f;
		style.Tail = color * 0.5f;
		style.Core = glm::vec4(glm::mix(color, glm::vec3(1.0f), 0.6f) * 1.1f, 1.0f);
		style.Width = 0.14f;
		style.WidthCurve = { 1.0f, 0.75f, 0.4f, 0.1f };
		style.Life = 0.16f;
		style.Emission = 0.5f;
		style.Tiling = 1.0f;
		style.Scroll = 0.0f;
		style.Taper = 0.06f;
		style.Spacing = 0.04f;
		return style;
	}

	namespace
	{
		constexpr float StormSky = 13.0f;
		constexpr float StormStep = 0.1f;
		constexpr float StormJag = 0.2f;
		constexpr uint32_t StormCoarse = 4;
		constexpr float StormStrokes[][2] = { { 0.0f, 1.0f }, { 0.075f, 0.7f }, { 0.16f, 0.45f } };

		LightningShape StormChannel(const glm::vec3& ground, uint32_t seed)
		{
			const float angle = Unit(Mix(seed, 911u)) * 6.2832f;
			const float lean = 0.8f + 1.2f * Unit(Mix(seed, 912u));
			LightningShape bolt;
			bolt.From = ground + glm::vec3(std::cos(angle) * lean, StormSky, std::sin(angle) * lean);
			bolt.To = ground + Up * 0.02f;
			bolt.Seed = seed;
			bolt.CoarseLevels = StormCoarse;
			bolt.Step = StormStep;
			bolt.Jag = StormJag;
			bolt.CoreColor = StormCore;
			bolt.GlowColor = StormGlow;
			return bolt;
		}
	}

	void DrawArcChannel(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, uint32_t seed, float time, float intensity)
	{
		if (intensity <= 0.0f)
			return;
		const uint32_t route = uint32_t(time * 14.0f);
		const uint32_t frame = uint32_t(time * 40.0f);
		LightningShape bolt;
		bolt.From = from;
		bolt.To = to;
		bolt.Seed = Mix(seed, route);
		bolt.Detail = frame;
		bolt.CoarseLevels = 2;
		bolt.Core = 0.032f;
		bolt.Glow = 0.34f;
		bolt.Jag = 0.12f;
		bolt.Step = 0.06f;
		bolt.Branches = 1.5f;
		bolt.BranchLength = std::min(0.22f, 0.75f / std::max(glm::length(to - from), 0.1f));
		bolt.BranchSpread = 0.75f;
		bolt.Depth = 2;
		bolt.Intensity = intensity;
		bolt.CoreColor = ArcCore;
		bolt.GlowColor = ArcGlow;
		renderer.DrawLightning(bolt);
		LightningShape strand = bolt;
		strand.CoarseLevels = 1;
		strand.Detail = frame + 101u;
		strand.Core = 0.014f;
		strand.Glow = 0.14f;
		strand.Jag = 0.16f;
		strand.Branches = 0.6f;
		strand.Depth = 1;
		strand.Intensity = intensity * 0.6f;
		renderer.DrawLightning(strand);
	}

	void DrawArcSparks(WorldRenderer& renderer, const glm::vec3& center, uint32_t seed, float reach, float intensity, uint32_t count)
	{
		for (uint32_t i = 0; i < count; i++)
		{
			const float angle = (float(i) + Unit(Mix(seed, i * 3u))) / float(count) * 6.2832f;
			const float rise = Unit(Mix(seed, i * 3u + 1u)) * 1.2f - 0.4f;
			LightningShape spark;
			spark.From = center;
			spark.To = center + glm::normalize(glm::vec3(std::cos(angle), rise, std::sin(angle))) * (reach * (0.5f + 0.5f * Unit(Mix(seed, i * 3u + 2u))));
			spark.Seed = Mix(seed, i + 17u);
			spark.CoarseLevels = 1;
			spark.Core = 0.016f;
			spark.Glow = 0.13f;
			spark.Jag = 0.25f;
			spark.Step = 0.04f;
			spark.Branches = 3.0f;
			spark.BranchLength = 0.4f;
			spark.BranchSpread = 1.0f;
			spark.Depth = 1;
			spark.Taper = 0.7f;
			spark.Intensity = intensity;
			spark.CoreColor = ArcCore;
			spark.GlowColor = ArcGlow;
			renderer.DrawLightning(spark);
		}
	}

	float ArcFadeIntensity(float age, uint32_t seed, float time)
	{
		const float t = age / ArcFadeSeconds;
		if (t < 0.0f || t >= 1.0f)
			return 0.0f;
		if (t < 0.2f)
			return 1.4f;
		const float flicker = Unit(Mix(seed, uint32_t(time * 45.0f))) > 0.3f ? 1.0f : 0.25f;
		return 1.1f * (1.0f - t) * (1.0f - t) * flicker;
	}

	float StormStroke(float age)
	{
		if (age < 0.0f || age >= StormBoltSeconds)
			return 0.0f;
		float light = 0.22f * (1.0f - age / StormBoltSeconds);
		for (const auto& stroke : StormStrokes)
			if (age >= stroke[0])
				light = std::max(light, stroke[1] * std::exp(-(age - stroke[0]) / 0.028f));
		return light;
	}

	glm::vec2 StormArcOffset(uint32_t seed, uint32_t index, float radius)
	{
		const float angle = (float(index) + 0.7f * Unit(Mix(seed, index * 5u + 300u))) / float(StormGroundArcs) * 6.2832f;
		const float reach = radius * (0.8f + 0.65f * Unit(Mix(seed, index * 5u + 301u)));
		return glm::vec2(std::cos(angle), std::sin(angle)) * reach;
	}

	void DrawStormLeader(WorldRenderer& renderer, const glm::vec3& ground, float radius, uint32_t seed, float progress, float time)
	{
		const uint32_t frame = uint32_t(time * 30.0f);
		LightningShape leader = StormChannel(ground, seed);
		leader.Detail = frame;
		leader.Core = 0.016f;
		leader.Glow = 0.22f;
		leader.Branches = 0.6f;
		leader.BranchLength = 0.12f;
		leader.BranchSpread = 0.8f;
		leader.Depth = 1;
		leader.Reach = 0.12f + 0.8f * progress * progress;
		leader.Intensity = 0.2f + 0.3f * progress;
		renderer.DrawLightning(leader);
		if (progress < 0.6f)
			return;
		const float rise = (progress - 0.6f) / 0.4f;
		for (uint32_t i = 0; i < 3; i++)
		{
			const glm::vec2 offset = StormArcOffset(seed + 77u, i * 2u, radius * 0.45f);
			const glm::vec3 base = ground + glm::vec3(offset.x, 0.02f, offset.y);
			LightningShape streamer;
			streamer.From = base;
			streamer.To = base + glm::vec3(-offset.x * 0.2f, 0.25f + 0.45f * rise, -offset.y * 0.2f);
			streamer.Seed = Mix(seed, i + 500u);
			streamer.Detail = frame + i;
			streamer.CoarseLevels = 1;
			streamer.Core = 0.012f;
			streamer.Glow = 0.1f;
			streamer.Jag = 0.25f;
			streamer.Step = 0.04f;
			streamer.Branches = 0.0f;
			streamer.Taper = 0.6f;
			streamer.Intensity = 0.5f * rise;
			streamer.CoreColor = StormCore;
			streamer.GlowColor = StormGlow;
			renderer.DrawLightning(streamer);
		}
	}

	void DrawStormBolt(WorldRenderer& renderer, const glm::vec3& ground, uint32_t seed, float age, float scale)
	{
		const float stroke = StormStroke(age);
		if (stroke <= 0.01f)
			return;
		LightningShape bolt = StormChannel(ground, seed);
		bolt.Detail = 1u + uint32_t(std::count_if(std::begin(StormStrokes) + 1, std::end(StormStrokes), [age](const auto& s) { return age >= s[0]; }));
		bolt.Core = 0.075f * scale;
		bolt.Glow = 0.85f * scale;
		bolt.Branches = 0.55f;
		bolt.BranchLength = 0.3f;
		bolt.BranchSpread = 0.75f;
		bolt.Depth = 2;
		bolt.Intensity = stroke;
		renderer.DrawLightning(bolt);
	}

	void DrawStormStrike(WorldRenderer& renderer, const StormStrikeLook& look, float time)
	{
		const float age = look.Age;
		const float stroke = StormStroke(age);
		DrawStormBolt(renderer, look.Ground, look.Seed, age, look.Scale);
		const float flash = std::exp(-age / 0.06f);
		DrawGlow(renderer, look.Ground + Up * 0.35f, look.Radius * (0.6f + 0.3f * flash), StormGlow * (0.4f * flash + 0.15f * stroke), 0.3f);
		DrawGlow(renderer, look.Ground + Up * 0.2f, 0.4f + 0.35f * stroke, StormCore * (0.3f * stroke));
		DrawGroundGlow(renderer, look.Ground, look.Radius * 1.7f, StormGlow, 0.55f * flash + 0.2f * stroke);
		const uint32_t frame = uint32_t(time * 24.0f);
		for (uint32_t i = 0; i < StormGroundArcs; i++)
		{
			const float start = 0.06f * Unit(Mix(look.Seed, i * 3u + 200u));
			const float life = 0.14f + 0.12f * Unit(Mix(look.Seed, i * 3u + 201u));
			const float t = (age - start) / life;
			if (t < 0.0f || t >= 1.0f)
				continue;
			const glm::vec3 out = look.Ends[i] - look.Ground;
			const glm::vec3 flat = glm::normalize(glm::vec3(out.x, 0.0f, out.z) + glm::vec3(1e-5f, 0.0f, 0.0f));
			LightningShape arc;
			arc.From = look.Ground + flat * 0.1f + Up * 0.08f;
			arc.To = look.Ends[i] + Up * 0.05f;
			arc.Seed = Mix(look.Seed, i + 40u);
			arc.Detail = frame;
			arc.CoarseLevels = 2;
			arc.Core = 0.028f;
			arc.Glow = 0.28f;
			arc.Jag = 0.2f;
			arc.Step = 0.06f;
			arc.Branches = 1.8f;
			arc.BranchLength = 0.3f;
			arc.BranchSpread = 1.0f;
			arc.Depth = 1;
			arc.Flatten = 0.85f;
			arc.Taper = 0.6f;
			arc.Reach = std::min(1.0f, 0.35f + t * 5.0f);
			arc.Intensity = std::pow(1.0f - t, 1.5f) * (0.55f + 0.45f * Unit(Mix(look.Seed + i, frame)));
			arc.CoreColor = StormCore;
			arc.GlowColor = StormGlow;
			renderer.DrawLightning(arc);
		}
	}

	void DrawStormJump(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, uint32_t seed, float age, float time)
	{
		const float light = std::max(std::exp(-age / 0.05f), age >= 0.09f ? 0.65f * std::exp(-(age - 0.09f) / 0.035f) : 0.0f);
		if (light < 0.02f)
			return;
		LightningShape bolt;
		bolt.From = from;
		bolt.To = to;
		bolt.Seed = seed;
		bolt.Detail = age < 0.09f ? 0u : 1u + uint32_t(time * 30.0f) % 2u;
		bolt.CoarseLevels = 3;
		bolt.Core = 0.05f;
		bolt.Glow = 0.55f;
		bolt.Jag = 0.18f;
		bolt.Step = 0.07f;
		bolt.Branches = 1.0f;
		bolt.BranchLength = 0.25f;
		bolt.BranchSpread = 0.8f;
		bolt.Depth = 2;
		bolt.Intensity = light;
		bolt.CoreColor = StormCore;
		bolt.GlowColor = StormGlow;
		renderer.DrawLightning(bolt);
		DrawGlow(renderer, to, 0.55f * light + 0.1f, StormGlow * (0.9f * light));
	}

	void DrawShockwave(WorldRenderer& renderer, const glm::vec3& ground, float radius, float width, float strength, float facing, float halfArc)
	{
		Distorter wave;
		wave.Center = ground + Up * 0.2f;
		wave.Shape = DistortionShape::Shockwave;
		wave.Align = DistortionAlign::Ground;
		wave.Extent = glm::vec2(radius + 2.5f * width);
		wave.Yaw = facing;
		wave.Strength = strength;
		wave.Soft = 0.12f;
		wave.Params = glm::vec4(radius, width, halfArc, 0.35f);
		renderer.DrawDistortion(wave);
	}

	void DrawAirShockwave(WorldRenderer& renderer, const glm::vec3& center, float radius, float width, float strength)
	{
		Distorter wave;
		wave.Center = center;
		wave.Shape = DistortionShape::Shockwave;
		wave.Extent = glm::vec2(radius + 2.5f * width);
		wave.Strength = strength;
		wave.Soft = 0.12f;
		wave.Params = glm::vec4(radius, width, 3.1416f, 0.35f);
		renderer.DrawDistortion(wave);
	}

	void DrawHeatHaze(WorldRenderer& renderer, const glm::vec3& center, float radius, float strength, float frequency, float rise, uint32_t seed)
	{
		Distorter haze;
		haze.Center = center;
		haze.Shape = DistortionShape::Haze;
		haze.Extent = glm::vec2(radius);
		haze.Strength = strength;
		haze.Soft = 0.2f;
		haze.Params = glm::vec4(frequency, rise, 0.6f * radius, 0.0f);
		haze.Seed = seed;
		renderer.DrawDistortion(haze);
	}

	void DrawFlame(WorldRenderer& renderer, const FlameLook& look)
	{
		const float length = look.Height / 1.25f;
		const glm::vec3 axis = glm::normalize(look.Axis);
		ParticleSprite flame = Sprite(ParticleShape::FlameSheet, look.Base + axis * (0.45f * length), look.Width, look.Color);
		flame.Align = ParticleAlign::Velocity;
		flame.Axis = axis;
		flame.Stretch = std::max(length - look.Width, 0.0f);
		flame.Additive = look.Additive;
		flame.Emission = look.Emission;
		flame.Softness = 0.15f;
		flame.Params = glm::vec4(look.Wild, look.Speed, look.Comet, 0.0f);
		flame.Seed = look.Seed;
		renderer.DrawSprite(flame);
	}

	void DrawImplosion(WorldRenderer& renderer, const glm::vec3& center, float radius, float strength, float phase, uint32_t pulses)
	{
		Distorter pull;
		pull.Center = center;
		pull.Shape = DistortionShape::Implosion;
		pull.Extent = glm::vec2(radius);
		pull.Strength = strength;
		pull.Soft = 0.2f;
		pull.Params = glm::vec4(radius, phase, float(pulses), 0.0f);
		renderer.DrawDistortion(pull);
	}

	void DrawGravityLens(WorldRenderer& renderer, const glm::vec3& center, float horizon, float einstein, const glm::vec3& ring, float strength)
	{
		Distorter lens;
		lens.Center = center;
		lens.Shape = DistortionShape::Lens;
		lens.Extent = glm::vec2(3.2f * einstein);
		lens.Strength = strength;
		lens.Soft = 0.25f;
		lens.Params = glm::vec4(horizon, einstein, 0.0f, 0.0f);
		lens.Color = ring;
		renderer.DrawDistortion(lens);
	}

	void DrawCrystalWard(WorldRenderer& renderer, const glm::vec3& base, const glm::vec3& eye, float form, float time, uint32_t seed)
	{
		if (form <= 0.01f)
			return;
		constexpr glm::vec3 WardLight{ 0.42f, 0.78f, 1.3f };
		constexpr uint32_t Pearls = 3;
		constexpr float Orbit = 0.74f;
		constexpr float Tilt = 0.95f;
		TrailStyle trail;
		trail.Look = TrailLook::Wisp;
		trail.Head = WardLight * 0.8f;
		trail.Tail = WardLight * 0.25f;
		trail.Core = glm::vec4(glm::mix(WardLight, glm::vec3(1.0f), 0.5f) * 0.8f, 1.0f);
		trail.Width = 0.06f;
		trail.WidthCurve = { 1.0f, 0.7f, 0.4f, 0.1f };
		trail.AlphaCurve = { 0.8f, 0.5f, 0.25f, 0.0f };
		trail.Life = 0.35f;
		trail.Emission = 0.45f;
		trail.Scroll = 0.0f;
		trail.Taper = 0.05f;
		trail.Spacing = 0.05f;
		const float grow = form * form * (3.0f - 2.0f * form);
		const glm::vec3 center = base + Up * 1.0f;
		const float radius = Orbit * (0.3f + 0.7f * grow);
		const float phase = Unit(seed) * 6.2832f;
		std::array<glm::vec3, Pearls> points{};
		for (uint32_t i = 0; i < Pearls; i++)
		{
			const float node = time * 0.45f + phase + float(i) * 2.0944f;
			const glm::vec3 across(std::cos(node), 0.0f, std::sin(node));
			const glm::vec3 lift(-std::sin(node) * std::cos(Tilt), std::sin(Tilt), std::cos(node) * std::cos(Tilt));
			const float angle = time * 2.2f + phase * 1.7f + float(i) * 2.0944f;
			points[i] = center + (across * std::cos(angle) + lift * std::sin(angle)) * radius;
			renderer.DrawTrail(uint64_t(seed) | (26ull << 40) | (uint64_t(i) << 36), points[i], trail, form);
			ParticleSprite crystal = Sprite(ParticleShape::Pearl, points[i], 0.1f * (0.6f + 0.4f * grow), WardLight * 1.05f);
			crystal.Seed = Mix(seed, i);
			crystal.Emission = 0.8f;
			renderer.DrawSprite(crystal);
			DrawGlow(renderer, points[i], 0.26f, WardLight * (0.2f * form), 0.5f);
		}
		glm::vec3 toward = eye - center;
		const float distance = glm::length(toward);
		if (distance > 2.0f)
		{
			const float lift = 0.8f;
			ParticleSprite shell = Sprite(ParticleShape::Bubble, center + toward / distance * lift, (radius + 0.12f) * (distance - lift) / distance, WardLight * 0.55f);
			shell.Softness = 0.12f;
			shell.Emission = 0.4f;
			shell.Params = glm::vec4(form, 0.0f, 0.0f, 1.0f);
			shell.Seed = seed;
			renderer.DrawSprite(shell);
		}
	}
}
