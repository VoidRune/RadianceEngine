#include "LevelArt.h"
#include "Rendering/LevelRenderer.h"
#include "Rendering/Models.h"
#include <RadianceEngine/Core/Log.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Game
{
	namespace
	{
		constexpr float TwoPi = 2.0f * Pi;
		constexpr float Module = 0.5f;
		constexpr int32_t ChunkSize = 8;
		constexpr float VoidDepth = 3.5f;
		constexpr float LiquidDrop = 0.6f;
		constexpr float FoundationOverlap = 0.05f;
		constexpr float NoFace = std::numeric_limits<float>::max();

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

		glm::mat4 RotationY(float angle)
		{
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			glm::mat4 m(1.0f);
			m[0] = glm::vec4(c, 0.0f, s, 0.0f);
			m[2] = glm::vec4(-s, 0.0f, c, 0.0f);
			return m;
		}

		glm::mat4 RotationZ(float angle)
		{
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			glm::mat4 m(1.0f);
			m[0] = glm::vec4(c, s, 0.0f, 0.0f);
			m[1] = glm::vec4(-s, c, 0.0f, 0.0f);
			return m;
		}

		glm::mat4 Translation(const glm::vec3& offset)
		{
			return glm::translate(glm::mat4(1.0f), offset);
		}

		glm::mat4 Scaling(const glm::vec3& scale)
		{
			return glm::scale(glm::mat4(1.0f), scale);
		}

		struct ArtPart
		{
			const char* Mesh = "";
			glm::vec3 Offset{ 0.0f };
			glm::vec3 Scale{ 1.0f };
			float Yaw = 0.0f;
			float Roll = 0.0f;
			glm::vec3 Pivot{ 0.0f };
			glm::vec4 Tint{ 1.0f, 1.0f, 1.0f, 0.0f };
			float Glow = 0.0f;
		};

		struct PropArt
		{
			std::vector<std::vector<ArtPart>> Variants;
			bool Natural = false;
			float KeepAbove = -1.0f;
		};

		constexpr glm::vec3 FlameColor{ 1.0f, 0.62f, 0.25f };

		glm::vec3 Uniform(float scale)
		{
			return glm::vec3(scale);
		}

		ArtPart Flame(glm::vec3 offset, float radius)
		{
			return { .Offset = offset, .Glow = radius };
		}

		void HouseSide(std::vector<ArtPart>& parts, std::initializer_list<const char*> meshes, Vec2 from, Vec2 to, float outward, float height, const glm::vec4& tint)
		{
			const size_t count = meshes.size();
			size_t i = 0;
			for (const char* mesh : meshes)
			{
				const Vec2 start = Lerp(from, to, float(i) / float(count));
				const Vec2 end = Lerp(from, to, float(i + 1) / float(count));
				const Vec2 center = (start + end) * 0.5f;
				parts.push_back({ .Mesh = mesh, .Offset = { center.x, 0.0f, center.y }, .Scale = { Length(end - start) / 4.0f, height / 4.0f, 0.3f }, .Yaw = outward - Pi, .Tint = tint });
				i++;
			}
		}

		std::vector<ArtPart> House(float halfX, float halfY, float height, std::initializer_list<const char*> front, std::initializer_list<const char*> back, std::initializer_list<const char*> left, std::initializer_list<const char*> right, const glm::vec4& tint)
		{
			const float overlap = 0.15f;
			std::vector<ArtPart> parts;
			HouseSide(parts, front, { -halfX - overlap, -halfY }, { halfX + overlap, -halfY }, Pi, height, tint);
			HouseSide(parts, back, { -halfX - overlap, halfY }, { halfX + overlap, halfY }, 0.0f, height, tint);
			HouseSide(parts, left, { -halfX, -halfY }, { -halfX, halfY }, HalfPi, height, tint);
			HouseSide(parts, right, { halfX, -halfY }, { halfX, halfY }, -HalfPi, height, tint);
			parts.push_back({ .Mesh = "floor_wood_large", .Offset = { 0.0f, 0.01f, 0.0f }, .Scale = { halfX * 0.5f, 0.5f, halfY * 0.5f }, .Tint = { 0.85f, 0.85f, 0.85f, 0.0f } });
			return parts;
		}

		PropArt GetPropArt(PropId id)
		{
			const glm::vec4 marble(1.0f, 1.0f, 1.0f, 0.45f);
			const glm::vec4 warm(1.12f, 1.0f, 0.84f, 0.0f);
			const glm::vec4 stone(1.0f, 1.0f, 1.0f, 0.0f);
			switch (id)
			{
			case PropId::House:
				return { {
					House(2.0f, 2.0f, 2.4f, { "wall_window_closed", "wall_doorway" }, { "wall", "wall" }, { "wall_window_closed", "wall" }, { "wall", "wall_window_closed" }, warm),
					House(2.0f, 2.0f, 2.4f, { "wall_window_open", "wall_doorway" }, { "wall", "wall_window_closed" }, { "wall", "wall" }, { "wall_window_closed", "wall" }, stone),
				}, false, 2.35f };
			case PropId::LongHouse:
				return { {
					House(3.0f, 2.0f, 2.6f, { "wall_doorway", "wall_window_closed", "wall_window_closed" }, { "wall", "wall_window_closed", "wall" }, { "wall", "wall" }, { "wall_window_closed", "wall" }, warm),
					House(3.0f, 2.0f, 2.6f, { "wall_doorway", "wall_window_open", "wall_window_closed" }, { "wall", "wall", "wall" }, { "wall_window_closed", "wall" }, { "wall", "wall" }, stone),
				}, false, 2.55f };
			case PropId::Barrel:
				return { { { { .Mesh = "barrel_large", .Scale = Uniform(0.4f) } }, { { .Mesh = "barrel_small", .Scale = Uniform(0.68f) } } } };
			case PropId::Crate:
				return { { { { .Mesh = "box_small", .Scale = Uniform(0.82f) } }, { { .Mesh = "box_large", .Scale = Uniform(0.55f) } } } };
			case PropId::CrateStack:
				return { { { { .Mesh = "crates_stacked", .Scale = Uniform(0.44f) } }, { { .Mesh = "box_stacked", .Scale = Uniform(0.27f) } } } };
			case PropId::StonePillar:
				return { { { { .Mesh = "pillar", .Scale = Uniform(0.6f) } } } };
			case PropId::MarblePillar:
				return { { { { .Mesh = "pillar", .Scale = Uniform(0.66f), .Tint = marble } } } };
			case PropId::BrokenPillar:
				return { { {
					{ .Mesh = "column", .Scale = Uniform(0.85f) },
					{ .Mesh = "column", .Offset = { 0.75f, 0.26f, 0.35f }, .Scale = Uniform(0.75f), .Yaw = 0.6f + HalfPi, .Roll = HalfPi, .Pivot = { 0.0f, 0.7f, 0.0f } },
				} } };
			case PropId::Bookshelf:
				return { { { { .Mesh = "wall_shelves", .Scale = { 0.5f, 0.55f, 0.42f }, .Pivot = { 0.0f, 0.0f, 0.184f } } } } };
			case PropId::DoubleBookshelf:
				return { { {
					{ .Mesh = "wall_shelves", .Scale = { 0.5f, 0.55f, 0.3f }, .Pivot = { 0.0f, 0.0f, -0.5f } },
					{ .Mesh = "wall_shelves", .Scale = { 0.5f, 0.55f, 0.3f }, .Yaw = Pi, .Pivot = { 0.0f, 0.0f, -0.5f } },
				} } };
			case PropId::ReadingTable:
				return { { {
					{ .Mesh = "table_long", .Scale = Uniform(0.6f), .Yaw = HalfPi },
					{ .Mesh = "candle_lit", .Offset = { 0.75f, 0.6f, 0.1f }, .Scale = Uniform(0.35f) },
					Flame({ 0.75f, 0.97f, 0.1f }, 0.035f),
				} } };
			case PropId::RoundTable:
				return { { { { .Mesh = "table_medium", .Scale = Uniform(0.56f) } }, { { .Mesh = "table_medium_tablecloth", .Scale = Uniform(0.56f) } } } };
			case PropId::Chair:
				return { { { { .Mesh = "chair", .Scale = Uniform(0.72f) } } } };
			case PropId::Armchair:
				return { { { { .Mesh = "chair", .Scale = Uniform(0.86f) } } } };
			case PropId::Desk:
				return { { { { .Mesh = "table_long_decorated_A", .Scale = Uniform(0.62f), .Yaw = HalfPi } } } };
			case PropId::OakTree:
				return { {
					{ { .Mesh = "Tree_1_A_Color1", .Scale = Uniform(0.8f) } },
					{ { .Mesh = "Tree_1_B_Color1", .Scale = Uniform(0.72f) } },
					{ { .Mesh = "Tree_3_A_Color1", .Scale = Uniform(0.95f) } },
					{ { .Mesh = "Tree_3_B_Color1", .Scale = Uniform(0.8f) } },
				}, true };
			case PropId::PineTree:
				return { {
					{ { .Mesh = "Tree_4_A_Color1", .Scale = Uniform(0.62f) } },
					{ { .Mesh = "Tree_4_B_Color1", .Scale = Uniform(0.52f) } },
					{ { .Mesh = "Tree_2_A_Color1", .Scale = Uniform(0.72f) } },
					{ { .Mesh = "Tree_2_B_Color1", .Scale = Uniform(0.6f) } },
				}, true };
			case PropId::Bush:
				return { {
					{ { .Mesh = "Bush_1_B_Color1", .Scale = Uniform(1.0f) } },
					{ { .Mesh = "Bush_1_C_Color1", .Scale = Uniform(0.75f) } },
					{ { .Mesh = "Bush_2_B_Color1", .Scale = Uniform(0.85f) } },
					{ { .Mesh = "Bush_3_A_Color1", .Scale = Uniform(0.9f) } },
					{ { .Mesh = "Bush_4_B_Color1", .Scale = Uniform(0.85f) } },
				}, true };
			case PropId::Boulder:
				return { {
					{ { .Mesh = "Rock_1_G_Color1", .Scale = Uniform(0.85f) } },
					{ { .Mesh = "Rock_1_H_Color1", .Scale = Uniform(0.9f) } },
					{ { .Mesh = "Rock_1_I_Color1", .Scale = Uniform(0.85f) } },
					{ { .Mesh = "Rock_3_G_Color1", .Scale = Uniform(0.8f) } },
					{ { .Mesh = "Rock_3_H_Color1", .Scale = Uniform(0.75f) } },
				}, true };
			case PropId::Rock:
				return { {
					{ { .Mesh = "Rock_1_D_Color1", .Scale = Uniform(0.65f) } },
					{ { .Mesh = "Rock_1_E_Color1", .Scale = Uniform(0.65f) } },
					{ { .Mesh = "Rock_1_F_Color1", .Scale = Uniform(0.68f) } },
					{ { .Mesh = "Rock_3_C_Color1", .Scale = Uniform(0.8f) } },
					{ { .Mesh = "Rock_3_E_Color1", .Scale = Uniform(0.6f) } },
				}, true };
			case PropId::FallenLog:
				return { { { { .Mesh = "Tree_Bare_2_A_Color1", .Offset = { 0.0f, 0.18f, 0.0f }, .Scale = Uniform(0.58f), .Yaw = HalfPi, .Roll = HalfPi, .Pivot = { 0.0f, 1.7f, 0.0f } } } }, true };
			case PropId::Rubble:
				return { { {
					{ .Mesh = "Rock_3_A_Color1", .Scale = Uniform(0.3f) },
					{ .Mesh = "Rock_3_B_Color1", .Offset = { 0.25f, 0.0f, 0.15f }, .Scale = Uniform(0.24f), .Yaw = 1.1f },
					{ .Mesh = "Rock_2_B_Color1", .Offset = { -0.2f, 0.0f, 0.2f }, .Scale = Uniform(0.38f), .Yaw = 0.7f },
				} }, true };
			case PropId::Banner:
				return { { { { .Mesh = "banner_patternA_red", .Offset = { 0.0f, 0.55f, 0.27f }, .Scale = Uniform(0.5f) } } } };
			case PropId::WallTorch:
				return { { {
					{ .Mesh = "torch_mounted", .Offset = { 0.0f, 1.5f, 0.0f }, .Scale = Uniform(0.85f) },
					Flame({ 0.0f, 1.97f, -0.38f }, 0.09f),
				} } };
			case PropId::PottedPlant:
				return { { {
					{ .Mesh = "barrel_small", .Scale = { 0.44f, 0.36f, 0.44f } },
					{ .Mesh = "Bush_1_B_Color1", .Offset = { 0.0f, 0.3f, 0.0f }, .Scale = Uniform(0.85f) },
				}, {
					{ .Mesh = "barrel_small", .Scale = { 0.44f, 0.36f, 0.44f } },
					{ .Mesh = "Bush_4_B_Color1", .Offset = { 0.0f, 0.3f, 0.0f }, .Scale = Uniform(0.7f) },
				} } };
			case PropId::StashChest:
				return { { { { .Mesh = "chest", .Scale = Uniform(0.72f) } } } };
			case PropId::Door:
				return { { { { .Mesh = "wall_doorway", .Scale = { 0.5f, 0.7f, 0.45f } } } } };
			default:
				return {};
			}
		}

		struct ResolvedPart
		{
			uint32_t Mesh = 0;
			glm::mat4 Local{ 1.0f };
			glm::vec4 Tint{ 1.0f };
			float Glow = 0.0f;
		};

		struct ResolvedArt
		{
			std::vector<std::vector<ResolvedPart>> Variants;
			bool Natural = false;
			float KeepAbove = -1.0f;
		};

		struct WeightedMesh
		{
			uint32_t Mesh = 0;
			float Scale = 1.0f;
		};

		enum FoundationMask : uint8_t
		{
			SideNorth = 1 << 0,
			SideEast = 1 << 1,
			SideSouth = 1 << 2,
			SideWest = 1 << 3,
		};

		float SlopeYaw(Direction slope)
		{
			switch (slope)
			{
			case Direction::North:
				return 0.0f;
			case Direction::East:
				return -HalfPi;
			case Direction::South:
				return Pi;
			default:
				return HalfPi;
			}
		}

		uint8_t RotateMask(uint8_t mask, uint32_t turns)
		{
			uint8_t result = 0;
			for (uint32_t side = 0; side < 4; side++)
				if (mask & (1u << side))
					result |= uint8_t(1u << ((side + 4u - turns % 4u) % 4u));
			return result;
		}

		bool IsBuilt(TerrainId terrain)
		{
			switch (terrain)
			{
			case TerrainId::Flagstone:
			case TerrainId::Cobblestone:
			case TerrainId::Marble:
			case TerrainId::WoodFloor:
			case TerrainId::Carpet:
				return true;
			default:
				return false;
			}
		}

		class SceneBuilder
		{
		public:
			SceneBuilder(const Level& level, const WorldRenderer& renderer, LevelScene& scene)
				: m_Level(level), m_Map(level.Map), m_Renderer(renderer), m_Models(renderer.GetModels()), m_Scene(scene)
			{
			}

			void Build()
			{
				const int32_t width = m_Map.GetWidth();
				const int32_t height = m_Map.GetHeight();
				m_ChunksX = (width + ChunkSize - 1) / ChunkSize;
				m_ChunksY = (height + ChunkSize - 1) / ChunkSize;
				m_Scene.Chunks.assign(size_t(m_ChunksX) * size_t(m_ChunksY), {});
				m_Scene.Cells.assign(size_t(width) * size_t(height), 0);
				m_Scene.PropDrawn.assign(m_Level.Props.size(), 0);
				m_Scene.WallDrawn.assign(m_Level.Walls.size(), 0);
				ResolveMeshes();
				Floors();
				Rocks();
				Walls();
				Props();
				Decor();
				if (!m_Missing.empty())
				{
					std::string list;
					for (const std::string& name : m_Missing)
						list += (list.empty() ? "" : ", ") + name;
					RDN_LOG_WARNING("The model library has no {}, those parts of the level are drawn as boxes", list);
				}
			}

		private:
			std::optional<uint32_t> Find(std::string_view name)
			{
				if (const std::optional<uint32_t> mesh = m_Models.FindMesh(name))
					return mesh;
				const std::string missing(name);
				if (std::find(m_Missing.begin(), m_Missing.end(), missing) == m_Missing.end())
					m_Missing.push_back(missing);
				return std::nullopt;
			}

			std::vector<uint32_t> FindAll(std::initializer_list<const char*> names)
			{
				std::vector<uint32_t> result;
				for (const char* name : names)
					if (const std::optional<uint32_t> mesh = Find(name))
						result.push_back(*mesh);
				return result;
			}

			std::vector<WeightedMesh> FindWeighted(std::initializer_list<std::pair<const char*, float>> entries)
			{
				std::vector<WeightedMesh> result;
				for (const auto& [name, scale] : entries)
					if (const std::optional<uint32_t> mesh = Find(name))
						result.push_back({ *mesh, scale });
				return result;
			}

			void ResolveMeshes()
			{
				m_TileSmall = Find("floor_tile_small");
				m_TileLarge = Find("floor_tile_large");
				m_TileBroken = FindAll({ "floor_tile_small_broken_A", "floor_tile_small_broken_B" });
				m_TileWeeds = FindAll({ "floor_tile_small_weeds_A", "floor_tile_small_weeds_B" });
				m_Wood = Find("floor_wood_small");
				m_Dirt = FindAll({ "floor_dirt_small_A", "floor_dirt_small_B", "floor_dirt_small_C", "floor_dirt_small_D" });
				m_FoundationFront = Find("floor_foundation_front");
				m_FoundationCorner = Find("floor_foundation_corner");
				m_FoundationOpposite = Find("floor_foundation_front_and_back");
				m_FoundationThree = Find("floor_foundation_front_and_sides");
				m_FoundationAll = Find("floor_foundation_allsides");
				m_Stairs = Find("stairs_modular_center");
				m_Wall = Find("wall");
				m_WallCracked = Find("wall_cracked");
				m_WallWindow = Find("wall_window_open");
				m_Barrier = Find("barrier_column");
				m_Hedge = FindWeighted({ { "Bush_2_C_Color1", 0.92f }, { "Bush_2_B_Color1", 1.35f }, { "Bush_1_C_Color1", 1.05f } });
				m_EdgeRocks = FindWeighted({ { "Rock_1_G_Color1", 0.85f }, { "Rock_1_H_Color1", 0.9f }, { "Rock_1_I_Color1", 0.85f }, { "Rock_3_G_Color1", 0.85f }, { "Rock_3_H_Color1", 0.8f }, { "Rock_3_I_Color1", 0.85f } });
				m_CliffRocks = FindWeighted({ { "Rock_1_D_Color1", 0.6f }, { "Rock_1_E_Color1", 0.6f }, { "Rock_3_E_Color1", 0.55f }, { "Rock_3_F_Color1", 0.55f } });
				m_Pebbles = FindWeighted({ { "Rock_2_A_Color1", 1.0f }, { "Rock_2_B_Color1", 0.45f }, { "Rock_3_A_Color1", 0.3f }, { "Rock_3_C_Color1", 0.28f } });
				m_Grass = FindWeighted({ { "Grass_1_A_Singlesided_Color1", 0.8f }, { "Grass_1_B_Singlesided_Color1", 0.75f }, { "Grass_1_C_Singlesided_Color1", 0.7f }, { "Grass_2_A_Singlesided_Color1", 0.6f }, { "Grass_2_B_Singlesided_Color1", 0.6f }, { "Grass_2_C_Singlesided_Color1", 0.55f } });
				m_SmallBushes = FindWeighted({ { "Bush_1_A_Color1", 1.0f }, { "Bush_1_B_Color1", 0.7f }, { "Bush_4_A_Color1", 0.8f } });
				m_Saplings = FindWeighted({ { "Tree_Bare_1_A_Color1", 0.5f }, { "Tree_Bare_2_A_Color1", 0.4f } });

				m_PropArt.resize(size_t(PropId::Count));
				for (size_t id = 0; id < size_t(PropId::Count); id++)
				{
					const PropArt art = GetPropArt(PropId(id));
					ResolvedArt& resolved = m_PropArt[id];
					resolved.Natural = art.Natural;
					resolved.KeepAbove = art.KeepAbove;
					for (const std::vector<ArtPart>& variant : art.Variants)
					{
						std::vector<ResolvedPart> parts;
						bool complete = true;
						for (const ArtPart& part : variant)
						{
							if (part.Glow > 0.0f)
							{
								parts.push_back({ 0, Translation(part.Offset), glm::vec4(1.0f), part.Glow });
								continue;
							}
							const std::optional<uint32_t> mesh = Find(part.Mesh);
							if (!mesh)
							{
								complete = false;
								break;
							}
							const glm::mat4 local = Translation(part.Offset) * RotationY(Pi + part.Yaw) * RotationZ(part.Roll) * Scaling(part.Scale) * Translation(-part.Pivot);
							parts.push_back({ *mesh, local, part.Tint });
						}
						if (complete && !parts.empty())
							resolved.Variants.push_back(std::move(parts));
					}
				}
			}

			size_t CellIndex(int32_t x, int32_t y) const
			{
				return size_t(y) * size_t(m_Map.GetWidth()) + size_t(x);
			}

			void Add(uint32_t mesh, const glm::mat4& transform, const glm::vec4& tint, float ground, bool cut, float base)
			{
				const ModelMesh& source = m_Models.GetMesh(mesh);
				glm::vec3 low(1e9f);
				glm::vec3 high(-1e9f);
				for (uint32_t corner = 0; corner < 8; corner++)
				{
					const glm::vec3 local((corner & 1) ? source.Max.x : source.Min.x, (corner & 2) ? source.Max.y : source.Min.y, (corner & 4) ? source.Max.z : source.Min.z);
					const glm::vec3 world = glm::vec3(transform * glm::vec4(local, 1.0f));
					low = glm::min(low, world);
					high = glm::max(high, world);
				}
				SceneItem item;
				item.Mesh = mesh;
				item.Instance = m_Renderer.MakeModelInstance(mesh, transform, NoPalette, tint, 0.0f, ground);
				item.Center = { (low.x + high.x) * 0.5f, (low.z + high.z) * 0.5f };
				item.Reach = 0.5f * std::sqrt((high.x - low.x) * (high.x - low.x) + (high.z - low.z) * (high.z - low.z));
				item.Base = base;
				item.Top = high.y;
				item.Cut = cut;
				const int32_t cx = std::clamp(int32_t(std::floor(item.Center.x)) / ChunkSize, 0, m_ChunksX - 1);
				const int32_t cy = std::clamp(int32_t(std::floor(item.Center.y)) / ChunkSize, 0, m_ChunksY - 1);
				SceneChunk& chunk = m_Scene.Chunks[size_t(cy) * size_t(m_ChunksX) + size_t(cx)];
				chunk.Min = { std::min(chunk.Min.x, low.x), std::min(chunk.Min.y, low.z) };
				chunk.Max = { std::max(chunk.Max.x, high.x), std::max(chunk.Max.y, high.z) };
				chunk.Items.push_back(item);
			}

			void AddBox(const SceneBox& box)
			{
				const Vec2 anchor = { box.Center.x, box.Center.z };
				const int32_t cx = std::clamp(int32_t(std::floor(anchor.x)) / ChunkSize, 0, m_ChunksX - 1);
				const int32_t cy = std::clamp(int32_t(std::floor(anchor.y)) / ChunkSize, 0, m_ChunksY - 1);
				SceneChunk& chunk = m_Scene.Chunks[size_t(cy) * size_t(m_ChunksX) + size_t(cx)];
				chunk.Min = { std::min(chunk.Min.x, anchor.x - box.Reach), std::min(chunk.Min.y, anchor.y - box.Reach) };
				chunk.Max = { std::max(chunk.Max.x, anchor.x + box.Reach), std::max(chunk.Max.y, anchor.y + box.Reach) };
				chunk.Boxes.push_back(box);
			}

			float CellHeight(int32_t x, int32_t y) const
			{
				return float(m_Map.Get(x, y).Level) * HeightUnit;
			}

			float NeighborEdge(int32_t x, int32_t y, Direction side, float own) const
			{
				int32_t nx = x;
				int32_t ny = y;
				TileMap::Step(side, nx, ny);
				const Cell& neighbor = m_Map.Get(nx, ny);
				switch (GetTerrain(neighbor.Terrain).Style)
				{
				case TerrainStyle::Solid:
					return NoFace;
				case TerrainStyle::Void:
					return own - VoidDepth;
				case TerrainStyle::Liquid:
					return float(neighbor.Level) * HeightUnit - LiquidDrop;
				default:
					break;
				}
				switch (side)
				{
				case Direction::North:
					return std::min(m_Map.GetHeightAt(nx, ny, 0.0f, 0.0f), m_Map.GetHeightAt(nx, ny, 1.0f, 0.0f));
				case Direction::South:
					return std::min(m_Map.GetHeightAt(nx, ny, 0.0f, 1.0f), m_Map.GetHeightAt(nx, ny, 1.0f, 1.0f));
				case Direction::East:
					return std::min(m_Map.GetHeightAt(nx, ny, 0.0f, 0.0f), m_Map.GetHeightAt(nx, ny, 0.0f, 1.0f));
				default:
					return std::min(m_Map.GetHeightAt(nx, ny, 1.0f, 0.0f), m_Map.GetHeightAt(nx, ny, 1.0f, 1.0f));
				}
			}

			bool ContinuesStairs(int32_t x, int32_t y, Direction from) const
			{
				const Cell& cell = m_Map.Get(x, y);
				int32_t px = x;
				int32_t py = y;
				TileMap::Step(from, px, py);
				const Cell& previous = m_Map.Get(px, py);
				return m_Map.IsInside(px, py) && previous.Slope == cell.Slope && previous.Climb > 0 && previous.Terrain == cell.Terrain && int32_t(previous.Level) + int32_t(previous.Climb) == int32_t(cell.Level);
			}

			void Tile(float height, uint32_t mesh, glm::vec3 scale, uint32_t turns, const glm::vec4& tint, Vec2 center)
			{
				const glm::mat4 transform = Translation({ center.x, height - 0.05f * scale.y, center.y }) * RotationY(float(turns % 4u) * HalfPi) * Scaling(scale);
				Add(mesh, transform, tint, height, false, height);
			}

			bool Foundation(int32_t x, int32_t y, float top, float own)
			{
				uint8_t mask = 0;
				float lowest = top;
				for (uint32_t side = 0; side < 4; side++)
				{
					const float edge = NeighborEdge(x, y, Direction(side), own);
					if (edge == NoFace || edge > top - 0.04f)
						continue;
					mask |= uint8_t(1u << side);
					lowest = std::min(lowest, edge);
				}
				if (mask == 0)
					return true;
				struct Piece
				{
					const std::optional<uint32_t>& Mesh;
					uint8_t Native;
				};
				const Piece pieces[] = {
					{ m_FoundationFront, SideNorth },
					{ m_FoundationCorner, SideNorth | SideEast },
					{ m_FoundationOpposite, SideNorth | SideSouth },
					{ m_FoundationThree, SideNorth | SideEast | SideWest },
					{ m_FoundationAll, SideNorth | SideEast | SideSouth | SideWest },
				};
				for (const Piece& piece : pieces)
				{
					if (!piece.Mesh)
						continue;
					for (uint32_t turns = 0; turns < 4; turns++)
					{
						if (RotateMask(piece.Native, turns) != mask)
							continue;
						const float depth = top - lowest + FoundationOverlap;
						const int32_t stack = std::max(1, int32_t(std::ceil(depth / 1.05f)));
						const float slice = depth / float(stack);
						for (int32_t i = 0; i < stack; i++)
						{
							const float bottom = top - slice * float(i + 1);
							const glm::mat4 transform = Translation({ float(x) + 0.5f, bottom, float(y) + 0.5f }) * RotationY(float(turns) * HalfPi) * Scaling({ Module, slice * 0.5f, Module });
							Add(*piece.Mesh, transform, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), lowest, false, bottom);
						}
						return true;
					}
				}
				return false;
			}

			void Floors()
			{
				const int32_t width = m_Map.GetWidth();
				const int32_t height = m_Map.GetHeight();
				std::vector<uint8_t> large(size_t(width) * size_t(height), 0);
				if (m_TileLarge)
				{
					for (int32_t y = 0; y + 1 < height; y += 2)
						for (int32_t x = 0; x + 1 < width; x += 2)
						{
							const Cell& first = m_Map.Get(x, y);
							if (first.Terrain != TerrainId::Flagstone || first.Slope != Direction::None)
								continue;
							bool uniform = true;
							for (int32_t dy = 0; dy < 2 && uniform; dy++)
								for (int32_t dx = 0; dx < 2 && uniform; dx++)
								{
									const Cell& other = m_Map.Get(x + dx, y + dy);
									uniform = other.Terrain == first.Terrain && other.Level == first.Level && other.Slope == Direction::None;
								}
							if (!uniform)
								continue;
							for (int32_t dy = 0; dy < 2; dy++)
								for (int32_t dx = 0; dx < 2; dx++)
									large[CellIndex(x + dx, y + dy)] = 1;
							const float top = CellHeight(x, y);
							Tile(top, *m_TileLarge, Uniform(Module), Hash(x, y, 11) % 4, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), { float(x) + 1.0f, float(y) + 1.0f });
						}
				}

				for (int32_t y = 0; y < height; y++)
					for (int32_t x = 0; x < width; x++)
					{
						const Cell& cell = m_Map.Get(x, y);
						const TerrainDef& def = GetTerrain(cell.Terrain);
						if (!def.Walkable)
							continue;
						uint8_t& flags = m_Scene.Cells[CellIndex(x, y)];
						const float top = CellHeight(x, y);
						const bool built = IsBuilt(cell.Terrain);
						const Vec2 center = { float(x) + 0.5f, float(y) + 0.5f };
						if (cell.Slope != Direction::None && cell.Climb > 0)
						{
							if (!m_Stairs || def.Style == TerrainStyle::Ground && cell.Terrain != TerrainId::Dirt && cell.Terrain != TerrainId::CaveFloor)
								continue;
							const glm::vec4 stairTint = cell.Terrain == TerrainId::CaveFloor ? glm::vec4(0.74f, 0.74f, 0.8f, 0.0f) : cell.Terrain == TerrainId::Dirt ? glm::vec4(1.05f, 0.95f, 0.82f, 0.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
							const float rise = float(cell.Climb) * HeightUnit;
							if (!ContinuesStairs(x, y, Direction((uint32_t(cell.Slope) + 2u) % 4u)))
							{
								int32_t endX = x;
								int32_t endY = y;
								int32_t depth = 1;
								while (true)
								{
									int32_t nextX = endX;
									int32_t nextY = endY;
									TileMap::Step(cell.Slope, nextX, nextY);
									if (!m_Map.IsInside(nextX, nextY) || !ContinuesStairs(nextX, nextY, Direction((uint32_t(cell.Slope) + 2u) % 4u)))
										break;
									endX = nextX;
									endY = nextY;
									depth++;
								}
								const Cell& last = m_Map.Get(endX, endY);
								const float runRise = float(int32_t(last.Level) + int32_t(last.Climb) - int32_t(cell.Level)) * HeightUnit;
								const Vec2 middle = (center + Vec2(float(endX) + 0.5f, float(endY) + 0.5f)) * 0.5f;
								const uint32_t turns = (6u - uint32_t(cell.Slope)) % 4u;
								const glm::mat4 transform = Translation({ middle.x, top, middle.y }) * RotationY(float(turns) * HalfPi) * Scaling({ Module, runRise * 0.25f, float(depth) * 0.25f }) * Translation({ 0.0f, 0.0f, -2.0f });
								Add(*m_Stairs, transform, stairTint, top, false, top);
							}
							AddBox({ .Mesh = MeshKind::Wedge, .Center = { center.x, top + rise * 0.5f - 0.02f, center.y }, .Half = { 0.5f, rise * 0.5f, 0.5f }, .Color = ToColor(0x4E4B48), .Yaw = SlopeYaw(cell.Slope), .Base = top, .Reach = 0.71f });
							flags |= CellTop;
							if (Foundation(x, y, top, top))
								flags |= CellSides;
							continue;
						}

						const uint32_t roll = Hash(x, y, 3) % 100u;
						const uint32_t turns = Hash(x, y, 5) % 4u;
						switch (cell.Terrain)
						{
						case TerrainId::Flagstone:
							if (!large[CellIndex(x, y)] && m_TileSmall)
							{
								uint32_t mesh = *m_TileSmall;
								if (roll < 8 && !m_TileBroken.empty())
									mesh = m_TileBroken[Hash(x, y, 7) % m_TileBroken.size()];
								else if (roll < 12 && !m_TileWeeds.empty())
									mesh = m_TileWeeds[Hash(x, y, 7) % m_TileWeeds.size()];
								Tile(top, mesh, Uniform(Module), turns, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), center);
							}
							if (large[CellIndex(x, y)] || m_TileSmall)
								flags |= CellTop;
							break;
						case TerrainId::Cobblestone:
							if (m_TileLarge)
							{
								Tile(top, *m_TileLarge, { 0.25f, Module, 0.25f }, turns, glm::vec4(0.82f, 0.82f, 0.84f, 0.0f), center);
								flags |= CellTop;
							}
							break;
						case TerrainId::Marble:
							if (m_TileSmall)
							{
								Tile(top, *m_TileSmall, Uniform(Module), turns, glm::vec4(1.0f, 1.0f, 1.0f, ((x + y) & 1) ? 0.5f : 0.38f), center);
								flags |= CellTop;
							}
							break;
						case TerrainId::WoodFloor:
							if (m_Wood)
							{
								Tile(top, *m_Wood, Uniform(Module), (Hash(x, y, 5) % 2u) * 2u, glm::vec4(0.92f, 0.92f, 0.92f, 0.0f), center);
								flags |= CellTop;
							}
							break;
						case TerrainId::Dirt:
						case TerrainId::CaveFloor:
							if (!m_Dirt.empty())
							{
								const uint32_t mesh = m_Dirt[Hash(x, y, 7) % m_Dirt.size()];
								const glm::vec4 tint = cell.Terrain == TerrainId::CaveFloor ? glm::vec4(0.74f, 0.74f, 0.8f, 0.0f) : glm::vec4(1.12f, 0.95f, 0.76f, 0.0f);
								Tile(top, mesh, Uniform(Module), turns, tint, center);
								flags |= CellTop;
							}
							break;
						default:
							break;
						}
						if (built && Foundation(x, y, top - FloorBottom, top))
							flags |= CellSides;
					}
			}

			void Rocks()
			{
				const int32_t width = m_Map.GetWidth();
				const int32_t height = m_Map.GetHeight();
				if (m_EdgeRocks.empty())
					return;
				for (int32_t y = 0; y < height; y++)
					for (int32_t x = 0; x < width; x++)
					{
						const Cell& cell = m_Map.Get(x, y);
						if (GetTerrain(cell.Terrain).Style != TerrainStyle::Solid)
							continue;
						m_Scene.Cells[CellIndex(x, y)] = CellTop | CellSides;
						Vec2 open;
						int32_t count = 0;
						for (int32_t dy = -1; dy <= 1; dy++)
							for (int32_t dx = -1; dx <= 1; dx++)
							{
								if (dx == 0 && dy == 0)
									continue;
								const TerrainDef& neighbor = GetTerrain(m_Map.GetTerrain(x + dx, y + dy));
								if (neighbor.Walkable || neighbor.Style == TerrainStyle::Liquid)
								{
									open += Vec2(float(dx), float(dy)) * (dx == 0 || dy == 0 ? 1.0f : 0.6f);
									count++;
								}
							}
						if (count == 0)
							continue;
						const float length = Length(open);
						const Vec2 back = length > 1e-3f ? open * (-0.18f / length) : Vec2();
						const float base = CellHeight(x, y);
						const uint32_t extra = count >= 5 ? 2u : 0u;
						for (uint32_t i = 0; i <= extra; i++)
						{
							const uint32_t salt = 21u + i * 10u;
							const WeightedMesh& rock = m_EdgeRocks[Hash(x, y, salt) % m_EdgeRocks.size()];
							const float scale = rock.Scale * (i == 0 ? 0.9f + 0.3f * Unit(x, y, salt + 2) : 0.75f + 0.2f * Unit(x, y, salt + 2));
							const Vec2 jitter = i == 0 ? Vec2() : Vec2(Unit(x, y, salt + 3) - 0.5f, Unit(x, y, salt + 4) - 0.5f) * 0.5f;
							const Vec2 center = Vec2(float(x) + 0.5f, float(y) + 0.5f) + back + jitter;
							const glm::mat4 transform = Translation({ center.x, base, center.y }) * RotationY(Unit(x, y, salt + 5) * TwoPi) * Scaling(Uniform(scale));
							Add(rock.Mesh, transform, glm::vec4(0.66f, 0.64f, 0.62f, 0.0f), base, true, base);
						}
					}
			}

			void WallPieces(size_t index, uint32_t baseMesh, std::optional<uint32_t> variant, uint32_t variantChance, float pieceLength, float height, float thickness, const glm::vec4& tint, float heightScale)
			{
				const WallSegment& wall = m_Level.Walls[index];
				const Vec2 span = wall.To - wall.From;
				const float length = Length(span);
				if (length < 1e-4f)
					return;
				const Vec2 direction = span / length;
				const float yaw = AngleOf(span);
				const int32_t pieces = std::max(1, int32_t(std::lround(length / pieceLength)));
				for (int32_t i = 0; i < pieces; i++)
				{
					Vec2 start = Lerp(wall.From, wall.To, float(i) / float(pieces));
					Vec2 end = Lerp(wall.From, wall.To, float(i + 1) / float(pieces));
					if (i == 0)
						start -= direction * (thickness * 0.5f);
					if (i == pieces - 1)
						end += direction * (thickness * 0.5f);
					const Vec2 center = (start + end) * 0.5f;
					uint32_t mesh = baseMesh;
					if (const std::optional<uint32_t> window = WindowAt(center, direction); window && m_WallWindow)
						mesh = *m_WallWindow;
					else if (variant && Hash(int32_t(std::floor(center.x * 4.0f)), int32_t(std::floor(center.y * 4.0f)), 31) % 100u < variantChance)
						mesh = *variant;
					const glm::mat4 transform = Translation({ center.x, wall.Base, center.y }) * RotationY(yaw) * Scaling({ Length(end - start) / 4.0f, height * heightScale, thickness });
					Add(mesh, transform, tint, wall.Base, true, wall.Base);
				}
				m_Scene.WallDrawn[index] = 1;
			}

			std::optional<uint32_t> WindowAt(Vec2 center, Vec2 direction)
			{
				for (size_t i = 0; i < m_Level.Props.size(); i++)
				{
					const PlacedProp& prop = m_Level.Props[i];
					if (prop.Id != PropId::Window)
						continue;
					const Vec2 offset = prop.Position - center;
					if (std::abs(Dot(offset, direction)) < 0.6f && std::abs(Dot(offset, Perpendicular(direction))) < 0.5f)
					{
						m_Scene.PropDrawn[i] = 1;
						return uint32_t(i);
					}
				}
				return std::nullopt;
			}

			void Walls()
			{
				for (size_t i = 0; i < m_Level.Walls.size(); i++)
				{
					const WallSegment& wall = m_Level.Walls[i];
					const WallDef& def = GetWall(wall.Style);
					switch (wall.Style)
					{
					case WallId::Stone:
						if (m_Wall)
							WallPieces(i, *m_Wall, m_WallCracked, 15, 2.0f, def.Height, def.Thickness, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.25f);
						break;
					case WallId::Library:
						if (m_Wall)
							WallPieces(i, *m_Wall, std::nullopt, 0, 2.0f, def.Height, def.Thickness, glm::vec4(1.1f, 1.0f, 0.86f, 0.0f), 0.25f);
						break;
					case WallId::Balustrade:
						if (m_Barrier)
							WallPieces(i, *m_Barrier, std::nullopt, 0, 2.0f, def.Height, def.Thickness / 0.7f, glm::vec4(1.0f, 1.0f, 1.0f, 0.3f), 1.0f / 1.4f);
						break;
					case WallId::Hedge:
						Hedge(i);
						break;
					default:
						break;
					}
				}
			}

			void Hedge(size_t index)
			{
				if (m_Hedge.empty())
					return;
				const WallSegment& wall = m_Level.Walls[index];
				const Vec2 span = wall.To - wall.From;
				const float length = Length(span);
				const int32_t count = std::max(1, int32_t(std::lround(length / 0.7f)));
				for (int32_t i = 0; i < count; i++)
				{
					const Vec2 position = Lerp(wall.From, wall.To, (float(i) + 0.5f) / float(count));
					const int32_t hx = int32_t(std::floor(position.x * 8.0f));
					const int32_t hy = int32_t(std::floor(position.y * 8.0f));
					const WeightedMesh& bush = m_Hedge[Hash(hx, hy, 41) % m_Hedge.size()];
					const float scale = bush.Scale * (0.9f + 0.2f * Unit(hx, hy, 43));
					const glm::mat4 transform = Translation({ position.x, wall.Base, position.y }) * RotationY(Unit(hx, hy, 45) * TwoPi) * Scaling(Uniform(scale));
					Add(bush.Mesh, transform, glm::vec4(0.82f, 0.86f, 0.82f, 0.0f), wall.Base, true, wall.Base);
				}
				m_Scene.WallDrawn[index] = 1;
			}

			void Props()
			{
				for (size_t i = 0; i < m_Level.Props.size(); i++)
				{
					const PlacedProp& prop = m_Level.Props[i];
					const ResolvedArt& art = m_PropArt[size_t(prop.Id)];
					if (art.Variants.empty())
						continue;
					const int32_t hx = int32_t(std::floor(prop.Position.x * 16.0f));
					const int32_t hy = int32_t(std::floor(prop.Position.y * 16.0f));
					const std::vector<ResolvedPart>& variant = art.Variants[Hash(hx, hy, uint32_t(i) * 7u + 51u) % art.Variants.size()];
					const float jitter = art.Natural ? 0.88f + 0.24f * Unit(hx, hy, 53) : 1.0f;
					const glm::mat4 place = Translation({ prop.Position.x, prop.Base, prop.Position.y }) * RotationY(prop.Yaw) * Scaling(Uniform(jitter));
					const bool tall = GetProp(prop.Id).Tall;
					for (const ResolvedPart& part : variant)
					{
						if (part.Glow > 0.0f)
						{
							const glm::vec3 flame = glm::vec3((place * part.Local)[3]);
							AddBox({ .Mesh = MeshKind::Sphere, .Center = flame, .Half = glm::vec3(part.Glow * jitter), .Color = ToColor(PropData::Flame), .Emission = 1.0f, .Base = prop.Base, .Reach = part.Glow });
							continue;
						}
						Add(part.Mesh, place * part.Local, part.Tint, prop.Base, tall, prop.Base);
					}
					if (art.KeepAbove >= 0.0f)
					{
						const Vec2 axis = FromAngle(prop.Yaw);
						for (const PropPart& part : GetProp(prop.Id).Parts)
						{
							if (part.Base < art.KeepAbove)
								continue;
							const Vec2 center = prop.Position + Rotate(part.Offset, axis);
							AddBox({ .Mesh = part.Mesh, .Center = { center.x, prop.Base + part.Base + part.Height * 0.5f, center.y }, .Half = { part.Half.x, part.Height * 0.5f, part.Half.y }, .Color = ToColor(part.Color), .Yaw = prop.Yaw + part.Yaw, .Emission = part.Emission, .Base = prop.Base, .Reach = Length(part.Half), .Cut = tall });
						}
					}
					m_Scene.PropDrawn[i] = 1;
				}
			}

			bool IsClear(Vec2 point, float radius) const
			{
				bool clear = true;
				const int32_t x = int32_t(std::floor(point.x));
				const int32_t y = int32_t(std::floor(point.y));
				m_Level.ForEachShape(x - 1, y - 1, x + 1, y + 1, [&](const Shape& shape)
				{
					if (clear && DistanceTo(shape, point) < radius)
						clear = false;
				});
				if (!clear)
					return false;
				for (const PlacedProp& prop : m_Level.Props)
					if (LengthSquared(prop.Position - point) < 0.36f && GetProp(prop.Id).Colliders.empty())
						return false;
				for (const Portal& portal : m_Level.Portals)
					if (LengthSquared(portal.Position - point) < 1.0f)
						return false;
				return true;
			}

			void Scatter(const std::vector<WeightedMesh>& meshes, int32_t x, int32_t y, uint32_t salt, float height, const glm::vec4& tint)
			{
				if (meshes.empty())
					return;
				const Vec2 point = { float(x) + 0.2f + 0.6f * Unit(x, y, salt + 1), float(y) + 0.2f + 0.6f * Unit(x, y, salt + 2) };
				if (!IsClear(point, 0.3f))
					return;
				const WeightedMesh& mesh = meshes[Hash(x, y, salt + 3) % meshes.size()];
				const float scale = mesh.Scale * (0.8f + 0.4f * Unit(x, y, salt + 4));
				const glm::mat4 transform = Translation({ point.x, height, point.y }) * RotationY(Unit(x, y, salt + 5) * TwoPi) * Scaling(Uniform(scale));
				Add(mesh.Mesh, transform, tint, height, false, height);
			}

			void Decor()
			{
				const int32_t width = m_Map.GetWidth();
				const int32_t height = m_Map.GetHeight();
				for (int32_t y = 0; y < height; y++)
					for (int32_t x = 0; x < width; x++)
					{
						const Cell& cell = m_Map.Get(x, y);
						const TerrainDef& def = GetTerrain(cell.Terrain);
						if (!def.Walkable || cell.Slope != Direction::None)
							continue;
						const float top = CellHeight(x, y);
						const uint32_t roll = Hash(x, y, 61) % 1000u;
						switch (cell.Terrain)
						{
						case TerrainId::Grass:
							if (roll < 330)
								Scatter(m_Grass, x, y, 70, top, glm::vec4(0.85f, 0.85f, 0.85f, 0.0f));
							else if (roll < 380)
								Scatter(m_SmallBushes, x, y, 80, top, glm::vec4(0.85f, 0.88f, 0.85f, 0.0f));
							else if (roll < 400)
								Scatter(m_Pebbles, x, y, 90, top, glm::vec4(0.9f, 0.9f, 0.9f, 0.0f));
							break;
						case TerrainId::Dirt:
						case TerrainId::Sand:
							if (roll < 50)
								Scatter(m_Pebbles, x, y, 90, top, glm::vec4(0.9f, 0.9f, 0.9f, 0.0f));
							break;
						case TerrainId::Ash:
							if (roll < 40)
								Scatter(m_Pebbles, x, y, 90, top, glm::vec4(0.6f, 0.58f, 0.56f, 0.0f));
							else if (roll < 60)
								Scatter(m_Saplings, x, y, 100, top, glm::vec4(0.45f, 0.4f, 0.38f, 0.0f));
							break;
						case TerrainId::CaveFloor:
							if (roll < 70)
								Scatter(m_Pebbles, x, y, 90, top, glm::vec4(0.75f, 0.75f, 0.8f, 0.0f));
							break;
						default:
							break;
						}
						if (IsBuilt(cell.Terrain) || m_CliffRocks.empty())
							continue;
						for (uint32_t side = 0; side < 4; side++)
						{
							const float edge = NeighborEdge(x, y, Direction(side), top);
							if (edge == NoFace || top - edge < 0.4f || Hash(x, y, 120 + side) % 100u >= 70u)
								continue;
							int32_t nx = x;
							int32_t ny = y;
							TileMap::Step(Direction(side), nx, ny);
							const Vec2 outward = Vec2(float(nx - x), float(ny - y));
							const Vec2 along = Perpendicular(outward);
							const Vec2 point = Vec2(float(x) + 0.5f, float(y) + 0.5f) + outward * 0.5f + along * ((Unit(x, y, 130 + side) - 0.5f) * 0.6f);
							const WeightedMesh& rock = m_CliffRocks[Hash(x, y, 140 + side) % m_CliffRocks.size()];
							const float scale = rock.Scale * (0.85f + 0.3f * Unit(x, y, 150 + side));
							const float base = std::max(edge, top - 1.2f);
							const glm::mat4 transform = Translation({ point.x, base, point.y }) * RotationY(Unit(x, y, 160 + side) * TwoPi) * Scaling(Uniform(scale));
							Add(rock.Mesh, transform, glm::vec4(0.82f, 0.8f, 0.78f, 0.0f), base, false, base);
						}
					}
			}

			const Level& m_Level;
			const TileMap& m_Map;
			const WorldRenderer& m_Renderer;
			const ModelLibrary& m_Models;
			LevelScene& m_Scene;
			int32_t m_ChunksX = 0;
			int32_t m_ChunksY = 0;
			std::vector<std::string> m_Missing;
			std::vector<ResolvedArt> m_PropArt;
			std::optional<uint32_t> m_TileSmall;
			std::optional<uint32_t> m_TileLarge;
			std::vector<uint32_t> m_TileBroken;
			std::vector<uint32_t> m_TileWeeds;
			std::optional<uint32_t> m_Wood;
			std::vector<uint32_t> m_Dirt;
			std::optional<uint32_t> m_FoundationFront;
			std::optional<uint32_t> m_FoundationCorner;
			std::optional<uint32_t> m_FoundationOpposite;
			std::optional<uint32_t> m_FoundationThree;
			std::optional<uint32_t> m_FoundationAll;
			std::optional<uint32_t> m_Stairs;
			std::optional<uint32_t> m_Wall;
			std::optional<uint32_t> m_WallCracked;
			std::optional<uint32_t> m_WallWindow;
			std::optional<uint32_t> m_Barrier;
			std::vector<WeightedMesh> m_Hedge;
			std::vector<WeightedMesh> m_EdgeRocks;
			std::vector<WeightedMesh> m_CliffRocks;
			std::vector<WeightedMesh> m_Pebbles;
			std::vector<WeightedMesh> m_Grass;
			std::vector<WeightedMesh> m_SmallBushes;
			std::vector<WeightedMesh> m_Saplings;
		};
	}

	LevelScene BuildLevelScene(const Level& level, const WorldRenderer& renderer)
	{
		LevelScene scene;
		SceneBuilder(level, renderer, scene).Build();
		return scene;
	}
}
