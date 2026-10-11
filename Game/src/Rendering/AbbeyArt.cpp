#include "LevelArt.h"
#include "Rendering/Models.h"
#include <RadianceEngine/Core/Log.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <tuple>

namespace Game
{
	namespace
	{
		constexpr int32_t ChunkSize = 8;
		constexpr float Margin = 14.0f;
		constexpr float Step = 0.5f;
		constexpr float SkyTexels = 2.0f;

		uint32_t Hash(int32_t x, int32_t y, uint32_t salt)
		{
			uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + salt * 2246822519u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return h ^ (h >> 16);
		}

		float Unit(int32_t x, int32_t y, uint32_t salt)
		{
			return float(Hash(x, y, salt) & 0xFFFF) / 65535.0f;
		}

		float ValueNoise(float x, float y, uint32_t salt)
		{
			const float fx = std::floor(x);
			const float fy = std::floor(y);
			const int32_t ix = int32_t(fx);
			const int32_t iy = int32_t(fy);
			float tx = x - fx;
			float ty = y - fy;
			tx = tx * tx * (3.0f - 2.0f * tx);
			ty = ty * ty * (3.0f - 2.0f * ty);
			const float a = glm::mix(Unit(ix, iy, salt), Unit(ix + 1, iy, salt), tx);
			const float b = glm::mix(Unit(ix, iy + 1, salt), Unit(ix + 1, iy + 1, salt), tx);
			return glm::mix(a, b, ty);
		}

		float Fbm(float x, float y, uint32_t salt)
		{
			return 0.6f * ValueNoise(x, y, salt) + 0.3f * ValueNoise(x * 2.1f, y * 2.1f, salt + 1) + 0.1f * ValueNoise(x * 4.3f, y * 4.3f, salt + 2);
		}

		float Smooth(float edge0, float edge1, float value)
		{
			const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		glm::mat4 RotationY(float angle)
		{
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			glm::mat4 m(1.0f);
			m[0] = glm::vec4(c, 0.0f, s, 0.0f);
			m[2] = glm::vec4(-s, 0.0f, c, 0.0f);
			return m;
		}

		glm::mat4 RotationX(float angle)
		{
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			glm::mat4 m(1.0f);
			m[1] = glm::vec4(0.0f, c, s, 0.0f);
			m[2] = glm::vec4(0.0f, -s, c, 0.0f);
			return m;
		}

		glm::mat4 Translation(const glm::vec3& offset)
		{
			return glm::translate(glm::mat4(1.0f), offset);
		}

		glm::mat4 Scaling(float scale)
		{
			return glm::scale(glm::mat4(1.0f), glm::vec3(scale));
		}

		uint32_t Snorm10(float value)
		{
			return uint32_t(int32_t(std::lround(std::clamp(value, -1.0f, 1.0f) * 511.0f))) & 0x3FFu;
		}

		uint32_t PackTangent(const glm::vec3& tangent, float sign)
		{
			return Snorm10(tangent.x) | (Snorm10(tangent.y) << 10) | (Snorm10(tangent.z) << 20) | (sign < 0.0f ? 1u << 30 : 0u);
		}

		uint32_t PackColor(const glm::vec4& color)
		{
			uint32_t packed = 0;
			for (int c = 0; c < 4; c++)
				packed |= uint32_t(std::lround(std::clamp(color[c], 0.0f, 1.0f) * 255.0f)) << (8 * c);
			return packed;
		}

		struct MeshData
		{
			std::vector<ModelVertex> Vertices;
			std::vector<uint32_t> Indices;
			glm::vec3 Min{ 1e9f };
			glm::vec3 Max{ -1e9f };

			void Quad(const std::array<glm::vec3, 4>& p, const std::array<glm::vec2, 4>& uv, const glm::vec3& normal, const std::array<glm::vec4, 4>& color)
			{
				const glm::vec3 e1 = p[1] - p[0];
				const glm::vec3 e2 = p[2] - p[0];
				const glm::vec2 d1 = uv[1] - uv[0];
				const glm::vec2 d2 = uv[2] - uv[0];
				const float determinant = d1.x * d2.y - d2.x * d1.y;
				glm::vec3 dPdu = std::abs(determinant) > 1e-12f ? (e1 * d2.y - e2 * d1.y) / determinant : e1;
				const glm::vec3 dPdv = std::abs(determinant) > 1e-12f ? (e2 * d1.x - e1 * d2.x) / determinant : e2;
				dPdu -= normal * glm::dot(normal, dPdu);
				const glm::vec3 tangent = glm::length(dPdu) > 1e-8f ? glm::normalize(dPdu) : glm::vec3(1.0f, 0.0f, 0.0f);
				const float sign = glm::dot(glm::cross(normal, tangent), -dPdv) >= 0.0f ? 1.0f : -1.0f;
				const uint32_t base = uint32_t(Vertices.size());
				for (int i = 0; i < 4; i++)
				{
					ModelVertex vertex;
					vertex.Position = p[size_t(i)];
					vertex.U = uv[size_t(i)].x;
					vertex.V = uv[size_t(i)].y;
					vertex.Normal = normal;
					vertex.Weights = 255;
					vertex.Tangent = PackTangent(tangent, sign);
					vertex.Color = PackColor(color[size_t(i)]);
					Vertices.push_back(vertex);
					Min = glm::min(Min, p[size_t(i)]);
					Max = glm::max(Max, p[size_t(i)]);
				}
				const bool flip = glm::dot(glm::cross(p[1] - p[0], p[2] - p[0]), normal) < 0.0f;
				for (const uint32_t index : flip ? std::array<uint32_t, 6>{ 0, 2, 1, 0, 3, 2 } : std::array<uint32_t, 6>{ 0, 1, 2, 0, 2, 3 })
					Indices.push_back(base + index);
			}

			void Box(const glm::vec3& center, const glm::vec3& half, float yaw, float uOffset = 0.0f, bool bottom = false)
			{
				const glm::vec3 right(std::cos(yaw), 0.0f, std::sin(yaw));
				const glm::vec3 forward(-std::sin(yaw), 0.0f, std::cos(yaw));
				const glm::vec3 up(0.0f, 1.0f, 0.0f);
				const float top = center.y + half.y;
				const std::array<glm::vec4, 4> white{ glm::vec4(1.0f), glm::vec4(1.0f), glm::vec4(1.0f), glm::vec4(1.0f) };
				const auto side = [&](const glm::vec3& normal, const glm::vec3& along, float extent, float depth, float u0)
				{
					const glm::vec3 face = center + normal * depth;
					const glm::vec3 a = face - along * extent;
					const glm::vec3 b = face + along * extent;
					const float bottomY = center.y - half.y;
					Quad({ glm::vec3(a.x, top, a.z), glm::vec3(b.x, top, b.z), glm::vec3(b.x, bottomY, b.z), glm::vec3(a.x, bottomY, a.z) },
						{ glm::vec2(u0, 0.0f), glm::vec2(u0 + 2.0f * extent, 0.0f), glm::vec2(u0 + 2.0f * extent, top - bottomY), glm::vec2(u0, top - bottomY) }, normal, white);
				};
				side(-forward, right, half.x, half.z, uOffset);
				side(forward, -right, half.x, half.z, uOffset);
				side(right, forward, half.z, half.x, 0.0f);
				side(-right, -forward, half.z, half.x, 0.0f);
				const glm::vec3 c = center + up * half.y;
				const auto corner = [&](float sx, float sz) { return c + right * (sx * half.x) + forward * (sz * half.z); };
				const auto planar = [&](float sx, float sz) { return glm::vec2(uOffset + (sx + 1.0f) * half.x, (sz + 1.0f) * half.z); };
				Quad({ corner(-1, -1), corner(1, -1), corner(1, 1), corner(-1, 1) }, { planar(-1, -1), planar(1, -1), planar(1, 1), planar(-1, 1) }, up, white);
				if (bottom)
				{
					const glm::vec3 d = center - up * half.y;
					const auto low = [&](float sx, float sz) { return d + right * (sx * half.x) + forward * (sz * half.z); };
					Quad({ low(-1, -1), low(-1, 1), low(1, 1), low(1, -1) }, { planar(-1, -1), planar(-1, 1), planar(1, 1), planar(1, -1) }, -up, white);
				}
			}
		};

		struct PendingItem
		{
			uint32_t Mesh = 0;
			glm::mat4 Transform{ 1.0f };
			glm::vec3 Min{ 0.0f };
			glm::vec3 Max{ 0.0f };
			bool Cut = false;
		};

		struct Part
		{
			std::string Mesh;
			glm::vec3 Offset{ 0.0f };
			float Yaw = 0.0f;
			float Scale = 1.0f;
			float Pitch = 0.0f;
		};

		class AbbeyBuilder
		{
		public:
			AbbeyBuilder(const Level& level, WorldRenderer& renderer, LevelScene& scene)
				: m_Level(level), m_Map(level.Map), m_Renderer(renderer), m_Models(renderer.GetModels()), m_Scene(scene)
			{
			}

			bool Build()
			{
				for (const auto& [slot, name] : { std::pair{ &m_Cobble, "cobblestone_floor_08" }, std::pair{ &m_Forest, "forest_leaves_02" }, std::pair{ &m_Mud, "mud_forest" }, std::pair{ &m_Tiles, "large_floor_tiles_02" },
						 std::pair{ &m_CastleWall, "castle_wall_varriation" }, std::pair{ &m_OldStone, "old_stone_wall" }, std::pair{ &m_Planks, "weathered_planks" } })
				{
					const std::optional<uint32_t> material = m_Renderer.FindMaterialSlot(name);
					if (!material)
					{
						RDN_LOG_WARNING("The abbey material {} is missing, the abbey falls back to the default art", name);
						return false;
					}
					*slot = *material;
				}
				const int32_t width = m_Map.GetWidth();
				const int32_t height = m_Map.GetHeight();
				m_ChunksX = (width + ChunkSize - 1) / ChunkSize;
				m_ChunksY = (height + ChunkSize - 1) / ChunkSize;
				m_Scene.Chunks.assign(size_t(m_ChunksX) * size_t(m_ChunksY), {});
				m_Scene.Cells.assign(size_t(width) * size_t(height), uint8_t(CellTop | CellSides));
				m_Scene.PropDrawn.assign(m_Level.Props.size(), 1);
				m_Scene.WallDrawn.assign(m_Level.Walls.size(), 1);

				Ground();
				Walls();
				BuildShapes();
				const uint32_t base = m_Renderer.SetLevelGeometry(m_Vertices, m_Indices, m_Meshes);
				Props();
				for (const PendingItem& item : m_Pending)
					AddItem(base + item.Mesh, item.Transform, item.Min, item.Max, item.Cut);
				Wilds();
				Roofs();
				SkyMask();
				m_Scene.LevelGeometry = true;
				if (!m_Missing.empty())
				{
					std::string list;
					for (const std::string& name : m_Missing)
						list += (list.empty() ? "" : ", ") + name;
					RDN_LOG_WARNING("The abbey art is missing {}", list);
				}
				return true;
			}

		private:
			uint32_t AddMesh(const MeshData& mesh, uint32_t material, uint32_t layer, uint32_t flags)
			{
				const uint32_t firstIndex = uint32_t(m_Indices.size());
				const uint32_t baseVertex = uint32_t(m_Vertices.size());
				m_Vertices.insert(m_Vertices.end(), mesh.Vertices.begin(), mesh.Vertices.end());
				for (const uint32_t index : mesh.Indices)
					m_Indices.push_back(baseVertex + index);
				m_Meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(mesh.Indices.size()), .Material = material, .Layer = layer, .Flags = flags });
				return uint32_t(m_Meshes.size() - 1);
			}

			void Pending(uint32_t mesh, const MeshData& data, const glm::mat4& transform, bool cut)
			{
				glm::vec3 low(1e9f);
				glm::vec3 high(-1e9f);
				for (uint32_t corner = 0; corner < 8; corner++)
				{
					const glm::vec3 local((corner & 1) ? data.Max.x : data.Min.x, (corner & 2) ? data.Max.y : data.Min.y, (corner & 4) ? data.Max.z : data.Min.z);
					const glm::vec3 world = glm::vec3(transform * glm::vec4(local, 1.0f));
					low = glm::min(low, world);
					high = glm::max(high, world);
				}
				m_Pending.push_back({ mesh, transform, low, high, cut });
			}

			SceneChunk& ChunkAt(Vec2 point)
			{
				const int32_t cx = std::clamp(int32_t(std::floor(point.x)) / ChunkSize, 0, m_ChunksX - 1);
				const int32_t cy = std::clamp(int32_t(std::floor(point.y)) / ChunkSize, 0, m_ChunksY - 1);
				return m_Scene.Chunks[size_t(cy) * size_t(m_ChunksX) + size_t(cx)];
			}

			void AddItem(uint32_t mesh, const glm::mat4& transform, const glm::vec3& low, const glm::vec3& high, bool cut, const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f))
			{
				SceneItem item;
				item.Mesh = mesh;
				item.Instance = m_Renderer.MakeModelInstance(mesh, transform, NoPalette, tint, 0.0f, low.y);
				item.Center = { (low.x + high.x) * 0.5f, (low.z + high.z) * 0.5f };
				item.Reach = 0.5f * std::sqrt((high.x - low.x) * (high.x - low.x) + (high.z - low.z) * (high.z - low.z));
				item.Base = low.y;
				item.Top = high.y;
				item.Cut = cut;
				SceneChunk& chunk = ChunkAt(item.Center);
				chunk.Min = { std::min(chunk.Min.x, low.x), std::min(chunk.Min.y, low.z) };
				chunk.Max = { std::max(chunk.Max.x, high.x), std::max(chunk.Max.y, high.z) };
				chunk.Items.push_back(item);
			}

			std::optional<uint32_t> Find(const std::string& name)
			{
				if (const std::optional<uint32_t> mesh = m_Models.FindMesh(name))
					return mesh;
				if (std::find(m_Missing.begin(), m_Missing.end(), name) == m_Missing.end())
					m_Missing.push_back(name);
				return std::nullopt;
			}

			void AddModel(const std::string& name, const glm::mat4& transform, bool cut, const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f))
			{
				const std::optional<uint32_t> first = Find(name);
				if (!first)
					return;
				for (uint32_t part = 0;; part++)
				{
					const std::optional<uint32_t> mesh = part == 0 ? first : m_Models.FindMesh(name + "." + std::to_string(part));
					if (!mesh)
						break;
					const ModelMesh& source = m_Models.GetMesh(*mesh);
					glm::vec3 low(1e9f);
					glm::vec3 high(-1e9f);
					for (uint32_t corner = 0; corner < 8; corner++)
					{
						const glm::vec3 local((corner & 1) ? source.Max.x : source.Min.x, (corner & 2) ? source.Max.y : source.Min.y, (corner & 4) ? source.Max.z : source.Min.z);
						const glm::vec3 world = glm::vec3(transform * glm::vec4(local, 1.0f));
						low = glm::min(low, world);
						high = glm::max(high, world);
					}
					AddItem(*mesh, transform, low, high, cut, tint);
				}
			}

			float WallDistance(Vec2 point) const
			{
				float best = 1e9f;
				for (const WallSegment& wall : m_Level.Walls)
				{
					const Vec2 span = wall.To - wall.From;
					const float length2 = LengthSquared(span);
					const float t = length2 > 1e-6f ? std::clamp(Dot(point - wall.From, span) / length2, 0.0f, 1.0f) : 0.0f;
					best = std::min(best, Length(point - (wall.From + span * t)) - GetWall(wall.Style).Thickness * 0.5f);
				}
				return best;
			}

			TerrainId TerrainAt(float x, float z) const
			{
				const int32_t cx = int32_t(std::floor(x));
				const int32_t cy = int32_t(std::floor(z));
				return m_Map.IsInside(cx, cy) ? m_Map.Get(cx, cy).Terrain : TerrainId::Rock;
			}

			float DirtFraction(float x, float z) const
			{
				float dirt = 0.0f;
				for (const glm::vec2 offset : { glm::vec2(-0.25f, -0.25f), glm::vec2(0.25f, -0.25f), glm::vec2(-0.25f, 0.25f), glm::vec2(0.25f, 0.25f) })
					dirt += TerrainAt(x + offset.x, z + offset.y) != TerrainId::Cobblestone ? 0.25f : 0.0f;
				return dirt;
			}

			float DirtNear(float x, float z) const
			{
				float dirt = 0.0f;
				for (int32_t j = -1; j <= 1; j++)
					for (int32_t i = -1; i <= 1; i++)
						dirt += TerrainAt(x + 0.8f * float(i), z + 0.8f * float(j)) == TerrainId::Dirt ? 1.0f / 9.0f : 0.0f;
				return dirt > 0.0f ? Smooth(0.05f, 0.45f, dirt + (Fbm(x * 0.7f, z * 0.7f, 53) - 0.5f) * 0.6f) : 0.0f;
			}

			glm::vec4 GroundColor(int group, float x, float z) const
			{
				const float wall = WallDistance({ x, z });
				if (group == 0)
				{
					const float dirt = DirtFraction(x, z);
					const float wallMud = Smooth(1.3f, 0.15f, wall) * (0.55f + 0.45f * Fbm(x * 0.9f, z * 0.9f, 11));
					const float patches = std::max(Fbm(x / 3.3f, z / 3.3f, 17) - 0.64f, 0.0f) * 2.6f;
					const float blend = std::clamp(std::max({ dirt, wallMud, patches }), 0.0f, 1.0f);
					float wet = 0.75f * Smooth(0.58f, 0.75f, Fbm(x / 4.2f, z / 4.2f, 23));
					for (const glm::vec3 puddle : { glm::vec3(30.0f, 13.6f, 3.5f), glm::vec3(28.6f, 31.6f, 1.6f), glm::vec3(31.8f, 31.2f, 1.2f), glm::vec3(24.5f, 26.0f, 2.2f), glm::vec3(36.5f, 16.0f, 1.8f), glm::vec3(26.2f, 13.4f, 1.3f), glm::vec3(33.6f, 13.2f, 1.1f), glm::vec3(19.4f, 17.6f, 1.2f) })
						wet += 1.1f * std::exp(-LengthSquared(Vec2(x, z) - Vec2(puddle.x, puddle.y)) / puddle.z) * (0.75f + 0.5f * Fbm(x * 1.3f, z * 1.3f, 41));
					wet += dirt * (0.2f + 0.5f * Smooth(0.45f, 0.7f, Fbm(x / 2.2f, z / 2.2f, 29)));
					wet *= Smooth(1.4f, 3.2f, Length(Vec2(x, z) - Vec2(30.0f, 20.5f)));
					return { blend, std::clamp(wet, 0.0f, 1.0f), Tone(x, z), 1.0f };
				}
				const float mud = std::max(std::clamp((Fbm(x / 2.7f, z / 2.7f, 31) - 0.45f) * 2.2f, 0.0f, 0.85f) + Smooth(0.9f, 0.1f, wall) * 0.3f, DirtNear(x, z));
				const float wet = 0.1f + 0.35f * Smooth(0.55f, 0.75f, Fbm(x / 3.5f, z / 3.5f, 37));
				return { std::clamp(mud, 0.0f, 1.0f), wet, Tone(x, z), 1.0f };
			}

			static float Tone(float x, float z)
			{
				return std::clamp(0.5f + 0.55f * (Fbm(x / 5.5f, z / 5.5f, 43) - 0.5f) + 0.25f * (ValueNoise(x / 1.3f, z / 1.3f, 47) - 0.5f), 0.2f, 0.8f);
			}

			void Ground()
			{
				const float width = float(m_Map.GetWidth());
				const float height = float(m_Map.GetHeight());
				const int32_t chunksX = int32_t(std::ceil((width + 2.0f * Margin) / float(ChunkSize)));
				const int32_t chunksY = int32_t(std::ceil((height + 2.0f * Margin) / float(ChunkSize)));
				const int32_t steps = int32_t(float(ChunkSize) / Step);
				const struct Group { uint32_t Material; uint32_t Layer; uint32_t Flags; } groups[] = {
					{ m_Cobble, m_Mud, MeshLayered | MeshWet },
					{ m_Forest, m_Mud, MeshLayered | MeshWet },
					{ m_Tiles, m_Tiles, 0 },
				};
				for (int32_t cy = 0; cy < chunksY; cy++)
					for (int32_t cx = 0; cx < chunksX; cx++)
					{
						std::array<MeshData, 3> meshes;
						for (int32_t j = 0; j < steps; j++)
							for (int32_t i = 0; i < steps; i++)
							{
								const float x0 = -Margin + float(cx * ChunkSize) + float(i) * Step;
								const float z0 = -Margin + float(cy * ChunkSize) + float(j) * Step;
								const TerrainId terrain = TerrainAt(x0 + 0.5f * Step, z0 + 0.5f * Step);
								const int group = terrain == TerrainId::Cobblestone || terrain == TerrainId::Dirt ? 0 : terrain == TerrainId::Flagstone ? 2 : 1;
								std::array<glm::vec3, 4> p{ glm::vec3(x0, 0.0f, z0), glm::vec3(x0 + Step, 0.0f, z0), glm::vec3(x0 + Step, 0.0f, z0 + Step), glm::vec3(x0, 0.0f, z0 + Step) };
								std::array<glm::vec2, 4> uv;
								std::array<glm::vec4, 4> color;
								for (size_t k = 0; k < 4; k++)
								{
									uv[k] = { p[k].x, p[k].z };
									color[k] = group == 2 ? glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) : GroundColor(group, p[k].x, p[k].z);
								}
								meshes[size_t(group)].Quad(p, uv, glm::vec3(0.0f, 1.0f, 0.0f), color);
							}
						for (size_t group = 0; group < meshes.size(); group++)
						{
							if (meshes[group].Indices.empty())
								continue;
							const uint32_t mesh = AddMesh(meshes[group], groups[group].Material, groups[group].Layer, groups[group].Flags);
							Pending(mesh, meshes[group], glm::mat4(1.0f), false);
						}
					}
			}

			void Walls()
			{
				for (const WallSegment& wall : m_Level.Walls)
				{
					const WallDef& def = GetWall(wall.Style);
					const uint32_t material = wall.Style == WallId::CryptWall ? m_OldStone : m_CastleWall;
					const Vec2 span = wall.To - wall.From;
					const float length = Length(span);
					if (length < 1e-4f)
						continue;
					const Vec2 direction = span / length;
					const float yaw = AngleOf(span);
					const int32_t pieces = std::max(1, int32_t(std::ceil(length / 3.0f)));
					for (int32_t i = 0; i < pieces; i++)
					{
						float start = length * float(i) / float(pieces);
						float end = length * float(i + 1) / float(pieces);
						if (i == 0)
							start -= def.Thickness * 0.5f;
						if (i == pieces - 1)
							end += def.Thickness * 0.5f;
						const Vec2 center = wall.From + direction * ((start + end) * 0.5f);
						const float half = (end - start) * 0.5f;
						MeshData body;
						body.Box({ center.x, wall.Base + def.Height * 0.5f - 0.05f, center.y }, { half, def.Height * 0.5f + 0.05f, def.Thickness * 0.5f }, yaw, start + 0.37f * float(i));
						const uint32_t bodyMesh = AddMesh(body, material, material, 0);
						Pending(bodyMesh, body, glm::mat4(1.0f), true);
						MeshData cap;
						cap.Box({ center.x, wall.Base + def.Height + 0.06f, center.y }, { half + 0.02f, 0.07f, def.Thickness * 0.5f + 0.06f }, yaw, start, true);
						const uint32_t capMesh = AddMesh(cap, m_Tiles, m_Tiles, 0);
						Pending(capMesh, cap, glm::mat4(1.0f), true);
					}
				}
			}

			void BuildShapes()
			{
				const auto shape = [&](uint32_t& slot, uint32_t material, const auto& build)
				{
					MeshData mesh;
					build(mesh);
					slot = AddMesh(mesh, material, material, 0);
					m_ShapeData[slot] = mesh;
				};
				shape(m_PillarShape, m_OldStone, [](MeshData& mesh)
				{
					mesh.Box({ 0.0f, 0.12f, 0.0f }, { 0.4f, 0.12f, 0.4f }, 0.0f);
					mesh.Box({ 0.0f, 1.5f, 0.0f }, { 0.28f, 1.26f, 0.28f }, 0.0f, 0.4f);
					mesh.Box({ 0.0f, 2.86f, 0.0f }, { 0.4f, 0.12f, 0.4f }, 0.0f, 0.8f, true);
				});
				shape(m_SarcophagusShape, m_OldStone, [](MeshData& mesh) { mesh.Box({ 0.0f, 0.36f, 0.0f }, { 1.02f, 0.36f, 0.45f }, 0.0f); });
				shape(m_LidShape, m_Tiles, [](MeshData& mesh) { mesh.Box({ 0.0f, 0.8f, 0.0f }, { 1.1f, 0.08f, 0.52f }, 0.0f, 0.3f, true); });
				shape(m_AltarShape, m_OldStone, [](MeshData& mesh) { mesh.Box({ 0.0f, 0.46f, 0.0f }, { 0.98f, 0.46f, 0.43f }, 0.0f); });
				shape(m_SlabShape, m_Tiles, [](MeshData& mesh)
				{
					mesh.Box({ 0.0f, 0.97f, 0.0f }, { 1.06f, 0.05f, 0.5f }, 0.0f, 0.0f, true);
				});
				shape(m_PlinthShape, m_Tiles, [](MeshData& mesh)
				{
					mesh.Box({ 0.0f, 0.06f, 0.0f }, { 0.95f, 0.06f, 0.95f }, 0.0f);
					mesh.Box({ 0.0f, 0.26f, 0.0f }, { 0.82f, 0.14f, 0.82f }, 0.0f, 0.5f);
				});
				shape(m_PedestalShape, m_OldStone, [](MeshData& mesh)
				{
					mesh.Box({ 0.0f, 0.5f, 0.0f }, { 0.22f, 0.5f, 0.22f }, 0.0f);
					mesh.Box({ 0.0f, 1.03f, 0.0f }, { 0.28f, 0.03f, 0.28f }, 0.0f, 0.2f, true);
				});
				shape(m_PostShape, m_Planks, [](MeshData& mesh)
				{
					mesh.Box({ 0.0f, 1.05f, 0.0f }, { 0.07f, 1.05f, 0.07f }, 0.0f);
					mesh.Box({ 0.0f, 2.12f, 0.0f }, { 0.16f, 0.03f, 0.16f }, 0.0f, 0.3f, true);
				});
			}

			void Shape(uint32_t shape, const glm::mat4& transform, bool cut)
			{
				Pending(shape, m_ShapeData[shape], transform, cut);
			}

			static glm::mat4 PartTransform(const glm::mat4& place, const Part& part)
			{
				return place * Translation(part.Offset) * RotationY(Pi + part.Yaw) * RotationX(part.Pitch) * Scaling(part.Scale);
			}

			void AddParts(const glm::mat4& place, std::initializer_list<Part> parts, bool cut, const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f))
			{
				for (const Part& part : parts)
					AddModel(part.Mesh, PartTransform(place, part), cut, tint);
			}

			void Fire(const glm::mat4& place, const glm::vec3& offset, FireKind kind, float scale = 1.0f)
			{
				m_Scene.Fires.push_back({ glm::vec3(place * glm::vec4(offset, 1.0f)), scale, kind });
			}

			void Glow(const glm::mat4& place, const glm::vec3& offset, const glm::vec3& half, MeshKind mesh = MeshKind::Sphere, const glm::vec3& color = { 1.0f, 0.62f, 0.25f })
			{
				SceneBox box;
				box.Mesh = mesh;
				box.Center = glm::vec3(place * glm::vec4(offset, 1.0f));
				box.Half = half;
				box.Color = color;
				box.Emission = 1.0f;
				box.Base = place[3].y;
				box.Reach = glm::length(half);
				SceneChunk& chunk = ChunkAt({ box.Center.x, box.Center.z });
				chunk.Min = { std::min(chunk.Min.x, box.Center.x - box.Reach), std::min(chunk.Min.y, box.Center.z - box.Reach) };
				chunk.Max = { std::max(chunk.Max.x, box.Center.x + box.Reach), std::max(chunk.Max.y, box.Center.z + box.Reach) };
				chunk.Boxes.push_back(box);
			}

			void Candle(const glm::mat4& place, const glm::vec3& tip, float scale = 1.0f)
			{
				Glow(place, tip, glm::vec3(0.016f, 0.026f, 0.016f) * scale);
				Fire(place, tip + glm::vec3(0.0f, 0.02f, 0.0f), FireKind::Candle, scale);
			}

			void Props()
			{
				const char* grass[] = { "grass_medium_01_tall_a_LOD0", "grass_medium_01_tall_b_LOD0", "grass_medium_01_tall_c_LOD0", "grass_medium_01_mid_a_LOD0", "grass_medium_01_mid_b_LOD0", "grass_medium_01_mid_c_LOD0", "grass_medium_01_small_a_LOD0", "grass_medium_01_small_b_LOD0" };
				const char* weeds[] = { "weed_plant_02_a_LOD0", "weed_plant_02_b_LOD0", "weed_plant_02_c_LOD0", "weed_plant_02_d_LOD0", "weed_plant_02_e_LOD0" };
				const char* ferns[] = { "fern_02_a", "fern_02_b", "fern_02_c", "fern_02_d" };
				const char* branches[] = { "dry_branches_medium_01_a", "dry_branches_medium_01_b", "dry_branches_medium_01_c" };
				const char* rocks[] = { "rock_moss_set_02_rock08", "rock_moss_set_02_rock09", "rock_moss_set_02_rock12", "rock_moss_set_02_rock07" };
				const char* boulders[] = { "rock_moss_set_02_rock13", "rock_moss_set_02_rock10", "rock_moss_set_02_rock11" };
				for (size_t i = 0; i < m_Level.Props.size(); i++)
				{
					const PlacedProp& prop = m_Level.Props[i];
					const int32_t hx = int32_t(std::floor(prop.Position.x * 16.0f));
					const int32_t hy = int32_t(std::floor(prop.Position.y * 16.0f));
					const uint32_t pick = Hash(hx, hy, uint32_t(i) * 7u + 51u);
					const float jitter = 0.88f + 0.24f * Unit(hx, hy, 53);
					const glm::mat4 place = Translation({ prop.Position.x, prop.Base, prop.Position.y }) * RotationY(prop.Yaw);
					const auto variant = [&](const auto& list, const char* prefix) { return std::string(prefix) + list[pick % std::size(list)]; };
					const glm::vec4 natural(glm::vec3(0.92f + 0.16f * Unit(hx, hy, 61)), 0.0f);
					switch (prop.Id)
					{
					case PropId::WineBarrel:
						AddParts(place, { { "wine_barrel_01" } }, false);
						break;
					case PropId::OldBarrel:
						AddParts(place, { { "barrel_03" } }, false);
						break;
					case PropId::LowCrate:
						AddParts(place, { { "wooden_crate_01" } }, false);
						break;
					case PropId::LongCrate:
						AddParts(place, { { "wooden_crate_02" } }, false);
						break;
					case PropId::Bucket:
						AddParts(place, { { "wooden_bucket_02" } }, false);
						break;
					case PropId::TreasureChest:
						AddParts(place, { { "treasure_chest" } }, false);
						break;
					case PropId::LanternPost:
						Shape(m_PostShape, place, true);
						AddParts(place, { { "wooden_lantern_01", { 0.0f, 2.15f, 0.0f } } }, true);
						Glow(place, { 0.0f, 2.34f, 0.0f }, { 0.05f, 0.075f, 0.05f }, MeshKind::Box, { 1.0f, 0.7f, 0.35f });
						Fire(place, { 0.0f, 2.32f, 0.0f }, FireKind::Lantern);
						break;
					case PropId::OilLamp:
						AddParts(place, { { "vintage_oil_lamp" } }, false);
						Candle(place, { 0.0f, 0.43f, 0.0f });
						break;
					case PropId::CandleStand:
						AddParts(place, { { "brass_candleholders/brass_candleholder_03" } }, false);
						Candle(place, { 0.0f, 0.82f, 0.0f });
						Candle(place, { -0.15f, 0.74f, 0.0f });
						Candle(place, { 0.15f, 0.74f, 0.0f });
						break;
					case PropId::FirePit:
						AddParts(place, { { "stone_fire_pit" }, { "dry_branches_medium_01/dry_branches_medium_01_a", { 0.1f, 0.08f, 0.0f }, 0.6f, 0.7f }, { "dry_branches_medium_01/dry_branches_medium_01_c", { -0.15f, 0.08f, 0.05f }, 2.1f, 0.7f } }, false);
						Glow(place, { 0.0f, 0.06f, 0.0f }, { 0.34f, 0.08f, 0.34f }, MeshKind::Sphere, { 0.9f, 0.32f, 0.08f });
						Fire(place, { 0.0f, 0.22f, 0.0f }, FireKind::Campfire);
						break;
					case PropId::FireBowl:
						Shape(m_PedestalShape, place, true);
						AddParts(place, { { "stone_fire_pit", { 0.0f, 1.06f, 0.0f }, 0.0f, 0.42f } }, true);
						Glow(place, { 0.0f, 1.1f, 0.0f }, { 0.15f, 0.05f, 0.15f }, MeshKind::Sphere, { 0.9f, 0.32f, 0.08f });
						Fire(place, { 0.0f, 1.14f, 0.0f }, FireKind::Campfire, 0.5f);
						break;
					case PropId::BronzeStatue:
						Shape(m_PlinthShape, place, true);
						AddParts(place, { { "gothic_statue", { 0.0f, 0.4f, 0.0f } } }, true);
						break;
					case PropId::PlankTable:
						AddParts(place, { { "WoodenTable_01" }, { "vintage_oil_lamp", { -0.5f, 0.55f, 0.05f }, 0.0f, 0.85f }, { "metal_jug", { 0.25f, 0.55f, -0.12f }, 0.8f }, { "brass_goblets", { 0.55f, 0.55f, 0.1f }, 0.2f }, { "brass_goblets", { 0.68f, 0.55f, -0.08f }, 1.7f } }, false);
						Candle(place, { -0.5f, 0.55f + 0.43f * 0.85f, 0.05f }, 0.85f);
						break;
					case PropId::Stool:
						AddParts(place, { { "folding_wooden_stool" } }, false);
						break;
					case PropId::MossRock:
						AddParts(place, { { variant(rocks, "rock_moss_set_02/"), {}, 0.0f, 0.85f * jitter } }, false, natural);
						break;
					case PropId::MossBoulder:
						AddParts(place, { { variant(boulders, "rock_moss_set_02/"), {}, 0.0f, jitter } }, false, natural);
						break;
					case PropId::TreeStump:
						AddParts(place, { { "tree_stump_01", {}, 0.0f, jitter } }, false);
						break;
					case PropId::DeadLog:
						AddParts(place, { { "dead_tree_trunk", { 0.0f, 0.0f, 0.0f }, 0.0f, jitter } }, false);
						break;
					case PropId::Fern:
						AddParts(place, { { variant(ferns, "fern_02/"), {}, 0.0f, jitter } }, false, natural);
						break;
					case PropId::Weeds:
						AddParts(place, { { variant(weeds, "weed_plant_02/"), {}, 0.0f, 1.6f * jitter } }, false, natural);
						break;
					case PropId::GrassTuft:
						AddParts(place, { { variant(grass, "grass_medium_01/"), {}, 0.0f, 1.5f * jitter } }, false, natural);
						break;
					case PropId::DryBranches:
						AddParts(place, { { variant(branches, "dry_branches_medium_01/"), {}, 0.0f, jitter } }, false);
						break;
					case PropId::IronGate:
						AddParts(place, { { "large_iron_gate/large_iron_gate_left_door", { -1.45f, 0.0f, -0.74f }, HalfPi }, { "large_iron_gate/large_iron_gate_right_door", { 1.45f, 0.0f, -0.74f }, -HalfPi } }, true);
						break;
					case PropId::CryptDoor:
						AddParts(place, { { "large_castle_door/large_castle_door_frame" }, { "large_castle_door/large_castle_door_left", { -0.93f, 0.0f, 0.5f }, -HalfPi }, { "large_castle_door/large_castle_door_right", { 0.93f, 0.0f, 0.5f }, HalfPi } }, true);
						break;
					case PropId::Sarcophagus:
					{
						Shape(m_SarcophagusShape, place, false);
						Shape(m_LidShape, place, false);
						const uint32_t decor = pick % 3;
						if (decor == 0)
							AddParts(place, { { "kite_shield", { 0.0f, 0.88f, 0.0f }, HalfPi, 0.9f, HalfPi } }, false);
						else if (decor == 1)
							AddParts(place, { { "antique_estoc", { 0.0f, 0.89f, 0.0f }, HalfPi, 1.0f, HalfPi } }, false);
						else
							AddParts(place, { { "ornate_war_hammer", { 0.2f, 0.89f, 0.05f }, HalfPi + 0.2f, 1.0f, HalfPi } }, false);
						break;
					}
					case PropId::Altar:
						Shape(m_AltarShape, place, false);
						Shape(m_SlabShape, place, false);
						AddParts(place, { { "brass_vase_01", { 0.0f, 1.02f, 0.18f } }, { "brass_pot_01", { -0.55f, 1.02f, 0.05f }, 0.6f }, { "brass_goblets", { 0.45f, 1.02f, -0.1f } }, { "antique_ceramic_vase_01", { 0.8f, 1.02f, 0.15f }, 2.0f },
							{ "brass_candleholders/brass_candleholder_01", { -0.85f, 1.02f, -0.2f } }, { "brass_candleholders/brass_candleholder_02", { 0.75f, 1.02f, -0.25f }, 0.3f } }, false);
						Candle(place, { -0.85f, 1.02f + 0.35f, -0.2f });
						Candle(place, { 0.75f, 1.02f + 0.26f, -0.25f });
						break;
					case PropId::CryptPillar:
						Shape(m_PillarShape, place, true);
						break;
					case PropId::MarbleBust:
						Shape(m_PedestalShape, place, true);
						AddParts(place, { { "marble_bust_01", { 0.0f, 1.06f, 0.0f } } }, true);
						break;
					default:
						m_Scene.PropDrawn[i] = 0;
						break;
					}
				}
			}

			bool IsOpen(Vec2 point, float radius) const
			{
				const int32_t x = int32_t(std::floor(point.x));
				const int32_t y = int32_t(std::floor(point.y));
				if (m_Map.IsInside(x, y) && m_Map.IsWalkable(x, y))
					return false;
				return WallDistance(point) > radius;
			}

			void Wilds()
			{
				const char* grass[] = { "grass_medium_01_mid_a_LOD0", "grass_medium_01_large_b_LOD0", "grass_medium_01_mid_c_LOD0", "grass_medium_01_tall_a_LOD0" };
				const char* ferns[] = { "fern_02_a", "fern_02_b", "fern_02_c", "fern_02_d" };
				const char* rocks[] = { "rock_moss_set_02_rock08", "rock_moss_set_02_rock09", "rock_moss_set_02_rock12", "rock_moss_set_02_rock13", "rock_moss_set_02_rock10" };
				const float width = float(m_Map.GetWidth());
				const float height = float(m_Map.GetHeight());
				for (float z = -Margin; z < height + Margin; z += 1.6f)
					for (float x = -Margin; x < width + Margin; x += 1.6f)
					{
						const int32_t ix = int32_t(std::floor(x * 4.0f));
						const int32_t iz = int32_t(std::floor(z * 4.0f));
						const Vec2 point = { x + 1.2f * Unit(ix, iz, 71), z + 1.2f * Unit(ix, iz, 72) };
						if (!IsOpen(point, 0.8f))
							continue;
						const float roll = Unit(ix, iz, 73);
						const float yaw = 2.0f * Pi * Unit(ix, iz, 74);
						const float scale = 0.8f + 0.5f * Unit(ix, iz, 75);
						const glm::mat4 place = Translation({ point.x, 0.0f, point.y }) * RotationY(yaw);
						const glm::vec4 natural(glm::vec3(0.9f + 0.2f * Unit(ix, iz, 76)), 0.0f);
						const uint32_t pick = Hash(ix, iz, 77);
						if (roll < 0.05f && IsOpen(point, 1.6f))
							AddParts(place, { { std::string("rock_moss_set_02/") + rocks[pick % std::size(rocks)], {}, 0.0f, scale } }, false, natural);
						else if (roll < 0.2f)
							AddParts(place, { { std::string("fern_02/") + ferns[pick % std::size(ferns)], {}, 0.0f, scale } }, false, natural);
						else if (roll < 0.55f)
							AddParts(place, { { std::string("grass_medium_01/") + grass[pick % std::size(grass)], {}, 0.0f, 1.6f * scale } }, false, natural);
					}
			}

			void Roofs()
			{
				for (const Roof& roof : m_Level.Roofs)
				{
					SceneBox box;
					box.Mesh = MeshKind::Box;
					box.Half = { (roof.Max.x - roof.Min.x) * 0.5f + 0.5f, 0.25f, (roof.Max.y - roof.Min.y) * 0.5f + 0.5f };
					box.Center = { (roof.Min.x + roof.Max.x) * 0.5f, roof.Height + 0.25f, (roof.Min.y + roof.Max.y) * 0.5f };
					box.Reach = Length(Vec2(box.Half.x, box.Half.z));
					box.ShadowOnly = true;
					SceneChunk& chunk = ChunkAt({ box.Center.x, box.Center.z });
					chunk.Min = { std::min(chunk.Min.x, box.Center.x - box.Reach), std::min(chunk.Min.y, box.Center.z - box.Reach) };
					chunk.Max = { std::max(chunk.Max.x, box.Center.x + box.Reach), std::max(chunk.Max.y, box.Center.z + box.Reach) };
					chunk.Boxes.push_back(box);
				}
			}

			void SkyMask()
			{
				const glm::vec2 origin(-Margin);
				const glm::vec2 size(float(m_Map.GetWidth()) + 2.0f * Margin, float(m_Map.GetHeight()) + 2.0f * Margin);
				const uint32_t width = uint32_t(size.x * SkyTexels);
				const uint32_t height = uint32_t(size.y * SkyTexels);
				std::vector<uint8_t> openness(size_t(width) * height, 255);
				for (uint32_t y = 0; y < height; y++)
					for (uint32_t x = 0; x < width; x++)
					{
						const Vec2 point = { origin.x + (float(x) + 0.5f) / SkyTexels, origin.y + (float(y) + 0.5f) / SkyTexels };
						float open = 1.0f;
						for (const Roof& roof : m_Level.Roofs)
						{
							const float dx = std::max(roof.Min.x - point.x, point.x - roof.Max.x);
							const float dz = std::max(roof.Min.y - point.y, point.y - roof.Max.y);
							open = std::min(open, 0.05f + 0.95f * Smooth(-0.9f, 0.4f, std::max(dx, dz)));
						}
						openness[size_t(y) * width + x] = uint8_t(std::lround(open * 255.0f));
					}
				m_Renderer.SetSkyMask(width, height, openness, origin, size);
			}

			const Level& m_Level;
			const TileMap& m_Map;
			WorldRenderer& m_Renderer;
			const ModelLibrary& m_Models;
			LevelScene& m_Scene;
			int32_t m_ChunksX = 1;
			int32_t m_ChunksY = 1;
			uint32_t m_Cobble = 0;
			uint32_t m_Forest = 0;
			uint32_t m_Mud = 0;
			uint32_t m_Tiles = 0;
			uint32_t m_CastleWall = 0;
			uint32_t m_OldStone = 0;
			uint32_t m_Planks = 0;
			uint32_t m_PillarShape = 0;
			uint32_t m_SarcophagusShape = 0;
			uint32_t m_LidShape = 0;
			uint32_t m_AltarShape = 0;
			uint32_t m_SlabShape = 0;
			uint32_t m_PlinthShape = 0;
			uint32_t m_PedestalShape = 0;
			uint32_t m_PostShape = 0;
			std::map<uint32_t, MeshData> m_ShapeData;
			std::vector<ModelVertex> m_Vertices;
			std::vector<uint32_t> m_Indices;
			std::vector<LevelMeshDesc> m_Meshes;
			std::vector<PendingItem> m_Pending;
			std::vector<std::string> m_Missing;
		};
	}

	bool BuildAbbeyScene(const Level& level, WorldRenderer& renderer, LevelScene& scene)
	{
		scene = {};
		if (!AbbeyBuilder(level, renderer, scene).Build())
		{
			scene = {};
			return false;
		}
		return true;
	}
}
