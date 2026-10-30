#include "LevelArt.h"
#include "Rendering/LevelRenderer.h"
#include "Rendering/Models.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Core/Timer.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

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
		constexpr float ChainPitch = 0.11f;
		constexpr uint32_t RootChance = 16;
		constexpr float LedgeReach = 0.4f;
		constexpr float StepDepth = 0.45f;
		constexpr float StepLip = 0.07f;
		constexpr float StepSkirt = 0.3f;
		constexpr float FenceInset = 0.15f;
		constexpr float FenceRailOffset = 0.06f;
		constexpr float FenceRailBase = 0.4f;
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
			int8_t Fire = -1;
			float FireScale = 1.0f;
		};

		struct PropArt
		{
			std::vector<std::vector<ArtPart>> Variants;
			bool Natural = false;
			float KeepAbove = -1.0f;
		};

		constexpr glm::vec3 FlameColor{ 1.0f, 0.62f, 0.25f };
		constexpr float ObsidianScale = 1.5f;
		constexpr float ObsidianWaterline = 0.45f;

		glm::vec3 Uniform(float scale)
		{
			return glm::vec3(scale);
		}

		ArtPart Flame(glm::vec3 offset, float radius)
		{
			return { .Offset = offset, .Glow = radius };
		}

		ArtPart Fire(glm::vec3 offset, FireKind kind, float scale = 1.0f)
		{
			return { .Offset = offset, .Fire = int8_t(kind), .FireScale = scale };
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
				return { { { { .Mesh = "Market/Barrel_A" } }, { { .Mesh = "Market/Barrel_B" } } } };
			case PropId::Crate:
				return { { { { .Mesh = "Market/Crate_A" } } } };
			case PropId::CrateStack:
				return { { { { .Mesh = "Market/CrateStack" } } } };
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
				return { { { { .Mesh = "Market/Bookshelf" } } } };
			case PropId::DoubleBookshelf:
				return { { {
					{ .Mesh = "Market/Bookshelf", .Offset = { 0.0f, 0.0f, -0.24f } },
					{ .Mesh = "Market/Bookshelf", .Offset = { 0.0f, 0.0f, 0.24f }, .Yaw = Pi },
				} } };
			case PropId::ReadingTable:
				return { { { { .Mesh = "Market/ReadingTable" }, Fire({ 0.75f, 0.965f, 0.1f }, FireKind::Candle) } } };
			case PropId::RoundTable:
				return { { { { .Mesh = "table_medium", .Scale = Uniform(0.56f) } }, { { .Mesh = "table_medium_tablecloth", .Scale = Uniform(0.56f) } } } };
			case PropId::Chair:
				return { { { { .Mesh = "chair", .Scale = Uniform(0.72f) } } } };
			case PropId::Armchair:
				return { { { { .Mesh = "chair", .Scale = Uniform(0.86f) } } } };
			case PropId::Desk:
				return { { { { .Mesh = "table_long_decorated_A", .Scale = Uniform(0.62f), .Yaw = HalfPi } } } };
			case PropId::OakTree:
				return { { { { .Mesh = "Nature/Oak_A" } }, { { .Mesh = "Nature/Oak_B" } } }, true };
			case PropId::PineTree:
				return { { { { .Mesh = "Nature/Pine_A" } }, { { .Mesh = "Nature/Pine_B" } } }, true };
			case PropId::Bush:
				return { { { { .Mesh = "Nature/Bush_A" } }, { { .Mesh = "Nature/Bush_B" } }, { { .Mesh = "Nature/Bush_C" } } }, true };
			case PropId::Boulder:
				return { { { { .Mesh = "Nature/Boulder_A" } }, { { .Mesh = "Nature/Boulder_B" } }, { { .Mesh = "Nature/Boulder_C" } } }, true };
			case PropId::MossBoulder:
				return { { { { .Mesh = "Nature/MossBoulder_A" } }, { { .Mesh = "Nature/MossBoulder_B" } } }, true };
			case PropId::Rock:
				return { { { { .Mesh = "Nature/Rock_A" } }, { { .Mesh = "Nature/Rock_B" } }, { { .Mesh = "Nature/Rock_C" } }, { { .Mesh = "Nature/Rock_D" } } }, true };
			case PropId::Flowers:
				return { { { { .Mesh = "Nature/Flowers_A" } }, { { .Mesh = "Nature/Flowers_B" } }, { { .Mesh = "Nature/Flowers_C" } } }, true };
			case PropId::Mushrooms:
				return { { { { .Mesh = "Nature/Mushrooms_A" } }, { { .Mesh = "Nature/Mushrooms_B" } } }, true };
			case PropId::FallenLog:
				return { { { { .Mesh = "Tree_Bare_2_A_Color1", .Offset = { 0.0f, 0.18f, 0.0f }, .Scale = Uniform(0.58f), .Yaw = HalfPi, .Roll = HalfPi, .Pivot = { 0.0f, 1.7f, 0.0f } } } }, true };
			case PropId::Rubble:
				return { { {
					{ .Mesh = "Rock_3_A_Color1", .Scale = Uniform(0.3f) },
					{ .Mesh = "Rock_3_B_Color1", .Offset = { 0.25f, 0.0f, 0.15f }, .Scale = Uniform(0.24f), .Yaw = 1.1f },
					{ .Mesh = "Rock_2_B_Color1", .Offset = { -0.2f, 0.0f, 0.2f }, .Scale = Uniform(0.38f), .Yaw = 0.7f },
				} }, true };
			case PropId::Banner:
				return { { { { .Mesh = "Market/Banner" } } } };
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
				return { { { { .Mesh = "Market/StashChest" } } } };
			case PropId::Door:
				return { { { { .Mesh = "wall_doorway", .Scale = { 0.5f, 0.7f, 0.45f } } } } };
			case PropId::Fountain:
				return { { { { .Mesh = "Fountain" } } } };
			case PropId::LampPost:
				return { { { { .Mesh = "HubProps/LampPost" }, Fire({ 0.0f, 2.55f, 0.0f }, FireKind::Lantern) } } };
			case PropId::Bench:
				return { { { { .Mesh = "HubProps/Bench" } } } };
			case PropId::Signpost:
				return { { { { .Mesh = "HubProps/Signpost" } } } };
			case PropId::NoticeBoard:
				return { { { { .Mesh = "HubProps/NoticeBoard" } } } };
			case PropId::CandleStand:
				return { { {
					{ .Mesh = "HubProps/CandleStand" },
					Fire({ 0.0f, 1.275f, 0.0f }, FireKind::Candle),
					Fire({ -0.075f, 1.275f, 0.0f }, FireKind::Candle),
					Fire({ 0.075f, 1.295f, 0.0f }, FireKind::Candle),
				} } };
			case PropId::Brazier:
				return { { { { .Mesh = "HubProps/Brazier" }, Fire({ 0.0f, 0.78f, 0.0f }, FireKind::Campfire, 0.55f) } } };
			case PropId::FireBowl:
				return { { { { .Mesh = "HubProps/FireBowl" }, Fire({ 0.0f, 0.98f, 0.0f }, FireKind::Campfire, 0.5f) } } };
			case PropId::OfferingBowl:
				return { { { { .Mesh = "HubProps/OfferingBowl" } } } };
			case PropId::Forge:
				return { { {
					{ .Mesh = "Stations/Forge" },
					Fire({ -0.25f, 1.22f, -0.05f }, FireKind::Campfire, 0.6f),
					Fire({ 0.0f, 0.24f, -0.3f }, FireKind::Campfire, 0.3f),
				} } };
			case PropId::Anvil:
				return { { { { .Mesh = "Stations/Anvil" } } } };
			case PropId::GemAltar:
				return { { { { .Mesh = "Stations/GemAltar" } } } };
			case PropId::MissionBoard:
				return { { { { .Mesh = "Stations/MissionBoard" }, Fire({ 0.0f, 2.22f, -0.08f }, FireKind::Lantern, 0.6f) } } };
			case PropId::Lectern:
				return { { { { .Mesh = "Stations/Lectern" } } } };
			case PropId::Crystal:
				return { { { { .Mesh = "Arcana/Crystal_A" } }, { { .Mesh = "Arcana/Crystal_B" } }, { { .Mesh = "Arcana/Crystal_C" } } }, true };
			case PropId::Globe:
				return { { { { .Mesh = "Arcana/Armillary" } } } };
			case PropId::Telescope:
				return { { { { .Mesh = "Arcana/Telescope" } } } };
			case PropId::Statue:
				return { { { { .Mesh = "Arcana/Statue" } } } };
			case PropId::CalderaGate:
				return { { { { .Mesh = "Emberhold/CalderaGate" } } } };
			case PropId::MageTower:
				return { { { { .Mesh = "Emberhold/MageTower" }, Fire({ 0.62f, 2.05f, -1.62f }, FireKind::Lantern, 0.7f) } } };
			case PropId::PierPost:
				return { { { { .Mesh = "Emberhold/PierPost" } } } };
			case PropId::ChainAnchor:
				return { { { { .Mesh = "Emberhold/ChainAnchor" } } } };
			case PropId::ChessTable:
				return { { { { .Mesh = "Chess/Table" } } } };
			case PropId::IcePillar:
				return { { { { .Mesh = "Glacier/IcePillar_A" } }, { { .Mesh = "Glacier/IcePillar_B" } }, { { .Mesh = "Glacier/IcePillar_C" } } } };
			case PropId::SnowRock:
				return { { { { .Mesh = "Glacier/SnowRock_A" } }, { { .Mesh = "Glacier/SnowRock_B" } }, { { .Mesh = "Glacier/SnowRock_C" } } }, true };
			case PropId::RuinColumn:
				return { { { { .Mesh = "Ruins/Column_A" } } } };
			case PropId::RuinBrokenColumn:
				return { { { { .Mesh = "Ruins/Column_B" } } } };
			case PropId::RuinStump:
				return { { { { .Mesh = "Ruins/Column_C" } } } };
			case PropId::RuinDrum:
				return { { { { .Mesh = "Ruins/Drum_A" } } } };
			case PropId::RuinCapital:
				return { { { { .Mesh = "Ruins/Capital_A" } } } };
			case PropId::RuinArch:
				return { { { { .Mesh = "Ruins/Arch_A" } } } };
			case PropId::RuinWall:
				return { { { { .Mesh = "Ruins/Wall_A" } } } };
			case PropId::RuinWallShort:
				return { { { { .Mesh = "Ruins/Wall_B" } } } };
			case PropId::RuinPedestal:
				return { { { { .Mesh = "Ruins/Pedestal_A" } } } };
			case PropId::RuinRubble:
				return { { { { .Mesh = "Ruins/Rubble_A" } }, { { .Mesh = "Ruins/Rubble_B" } } }, true };
			case PropId::StormSpire:
				return { { { { .Mesh = "Summit/StormSpire_A" } } } };
			case PropId::Cairn:
				return { { { { .Mesh = "Summit/Cairn_A" } }, { { .Mesh = "Summit/Cairn_B" } } } };
			case PropId::StandingStone:
				return { { { { .Mesh = "Summit/StandingStone_A" } }, { { .Mesh = "Summit/StandingStone_B" } }, { { .Mesh = "Summit/StandingStone_C" } } } };
			case PropId::GlyphStone:
				return { { { { .Mesh = "Summit/GlyphStone_A" } }, { { .Mesh = "Summit/GlyphStone_B" } } } };
			case PropId::PrayerPole:
				return { { { { .Mesh = "Summit/PrayerPole_A" } } } };
			case PropId::GraniteBoulder:
				return { { { { .Mesh = "Summit/Boulder_A" } }, { { .Mesh = "Summit/Boulder_B" } } }, true };
			case PropId::StoneGate:
				return { { { { .Mesh = "Summit/StoneGate_A" } } } };
			case PropId::ObsidianCluster:
			{
				const glm::vec3 sink(0.0f, LavaSurface - ObsidianWaterline * ObsidianScale, 0.0f);
				return { {
					{ { .Mesh = "ObsidianCluster/ObsidianCluster_A", .Offset = sink, .Scale = Uniform(ObsidianScale) } },
					{ { .Mesh = "ObsidianCluster/ObsidianCluster_B", .Offset = sink, .Scale = Uniform(ObsidianScale) } },
					{ { .Mesh = "ObsidianCluster/ObsidianCluster_C", .Offset = sink, .Scale = Uniform(ObsidianScale) } },
				}, true };
			}
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
			int8_t Fire = -1;
			float FireScale = 1.0f;
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
			std::vector<uint32_t> Extras;
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

		struct PavingStyle
		{
			TerrainId Terrain = TerrainId::Cobblestone;
			float Pitch = 0.3f;
			float Stretch = 1.4f;
			float Jitter = 0.3f;
			float Gap = 0.035f;
			float Corner = 0.3f;
			float Shoulder = 0.6f;
			float Dome = 0.02f;
			float Rim = 0.02f;
			float Bed = 0.06f;
			float Tilt = 0.05f;
			float Lift = 0.01f;
			float Texture = 1.0f;
			glm::vec3 Light{ 1.0f };
			glm::vec3 Dark{ 0.7f };
			glm::vec3 Joint{ 0.8f };
			uint32_t Salt = 0;
		};

		constexpr PavingStyle PavingStyles[] = {
			{ .Terrain = TerrainId::Cobblestone, .Pitch = 0.31f, .Stretch = 1.4f, .Jitter = 0.38f, .Gap = 0.036f, .Corner = 0.3f, .Shoulder = 0.58f, .Dome = 0.024f, .Rim = 0.024f, .Bed = 0.06f, .Tilt = 0.05f, .Lift = 0.012f, .Texture = 1.0f,
				.Light = { 1.0f, 0.97f, 0.93f }, .Dark = { 0.55f, 0.53f, 0.5f }, .Joint = { 0.85f, 0.8f, 0.75f }, .Salt = 0x5E77u },
			{ .Terrain = TerrainId::Flagstone, .Pitch = 0.64f, .Stretch = 1.3f, .Jitter = 0.42f, .Gap = 0.024f, .Corner = 0.08f, .Shoulder = 0.9f, .Dome = 0.004f, .Rim = 0.01f, .Bed = 0.045f, .Tilt = 0.014f, .Lift = 0.006f, .Texture = 0.8f,
				.Light = { 1.0f, 0.97f, 0.92f }, .Dark = { 0.64f, 0.61f, 0.57f }, .Joint = { 1.0f, 0.96f, 0.88f }, .Salt = 0xF1A6u },
		};

		using Outline = std::vector<Vec2>;

		void KeepBelow(Outline& outline, Vec2 normal, float offset)
		{
			Outline kept;
			for (size_t i = 0; i < outline.size(); i++)
			{
				const Vec2 a = outline[i];
				const Vec2 b = outline[(i + 1) % outline.size()];
				const float da = Dot(a, normal) - offset;
				const float db = Dot(b, normal) - offset;
				if (da <= 0.0f)
					kept.push_back(a);
				if ((da <= 0.0f) != (db <= 0.0f))
					kept.push_back(a + (b - a) * (da / (da - db)));
			}
			outline.swap(kept);
		}

		float SignedArea(const Outline& outline)
		{
			float area = 0.0f;
			for (size_t i = 0; i < outline.size(); i++)
			{
				const Vec2 a = outline[i];
				const Vec2 b = outline[(i + 1) % outline.size()];
				area += a.x * b.y - b.x * a.y;
			}
			return 0.5f * area;
		}

		void Inset(Outline& outline, float distance)
		{
			const Outline original = outline;
			for (size_t i = 0; i < original.size() && outline.size() >= 3; i++)
			{
				const Vec2 a = original[i];
				const Vec2 edge = original[(i + 1) % original.size()] - a;
				const float length = Length(edge);
				if (length < 1e-5f)
					continue;
				const Vec2 outward = Vec2(edge.y, -edge.x) / length;
				KeepBelow(outline, outward, Dot(a, outward) - distance);
			}
		}

		Outline Cleaned(const Outline& outline)
		{
			Outline cleaned;
			for (const Vec2 point : outline)
				if (cleaned.empty() || LengthSquared(point - cleaned.back()) > 1e-5f)
					cleaned.push_back(point);
			while (cleaned.size() > 1 && LengthSquared(cleaned.front() - cleaned.back()) <= 1e-5f)
				cleaned.pop_back();
			return cleaned;
		}

		Outline Rounded(const Outline& outline, float amount)
		{
			Outline rounded;
			for (size_t i = 0; i < outline.size(); i++)
			{
				const Vec2 corner = outline[i];
				rounded.push_back(Lerp(corner, outline[(i + outline.size() - 1) % outline.size()], amount));
				rounded.push_back(Lerp(corner, outline[(i + 1) % outline.size()], amount));
			}
			return rounded;
		}

		uint32_t PackTint(const glm::vec3& color)
		{
			const glm::uvec3 c(glm::clamp(color, 0.0f, 1.0f) * 255.0f + 0.5f);
			return c.r | (c.g << 8) | (c.b << 16) | 0xFF000000u;
		}

		uint32_t PackTangent(const glm::vec3& tangent, float sign)
		{
			const auto snorm = [](float value) { return uint32_t(int32_t(std::lround(std::clamp(value, -1.0f, 1.0f) * 511.0f))) & 0x3FFu; };
			return snorm(tangent.x) | (snorm(tangent.y) << 10) | (snorm(tangent.z) << 20) | (sign < 0.0f ? 1u << 30 : 0u);
		}

		struct PavingBatch
		{
			std::vector<ModelVertex> Vertices;
			std::vector<uint32_t> Indices;
			glm::vec3 Min{ 1e9f };
			glm::vec3 Max{ -1e9f };

			void Append(const std::vector<glm::vec3>& points, const std::vector<glm::vec2>& uvs, const std::vector<glm::vec3>& colors, const std::vector<uint32_t>& triangles, const glm::vec3& dPdu, const glm::vec3& dPdv)
			{
				std::vector<glm::vec3> normals(points.size(), glm::vec3(0.0f));
				for (size_t t = 0; t + 2 < triangles.size(); t += 3)
				{
					const glm::vec3 face = glm::cross(points[triangles[t + 1]] - points[triangles[t]], points[triangles[t + 2]] - points[triangles[t]]);
					for (size_t k = 0; k < 3; k++)
						normals[triangles[t + k]] += face;
				}
				const uint32_t first = uint32_t(Vertices.size());
				for (size_t i = 0; i < points.size(); i++)
				{
					const glm::vec3 normal = glm::length(normals[i]) > 1e-12f ? glm::normalize(normals[i]) : glm::vec3(0.0f, 1.0f, 0.0f);
					glm::vec3 tangent = dPdu - normal * glm::dot(normal, dPdu);
					tangent = glm::length(tangent) > 1e-6f ? glm::normalize(tangent) : glm::vec3(1.0f, 0.0f, 0.0f);
					ModelVertex vertex;
					vertex.Position = points[i];
					vertex.U = uvs[i].x;
					vertex.V = uvs[i].y;
					vertex.Normal = normal;
					vertex.Weights = 255;
					vertex.Tangent = PackTangent(tangent, glm::dot(glm::cross(normal, tangent), -dPdv) >= 0.0f ? 1.0f : -1.0f);
					vertex.Color = PackTint(colors[i]);
					Vertices.push_back(vertex);
					Min = glm::min(Min, points[i]);
					Max = glm::max(Max, points[i]);
				}
				for (const uint32_t index : triangles)
					Indices.push_back(first + index);
			}
		};

		void Orient(std::vector<uint32_t>& triangles, const std::vector<glm::vec3>& points, uint32_t a, uint32_t b, uint32_t c, const glm::vec3& facing)
		{
			if (glm::dot(glm::cross(points[b] - points[a], points[c] - points[a]), facing) < 0.0f)
				std::swap(b, c);
			triangles.insert(triangles.end(), { a, b, c });
		}

		void LayStone(PavingBatch& batch, const Outline& outline, const PavingStyle& style, float top, int32_t row, int32_t column, Vec2 grade = {})
		{
			const auto random = [&](uint32_t salt) { return Unit(column, row, style.Salt + salt); };
			const uint32_t count = uint32_t(outline.size());
			Vec2 center;
			for (const Vec2 point : outline)
				center += point;
			center = center / float(count);
			float sag = 0.0f;
			for (const Vec2 point : outline)
				sag = std::min(sag, Dot(grade, point - center));
			const Vec2 slope = Vec2(random(11) - 0.5f, random(12) - 0.5f) * (2.0f * style.Tilt) + grade;
			const float base = top + style.Lift * (random(13) - 0.3f);
			const auto surface = [&](Vec2 point) { return base + Dot(slope, point - center); };
			const Vec2 axis = FromAngle(random(14) * TwoPi);
			const Vec2 shift(random(15) * 7.0f, random(16) * 7.0f);
			const auto mapping = [&](Vec2 point)
			{
				const Vec2 uv = Rotate(point, axis) * style.Texture + shift;
				return glm::vec2(uv.x, uv.y);
			};
			glm::vec3 tint = glm::mix(style.Dark, style.Light, std::pow(random(17), 0.7f));
			const float hue = random(18);
			if (hue < 0.3f)
				tint *= glm::vec3(1.04f, 0.97f, 0.86f);
			else if (hue > 0.84f)
				tint *= glm::vec3(0.9f, 0.95f, 1.0f);
			const float dome = style.Dome * (0.45f + 1.1f * random(19));
			const float shoulder = style.Shoulder * (0.9f + 0.2f * random(20));

			std::vector<glm::vec3> points;
			std::vector<glm::vec2> uvs;
			std::vector<glm::vec3> colors;
			points.push_back({ center.x, surface(center) + dome, center.y });
			uvs.push_back(mapping(center));
			colors.push_back(tint);
			for (const Vec2 point : outline)
			{
				const Vec2 inner = Lerp(center, point, shoulder);
				points.push_back({ inner.x, surface(inner) + dome * (1.0f - shoulder * shoulder), inner.y });
				uvs.push_back(mapping(inner));
				colors.push_back(tint);
			}
			for (const Vec2 point : outline)
			{
				points.push_back({ point.x, surface(point) - style.Rim, point.y });
				uvs.push_back(mapping(point));
				colors.push_back(tint * 0.86f);
			}
			const float foot = top - style.Bed + sag;
			for (const Vec2 point : outline)
			{
				const float drop = surface(point) - style.Rim - foot;
				points.push_back({ point.x, foot, point.y });
				uvs.push_back(mapping(point + Normalize(point - center) * drop));
				colors.push_back(tint * 0.45f);
			}

			std::vector<uint32_t> triangles;
			const glm::vec3 up(0.0f, 1.0f, 0.0f);
			for (uint32_t i = 0; i < count; i++)
			{
				const uint32_t j = (i + 1) % count;
				const Vec2 middle = (outline[i] + outline[j]) * 0.5f - center;
				const glm::vec3 outward(middle.x, 0.0f, middle.y);
				Orient(triangles, points, 0, 1 + i, 1 + j, up);
				Orient(triangles, points, 1 + i, 1 + count + i, 1 + count + j, up);
				Orient(triangles, points, 1 + i, 1 + count + j, 1 + j, up);
				Orient(triangles, points, 1 + count + i, 1 + 2 * count + i, 1 + 2 * count + j, outward);
				Orient(triangles, points, 1 + count + i, 1 + 2 * count + j, 1 + count + j, outward);
			}
			const glm::vec3 dPdu = glm::vec3(axis.x, 0.0f, -axis.y) / style.Texture;
			const glm::vec3 dPdv = glm::vec3(axis.y, 0.0f, axis.x) / style.Texture;
			batch.Append(points, uvs, colors, triangles, dPdu, dPdv);
		}

		void LayJoint(PavingBatch& batch, int32_t x, int32_t y, float height, const PavingStyle& style)
		{
			const std::vector<glm::vec3> points = { { float(x), height, float(y) }, { float(x + 1), height, float(y) }, { float(x + 1), height, float(y + 1) }, { float(x), height, float(y + 1) } };
			const std::vector<glm::vec2> uvs = { { float(x), float(y) }, { float(x + 1), float(y) }, { float(x + 1), float(y + 1) }, { float(x), float(y + 1) } };
			const std::vector<glm::vec3> colors(4, style.Joint * 0.8f);
			std::vector<uint32_t> triangles;
			const glm::vec3 up(0.0f, 1.0f, 0.0f);
			Orient(triangles, points, 0, 1, 2, up);
			Orient(triangles, points, 0, 2, 3, up);
			batch.Append(points, uvs, colors, triangles, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		}

		float Ease(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float ValueNoise(Vec2 point, uint32_t salt)
		{
			const float fx = std::floor(point.x);
			const float fy = std::floor(point.y);
			const int32_t x = int32_t(fx);
			const int32_t y = int32_t(fy);
			const float u = Ease(point.x - fx);
			const float v = Ease(point.y - fy);
			return std::lerp(std::lerp(Unit(x, y, salt), Unit(x + 1, y, salt), u), std::lerp(Unit(x, y + 1, salt), Unit(x + 1, y + 1, salt), u), v);
		}

		enum SplatLayer : uint8_t
		{
			SplatGrass,
			SplatDirt,
			SplatRock,
			SplatSand,
		};

		enum GroundKind : uint8_t
		{
			GroundNone,
			GroundOpen,
			GroundPaved,
			GroundHill,
			GroundWater,
			GroundRim,
		};

		struct GroundCell
		{
			uint8_t Kind = GroundNone;
			uint8_t Layer = SplatGrass;
			float Height = 0.0f;
			float Shade = 1.0f;
		};

		constexpr int32_t TerrainMargin = 12;
		constexpr int32_t TerrainSteps = 4;
		constexpr float PavingBlend = 0.7f;
		constexpr float SkirtDrop = 0.35f;
		constexpr float WaterDrop = 0.15f;
		constexpr float FarAway = 1e20f;

		float Fbm(Vec2 point, uint32_t salt, int32_t octaves)
		{
			point = Vec2(point.x * 0.8f - point.y * 0.6f, point.x * 0.6f + point.y * 0.8f);
			float sum = 0.0f;
			float total = 0.0f;
			float amplitude = 1.0f;
			for (int32_t octave = 0; octave < octaves; octave++)
			{
				sum += amplitude * ValueNoise(point, salt + uint32_t(octave) * 7919u);
				total += amplitude;
				point = Vec2(point.x * 1.6f - point.y * 1.2f + 17.3f, point.x * 1.2f + point.y * 1.6f - 9.1f);
				amplitude *= 0.5f;
			}
			return sum / total;
		}

		float ValueNoise3(const glm::vec3& point, uint32_t salt)
		{
			const glm::vec3 cell = glm::floor(point);
			const int32_t x = int32_t(cell.x);
			const int32_t y = int32_t(cell.y);
			const int32_t z = int32_t(cell.z);
			const float u = Ease(point.x - cell.x);
			const float v = Ease(point.y - cell.y);
			const float w = Ease(point.z - cell.z);
			const auto at = [&](int32_t dx, int32_t dy, int32_t dz) { return Unit(x + dx, z + dz, salt + uint32_t(y + dy) * 2654435761u); };
			const float below = std::lerp(std::lerp(at(0, 0, 0), at(1, 0, 0), u), std::lerp(at(0, 0, 1), at(1, 0, 1), u), w);
			const float above = std::lerp(std::lerp(at(0, 1, 0), at(1, 1, 0), u), std::lerp(at(0, 1, 1), at(1, 1, 1), u), w);
			return std::lerp(below, above, v);
		}

		struct Relief
		{
			float Offset = 0.0f;
			float Cavity = 0.0f;
		};

		float Plates(const glm::vec3& point, float& border)
		{
			const glm::vec3 scaled = point * glm::vec3(1.7f, 2.4f, 1.7f);
			const glm::ivec3 cell(glm::floor(scaled));
			float nearest = 1e9f;
			float second = 1e9f;
			glm::ivec3 owner(0);
			glm::vec3 feature(0.0f);
			for (int32_t dz = -1; dz <= 1; dz++)
				for (int32_t dy = -1; dy <= 1; dy++)
					for (int32_t dx = -1; dx <= 1; dx++)
					{
						const glm::ivec3 id = cell + glm::ivec3(dx, dy, dz);
						const uint32_t h = Hash(id.x, id.z, 851u + uint32_t(id.y) * 2654435761u);
						const glm::vec3 spot = glm::vec3(id) + glm::vec3(float(h & 1023u), float((h >> 10) & 1023u), float((h >> 20) & 1023u)) / 1023.0f;
						const float distance = glm::dot(scaled - spot, scaled - spot);
						if (distance < nearest)
						{
							second = nearest;
							nearest = distance;
							owner = id;
							feature = spot;
						}
						else if (distance < second)
							second = distance;
					}
			border = std::sqrt(second) - std::sqrt(nearest);
			const uint32_t h = Hash(owner.x, owner.z, 857u + uint32_t(owner.y) * 2654435761u);
			const glm::vec3 tilt = glm::vec3(float(h & 1023u), float((h >> 10) & 1023u), float((h >> 20) & 1023u)) / 1023.0f - 0.5f;
			return 0.11f * (Unit(owner.x, owner.z, 859u + uint32_t(owner.y) * 2654435761u) - 0.5f) + 0.09f * glm::dot(tilt, scaled - feature);
		}

		Relief RockRelief(const glm::vec3& point)
		{
			float border = 0.0f;
			const float plate = Plates(point, border);
			const float bulge = 2.0f * ValueNoise3(point * glm::vec3(0.5f, 0.75f, 0.5f), 801u) - 1.0f;
			const float lumps = 2.0f * ValueNoise3(point * 1.7f + glm::vec3(3.1f, 0.0f, 7.7f), 809u) - 1.0f;
			const float layer = point.y * 1.9f + 1.3f * ValueNoise3(point * 0.3f, 821u);
			const float shelf = Ease((layer - std::floor(layer) - 0.55f) / 0.35f);
			const float ridge = 1.0f - std::abs(2.0f * ValueNoise3(glm::vec3(point.x * 2.2f, point.y * 0.45f, point.z * 2.2f), 827u) - 1.0f);
			const float crevice = Ease((ridge - 0.86f) / 0.1f);
			const float seam = 1.0f - Ease(border / 0.12f);
			const float offset = 0.12f * bulge + plate + 0.035f * lumps + 0.03f * shelf - 0.1f * crevice - 0.035f * seam;
			return { offset, std::clamp(0.75f * crevice + 0.45f * seam + 0.25f * (1.0f - shelf) * Ease(-bulge), 0.0f, 1.0f) };
		}

		float CrispNoise(Vec2 point)
		{
			return 0.02f * (2.0f * Fbm(point * 0.55f, 431u, 2) - 1.0f);
		}

		uint64_t FoldKey(size_t i, size_t j, size_t width, bool main)
		{
			return (uint64_t(j * width + i) << 1) | (main ? 0u : 1u);
		}

		constexpr float LedgeBury = 0.12f;
		constexpr int32_t LedgeColumns = 2;
		constexpr int32_t CliffRows = 12;
		constexpr int32_t CliffColumns = 2;

		glm::vec3 LedgeFace(Vec2 base, Vec2 outward, float high, float low, float t)
		{
			const float y = std::lerp(high, low - LedgeBury, t);
			const glm::vec3 point(base.x, y, base.y);
			const float strength = 0.75f * std::clamp(high - low, 0.35f, 1.0f);
			float offset = (RockRelief(point).Offset * strength + 0.06f * Ease((t - 0.5f) / 0.5f)) * Ease(t / 0.22f);
			offset = std::max(offset, -0.07f);
			if (y < low + 0.08f)
				offset = std::max(offset, 0.02f);
			return point + glm::vec3(outward.x, 0.0f, outward.y) * offset;
		}

		glm::vec3 CliffFace(Vec2 base, Vec2 outward, float top, float bottom, float t)
		{
			const glm::vec3 point(base.x, std::lerp(top, bottom, t), base.y);
			const float envelope = Ease(t / 0.18f) * (1.0f - Ease((t - 0.72f) / 0.28f));
			return point + glm::vec3(outward.x, 0.0f, outward.y) * (1.4f * RockRelief(point).Offset * envelope);
		}

		template <typename Face>
		glm::vec3 FaceNormal(const Face& face, Vec2 base, Vec2 outward, float top, float bottom, float t)
		{
			const Vec2 along = Perpendicular(outward) * 0.04f;
			const float before = std::max(t - 0.03f, 0.0f);
			const float after = std::min(t + 0.03f, 1.0f);
			const glm::vec3 ds = face(base + along, outward, top, bottom, t) - face(base - along, outward, top, bottom, t);
			const glm::vec3 dt = face(base, outward, top, bottom, after) - face(base, outward, top, bottom, before);
			const glm::vec3 normal = glm::cross(ds, dt);
			return glm::length(normal) > 1e-9f ? glm::normalize(normal) : glm::vec3(outward.x, 0.0f, outward.y);
		}

		glm::mat4 AlignUp(const glm::vec3& up, float spin)
		{
			const glm::vec3 y = glm::normalize(up);
			const glm::vec3 helper = std::abs(y.y) < 0.95f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
			const glm::vec3 x = glm::normalize(glm::cross(helper, y));
			const glm::vec3 z = glm::cross(x, y);
			glm::mat4 basis(1.0f);
			basis[0] = glm::vec4(x, 0.0f);
			basis[1] = glm::vec4(y, 0.0f);
			basis[2] = glm::vec4(z, 0.0f);
			return basis * RotationY(spin);
		}

		float SoftPositive(float x, float knee)
		{
			if (x <= -knee)
				return 0.0f;
			if (x >= knee)
				return x;
			return (x + knee) * (x + knee) / (4.0f * knee);
		}

		float BedOf(TerrainId terrain)
		{
			for (const PavingStyle& style : PavingStyles)
				if (style.Terrain == terrain)
					return style.Bed;
			return 0.06f;
		}

		uint32_t MixColor(uint32_t a, uint32_t b, float t)
		{
			uint32_t result = 0;
			for (uint32_t shift = 0; shift < 32; shift += 8)
				result |= uint32_t(std::lround(std::lerp(float((a >> shift) & 0xFFu), float((b >> shift) & 0xFFu), t))) << shift;
			return result;
		}

		uint32_t PackSplat(const std::array<float, 4>& weights, float shade)
		{
			const auto unorm = [](float value) { return uint32_t(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };
			return unorm(weights[SplatDirt]) | (unorm(weights[SplatRock]) << 8) | (unorm(weights[SplatSand]) << 16) | (unorm(shade) << 24);
		}

		uint32_t WallColor(const glm::vec3& point, const glm::vec3& normal, float base)
		{
			const Relief relief = RockRelief(point);
			const float moss = Ease((normal.y - 0.42f) / 0.3f);
			std::array<float, 4> weights{};
			weights[SplatDirt] = 0.12f + 0.5f * relief.Cavity;
			weights[SplatRock] = (1.0f - weights[SplatDirt]) * (1.0f - 0.8f * moss);
			const float shade = base * (1.0f - 0.55f * relief.Cavity) * (0.8f + 0.4f * ValueNoise3(point * 0.8f, 833u)) * (0.8f + 0.45f * Ease(relief.Offset / 0.25f + 0.5f));
			return PackSplat(weights, shade);
		}

		float Crags(Vec2 point)
		{
			const Vec2 scaled = point * 0.55f;
			const int32_t cx = int32_t(std::floor(scaled.x));
			const int32_t cy = int32_t(std::floor(scaled.y));
			float nearest = 1e9f;
			float second = 1e9f;
			int32_t ox = cx;
			int32_t oy = cy;
			Vec2 feature = scaled;
			for (int32_t dy = -1; dy <= 1; dy++)
				for (int32_t dx = -1; dx <= 1; dx++)
				{
					const Vec2 spot(float(cx + dx) + Unit(cx + dx, cy + dy, 1201u), float(cy + dy) + Unit(cx + dx, cy + dy, 1202u));
					if (const float distance = LengthSquared(scaled - spot); distance < nearest)
					{
						second = nearest;
						nearest = distance;
						ox = cx + dx;
						oy = cy + dy;
						feature = spot;
					}
					else if (distance < second)
						second = distance;
				}
			const Vec2 tilt(Unit(ox, oy, 1203u) - 0.5f, Unit(ox, oy, 1204u) - 0.5f);
			const float border = std::sqrt(second) - std::sqrt(nearest);
			return (0.25f + 0.75f * Unit(ox, oy, 1205u) + 0.6f * Dot(tilt, scaled - feature)) * Ease(border / 0.3f);
		}

		constexpr int32_t MountainFields = ElevationCliffs + 1;
		constexpr float MountainActiveDrop = 0.03f;
		constexpr float MountainWallRow = 0.14f;
		constexpr float MountainWallColumn = 0.24f;
		constexpr float MountainBury = 0.1f;
		constexpr int32_t MountainChunk = 32;
		constexpr size_t PeakSnow = SplatGrass;
		constexpr size_t PeakDirt = SplatDirt;
		constexpr size_t PeakRock = SplatRock;
		constexpr size_t PeakGrass = SplatSand;

		struct MountainSurface
		{
			const Elevation& Relief;

			float Field(int32_t field, const Elevation::Spot& spot) const
			{
				return field == 0 ? Relief.Read(Relief.Rim, spot) : Relief.Read(Relief.Edges[size_t(field - 1)], spot);
			}

			float Drop(int32_t field, const Elevation::Spot& spot) const
			{
				return field == 0 ? 1.0f : Relief.Read(Relief.Drops[size_t(field - 1)], spot);
			}

			Vec2 Downhill(int32_t field, Vec2 point) const
			{
				const Vec2 rise = Relief.Gradient(field == 0 ? Relief.Rim : Relief.Edges[size_t(field - 1)], point);
				return LengthSquared(rise) > 1e-10f ? Normalize(rise) * -1.0f : Normalize(point - Relief.Summit);
			}

			float Flank(Vec2 point) const
			{
				const float d = std::max(-Relief.RimDistance(point), 0.0f);
				const Vec2 away = LengthSquared(point - Relief.Summit) > 1e-6f ? Normalize(point - Relief.Summit) : Vec2(1.0f, 0.0f);
				const glm::vec3 radial(away.x * 4.5f, d * 0.11f, away.y * 4.5f);
				const float ridge = 1.0f - std::abs(1.4f * ValueNoise3(radial, 709u) + 0.6f * ValueNoise3(radial * 2.1f, 711u) - 1.0f);
				const float rough = Fbm(point * 0.45f, 719u, 3) - 0.5f;
				const float drop = 2.3f + 1.4f * Fbm(point * 0.17f, 703u, 2);
				return -drop - d * (1.8f + 0.55f * Fbm(point * 0.11f, 727u, 2)) - 0.045f * d * d + ridge * std::min(d, 9.0f) * 0.45f + rough * std::min(d * 0.6f, 1.4f) + (Crags(point) - 0.5f) * 1.3f * Ease(d / 2.5f);
			}

			float Height(Vec2 point, uint32_t sides) const
			{
				if ((sides & 1u) == 0)
					return Flank(point);
				const Elevation::Spot spot = Relief.Locate(point);
				float height = Relief.Read(Relief.Floor, spot);
				for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
					if ((sides & (2u << cliff)) != 0)
						height += Relief.Read(Relief.Drops[size_t(cliff)], spot);
				return height;
			}

			uint32_t SidesAt(Vec2 point) const
			{
				const Elevation::Spot spot = Relief.Locate(point);
				uint32_t sides = 0;
				for (int32_t field = 0; field < MountainFields; field++)
					sides |= Field(field, spot) >= 0.0f ? 1u << field : 0u;
				return sides;
			}

			glm::vec3 Normal(Vec2 point, uint32_t sides) const
			{
				const float h = 0.12f;
				const float dx = Height(point + Vec2(h, 0.0f), sides) - Height(point - Vec2(h, 0.0f), sides);
				const float dz = Height(point + Vec2(0.0f, h), sides) - Height(point - Vec2(0.0f, h), sides);
				return glm::normalize(glm::vec3(-dx, 2.0f * h, -dz));
			}

			uint32_t TopSplat(Vec2 point, uint32_t sides, const glm::vec3& normal) const
			{
				const Elevation::Spot spot = Relief.Locate(point);
				const float altitude = float(std::popcount(sides >> 1)) / float(ElevationCliffs);
				const float ritual = Length(point - Relief.Summit) - Relief.Ritual;
				float drift = 0.0f;
				float brink = 0.0f;
				float scree = 0.0f;
				for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
				{
					const float edge = Relief.Read(Relief.Edges[size_t(cliff)], spot);
					const float drop = Relief.Read(Relief.Drops[size_t(cliff)], spot);
					drift = std::max(drift, drop * (1.0f - Ease((-edge - 0.05f) / 0.7f)) * (edge < 0.0f ? 1.0f : 0.0f));
					brink = std::max(brink, drop * (1.0f - Ease(edge / 0.25f)) * (edge >= 0.0f ? 1.0f : 0.0f));
					scree = std::max(scree, (1.0f - drop) * (1.0f - Ease((std::abs(edge) - 0.9f) / 0.6f)));
				}
				const float rim = Relief.Read(Relief.Rim, spot);
				brink = std::max(brink, 1.0f - Ease(rim / 0.45f));
				std::array<float, 4> weights{};
				float snow = Ease((Fbm(point * 0.17f, 919u, 3) + 0.34f * altitude - 0.7f) / 0.26f);
				snow = std::max(snow, 0.7f * drift * Ease((Fbm(point * 0.5f, 923u, 2) - 0.45f) / 0.3f));
				snow *= Ease((ritual - 0.6f) / 0.8f) * (1.0f - 0.8f * scree);
				const float grass = (1.0f - 0.7f * altitude) * Ease((Fbm(point * 0.22f, 913u, 3) - 0.42f) / 0.26f) * (1.0f - scree) * (1.0f - brink);
				const float rock = std::max({ Ease((Fbm(point * 0.3f, 929u, 3) - 0.66f) / 0.12f), 0.7f * brink, 0.15f * scree, Ease((glm::length(glm::vec2(normal.x, normal.z)) - 0.55f) / 0.3f) });
				weights[PeakSnow] = snow;
				weights[PeakGrass] = grass * (1.0f - snow);
				weights[PeakRock] = rock * (1.0f - snow) * (1.0f - 0.5f * weights[PeakGrass]);
				weights[PeakDirt] = std::max(1.0f - weights[PeakSnow] - weights[PeakGrass] - weights[PeakRock], 0.0f);
				const float total = weights[0] + weights[1] + weights[2] + weights[3];
				for (float& weight : weights)
					weight /= std::max(total, 1e-4f);
				const float shade = (0.9f + 0.16f * (Fbm(point * 0.21f, 941u, 2) - 0.5f)) * (1.0f - 0.22f * drift);
				return PackSplat(weights, shade);
			}

			uint32_t WallSplat(const glm::vec3& point, const glm::vec3& normal) const
			{
				const auto relief = RockRelief(point);
				std::array<float, 4> weights{};
				weights[PeakSnow] = Ease((normal.y - 0.62f) / 0.2f) * (0.4f + 0.6f * ValueNoise3(point * 0.9f, 963u));
				weights[PeakDirt] = (1.0f - weights[PeakSnow]) * 0.55f * relief.Cavity;
				weights[PeakRock] = std::max(1.0f - weights[PeakSnow] - weights[PeakDirt], 0.0f);
				const float shade = 0.72f * (1.0f - 0.5f * relief.Cavity) * (0.82f + 0.36f * ValueNoise3(point * 0.7f, 967u)) * (0.85f + 0.3f * Ease(relief.Offset / 0.25f + 0.5f));
				return PackSplat(weights, shade);
			}

			std::array<float, 4> Cover(Vec2 point, uint32_t sides) const
			{
				const uint32_t splat = TopSplat(point, sides, Normal(point, sides));
				std::array<float, 4> weights{};
				weights[PeakDirt] = float(splat & 0xFFu) / 255.0f;
				weights[PeakRock] = float((splat >> 8) & 0xFFu) / 255.0f;
				weights[PeakGrass] = float((splat >> 16) & 0xFFu) / 255.0f;
				weights[PeakSnow] = std::max(1.0f - weights[PeakDirt] - weights[PeakRock] - weights[PeakGrass], 0.0f);
				return weights;
			}

			uint32_t FlankSplat(Vec2 point, const glm::vec3& normal) const
			{
				const float d = std::max(-Relief.RimDistance(point), 0.0f);
				std::array<float, 4> weights{};
				const float snow = Ease((normal.y - 0.5f + 0.25f * (Fbm(point * 0.35f, 953u, 2) - 0.5f)) / 0.14f) * Ease((d - 0.8f) / 1.5f);
				const float scree = (1.0f - Ease((d - 0.4f) / 1.6f)) * 0.6f;
				weights[PeakSnow] = snow;
				weights[PeakDirt] = (1.0f - snow) * (scree + 0.12f * Fbm(point * 0.8f, 957u, 2));
				weights[PeakRock] = std::max(1.0f - weights[PeakSnow] - weights[PeakDirt], 0.0f);
				const float shade = (0.78f + 0.2f * (Fbm(point * 0.3f, 961u, 2) - 0.5f)) * (1.0f - 0.35f * Ease(d / 18.0f));
				return PackSplat(weights, shade);
			}
		};

		glm::vec3 MountainFace(Vec2 base, Vec2 outward, float top, float bottom, float t)
		{
			const glm::vec3 point(base.x, std::lerp(top, bottom, t), base.y);
			const float envelope = Ease(t / 0.15f) * (1.0f - Ease((t - 0.8f) / 0.2f));
			const float push = std::clamp(1.3f * RockRelief(point).Offset - 0.04f, -0.32f, 0.1f) * envelope;
			return point + glm::vec3(outward.x, 0.0f, outward.y) * push;
		}

		struct MountainPoint
		{
			Vec2 Position;
			std::array<float, MountainFields> Fields{};
			uint64_t Key = 0;
			uint8_t Edges = 0;
			uint8_t On = 0;
		};

		void SplitPolygon(const std::vector<MountainPoint>& polygon, int32_t field, const std::array<uint64_t, 4>& edgeIds, uint64_t& interior, std::vector<MountainPoint>& inside, std::vector<MountainPoint>& outside)
		{
			inside.clear();
			outside.clear();
			for (size_t n = 0; n < polygon.size(); n++)
			{
				const MountainPoint& a = polygon[n];
				const MountainPoint& b = polygon[(n + 1) % polygon.size()];
				const bool inA = a.Fields[size_t(field)] >= 0.0f;
				const bool inB = b.Fields[size_t(field)] >= 0.0f;
				(inA ? inside : outside).push_back(a);
				if (inA == inB)
					continue;
				const float t = a.Fields[size_t(field)] / (a.Fields[size_t(field)] - b.Fields[size_t(field)]);
				MountainPoint crossing;
				crossing.Position = Lerp(a.Position, b.Position, t);
				for (size_t f = 0; f < crossing.Fields.size(); f++)
					crossing.Fields[f] = std::lerp(a.Fields[f], b.Fields[f], t);
				crossing.Fields[size_t(field)] = 0.0f;
				crossing.Edges = uint8_t(a.Edges & b.Edges);
				crossing.On = uint8_t((a.On & b.On) | (1u << field));
				crossing.Key = crossing.Edges != 0 ? (2ull << 62) | (edgeIds[size_t(std::countr_zero(crossing.Edges))] << 3) | uint64_t(field) : (3ull << 62) | interior++;
				inside.push_back(crossing);
				outside.push_back(crossing);
			}
		}

		class SummedArea
		{
		public:
			SummedArea(const std::vector<float>& values, int32_t width, int32_t height)
				: m_Width(width), m_Height(height), m_Sums(size_t(width + 1) * size_t(height + 1), 0.0)
			{
				for (int32_t y = 0; y < height; y++)
				{
					double row = 0.0;
					for (int32_t x = 0; x < width; x++)
					{
						row += values[size_t(y) * size_t(width) + size_t(x)];
						m_Sums[Index(x + 1, y + 1)] = m_Sums[Index(x + 1, y)] + row;
					}
				}
			}

			double Box(float x0, float y0, float x1, float y1) const
			{
				return Corner(x1, y1) - Corner(x0, y1) - Corner(x1, y0) + Corner(x0, y0);
			}

			double Around(float x, float y, float radius) const
			{
				return Box(x - radius, y - radius, x + radius, y + radius);
			}

			double Mean(float x, float y, float radius) const
			{
				const double area = double(std::min(x + radius, float(m_Width)) - std::max(x - radius, 0.0f)) * double(std::min(y + radius, float(m_Height)) - std::max(y - radius, 0.0f));
				return area > 0.0 ? Around(x, y, radius) / area : 0.0;
			}

			double Total() const
			{
				return m_Sums.back();
			}

		private:
			size_t Index(int32_t x, int32_t y) const
			{
				return size_t(y) * size_t(m_Width + 1) + size_t(x);
			}

			double Corner(float x, float y) const
			{
				x = std::clamp(x, 0.0f, float(m_Width));
				y = std::clamp(y, 0.0f, float(m_Height));
				const int32_t ix = std::min(int32_t(x), m_Width - 1);
				const int32_t iy = std::min(int32_t(y), m_Height - 1);
				const double fx = double(x) - double(ix);
				const double fy = double(y) - double(iy);
				const double low = m_Sums[Index(ix, iy)] * (1.0 - fx) + m_Sums[Index(ix + 1, iy)] * fx;
				const double high = m_Sums[Index(ix, iy + 1)] * (1.0 - fx) + m_Sums[Index(ix + 1, iy + 1)] * fx;
				return low * (1.0 - fy) + high * fy;
			}

			int32_t m_Width = 0;
			int32_t m_Height = 0;
			std::vector<double> m_Sums;
		};

		void LowerEnvelope(const double* f, int32_t n, double* d, int32_t* v, double* z)
		{
			int32_t k = 0;
			v[0] = 0;
			z[0] = -1e30;
			z[1] = 1e30;
			for (int32_t q = 1; q < n; q++)
			{
				const auto meet = [&](int32_t p) { return ((f[q] + double(q) * double(q)) - (f[p] + double(p) * double(p))) / (2.0 * double(q - p)); };
				double s = meet(v[k]);
				while (s <= z[k])
					s = meet(v[--k]);
				k++;
				v[k] = q;
				z[k] = s;
				z[k + 1] = 1e30;
			}
			k = 0;
			for (int32_t q = 0; q < n; q++)
			{
				while (z[k + 1] < double(q))
					k++;
				d[q] = double(q - v[k]) * double(q - v[k]) + f[v[k]];
			}
		}

		void DistanceField(std::vector<float>& field, int32_t width, int32_t height)
		{
			const size_t longest = size_t(std::max(width, height));
			std::vector<double> f(longest, 0.0);
			std::vector<double> d(longest, 0.0);
			std::vector<double> z(longest + 1, 0.0);
			std::vector<int32_t> v(longest, 0);
			std::vector<double> squared(field.size());
			for (int32_t y = 0; y < height; y++)
			{
				for (int32_t x = 0; x < width; x++)
					f[size_t(x)] = field[size_t(y) * size_t(width) + size_t(x)];
				LowerEnvelope(f.data(), width, d.data(), v.data(), z.data());
				for (int32_t x = 0; x < width; x++)
					squared[size_t(y) * size_t(width) + size_t(x)] = d[size_t(x)];
			}
			for (int32_t x = 0; x < width; x++)
			{
				for (int32_t y = 0; y < height; y++)
					f[size_t(y)] = squared[size_t(y) * size_t(width) + size_t(x)];
				LowerEnvelope(f.data(), height, d.data(), v.data(), z.data());
				for (int32_t y = 0; y < height; y++)
					field[size_t(y) * size_t(width) + size_t(x)] = float(std::sqrt(d[size_t(y)]));
			}
		}

		float SampleField(const GroundField& field, Vec2 position)
		{
			const float gx = (position.x + float(TerrainMargin)) * float(TerrainSteps);
			const float gy = (position.y + float(TerrainMargin)) * float(TerrainSteps);
			const int32_t i = std::clamp(int32_t(std::floor(gx)), 0, field.Columns - 2);
			const int32_t j = std::clamp(int32_t(std::floor(gy)), 0, field.Rows - 2);
			const float u = std::clamp(gx - float(i), 0.0f, 1.0f);
			const float v = std::clamp(gy - float(j), 0.0f, 1.0f);
			const auto at = [&](int32_t a, int32_t b) { return field.Heights[size_t(b) * size_t(field.Columns) + size_t(a)]; };
			return std::lerp(std::lerp(at(i, j), at(i + 1, j), u), std::lerp(at(i, j + 1), at(i + 1, j + 1), u), v);
		}

		struct LedgeSpot
		{
			Vec2 Position;
			Vec2 Outward;
			TileCoord High;
			TileCoord Low;
			float Arc = 0.0f;
			float Miter = 1.0f;
		};

		struct LedgeRun
		{
			std::vector<LedgeSpot> Spots;
		};

		struct WallSpot
		{
			Vec2 Base;
			Vec2 Outward;
			float Top = 0.0f;
			float Bottom = 0.0f;
			bool Cliff = false;
			bool HighStep = false;
			bool LowStep = false;
			bool LowPaved = false;
			bool Rim = false;
		};

		class SceneBuilder
		{
		public:
			SceneBuilder(const Level& level, WorldRenderer& renderer, LevelScene& scene)
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
				m_Scene.Width = width;
				ResolveMeshes();
				if (m_Level.Skyborne)
				{
					if (!m_Level.Relief.IsEmpty())
					{
						Mountain();
						SummitStones();
						MountainSlabs();
						MountainDressing();
					}
					Props();
					if (m_Level.Relief.IsEmpty())
						RuinIslets();
					return;
				}
				Paving();
				Terrain();
				Floors();
				Rocks();
				Walls();
				Props();
				Decor();
				Roots();
				Tethers();
				Steps();
				Railings();
				WallDressing();
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
					{
						WeightedMesh& entry = result.emplace_back(WeightedMesh{ *mesh, scale });
						for (uint32_t extra = 1;; extra++)
						{
							const std::optional<uint32_t> more = m_Models.FindMesh(std::string(name) + "." + std::to_string(extra));
							if (!more)
								break;
							entry.Extras.push_back(*more);
						}
					}
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
				m_ChainLink = Find("Emberhold/ChainLink");
				m_Roots = FindWeighted({ { "Nature/Roots_A", 1.0f }, { "Nature/Roots_B", 1.0f }, { "Nature/Roots_C", 1.0f } });
				m_StepBlocks = FindWeighted({ { "Emberhold/StepBlock_A", 1.0f }, { "Emberhold/StepBlock_B", 1.0f }, { "Emberhold/StepBlock_C", 1.0f }, { "Emberhold/StepBlock_D", 1.0f }, { "Emberhold/StepBlock_E", 1.0f }, { "Emberhold/StepBlock_F", 1.0f } });
				m_FencePosts = FindWeighted({ { "Market/FencePost", 1.0f } });
				m_FenceRails = FindWeighted({ { "Market/FenceRail", 1.0f } });
				m_Hedge = FindWeighted({ { "Nature/Bush_A", 0.95f }, { "Nature/Bush_C", 0.85f }, { "Nature/Bush_B", 1.1f } });
				m_EdgeRocks = FindWeighted({ { "Nature/Boulder_A", 0.72f }, { "Nature/Boulder_B", 0.68f }, { "Nature/Boulder_C", 0.7f }, { "Nature/Rock_A", 1.5f }, { "Nature/Rock_B", 1.45f }, { "Nature/Rock_C", 1.5f } });
				m_CliffRocks = FindWeighted({ { "Nature/Rock_A", 1.0f }, { "Nature/Rock_B", 1.0f }, { "Nature/Rock_C", 0.95f }, { "Nature/Rock_D", 1.05f } });
				m_Pebbles = FindWeighted({ { "Nature/Rock_A", 0.32f }, { "Nature/Rock_C", 0.28f }, { "Nature/Rock_D", 0.36f }, { "Nature/Rock_B", 0.25f } });
				if (m_Level.Kind == MapKind::StormPeak)
					if (std::vector<WeightedMesh> granite = FindWeighted({ { "Summit/Rock_A", 1.0f }, { "Summit/Rock_B", 1.0f }, { "Summit/Rock_C", 1.0f }, { "Summit/Rock_D", 1.0f }, { "Summit/Slab_A", 1.0f }, { "Summit/Slab_B", 1.0f } }); !granite.empty())
					{
						m_CliffRocks = granite;
						m_EdgeRocks = granite;
					}
				if (m_Level.Kind == MapKind::FrozenLake)
					if (std::vector<WeightedMesh> snowy = FindWeighted({ { "Glacier/SnowRock_A", 0.85f }, { "Glacier/SnowRock_B", 1.1f }, { "Glacier/SnowRock_C", 1.5f } }); !snowy.empty())
					{
						m_EdgeRocks = snowy;
						m_CliffRocks = snowy;
					}
				m_Grass = FindWeighted({ { "Nature/Grass_A", 1.0f }, { "Nature/Grass_B", 0.9f }, { "Nature/Grass_C", 1.0f } });
				m_SmallBushes = FindWeighted({ { "Nature/Bush_B", 0.6f }, { "Nature/Bush_A", 0.5f }, { "Nature/Bush_C", 0.45f } });
				m_Saplings = FindWeighted({ { "Tree_Bare_1_A_Color1", 0.5f }, { "Tree_Bare_2_A_Color1", 0.4f } });
				const std::optional<uint32_t> layers[] = { m_Renderer.FindMaterialSlot(m_Level.Kind == MapKind::FrozenLake && m_Renderer.FindMaterialSlot("GroundSnow") ? "GroundSnow" : "GroundGrass"), m_Renderer.FindMaterialSlot("GroundDirt"), m_Renderer.FindMaterialSlot("GroundRock"), m_Renderer.FindMaterialSlot("GroundSand") };
				m_Terrain = true;
				for (size_t i = 0; i < std::size(layers); i++)
				{
					m_Terrain = m_Terrain && layers[i].has_value();
					m_Splat[i] = layers[i].value_or(0u);
				}

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
							if (part.Glow > 0.0f || part.Fire >= 0)
							{
								parts.push_back({ 0, Translation(part.Offset), glm::vec4(1.0f), part.Glow, part.Fire, part.FireScale });
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
							for (uint32_t extra = 1;; extra++)
							{
								const std::optional<uint32_t> more = m_Models.FindMesh(std::string(part.Mesh) + "." + std::to_string(extra));
								if (!more)
									break;
								parts.push_back({ *more, local, part.Tint });
							}
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

			void Bounds(uint32_t mesh, const glm::mat4& transform, glm::vec3& low, glm::vec3& high) const
			{
				const ModelMesh& source = m_Models.GetMesh(mesh);
				for (uint32_t corner = 0; corner < 8; corner++)
				{
					const glm::vec3 local((corner & 1) ? source.Max.x : source.Min.x, (corner & 2) ? source.Max.y : source.Min.y, (corner & 4) ? source.Max.z : source.Min.z);
					const glm::vec3 world = glm::vec3(transform * glm::vec4(local, 1.0f));
					low = glm::min(low, world);
					high = glm::max(high, world);
				}
			}

			void Add(uint32_t mesh, const glm::mat4& transform, const glm::vec4& tint, float ground, bool cut, float base)
			{
				glm::vec3 low(1e9f);
				glm::vec3 high(-1e9f);
				Bounds(mesh, transform, low, high);
				Insert(mesh, m_Renderer.MakeModelInstance(mesh, transform, NoPalette, tint, 0.0f, ground), low, high, cut, base);
			}

			void Add(const WeightedMesh& mesh, const glm::mat4& transform, const glm::vec4& tint, float ground, bool cut, float base)
			{
				glm::vec3 low(1e9f);
				glm::vec3 high(-1e9f);
				Bounds(mesh.Mesh, transform, low, high);
				for (uint32_t extra : mesh.Extras)
					Bounds(extra, transform, low, high);
				Insert(mesh.Mesh, m_Renderer.MakeModelInstance(mesh.Mesh, transform, NoPalette, tint, 0.0f, ground), low, high, cut, base);
				for (uint32_t extra : mesh.Extras)
					Insert(extra, m_Renderer.MakeModelInstance(extra, transform, NoPalette, tint, 0.0f, ground), low, high, cut, base);
			}

			SceneItem MakeItem(uint32_t mesh, const ModelInstance& instance, const glm::vec3& low, const glm::vec3& high, bool cut, float base, float bob = 0.0f, float phase = 0.0f) const
			{
				SceneItem item;
				item.Mesh = mesh;
				item.Instance = instance;
				item.Center = { (low.x + high.x) * 0.5f, (low.z + high.z) * 0.5f };
				item.Reach = 0.5f * std::sqrt((high.x - low.x) * (high.x - low.x) + (high.z - low.z) * (high.z - low.z)) + bob;
				item.Extent = { (high.x - low.x) * 0.5f, (high.z - low.z) * 0.5f };
				item.Base = base;
				item.Top = high.y;
				item.Cut = cut;
				item.Bob = bob;
				item.BobPhase = phase;
				return item;
			}

			void Insert(uint32_t mesh, const ModelInstance& instance, const glm::vec3& low, const glm::vec3& high, bool cut, float base, float bob = 0.0f, float phase = 0.0f)
			{
				const SceneItem item = MakeItem(mesh, instance, low, high, cut, base, bob, phase);
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

			bool Foundation(int32_t x, int32_t y, float top, float own, bool overGround = false)
			{
				uint8_t mask = 0;
				float lowest = top;
				for (uint32_t side = 0; side < 4; side++)
				{
					int32_t nx = x;
					int32_t ny = y;
					TileMap::Step(Direction(side), nx, ny);
					if (const GroundCell* neighbor = FindGround(nx, ny); (overGround && neighbor && neighbor->Kind == GroundOpen) || (m_Level.Rounded && m_Map.IsAbyss(nx, ny)))
						continue;
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

			void Paving()
			{
				const int32_t width = m_Map.GetWidth();
				const int32_t height = m_Map.GetHeight();
				m_Paved.assign(size_t(width) * size_t(height), 0);
				const std::optional<uint32_t> stone = m_Renderer.FindMaterialSlot("PavingStone");
				const std::optional<uint32_t> joint = m_Renderer.FindMaterialSlot("PavingBed");
				if (!stone || !joint)
					return;
				std::vector<PavingBatch> stones(m_Scene.Chunks.size());
				std::vector<PavingBatch> joints(m_Scene.Chunks.size());
				const auto chunkOf = [&](int32_t x, int32_t y) { return size_t(std::clamp(y / ChunkSize, 0, m_ChunksY - 1)) * size_t(m_ChunksX) + size_t(std::clamp(x / ChunkSize, 0, m_ChunksX - 1)); };
				std::vector<uint8_t> used(std::size(PavingStyles), 0);
				for (size_t index = 0; index < std::size(PavingStyles); index++)
					for (int32_t y = 0; y < height; y++)
						for (int32_t x = 0; x < width; x++)
						{
							const Cell& cell = m_Map.Get(x, y);
							if (cell.Terrain != PavingStyles[index].Terrain || cell.Slope != Direction::None)
								continue;
							m_Paved[CellIndex(x, y)] = 1;
							if (!m_Terrain)
								LayJoint(joints[chunkOf(x, y)], x, y, CellHeight(x, y) - PavingStyles[index].Bed + 0.004f, PavingStyles[index]);
							used[index] = 1;
						}
				for (size_t index = 0; index < std::size(PavingStyles); index++)
				{
					const PavingStyle& style = PavingStyles[index];
					if (!used[index])
						continue;
					const auto paves = [&](int32_t x, int32_t y, int8_t level)
					{
						if (!m_Map.IsInside(x, y))
							return false;
						const Cell& cell = m_Map.Get(x, y);
						return cell.Terrain == style.Terrain && cell.Slope == Direction::None && cell.Level == level;
					};
					const auto open = [&](int32_t x, int32_t y, int8_t level)
					{
						if (paves(x, y, level))
							return true;
						if (!m_Map.IsInside(x, y))
							return false;
						const Cell& cell = m_Map.Get(x, y);
						return GetTerrain(cell.Terrain).Style == TerrainStyle::Ground && cell.Slope == Direction::None && (cell.Level == level || (m_Terrain && cell.Level > level));
					};
					const float pitchX = style.Pitch * style.Stretch;
					const float pitchY = style.Pitch;
					const auto seedAt = [&](int32_t row, int32_t column)
					{
						const float stagger = (row & 1) ? 0.5f : 0.0f;
						return Vec2((float(column) + 0.5f + stagger + (Unit(column, row, style.Salt) - 0.5f) * style.Jitter) * pitchX, (float(row) + 0.5f + (Unit(column, row, style.Salt + 1) - 0.5f) * style.Jitter) * pitchY);
					};
					const int32_t rows = int32_t(std::ceil(float(height) / pitchY)) + 1;
					const int32_t columns = int32_t(std::ceil(float(width) / pitchX)) + 1;
					const float reach = 2.0f * std::max(pitchX, pitchY);
					const float limit = 0.72f * std::max(pitchX, pitchY);
					for (int32_t row = -1; row <= rows; row++)
						for (int32_t column = -1; column <= columns; column++)
						{
							const Vec2 seed = seedAt(row, column);
							const int32_t cx = int32_t(std::floor(seed.x));
							const int32_t cy = int32_t(std::floor(seed.y));
							if (!m_Map.IsInside(cx, cy))
								continue;
							const int8_t level = m_Map.Get(cx, cy).Level;
							if (!paves(cx, cy, level))
								continue;
							Outline outline = { seed + Vec2(-reach, -reach), seed + Vec2(reach, -reach), seed + Vec2(reach, reach), seed + Vec2(-reach, reach) };
							for (int32_t dr = -2; dr <= 2; dr++)
								for (int32_t dc = -3; dc <= 3; dc++)
								{
									const Vec2 other = seedAt(row + dr, column + dc);
									if ((dr == 0 && dc == 0) || !paves(int32_t(std::floor(other.x)), int32_t(std::floor(other.y)), level))
										continue;
									KeepBelow(outline, other - seed, Dot((seed + other) * 0.5f, other - seed));
								}
							const float spin = Unit(column, row, style.Salt + 2) * TwoPi;
							for (int32_t k = 0; k < 8; k++)
							{
								const Vec2 normal = FromAngle(spin + float(k) * Pi * 0.25f);
								KeepBelow(outline, normal, Dot(seed, normal) + limit);
							}
							for (int32_t dy = -2; dy <= 2; dy++)
								for (int32_t dx = -2; dx <= 2; dx++)
								{
									if ((dx == 0 && dy == 0) || open(cx + dx, cy + dy, level))
										continue;
									const Vec2 corner(float(dx > 0 ? cx + dx : cx + dx + 1), float(dy > 0 ? cy + dy : cy + dy + 1));
									const Vec2 side(float(dx > 0) - float(dx < 0), float(dy > 0) - float(dy < 0));
									const Vec2 normal = Normalize(side);
									KeepBelow(outline, normal, Dot(corner, normal));
								}
							for (int32_t dy = -2; dy <= 2 && m_Level.Rounded; dy++)
								for (int32_t dx = -2; dx <= 2; dx++)
								{
									const int32_t x = cx + dx;
									const int32_t y = cy + dy;
									const uint8_t cuts = m_Map.GetCuts(x, y);
									if (cuts == 0 || !open(x, y, level))
										continue;
									const Vec2 corner = { float(x), float(y) };
									constexpr float Diagonal = 0.70710678f;
									if ((cuts & CornerSouthWest) && !open(x - 1, y, level) && !open(x, y - 1, level))
										KeepBelow(outline, Vec2(-Diagonal, -Diagonal), Dot(corner + Vec2(0.0f, 0.5f), Vec2(-Diagonal, -Diagonal)));
									if ((cuts & CornerSouthEast) && !open(x + 1, y, level) && !open(x, y - 1, level))
										KeepBelow(outline, Vec2(Diagonal, -Diagonal), Dot(corner + Vec2(0.5f, 0.0f), Vec2(Diagonal, -Diagonal)));
									if ((cuts & CornerNorthEast) && !open(x + 1, y, level) && !open(x, y + 1, level))
										KeepBelow(outline, Vec2(Diagonal, Diagonal), Dot(corner + Vec2(1.0f, 0.5f), Vec2(Diagonal, Diagonal)));
									if ((cuts & CornerNorthWest) && !open(x - 1, y, level) && !open(x, y + 1, level))
										KeepBelow(outline, Vec2(-Diagonal, Diagonal), Dot(corner + Vec2(0.0f, 0.5f), Vec2(-Diagonal, Diagonal)));
								}
							if (outline.size() < 3 || SignedArea(outline) < 0.004f)
								continue;
							Inset(outline, style.Gap * 0.5f);
							outline = Cleaned(outline);
							if (outline.size() < 3 || SignedArea(outline) < 0.003f)
								continue;
							LayStone(stones[chunkOf(cx, cy)], Rounded(outline, style.Corner), style, CellHeight(cx, cy), row, column);
						}
				}

				std::vector<ModelVertex> vertices;
				std::vector<uint32_t> indices;
				std::vector<LevelMeshDesc> meshes;
				std::vector<std::pair<glm::vec3, glm::vec3>> bounds;
				const auto append = [&](const PavingBatch& batch, uint32_t material)
				{
					if (batch.Indices.empty())
						return;
					const uint32_t firstIndex = uint32_t(indices.size());
					const uint32_t baseVertex = uint32_t(vertices.size());
					vertices.insert(vertices.end(), batch.Vertices.begin(), batch.Vertices.end());
					for (const uint32_t index : batch.Indices)
						indices.push_back(baseVertex + index);
					meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(batch.Indices.size()), .Material = material, .Layer = material, .Flags = MeshVertexTint | MeshGround });
					bounds.push_back({ batch.Min, batch.Max });
				};
				for (size_t i = 0; i < stones.size(); i++)
				{
					append(stones[i], *stone);
					append(joints[i], *joint);
				}
				const uint32_t base = m_Renderer.AddLevelGeometry(vertices, indices, meshes);
				for (uint32_t i = 0; i < meshes.size(); i++)
				{
					const auto& [low, high] = bounds[i];
					Insert(base + i, m_Renderer.MakeModelInstance(base + i, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, low.y), low, high, false, low.y);
				}
			}

			bool IsStepCell(int32_t x, int32_t y) const
			{
				const Cell& cell = m_Map.Get(x, y);
				return m_Terrain && !m_StepBlocks.empty() && cell.Terrain == TerrainId::Dirt && cell.Slope != Direction::None && cell.Climb > 0;
			}

			GroundCell Classify(int32_t x, int32_t y) const
			{
				const Cell& cell = m_Map.Get(x, y);
				const float top = CellHeight(x, y);
				if (m_Paved[CellIndex(x, y)])
					return { GroundPaved, SplatDirt, top - BedOf(cell.Terrain) + 0.004f, 0.62f };
				switch (cell.Terrain)
				{
				case TerrainId::Grass:
					return { GroundOpen, SplatGrass, top, 1.0f };
				case TerrainId::Dirt:
					if (IsStepCell(x, y))
						return { GroundNone, SplatDirt, top, 1.0f };
					return { GroundOpen, SplatDirt, top, 1.0f };
				case TerrainId::Sand:
					return { GroundOpen, SplatSand, top, 1.0f };
				case TerrainId::Ash:
					return { GroundOpen, SplatRock, top, 0.55f };
				case TerrainId::CaveFloor:
					return { GroundOpen, SplatDirt, top, 0.9f };
				default:
					return { GetTerrain(cell.Terrain).Style == TerrainStyle::Solid ? GroundHill : GroundNone, SplatRock, top, 1.0f };
				}
			}

			std::optional<float> SurfaceAt(int32_t x, int32_t y, float u, float v) const
			{
				const Cell& cell = m_Map.Get(x, y);
				switch (GetTerrain(cell.Terrain).Style)
				{
				case TerrainStyle::Void:
					return std::nullopt;
				case TerrainStyle::Liquid:
					return CellHeight(x, y) - WaterDrop;
				case TerrainStyle::Molten:
					return LavaSurface;
				default:
					return IsBrawlFloor(cell.Terrain) ? LavaSurface : m_Map.GetHeightAt(x, y, u, v);
				}
			}

			const GroundCell* FindGround(int32_t x, int32_t y) const
			{
				const int32_t ex = x + TerrainMargin;
				const int32_t ey = y + TerrainMargin;
				if (ex < 0 || ey < 0 || ex >= m_GroundColumns || ey >= m_GroundRows)
					return nullptr;
				return &m_Ground[size_t(ey) * size_t(m_GroundColumns) + size_t(ex)];
			}

			std::optional<float> TerrainHeight(Vec2 position) const
			{
				const GroundCell* ground = FindGround(int32_t(std::floor(position.x)), int32_t(std::floor(position.y)));
				if (!ground || (ground->Kind != GroundOpen && ground->Kind != GroundHill) || m_Scene.Ground.Heights.empty())
					return std::nullopt;
				return SampleField(m_Scene.Ground, position);
			}

			bool IsGroundRegion(TileCoord cell) const
			{
				if (!m_Map.IsInside(cell.X, cell.Y) || !m_Map.IsWalkable(cell.X, cell.Y))
					return false;
				const GroundCell* ground = FindGround(cell.X, cell.Y);
				return ground && (ground->Kind == GroundOpen || ground->Kind == GroundPaved || IsStepCell(cell.X, cell.Y));
			}

			float SurfaceOf(TileCoord cell, Vec2 point) const
			{
				return m_Map.GetHeightAt(cell.X, cell.Y, std::clamp(point.x - float(cell.X), 0.0f, 1.0f), std::clamp(point.y - float(cell.Y), 0.0f, 1.0f));
			}

			float RegionTop(TileCoord cell, Vec2 point) const
			{
				const GroundCell* ground = FindGround(cell.X, cell.Y);
				return ground && ground->Kind == GroundPaved ? ground->Height : SurfaceOf(cell, point);
			}

			bool IsLandAt(Vec2 point) const
			{
				const TileCoord cell = m_Map.CellAt(point);
				return m_Map.IsInside(cell.X, cell.Y) && GetTerrain(m_Map.GetTerrain(cell.X, cell.Y)).Style != TerrainStyle::Void;
			}

			std::optional<float> RoundedLedgeHeight(Vec2 point) const
			{
				float lowest = std::numeric_limits<float>::max();
				float highest = std::numeric_limits<float>::lowest();
				for (int32_t dy = -1; dy <= 1; dy++)
					for (int32_t dx = -1; dx <= 1; dx++)
					{
						const TileCoord cell = m_Map.CellAt(point + Vec2(float(dx), float(dy)) * LedgeReach);
						if (!IsGroundRegion(cell))
							continue;
						const float top = SurfaceOf(cell, point);
						lowest = std::min(lowest, top);
						highest = std::max(highest, top);
					}
				if (highest - lowest <= MaxStepHeight + 0.01f)
					return std::nullopt;
				std::optional<float> own;
				float ownTop = std::numeric_limits<float>::lowest();
				for (const Vec2 offset : { Vec2(1.0f, 1.0f), Vec2(-1.0f, 1.0f), Vec2(1.0f, -1.0f), Vec2(-1.0f, -1.0f) })
				{
					const TileCoord cell = m_Map.CellAt(point + offset * 0.002f);
					if (!IsGroundRegion(cell))
						continue;
					if (const float top = SurfaceOf(cell, point); top > ownTop)
					{
						ownTop = top;
						own = RegionTop(cell, point);
					}
				}
				return own;
			}

			void BuildLedgeRuns()
			{
				struct Piece
				{
					Vec2 From;
					Vec2 To;
					Vec2 Inside;
				};
				const auto key = [](Vec2 point) { return (uint64_t(uint32_t(int32_t(std::lround(point.x * 4.0f)))) << 32) | uint64_t(uint32_t(int32_t(std::lround(point.y * 4.0f)))); };
				std::vector<Piece> pieces;
				std::unordered_multimap<uint64_t, size_t> starts;
				std::unordered_map<uint64_t, int32_t> incoming;
				for (const Boundary& boundary : m_Level.Boundaries)
				{
					if (boundary.Kind != BoundaryKind::Ledge)
						continue;
					const Vec2 middle = (boundary.From + boundary.To) * 0.5f;
					if (!IsGroundRegion(m_Map.CellAt(middle + boundary.Inside * 0.01f)) || !IsGroundRegion(m_Map.CellAt(middle - boundary.Inside * 0.01f)))
						continue;
					Piece piece{ boundary.From, boundary.To, boundary.Inside };
					if (Dot(Perpendicular(piece.To - piece.From), piece.Inside) < 0.0f)
						std::swap(piece.From, piece.To);
					starts.emplace(key(piece.From), pieces.size());
					incoming[key(piece.To)]++;
					pieces.push_back(piece);
				}
				std::vector<uint8_t> taken(pieces.size(), 0);
				const auto follow = [&](size_t first)
				{
					std::vector<size_t> chain;
					for (size_t current = first;;)
					{
						taken[current] = 1;
						chain.push_back(current);
						size_t next = pieces.size();
						const auto [begin, end] = starts.equal_range(key(pieces[current].To));
						for (auto it = begin; it != end && next == pieces.size(); ++it)
							if (!taken[it->second])
								next = it->second;
						if (next == pieces.size())
							break;
						current = next;
					}
					LedgeRun run;
					const bool closed = key(pieces[chain.back()].To) == key(pieces[chain.front()].From);
					float arc = 0.0f;
					for (size_t n = 0; n < chain.size(); n++)
					{
						const Piece& piece = pieces[chain[n]];
						const Piece* previous = n > 0 ? &pieces[chain[n - 1]] : closed ? &pieces[chain.back()] : nullptr;
						const Vec2 middle = (piece.From + piece.To) * 0.5f;
						const TileCoord high = m_Map.CellAt(middle + piece.Inside * 0.01f);
						const TileCoord low = m_Map.CellAt(middle - piece.Inside * 0.01f);
						const float half = Length(piece.To - piece.From) * 0.5f;
						for (int32_t s = 0; s < 2; s++)
						{
							Vec2 outward = piece.Inside * -1.0f;
							float miter = 1.0f;
							if (s == 0 && previous)
							{
								outward = Normalize(outward - previous->Inside);
								miter = 1.0f / std::max(Dot(outward, piece.Inside * -1.0f), 0.5f);
							}
							run.Spots.push_back({ Lerp(piece.From, piece.To, 0.5f * float(s)), outward, high, low, arc, miter });
							arc += half;
						}
					}
					const Piece& last = pieces[chain.back()];
					LedgeSpot final = run.Spots.front();
					if (!closed)
					{
						const Vec2 middle = (last.From + last.To) * 0.5f;
						final = { last.To, last.Inside * -1.0f, m_Map.CellAt(middle + last.Inside * 0.01f), m_Map.CellAt(middle - last.Inside * 0.01f), arc, 1.0f };
					}
					final.Arc = arc;
					run.Spots.push_back(final);
					m_LedgeRuns.push_back(std::move(run));
				};
				for (size_t i = 0; i < pieces.size(); i++)
					if (!taken[i] && incoming[key(pieces[i].From)] == 0)
						follow(i);
				for (size_t i = 0; i < pieces.size(); i++)
					if (!taken[i])
						follow(i);
			}

			void BuildCoast()
			{
				const int32_t width = m_Map.GetWidth();
				const int32_t height = m_Map.GetHeight();
				m_Coast.assign(size_t(width) * size_t(height), {});
				const auto land = [&](int32_t x, int32_t y) { return m_Map.IsInside(x, y) && GetTerrain(m_Map.GetTerrain(x, y)).Style != TerrainStyle::Void; };
				const auto add = [&](Vec2 from, Vec2 to)
				{
					const Vec2 middle = (from + to) * 0.5f;
					const int32_t x = std::clamp(int32_t(std::floor(middle.x)), 0, width - 1);
					const int32_t y = std::clamp(int32_t(std::floor(middle.y)), 0, height - 1);
					m_Coast[size_t(y) * size_t(width) + size_t(x)].push_back({ from, to });
				};
				const auto halves = [&](Vec2 from, Vec2 to)
				{
					const Vec2 middle = (from + to) * 0.5f;
					if (!m_Map.IsCutVertex(int32_t(std::lround(from.x)), int32_t(std::lround(from.y))))
						add(from, middle);
					if (!m_Map.IsCutVertex(int32_t(std::lround(to.x)), int32_t(std::lround(to.y))))
						add(middle, to);
				};
				for (int32_t y = 0; y < height; y++)
					for (int32_t x = 0; x < width; x++)
					{
						const bool here = land(x, y);
						if (here != land(x + 1, y))
							halves({ float(x + 1), float(y) }, { float(x + 1), float(y + 1) });
						if (here != land(x, y + 1))
							halves({ float(x), float(y + 1) }, { float(x + 1), float(y + 1) });
						if (here != land(x - 1, y) && x == 0)
							halves({ 0.0f, float(y) }, { 0.0f, float(y + 1) });
						if (here != land(x, y - 1) && y == 0)
							halves({ float(x), 0.0f }, { float(x + 1), 0.0f });
						const uint8_t cuts = m_Map.GetCuts(x, y);
						const Vec2 corner = { float(x), float(y) };
						if ((cuts & CornerSouthWest) && here != land(x - 1, y))
							add(corner + Vec2(0.0f, 0.5f), corner + Vec2(0.5f, 0.0f));
						if ((cuts & CornerSouthEast) && here != land(x + 1, y))
							add(corner + Vec2(0.5f, 0.0f), corner + Vec2(1.0f, 0.5f));
						if ((cuts & CornerNorthEast) && here != land(x + 1, y))
							add(corner + Vec2(1.0f, 0.5f), corner + Vec2(0.5f, 1.0f));
						if ((cuts & CornerNorthWest) && here != land(x - 1, y))
							add(corner + Vec2(0.5f, 1.0f), corner + Vec2(0.0f, 0.5f));
					}
			}

			float CoastDistance(Vec2 point) const
			{
				const int32_t width = m_Map.GetWidth();
				const int32_t height = m_Map.GetHeight();
				const int32_t cx = int32_t(std::floor(point.x));
				const int32_t cy = int32_t(std::floor(point.y));
				float best = 1.5f;
				for (int32_t y = std::max(cy - 2, 0); y <= std::min(cy + 2, height - 1); y++)
					for (int32_t x = std::max(cx - 2, 0); x <= std::min(cx + 2, width - 1); x++)
						for (const auto& [from, to] : m_Coast[size_t(y) * size_t(width) + size_t(x)])
							best = std::min(best, PointSegmentDistance(point, from, to));
				return IsLandAt(point) ? best : -best;
			}

			std::optional<float> LedgeHeight(Vec2 point) const
			{
				if (m_Level.Rounded)
					return RoundedLedgeHeight(point);
				float lowest = std::numeric_limits<float>::max();
				float highest = std::numeric_limits<float>::lowest();
				float ownTop = std::numeric_limits<float>::lowest();
				std::optional<float> own;
				for (int32_t y = int32_t(std::floor(point.y - LedgeReach)); y <= int32_t(std::floor(point.y + LedgeReach)); y++)
					for (int32_t x = int32_t(std::floor(point.x - LedgeReach)); x <= int32_t(std::floor(point.x + LedgeReach)); x++)
					{
						const GroundCell* ground = FindGround(x, y);
						if (!m_Map.IsInside(x, y) || !m_Map.IsWalkable(x, y) || !ground || (ground->Kind != GroundOpen && ground->Kind != GroundPaved && !IsStepCell(x, y)))
							continue;
						const float top = m_Map.GetHeightAt(x, y, std::clamp(point.x - float(x), 0.0f, 1.0f), std::clamp(point.y - float(y), 0.0f, 1.0f));
						lowest = std::min(lowest, top);
						highest = std::max(highest, top);
						const bool inside = point.x >= float(x) - 1e-4f && point.x <= float(x + 1) + 1e-4f && point.y >= float(y) - 1e-4f && point.y <= float(y + 1) + 1e-4f;
						if (inside && top > ownTop)
						{
							ownTop = top;
							own = ground->Kind == GroundPaved ? ground->Height : top;
						}
					}
				if (!own || highest - lowest <= MaxStepHeight + 0.01f)
					return std::nullopt;
				return own;
			}

			float NaturalBase(Vec2 position, float fallback) const
			{
				std::optional<float> lowest;
				for (const Vec2 offset : { Vec2(), Vec2(0.25f, 0.0f), Vec2(-0.25f, 0.0f), Vec2(0.0f, 0.25f), Vec2(0.0f, -0.25f) })
					if (const std::optional<float> ground = TerrainHeight(position + offset))
						lowest = std::min(lowest.value_or(*ground), *ground);
				return lowest.value_or(fallback);
			}

			void Terrain()
			{
				if (!m_Terrain)
					return;
				const int32_t width = m_Map.GetWidth();
				const int32_t height = m_Map.GetHeight();
				const int32_t columns = width + 2 * TerrainMargin;
				const int32_t rows = height + 2 * TerrainMargin;
				const int32_t steps = TerrainSteps;
				m_GroundColumns = columns;
				m_GroundRows = rows;
				m_Ground.assign(size_t(columns) * size_t(rows), {});
				std::vector<float> lakes(size_t(columns) * size_t(rows), FarAway);
				std::array<float, 4> coverage{};
				std::array<float, 4> coverShade{};
				for (int32_t ey = 0; ey < rows; ey++)
					for (int32_t ex = 0; ex < columns; ex++)
					{
						const int32_t x = ex - TerrainMargin;
						const int32_t y = ey - TerrainMargin;
						GroundCell& ground = m_Ground[size_t(ey) * size_t(columns) + size_t(ex)];
						const int32_t edgeX = std::clamp(x, 0, width - 1);
						const int32_t edgeY = std::clamp(y, 0, height - 1);
						if (GetTerrain(m_Map.GetTerrain(edgeX, edgeY)).Style == TerrainStyle::Liquid)
							lakes[size_t(ey) * size_t(columns) + size_t(ex)] = CellHeight(edgeX, edgeY) - WaterDrop;
						if (!m_Map.IsInside(x, y))
						{
							ground = Classify(edgeX, edgeY);
							if (ground.Kind == GroundPaved)
								ground = { GroundOpen, SplatDirt, CellHeight(edgeX, edgeY), 1.0f };
							continue;
						}
						ground = Classify(x, y);
						if (ground.Kind == GroundOpen)
						{
							coverage[ground.Layer] += 1.0f;
							coverShade[ground.Layer] += ground.Shade;
						}
					}
				const auto isVoid = [&](int32_t x, int32_t y) { return !m_Map.IsInside(x, y) || GetTerrain(m_Map.GetTerrain(x, y)).Style == TerrainStyle::Void; };
				int32_t border = 0;
				int32_t empty = 0;
				for (int32_t y = 0; y < height; y++)
					for (int32_t x = 0; x < width; x++)
						if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
						{
							border++;
							empty += isVoid(x, y) ? 1 : 0;
						}
				m_Floating = border > 0 && float(empty) >= 0.9f * float(border);
				const bool floating = m_Floating;
				const bool rounded = m_Level.Rounded;
				if (rounded)
				{
					BuildLedgeRuns();
					BuildCoast();
				}
				std::vector<uint8_t> land(size_t(columns) * size_t(rows), 0);
				if (floating)
				{
					for (int32_t ey = 0; ey < rows; ey++)
						for (int32_t ex = 0; ex < columns; ex++)
							land[size_t(ey) * size_t(columns) + size_t(ex)] = isVoid(ex - TerrainMargin, ey - TerrainMargin) ? 0 : 1;
					for (int32_t ey = 0; ey < rows; ey++)
						for (int32_t ex = 0; ex < columns; ex++)
						{
							const size_t c = size_t(ey) * size_t(columns) + size_t(ex);
							if (land[c])
								continue;
							bool near = false;
							for (int32_t ny = std::max(ey - 1, 0); ny <= std::min(ey + 1, rows - 1) && !near; ny++)
								for (int32_t nx = std::max(ex - 1, 0); nx <= std::min(ex + 1, columns - 1) && !near; nx++)
									near = land[size_t(ny) * size_t(columns) + size_t(nx)] != 0;
							if (near)
								m_Ground[c] = { GroundRim, SplatGrass, 0.0f, 1.0f };
						}
				}
				const std::optional<uint32_t> water = m_Renderer.FindMaterialSlot("GroundWater");
				const bool lakeside = water && std::any_of(lakes.begin(), lakes.end(), [](float level) { return level < FarAway; });
				std::vector<float> wetCells(size_t(columns) * size_t(rows), 0.0f);
				if (lakeside)
					for (int32_t ey = 0; ey < rows; ey++)
						for (int32_t ex = 0; ex < columns; ex++)
						{
							const size_t c = size_t(ey) * size_t(columns) + size_t(ex);
							if (lakes[c] >= FarAway)
								continue;
							wetCells[c] = 1.0f;
							bool shore = false;
							for (int32_t ny = std::max(ey - 2, 0); ny <= std::min(ey + 2, rows - 1) && !shore; ny++)
								for (int32_t nx = std::max(ex - 2, 0); nx <= std::min(ex + 2, columns - 1) && !shore; nx++)
									shore = lakes[size_t(ny) * size_t(columns) + size_t(nx)] >= FarAway;
							if (shore)
								m_Ground[c] = { GroundWater, SplatDirt, lakes[c], 0.7f };
						}
				const SummedArea wetSum(wetCells, columns, rows);
				const bool cliffs = coverage[SplatGrass] <= 0.0f;
				const bool snowy = m_Level.Kind == MapKind::FrozenLake;
				const uint8_t cover = cliffs && !snowy ? SplatRock : SplatGrass;
				const float hillShade = snowy ? 0.95f : cliffs ? 0.5f : coverShade[SplatGrass] / coverage[SplatGrass];

				const int32_t fineWidth = columns * steps;
				const int32_t fineHeight = rows * steps;
				const size_t fineCount = size_t(fineWidth) * size_t(fineHeight);
				std::vector<float> valid(fineCount, 0.0f);
				std::vector<float> surface(fineCount, 0.0f);
				std::vector<float> pavedValid(fineCount, 0.0f);
				std::vector<float> pavedSurface(fineCount, 0.0f);
				for (int32_t b = 0; b < fineHeight; b++)
					for (int32_t a = 0; a < fineWidth; a++)
					{
						const int32_t x = a / steps - TerrainMargin;
						const int32_t y = b / steps - TerrainMargin;
						const GroundCell& ground = m_Ground[size_t(b / steps) * size_t(columns) + size_t(a / steps)];
						if (ground.Kind == GroundHill)
							continue;
						const size_t index = size_t(b) * size_t(fineWidth) + size_t(a);
						if (!m_Map.IsInside(x, y))
						{
							if (ground.Kind == GroundOpen || ground.Kind == GroundWater)
							{
								valid[index] = 1.0f;
								surface[index] = ground.Height;
							}
							continue;
						}
						if (ground.Kind == GroundPaved)
						{
							pavedValid[index] = 1.0f;
							pavedSurface[index] = ground.Height;
						}
						if (const std::optional<float> level = SurfaceAt(x, y, (float(a % steps) + 0.5f) / float(steps), (float(b % steps) + 0.5f) / float(steps)))
						{
							valid[index] = 1.0f;
							surface[index] = *level;
						}
					}
				struct Clearing
				{
					Vec2 Center;
					float Radius = 0.0f;
					float Base = 0.0f;
				};
				std::vector<Clearing> clearings;
				for (const PlacedProp& prop : m_Level.Props)
				{
					if (m_PropArt[size_t(prop.Id)].Natural)
						continue;
					float reach = 0.3f;
					for (const PropPart& part : GetProp(prop.Id).Parts)
						reach = std::max(reach, Length(part.Offset) + std::max(part.Half.x, part.Half.y));
					clearings.push_back({ prop.Position, reach + 0.15f, prop.Base });
				}
				const auto gridOf = [&](float world, int32_t limit, bool upper)
				{
					const float scaled = (world + float(TerrainMargin)) * float(steps);
					return std::clamp(int32_t(upper ? std::ceil(scaled) : std::floor(scaled)), 0, limit - 1);
				};
				for (const Clearing& clearing : clearings)
					for (int32_t b = gridOf(clearing.Center.y - clearing.Radius, fineHeight, false); b <= gridOf(clearing.Center.y + clearing.Radius, fineHeight, true); b++)
						for (int32_t a = gridOf(clearing.Center.x - clearing.Radius, fineWidth, false); a <= gridOf(clearing.Center.x + clearing.Radius, fineWidth, true); a++)
						{
							const Vec2 sample((float(a) + 0.5f) / float(steps) - float(TerrainMargin), (float(b) + 0.5f) / float(steps) - float(TerrainMargin));
							if (m_Ground[size_t(b / steps) * size_t(columns) + size_t(a / steps)].Kind != GroundHill || Length(sample - clearing.Center) > clearing.Radius)
								continue;
							const size_t index = size_t(b) * size_t(fineWidth) + size_t(a);
							valid[index] = 1.0f;
							surface[index] = clearing.Base;
						}
				const SummedArea validSum(valid, fineWidth, fineHeight);
				const SummedArea surfaceSum(surface, fineWidth, fineHeight);
				const SummedArea pavedValidSum(pavedValid, fineWidth, fineHeight);
				const SummedArea pavedSum(pavedSurface, fineWidth, fineHeight);
				const double mean = validSum.Total() > 0.0 ? surfaceSum.Total() / validSum.Total() : 0.0;
				std::vector<float> landFine(floating ? fineCount : size_t(0), 0.0f);
				for (size_t index = 0; index < landFine.size(); index++)
					landFine[index] = land[(index / size_t(fineWidth) / size_t(steps)) * size_t(columns) + (index % size_t(fineWidth)) / size_t(steps)];
				const SummedArea landSum(landFine, floating ? fineWidth : 0, floating ? fineHeight : 0);

				const int32_t vw = fineWidth + 1;
				const int32_t vh = fineHeight + 1;
				const size_t count = size_t(vw) * size_t(vh);
				const auto pointOf = [&](int32_t i, int32_t j) { return Vec2(float(i) / float(steps) - float(TerrainMargin), float(j) / float(steps) - float(TerrainMargin)); };
				std::vector<uint8_t> used(count, 0);
				std::vector<float> beds(count, FarAway);
				std::vector<float> outside(count, FarAway);
				std::vector<float> inside(count, FarAway);
				std::vector<float> paved(count, FarAway);
				std::vector<float> dry(count, FarAway);
				std::vector<float> ashore(floating ? count : size_t(0), FarAway);
				for (int32_t j = 0; j < vh; j++)
					for (int32_t i = 0; i < vw; i++)
					{
						const size_t k = size_t(j) * size_t(vw) + size_t(i);
						const int32_t ex1 = i / steps;
						const int32_t ey1 = j / steps;
						for (int32_t ey = std::max(j % steps == 0 ? ey1 - 1 : ey1, 0); ey <= std::min(ey1, rows - 1); ey++)
							for (int32_t ex = std::max(i % steps == 0 ? ex1 - 1 : ex1, 0); ex <= std::min(ex1, columns - 1); ex++)
							{
								const GroundCell& ground = m_Ground[size_t(ey) * size_t(columns) + size_t(ex)];
								if (lakes[size_t(ey) * size_t(columns) + size_t(ex)] >= FarAway)
									dry[k] = 0.0f;
								if (floating && land[size_t(ey) * size_t(columns) + size_t(ex)])
								{
									ashore[k] = 0.0f;
									used[k] = 1;
								}
								if (ground.Kind != GroundNone)
									used[k] = 1;
								(ground.Kind == GroundHill ? inside : outside)[k] = 0.0f;
								if (ground.Kind == GroundPaved)
								{
									paved[k] = 0.0f;
									beds[k] = std::min(beds[k], ground.Height);
								}
							}
					}
				std::vector<float> keep(count, 1.0f);
				std::vector<float> pads(count, 0.0f);
				for (const Clearing& clearing : clearings)
				{
					const float outer = clearing.Radius + 1.0f;
					for (int32_t j = gridOf(clearing.Center.y - outer, vh, false); j <= gridOf(clearing.Center.y + outer, vh, true); j++)
						for (int32_t i = gridOf(clearing.Center.x - outer, vw, false); i <= gridOf(clearing.Center.x + outer, vw, true); i++)
						{
							const size_t k = size_t(j) * size_t(vw) + size_t(i);
							const float distance = Length(pointOf(i, j) - clearing.Center);
							if (distance < clearing.Radius + 0.3f)
							{
								outside[k] = 0.0f;
								inside[k] = FarAway;
							}
							const float fade = Ease((distance - clearing.Radius) / (outer - clearing.Radius));
							if (fade < keep[k])
							{
								keep[k] = fade;
								pads[k] = clearing.Base;
							}
						}
				}
				DistanceField(outside, vw, vh);
				DistanceField(inside, vw, vh);
				DistanceField(paved, vw, vh);
				if (lakeside)
					DistanceField(dry, vw, vh);
				std::vector<float> field(count, 1.0f);
				std::vector<Vec2> fieldSlope(floating ? count : size_t(0));
				if (floating)
				{
					DistanceField(ashore, vw, vh);
					for (int32_t j = 0; j < vh; j++)
						for (int32_t i = 0; i < vw; i++)
						{
							const size_t k = size_t(j) * size_t(vw) + size_t(i);
							if (!used[k])
							{
								field[k] = -1.0f;
								continue;
							}
							const Vec2 point = pointOf(i, j);
							const float share = float(landSum.Mean(float(i), float(j), 0.75f * float(steps)));
							const float stone = 0.3f * (1.0f - Ease(paved[k] / float(steps) / 0.6f));
							field[k] = rounded ? CoastDistance(point) + 0.03f + 0.08f * (2.0f * Fbm(point * 0.6f, 593u, 2) - 1.0f) + 0.04f * (2.0f * Fbm(point * 2.2f, 597u, 2) - 1.0f)
								: share - 0.36f + 0.12f * (2.0f * Fbm(point * 0.9f, 591u, 2) - 1.0f) + stone;
						}
					m_Rim = { field, vw, vh };
					for (int32_t j = 0; j < vh; j++)
						for (int32_t i = 0; i < vw; i++)
						{
							const int32_t i0 = std::max(i - 1, 0);
							const int32_t i1 = std::min(i + 1, vw - 1);
							const int32_t j0 = std::max(j - 1, 0);
							const int32_t j1 = std::min(j + 1, vh - 1);
							const auto at = [&](int32_t a, int32_t b) { return field[size_t(b) * size_t(vw) + size_t(a)]; };
							fieldSlope[size_t(j) * size_t(vw) + size_t(i)] = Vec2((at(i1, j) - at(i0, j)) / float(i1 - i0), (at(i, j1) - at(i, j0)) / float(j1 - j0)) * float(steps);
						}
				}

				std::vector<float> depth(count, 0.0f);
				for (size_t k = 0; k < count; k++)
					depth[k] = std::clamp((outside[k] - inside[k]) / float(steps), 0.0f, 6.0f);
				const int32_t spread = 3 * steps;
				std::vector<float> line(size_t(std::max(vw, vh)), 0.0f);
				for (int32_t pass = 0; pass < 2; pass++)
				{
					const int32_t lines = pass == 0 ? vh : vw;
					const int32_t length = pass == 0 ? vw : vh;
					const size_t stride = pass == 0 ? 1 : size_t(vw);
					for (int32_t l = 0; l < lines; l++)
					{
						const size_t origin = pass == 0 ? size_t(l) * size_t(vw) : size_t(l);
						for (int32_t s = 0; s < length; s++)
							line[size_t(s)] = depth[origin + size_t(s) * stride];
						for (int32_t s = 0; s < length; s++)
						{
							float deepest = 0.0f;
							for (int32_t t = std::max(s - spread, 0); t <= std::min(s + spread, length - 1); t++)
								deepest = std::max(deepest, line[size_t(t)]);
							depth[origin + size_t(s) * stride] = deepest;
						}
					}
				}
				const SummedArea depthSum(depth, vw, vh);

				std::vector<float> heights(count, 0.0f);
				std::vector<float> hilly(count, 0.0f);
				std::vector<float> rocks(count, 0.0f);
				for (int32_t j = 0; j < vh; j++)
					for (int32_t i = 0; i < vw; i++)
					{
						const size_t k = size_t(j) * size_t(vw) + size_t(i);
						if (!used[k])
							continue;
						const Vec2 point = pointOf(i, j);
						const float fi = float(i);
						const float fj = float(j);
						const float signedDistance = (outside[k] - inside[k]) / float(steps);
						const bool slope = signedDistance > -1.2f;
						float lift = 0.0f;
						if (slope)
						{
							const float ridgeDepth = float(depthSum.Mean(fi + 0.5f, fj + 0.5f, 1.5f * float(steps)));
							const float wide = Ease((ridgeDepth - 0.5f) / 2.0f);
							const float reachOut = cliffs ? 0.15f : std::lerp(0.15f, 0.6f, wide);
							const float foot = signedDistance + std::lerp(0.12f, cliffs ? 0.3f : 0.5f, wide) * (2.0f * Fbm(point * 0.5f, 401u, 2) - 1.0f);
							hilly[k] = Ease((foot + reachOut * 0.5f) / (0.5f + reachOut));
							const float outcrop = signedDistance > 0.6f ? Ease((Fbm(point * 0.45f, 551u, 3) - 0.62f) / 0.08f) * Ease((signedDistance - 0.6f) / 1.2f) : 0.0f;
							rocks[k] = outcrop;
							const float peak = Fbm(point * 0.15f, 409u, 2);
							const float cap = cliffs ? std::min(1.8f + 0.5f * peak, 0.8f + 0.9f * ridgeDepth) : std::min(1.4f + 1.5f * peak, 0.7f + 0.8f * ridgeDepth);
							const float rise = std::lerp(0.6f, cliffs ? 1.5f : 3.5f, wide);
							lift = cap * Ease((foot + reachOut) / (rise + reachOut));
							if (signedDistance > 1.0f)
							{
								const float ridge = 1.0f - Ease(std::abs(Fbm(point * 0.15f, 419u, 2) - 0.5f) / 0.22f);
								lift += Ease((signedDistance - 1.0f) / 4.0f) * ((cliffs ? 1.2f : 4.0f) * (Fbm(point * 0.11f, 421u, 3) - 0.5f) + 0.45f * ridge);
							}
							if (outcrop > 0.0f)
								lift += outcrop * (0.8f + 1.2f * (Fbm(point * 1.6f, 561u, 2) - 0.5f));
						}
						if (beds[k] < FarAway)
						{
							heights[k] = LedgeHeight(point).value_or(beds[k]);
							continue;
						}
						const float reach = (0.35f + 1.4f * std::max(signedDistance, 0.0f)) * float(steps) + (floating && ashore[k] > 0.0f ? ashore[k] + 0.25f * float(steps) : 0.0f);
						const float base = float((surfaceSum.Around(fi, fj, reach) + 0.02 * mean) / (validSum.Around(fi, fj, reach) + 0.02));
						float ground = base + std::max(lift, 0.0f) + 0.03f * (2.0f * Fbm(point * 0.55f, 431u, 2) - 1.0f);
						ground = std::lerp(pads[k], ground, keep[k]);
						if (lakeside && wetSum.Mean(fi / float(steps), fj / float(steps), 1.6f) > 1e-6)
						{
							const Vec2 swirl = point + Vec2(float(TerrainMargin), float(TerrainMargin)) + Vec2(Fbm(point * 0.45f, 581u, 2) - 0.5f, Fbm(point * 0.45f, 583u, 2) - 0.5f) * 1.1f;
							ground -= 0.65f * Ease((float(wetSum.Mean(swirl.x, swirl.y, 0.9f)) - 0.15f) / 0.55f);
						}
						const float near = paved[k] / float(steps);
						if (near < PavingBlend)
						{
							const float span = (near + 0.4f) * float(steps);
							const double weight = pavedValidSum.Around(fi, fj, span);
							if (weight > 1e-3)
								ground = std::lerp(float(pavedSum.Around(fi, fj, span) / weight), ground, Ease(near / PavingBlend));
						}
						if (const std::optional<float> crisp = LedgeHeight(point))
							ground = *crisp + CrispNoise(point);
						if (rounded && floating && field[k] < 0.4f)
							ground -= 0.13f * Ease((Fbm(point * 1.1f, 611u, 2) - 0.42f) / 0.3f) * (1.0f - Ease(field[k] / 0.4f));
						heights[k] = ground;
					}

				const auto gridIndex = [&](Vec2 point) { return size_t(std::lround((point.y + float(TerrainMargin)) * float(steps))) * size_t(vw) + size_t(std::lround((point.x + float(TerrainMargin)) * float(steps))); };
				const auto brinkHeight = [&](size_t k, TileCoord region, Vec2 point) { return RegionTop(region, point) + (beds[k] < FarAway ? 0.0f : CrispNoise(point)); };
				std::vector<uint8_t> brink(rounded ? count : size_t(0), 0);
				std::unordered_set<uint64_t> folds;
				std::vector<float> ledgeNear(rounded ? count : size_t(0), FarAway);
				for (const LedgeRun& run : m_LedgeRuns)
					for (size_t s = 0; s < run.Spots.size(); s++)
					{
						const size_t k = gridIndex(run.Spots[s].Position);
						brink[k] = 1;
						ledgeNear[k] = 0.0f;
						if (s + 1 == run.Spots.size())
							continue;
						const size_t next = gridIndex(run.Spots[s + 1].Position);
						const int32_t di = int32_t(next % size_t(vw)) - int32_t(k % size_t(vw));
						const int32_t dj = int32_t(next / size_t(vw)) - int32_t(k / size_t(vw));
						if (di != 0 && dj != 0)
							folds.insert(FoldKey(size_t(std::min(int32_t(k % size_t(vw)), int32_t(next % size_t(vw)))), size_t(std::min(int32_t(k / size_t(vw)), int32_t(next / size_t(vw)))), size_t(vw), di * dj > 0));
					}
				if (rounded)
					DistanceField(ledgeNear, vw, vh);

				std::vector<glm::vec3> normals(count, glm::vec3(0.0f, 1.0f, 0.0f));
				std::vector<glm::vec2> slopes(count, glm::vec2(0.0f));
				for (int32_t j = 0; j < vh; j++)
					for (int32_t i = 0; i < vw; i++)
					{
						const size_t k = size_t(j) * size_t(vw) + size_t(i);
						if (!used[k])
							continue;
						const auto difference = [&](bool before, bool after, size_t back, size_t ahead)
						{
							if (before && after)
								return (heights[ahead] - heights[back]) * float(steps) * 0.5f;
							if (after)
								return (heights[ahead] - heights[k]) * float(steps);
							if (before)
								return (heights[k] - heights[back]) * float(steps);
							return 0.0f;
						};
						const size_t row = size_t(vw);
						const auto joined = [&](size_t other) { return used[other] && (!rounded || std::abs(heights[other] - heights[k]) < MaxStepHeight); };
						slopes[k].x = difference(i > 0 && joined(k - 1), i + 1 < vw && joined(k + 1), k - 1, k + 1);
						slopes[k].y = difference(j > 0 && joined(k - row), j + 1 < vh && joined(k + row), k - row, k + row);
						normals[k] = glm::normalize(glm::vec3(-slopes[k].x, 1.0f, -slopes[k].y));
					}

				std::vector<float> bottoms(floating ? count : size_t(0), 0.0f);
				std::vector<glm::vec3> bottomNormals(floating ? count : size_t(0), glm::vec3(0.0f, -1.0f, 0.0f));
				if (floating)
				{
					std::vector<float> inward(count, FarAway);
					std::vector<float> mass(count, 0.0f);
					std::vector<float> weight(count, 0.0f);
					for (size_t k = 0; k < count; k++)
					{
						if (field[k] < 0.0f)
							inward[k] = 0.0f;
						if (used[k])
						{
							mass[k] = heights[k];
							weight[k] = 1.0f;
						}
					}
					DistanceField(inward, vw, vh);
					const SummedArea massSum(mass, vw, vh);
					const SummedArea weightSum(weight, vw, vh);
					for (int32_t j = 0; j < vh; j++)
						for (int32_t i = 0; i < vw; i++)
						{
							const size_t k = size_t(j) * size_t(vw) + size_t(i);
							if (!used[k])
								continue;
							const Vec2 point = pointOf(i, j);
							const float d = inward[k] / float(steps);
							const float rim = 1.2f + 0.6f * Fbm(point * 0.4f, 601u, 2);
							const double spread = weightSum.Around(float(i) + 0.5f, float(j) + 0.5f, 2.0f * float(steps));
							const float smooth = spread > 0.5 ? float(massSum.Around(float(i) + 0.5f, float(j) + 0.5f, 2.0f * float(steps)) / spread) : heights[k];
							const float bulk = (0.7f + 0.6f * Fbm(point * 0.2f, 607u, 2)) * (14.0f * (1.0f - std::exp(-d / 2.2f)) + 10.0f * (1.0f - std::exp(-d / 8.0f)));
							const float spikes = 3.0f * Ease((Fbm(point * 0.55f, 613u, 2) - 0.56f) / 0.18f) * Ease(d / 2.0f);
							bottoms[k] = std::min(heights[k] - rim, smooth - rim - bulk - spikes);
						}
					for (int32_t j = 0; j < vh; j++)
						for (int32_t i = 0; i < vw; i++)
						{
							const size_t k = size_t(j) * size_t(vw) + size_t(i);
							if (!used[k])
								continue;
							const size_t row = size_t(vw);
							const auto difference = [&](bool before, bool after, size_t back, size_t ahead)
							{
								if (before && after)
									return (bottoms[ahead] - bottoms[back]) * float(steps) * 0.5f;
								if (after)
									return (bottoms[ahead] - bottoms[k]) * float(steps);
								if (before)
									return (bottoms[k] - bottoms[back]) * float(steps);
								return 0.0f;
							};
							const float dx = difference(i > 0 && used[k - 1], i + 1 < vw && used[k + 1], k - 1, k + 1);
							const float dz = difference(j > 0 && used[k - row], j + 1 < vh && used[k + row], k - row, k + row);
							bottomNormals[k] = glm::normalize(glm::vec3(dx, -1.0f, dz));
						}
				}

				const size_t cellCount = size_t(columns) * size_t(rows);
				std::array<std::vector<float>, 4> layerCells;
				for (std::vector<float>& cells : layerCells)
					cells.assign(cellCount, 0.0f);
				std::vector<float> validCells(cellCount, 0.0f);
				std::vector<float> shadeCells(cellCount, 0.0f);
				for (size_t c = 0; c < cellCount; c++)
				{
					const GroundCell& ground = m_Ground[c];
					if (ground.Kind != GroundOpen && ground.Kind != GroundPaved && ground.Kind != GroundWater)
						continue;
					layerCells[ground.Layer][c] = 1.0f;
					validCells[c] = 1.0f;
					shadeCells[c] = ground.Shade;
				}
				const std::array<SummedArea, 4> layerSums = { SummedArea(layerCells[0], columns, rows), SummedArea(layerCells[1], columns, rows), SummedArea(layerCells[2], columns, rows), SummedArea(layerCells[3], columns, rows) };
				const SummedArea validCellSum(validCells, columns, rows);
				const SummedArea shadeSum(shadeCells, columns, rows);
				std::vector<uint32_t> colors(count, 0u);
				for (int32_t j = 0; j < vh; j++)
					for (int32_t i = 0; i < vw; i++)
					{
						const size_t k = size_t(j) * size_t(vw) + size_t(i);
						if (!used[k])
							continue;
						const Vec2 point = pointOf(i, j);
						const Vec2 cell = point + Vec2(float(TerrainMargin), float(TerrainMargin));
						const Vec2 swirl = cell + Vec2(Fbm(point * 0.9f, 501u, 2) - 0.5f, Fbm(point * 0.9f, 503u, 2) - 0.5f) * 0.9f;
						std::array<float, 4> weights{};
						const double open = validCellSum.Around(swirl.x, swirl.y, 0.55f);
						if (open > 1e-4)
							for (size_t layer = 0; layer < 4; layer++)
								weights[layer] = float(layerSums[layer].Around(swirl.x, swirl.y, 0.55f) / open);
						else
							weights[cover] = 1.0f;
						const double near = validCellSum.Around(cell.x, cell.y, 0.2f);
						float shade = near > 1e-4 ? float(shadeSum.Around(cell.x, cell.y, 0.2f) / near) : hillShade;
						const float patch = weights[SplatGrass] > 0.0f ? 0.9f * Ease((Fbm(point * 0.33f, 521u, 3) - 0.62f) / 0.1f) : 0.0f;
						const float worn = paved[k] < 1.3f * float(steps) ? 0.85f * (1.0f - Ease((paved[k] / float(steps) + 0.4f * (Fbm(point * 1.3f, 531u, 2) - 0.5f)) / 0.9f)) : 0.0f;
						const float fromGrass = weights[SplatGrass] * std::max(patch, worn);
						const float fromSand = weights[SplatSand] * worn;
						weights[SplatGrass] -= fromGrass;
						weights[SplatSand] -= fromSand;
						weights[SplatDirt] += fromGrass + fromSand;
						if (hilly[k] > 0.0f)
						{
							const float rocky = std::max(Ease((glm::length(slopes[k]) - 0.8f + 0.4f * (Fbm(point * 0.8f, 511u, 2) - 0.5f)) / 0.35f), rocks[k]);
							for (uint8_t layer = 0; layer < 4; layer++)
							{
								const float hill = std::lerp(layer == cover ? 1.0f : 0.0f, layer == SplatRock ? 1.0f : 0.0f, rocky);
								weights[layer] = std::lerp(weights[layer], hill, hilly[k]);
							}
							shade = std::lerp(shade, hillShade * (0.8f + 0.35f * Fbm(point * 0.09f, 543u, 2)), hilly[k]);
						}
						shade *= 0.9f + 0.14f * Fbm(point * 0.25f, 541u, 2);
						if (const float steep = Ease((glm::length(slopes[k]) - 1.6f) / 1.2f); steep > 0.0f)
						{
							for (uint8_t layer = 0; layer < 4; layer++)
								weights[layer] = std::lerp(weights[layer], layer == SplatRock ? 1.0f : 0.0f, 0.85f * steep);
							shade *= 1.0f - 0.2f * steep;
						}
						if (rounded && ledgeNear[k] < 2.0f * float(steps))
						{
							const float edge = (1.0f - Ease((ledgeNear[k] / float(steps) - 0.05f) / 0.45f)) * (0.55f + 0.45f * Fbm(point * 1.7f, 573u, 2));
							for (uint8_t layer = 0; layer < 4; layer++)
								weights[layer] = std::lerp(weights[layer], layer == SplatRock ? 0.55f : layer == SplatDirt ? 0.45f : 0.0f, 0.85f * edge);
							shade *= 1.0f - 0.12f * edge;
						}
						if (floating && field[k] < 0.25f)
						{
							const float lip = 0.9f * (1.0f - Ease(field[k] / 0.25f));
							for (uint8_t layer = 0; layer < 4; layer++)
								weights[layer] = std::lerp(weights[layer], layer == SplatRock ? 1.0f : 0.0f, lip);
							shade *= 1.0f - 0.25f * lip;
						}
						colors[k] = PackSplat(weights, shade);
					}

				const auto tangentOf = [](const glm::vec3& normal, const glm::vec3& dPdu, const glm::vec3& dPdv)
				{
					glm::vec3 tangent = dPdu - normal * glm::dot(normal, dPdu);
					tangent = glm::length(tangent) > 1e-6f ? glm::normalize(tangent) : glm::vec3(1.0f, 0.0f, 0.0f);
					return PackTangent(tangent, glm::dot(glm::cross(normal, tangent), -dPdv) >= 0.0f ? 1.0f : -1.0f);
				};
				const auto gridVertex = [&](int32_t i, int32_t j)
				{
					const size_t k = size_t(j) * size_t(vw) + size_t(i);
					const Vec2 point = pointOf(i, j);
					ModelVertex vertex;
					vertex.Position = { point.x, heights[k], point.y };
					vertex.U = point.x;
					vertex.V = point.y;
					vertex.Normal = normals[k];
					vertex.Weights = 255;
					vertex.Tangent = tangentOf(normals[k], glm::vec3(1.0f, slopes[k].x, 0.0f), glm::vec3(0.0f, slopes[k].y, 1.0f));
					vertex.Color = colors[k];
					return vertex;
				};
				const uint32_t material = m_Splat[SplatGrass] | (m_Splat[SplatDirt] << 16);
				const uint32_t layer = m_Splat[SplatRock] | (m_Splat[SplatSand] << 16);
				std::vector<ModelVertex> vertices;
				std::vector<uint32_t> indices;
				std::vector<LevelMeshDesc> meshes;
				std::vector<std::pair<glm::vec3, glm::vec3>> bounds;
				struct ClipPoint
				{
					size_t A = 0;
					size_t B = 0;
					float T = 0.0f;
					uint64_t Key = 0;
					bool Cross = false;
					bool Exit = false;
				};
				const auto pointAt = [&](size_t k) { return pointOf(int32_t(k % size_t(vw)), int32_t(k / size_t(vw))); };
				const auto edgeKey = [&](int32_t i, int32_t j, uint64_t vertical) { return ((uint64_t(j) * uint64_t(vw) + uint64_t(i)) << 1) | vertical; };
				std::vector<ClipPoint> polygon;
				const auto clip = [&](int32_t i, int32_t j)
				{
					polygon.clear();
					const size_t k = size_t(j) * size_t(vw) + size_t(i);
					const size_t row = size_t(vw);
					const size_t corners[4] = { k, k + 1, k + row + 1, k + row };
					const uint64_t edges[4] = { edgeKey(i, j, 0), edgeKey(i + 1, j, 1), edgeKey(i, j + 1, 0), edgeKey(i, j, 1) };
					for (size_t n = 0; n < 4; n++)
					{
						const size_t a = corners[n];
						const size_t b = corners[(n + 1) % 4];
						const bool inA = field[a] >= 0.0f;
						const bool inB = field[b] >= 0.0f;
						if (inA)
							polygon.push_back({ a, a, 0.0f, 0, false, false });
						if (inA != inB)
							polygon.push_back({ a, b, field[a] / (field[a] - field[b]), edges[n], true, inA });
					}
				};
				const auto spotOf = [&](const ClipPoint& point) { return Lerp(pointAt(point.A), pointAt(point.B), point.T); };
				const uint32_t rockColor = PackSplat({ 0.0f, 0.0f, 1.0f, 0.0f }, 0.62f);
				for (int32_t cy = 0; cy * ChunkSize < rows; cy++)
					for (int32_t cx = 0; cx * ChunkSize < columns; cx++)
					{
						const int32_t ex0 = cx * ChunkSize;
						const int32_t ey0 = cy * ChunkSize;
						const int32_t ex1 = std::min(ex0 + ChunkSize, columns);
						const int32_t ey1 = std::min(ey0 + ChunkSize, rows);
						const uint32_t first = uint32_t(vertices.size());
						const uint32_t firstIndex = uint32_t(indices.size());
						const int32_t span = (ex1 - ex0) * steps + 1;
						glm::vec3 low(1e9f);
						glm::vec3 high(-1e9f);
						for (int32_t j = ey0 * steps; j <= ey1 * steps; j++)
							for (int32_t i = ex0 * steps; i <= ex1 * steps; i++)
							{
								vertices.push_back(gridVertex(i, j));
								if (used[size_t(j) * size_t(vw) + size_t(i)])
								{
									low = glm::min(low, vertices.back().Position);
									high = glm::max(high, vertices.back().Position);
								}
							}
						const auto local = [&](int32_t i, int32_t j) { return first + uint32_t((j - ey0 * steps) * span + (i - ex0 * steps)); };
						std::unordered_map<uint64_t, uint32_t> crossings;
						const auto topIndex = [&](const ClipPoint& point)
						{
							if (!point.Cross)
								return local(int32_t(point.A % size_t(vw)), int32_t(point.A / size_t(vw)));
							if (const auto found = crossings.find(point.Key); found != crossings.end())
								return found->second;
							const Vec2 spot = spotOf(point);
							const glm::vec3 normal = glm::normalize(glm::mix(normals[point.A], normals[point.B], point.T));
							const glm::vec2 slope = glm::mix(slopes[point.A], slopes[point.B], point.T);
							ModelVertex vertex;
							vertex.Position = { spot.x, std::lerp(heights[point.A], heights[point.B], point.T), spot.y };
							vertex.U = spot.x;
							vertex.V = spot.y;
							vertex.Normal = normal;
							vertex.Weights = 255;
							vertex.Tangent = tangentOf(normal, glm::vec3(1.0f, slope.x, 0.0f), glm::vec3(0.0f, slope.y, 1.0f));
							vertex.Color = MixColor(colors[point.A], colors[point.B], point.T);
							const uint32_t index = uint32_t(vertices.size());
							vertices.push_back(vertex);
							crossings.emplace(point.Key, index);
							return index;
						};
						std::unordered_map<uint64_t, uint32_t> copies;
						const auto corner = [&](size_t k, TileCoord region)
						{
							const int32_t i = int32_t(k % size_t(vw));
							const int32_t j = int32_t(k / size_t(vw));
							if (!rounded || !brink[k] || !IsGroundRegion(region))
								return local(i, j);
							const Vec2 point = pointOf(i, j);
							const float height = brinkHeight(k, region, point);
							if (std::abs(height - heights[k]) < 0.01f)
								return local(i, j);
							const uint64_t key = (uint64_t(k) << 16) | (uint64_t(uint8_t(region.X)) << 8) | uint64_t(uint8_t(region.Y));
							if (const auto found = copies.find(key); found != copies.end())
								return found->second;
							ModelVertex vertex = vertices[local(i, j)];
							vertex.Position.y = height;
							vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
							vertex.Tangent = tangentOf(vertex.Normal, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
							const uint32_t index = uint32_t(vertices.size());
							vertices.push_back(vertex);
							low = glm::min(low, vertex.Position);
							copies.emplace(key, index);
							return index;
						};
						for (int32_t ey = ey0; ey < ey1; ey++)
							for (int32_t ex = ex0; ex < ex1; ex++)
							{
								const GroundCell& ground = m_Ground[size_t(ey) * size_t(columns) + size_t(ex)];
								if (ground.Kind == GroundNone)
									continue;
								for (int32_t b = 0; b < steps; b++)
									for (int32_t a = 0; a < steps; a++)
									{
										const int32_t i = ex * steps + a;
										const int32_t j = ey * steps + b;
										const size_t k = size_t(j) * size_t(vw) + size_t(i);
										const size_t row = size_t(vw);
										if (floating && std::min({ field[k], field[k + 1], field[k + row], field[k + row + 1] }) < 0.0f)
										{
											if (std::max({ field[k], field[k + 1], field[k + row], field[k + row + 1] }) < 0.0f)
												continue;
											clip(i, j);
											for (size_t m = 1; m + 1 < polygon.size(); m++)
											{
												const bool folded = rounded && (brink[polygon[0].A] | brink[polygon[m].A] | brink[polygon[m + 1].A]);
												const TileCoord region = folded ? m_Map.CellAt((spotOf(polygon[0]) + spotOf(polygon[m]) + spotOf(polygon[m + 1])) / 3.0f) : TileCoord{};
												const auto pick = [&](const ClipPoint& point) { return folded && !point.Cross ? corner(point.A, region) : topIndex(point); };
												const uint32_t p0 = pick(polygon[0]);
												const uint32_t p1 = pick(polygon[m + 1]);
												const uint32_t p2 = pick(polygon[m]);
												indices.insert(indices.end(), { p0, p1, p2 });
											}
											continue;
										}
										const bool main = std::abs(heights[k] - heights[k + row + 1]) <= std::abs(heights[k + 1] - heights[k + row]);
										if (rounded && (brink[k] | brink[k + 1] | brink[k + row] | brink[k + row + 1]))
										{
											const bool along = folds.contains(FoldKey(size_t(i), size_t(j), size_t(vw), true));
											const bool against = folds.contains(FoldKey(size_t(i), size_t(j), size_t(vw), false));
											using Triangle = std::array<size_t, 3>;
											const std::array<Triangle, 2> triangles = (along != against ? along : main)
												? std::array<Triangle, 2>{ { { k, k + row + 1, k + 1 }, { k, k + row, k + row + 1 } } }
												: std::array<Triangle, 2>{ { { k, k + row, k + 1 }, { k + 1, k + row, k + row + 1 } } };
											for (const Triangle& triangle : triangles)
											{
												const TileCoord region = m_Map.CellAt((pointAt(triangle[0]) + pointAt(triangle[1]) + pointAt(triangle[2])) / 3.0f);
												for (const size_t vertex : triangle)
													indices.push_back(corner(vertex, region));
											}
											continue;
										}
										const uint32_t v00 = local(i, j);
										const uint32_t v10 = local(i + 1, j);
										const uint32_t v01 = local(i, j + 1);
										const uint32_t v11 = local(i + 1, j + 1);
										if (main)
											indices.insert(indices.end(), { v00, v11, v10, v00, v01, v11 });
										else
											indices.insert(indices.end(), { v00, v01, v10, v10, v01, v11 });
									}
								if (ground.Kind == GroundWater)
									continue;
								const int32_t x = ex - TerrainMargin;
								const int32_t y = ey - TerrainMargin;
								for (uint32_t side = 0; side < 4; side++)
								{
									int32_t nx = x;
									int32_t ny = y;
									TileMap::Step(Direction(side), nx, ny);
									const GroundCell* neighbor = FindGround(nx, ny);
									if (!neighbor || neighbor->Kind != GroundNone || (floating && isVoid(nx, ny)))
										continue;
									const bool across = Direction(side) == Direction::North || Direction(side) == Direction::South;
									const int32_t i0 = (Direction(side) == Direction::East ? ex + 1 : ex) * steps;
									const int32_t j0 = (Direction(side) == Direction::North ? ey + 1 : ey) * steps;
									if (rounded && IsStepCell(nx, ny) && brink[size_t(j0 + (across ? 0 : steps / 2)) * size_t(vw) + size_t(i0 + (across ? steps / 2 : 0))])
										continue;
									const glm::vec3 outward(float(nx - x), 0.0f, float(ny - y));
									const glm::vec3 along = across ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
									const uint32_t start = uint32_t(vertices.size());
									for (int32_t s = 0; s <= steps; s++)
									{
										const int32_t i = i0 + (across ? s : 0);
										const int32_t j = j0 + (across ? 0 : s);
										const size_t k = size_t(j) * size_t(vw) + size_t(i);
										const Vec2 point = pointOf(i, j);
										const float top = heights[k];
										const float edge = m_Map.IsInside(nx, ny) ? NeighborEdge(x, y, Direction(side), top) : NoFace;
										const float bottom = (edge == NoFace ? top - 1.0f : std::min(top, edge)) - SkirtDrop;
										std::array<float, 4> mix{};
										mix[((colors[k] >> 8) & 0xFFu) > 127u ? SplatRock : SplatDirt] = 1.0f;
										const float shade = float(colors[k] >> 24) / 255.0f;
										for (const float level : { top, bottom })
										{
											ModelVertex vertex;
											vertex.Position = { point.x, level, point.y };
											vertex.U = across ? point.x : point.y;
											vertex.V = -level;
											vertex.Normal = outward;
											vertex.Weights = 255;
											vertex.Tangent = tangentOf(outward, along, glm::vec3(0.0f, -1.0f, 0.0f));
											vertex.Color = PackSplat(mix, shade * (level == top ? 0.8f : 0.3f));
											vertices.push_back(vertex);
											low = glm::min(low, vertex.Position);
											high = glm::max(high, vertex.Position);
										}
									}
									for (int32_t s = 0; s < steps; s++)
									{
										const uint32_t topA = start + uint32_t(s) * 2u;
										const uint32_t bottomA = topA + 1u;
										const uint32_t topB = topA + 2u;
										const uint32_t bottomB = topA + 3u;
										const glm::vec3 a = vertices[topA].Position;
										if (glm::dot(glm::cross(vertices[topB].Position - a, vertices[bottomB].Position - a), outward) >= 0.0f)
											indices.insert(indices.end(), { topA, topB, bottomB, topA, bottomB, bottomA });
										else
											indices.insert(indices.end(), { topA, bottomB, topB, topA, bottomA, bottomB });
									}
								}
							}
						if (uint32_t(indices.size()) == firstIndex)
							vertices.resize(first);
						else
						{
							meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(indices.size()) - firstIndex, .Material = material, .Layer = layer, .Flags = MeshSplat });
							bounds.push_back({ low, high });
						}
						if (!floating)
							continue;
						const uint32_t lowerFirst = uint32_t(vertices.size());
						const uint32_t lowerIndex = uint32_t(indices.size());
						glm::vec3 lowerLow(1e9f);
						glm::vec3 lowerHigh(-1e9f);
						const auto grow = [&](const glm::vec3& position)
						{
							lowerLow = glm::min(lowerLow, position);
							lowerHigh = glm::max(lowerHigh, position);
						};
						for (int32_t j = ey0 * steps; j <= ey1 * steps; j++)
							for (int32_t i = ex0 * steps; i <= ex1 * steps; i++)
							{
								const size_t k = size_t(j) * size_t(vw) + size_t(i);
								const Vec2 point = pointOf(i, j);
								ModelVertex vertex;
								vertex.Position = { point.x, bottoms[k], point.y };
								vertex.U = point.x;
								vertex.V = point.y;
								vertex.Normal = bottomNormals[k];
								vertex.Weights = 255;
								vertex.Color = rockColor;
								vertices.push_back(vertex);
								if (used[k])
									grow(vertex.Position);
							}
						const auto lowerLocal = [&](int32_t i, int32_t j) { return lowerFirst + uint32_t((j - ey0 * steps) * span + (i - ex0 * steps)); };
						std::unordered_map<uint64_t, uint32_t> lowerCrossings;
						const auto lowerOf = [&](const ClipPoint& point)
						{
							if (!point.Cross)
								return lowerLocal(int32_t(point.A % size_t(vw)), int32_t(point.A / size_t(vw)));
							if (const auto found = lowerCrossings.find(point.Key); found != lowerCrossings.end())
								return found->second;
							const Vec2 spot = spotOf(point);
							ModelVertex vertex;
							vertex.Position = { spot.x, std::lerp(bottoms[point.A], bottoms[point.B], point.T), spot.y };
							vertex.U = spot.x;
							vertex.V = spot.y;
							vertex.Normal = glm::normalize(glm::mix(bottomNormals[point.A], bottomNormals[point.B], point.T));
							vertex.Weights = 255;
							vertex.Color = rockColor;
							const uint32_t index = uint32_t(vertices.size());
							vertices.push_back(vertex);
							lowerCrossings.emplace(point.Key, index);
							return index;
						};
						const auto outwardAt = [&](const ClipPoint& point, Vec2 fallback)
						{
							const Vec2 gradient = Lerp(fieldSlope[point.A], fieldSlope[point.B], point.T);
							return LengthSquared(gradient) > 1e-6f ? Normalize(gradient) * -1.0f : fallback;
						};
						const auto cliff = [&](const ClipPoint& exit, const ClipPoint& entry)
						{
							const Vec2 a = spotOf(exit);
							const Vec2 b = spotOf(entry);
							if (LengthSquared(b - a) < 1e-8f)
								return;
							Vec2 facing = Normalize(Vec2(b.y - a.y, a.x - b.x));
							if (Dot(facing, pointAt(exit.B) - a) < 0.0f)
								facing = facing * -1.0f;
							const Vec2 outA = outwardAt(exit, facing);
							const Vec2 outB = outwardAt(entry, facing);
							const float topA = std::lerp(heights[exit.A], heights[exit.B], exit.T);
							const float topB = std::lerp(heights[entry.A], heights[entry.B], entry.T);
							const float bottomA = std::lerp(bottoms[exit.A], bottoms[exit.B], exit.T);
							const float bottomB = std::lerp(bottoms[entry.A], bottoms[entry.B], entry.T);
							const uint32_t start = uint32_t(vertices.size());
							for (int32_t c = 0; c <= CliffColumns; c++)
							{
								const float s = float(c) / float(CliffColumns);
								const Vec2 base = Lerp(a, b, s);
								const Vec2 outward = Normalize(Lerp(outA, outB, s));
								const float top = std::lerp(topA, topB, s);
								const float bottom = std::lerp(bottomA, bottomB, s);
								for (int32_t r = 0; r <= CliffRows; r++)
								{
									const float t = float(r) / float(CliffRows);
									ModelVertex vertex;
									vertex.Position = CliffFace(base, outward, top, bottom, t);
									vertex.Normal = FaceNormal(CliffFace, base, outward, top, bottom, t);
									vertex.U = base.x + base.y;
									vertex.V = -vertex.Position.y;
									vertex.Weights = 255;
									vertex.Color = WallColor({ base.x, std::lerp(top, bottom, t), base.y }, vertex.Normal, 0.7f);
									vertices.push_back(vertex);
									grow(vertex.Position);
								}
							}
							const bool forward = Dot(Perpendicular(facing), b - a) >= 0.0f;
							for (int32_t c = 0; c < CliffColumns; c++)
								for (int32_t r = 0; r < CliffRows; r++)
								{
									const uint32_t v00 = start + uint32_t(c * (CliffRows + 1) + r);
									const uint32_t v10 = v00 + uint32_t(CliffRows + 1);
									if (forward)
										indices.insert(indices.end(), { v00, v10, v10 + 1, v00, v10 + 1, v00 + 1 });
									else
										indices.insert(indices.end(), { v00, v10 + 1, v10, v00, v00 + 1, v10 + 1 });
								}
							m_WallSpots.push_back({ .Base = Lerp(a, b, 0.5f), .Outward = Normalize(outA + outB), .Top = 0.5f * (topA + topB), .Bottom = 0.5f * (bottomA + bottomB), .Cliff = true });
						};
						for (int32_t ey = ey0; ey < ey1; ey++)
							for (int32_t ex = ex0; ex < ex1; ex++)
							{
								const size_t c = size_t(ey) * size_t(columns) + size_t(ex);
								if (!land[c] && m_Ground[c].Kind != GroundRim)
									continue;
								for (int32_t b = 0; b < steps; b++)
									for (int32_t a = 0; a < steps; a++)
									{
										const int32_t i = ex * steps + a;
										const int32_t j = ey * steps + b;
										const size_t k = size_t(j) * size_t(vw) + size_t(i);
										const size_t row = size_t(vw);
										if (std::max({ field[k], field[k + 1], field[k + row], field[k + row + 1] }) < 0.0f)
											continue;
										if (std::min({ field[k], field[k + 1], field[k + row], field[k + row + 1] }) >= 0.0f)
										{
											const uint32_t v00 = lowerLocal(i, j);
											const uint32_t v10 = lowerLocal(i + 1, j);
											const uint32_t v01 = lowerLocal(i, j + 1);
											const uint32_t v11 = lowerLocal(i + 1, j + 1);
											indices.insert(indices.end(), { v00, v10, v11, v00, v11, v01 });
											continue;
										}
										clip(i, j);
										for (size_t m = 1; m + 1 < polygon.size(); m++)
										{
											const uint32_t p0 = lowerOf(polygon[0]);
											const uint32_t p1 = lowerOf(polygon[m]);
											const uint32_t p2 = lowerOf(polygon[m + 1]);
											indices.insert(indices.end(), { p0, p1, p2 });
										}
										for (size_t m = 0; m < polygon.size(); m++)
										{
											const ClipPoint& exit = polygon[m];
											const ClipPoint& entry = polygon[(m + 1) % polygon.size()];
											if (exit.Cross && exit.Exit && entry.Cross && !entry.Exit)
												cliff(exit, entry);
										}
									}
							}
						if (uint32_t(indices.size()) == lowerIndex)
						{
							vertices.resize(lowerFirst);
							continue;
						}
						meshes.push_back({ .FirstIndex = lowerIndex, .IndexCount = uint32_t(indices.size()) - lowerIndex, .Material = material, .Layer = layer, .Flags = MeshSplat | MeshVeins | MeshGround });
						bounds.push_back({ lowerLow, lowerHigh });
					}
				for (const LedgeRun& run : m_LedgeRuns)
				{
					struct Column
					{
						Vec2 Base;
						Vec2 Outward;
						float High = 0.0f;
						float Low = 0.0f;
						float Arc = 0.0f;
					};
					std::vector<Column> line;
					for (size_t s = 0; s + 1 < run.Spots.size(); s++)
					{
						const LedgeSpot& from = run.Spots[s];
						const LedgeSpot& to = run.Spots[s + 1];
						const size_t a = gridIndex(from.Position);
						const size_t b = gridIndex(to.Position);
						const float lowA = brinkHeight(a, from.Low, from.Position);
						const float lowB = brinkHeight(b, from.Low, to.Position);
						for (int32_t c = 0; c < LedgeColumns; c++)
						{
							const float f = float(c) / float(LedgeColumns);
							line.push_back({ Lerp(from.Position, to.Position, f), Normalize(Lerp(from.Outward, to.Outward, f)), std::lerp(heights[a], heights[b], f), std::lerp(lowA, lowB, f), std::lerp(from.Arc, to.Arc, f) });
						}
					}
					const LedgeSpot& last = run.Spots.back();
					const size_t end = gridIndex(last.Position);
					line.push_back({ last.Position, last.Outward, heights[end], brinkHeight(end, last.Low, last.Position), last.Arc });
					float tallest = 0.0f;
					for (const Column& column : line)
						tallest = std::max(tallest, column.High - column.Low);
					const int32_t tiers = std::clamp(int32_t(std::ceil((tallest + LedgeBury) / 0.12f)), 5, 14);
					for (size_t first = 0; first + 1 < line.size(); first += 48)
					{
						const size_t final = std::min(first + 48, line.size() - 1);
						const uint32_t start = uint32_t(vertices.size());
						const uint32_t firstIndex = uint32_t(indices.size());
						glm::vec3 low(1e9f);
						glm::vec3 high(-1e9f);
						for (size_t c = first; c <= final; c++)
						{
							const Column& column = line[c];
							const Vec2 along = Perpendicular(column.Outward);
							for (int32_t r = 0; r <= tiers; r++)
							{
								const float t = float(r) / float(tiers);
								ModelVertex vertex;
								vertex.Position = LedgeFace(column.Base, column.Outward, column.High, column.Low, t);
								vertex.Normal = FaceNormal(LedgeFace, column.Base, column.Outward, column.High, column.Low, t);
								vertex.U = column.Arc;
								vertex.V = -vertex.Position.y;
								vertex.Weights = 255;
								vertex.Tangent = tangentOf(vertex.Normal, glm::vec3(along.x, 0.0f, along.y), glm::vec3(0.0f, -1.0f, 0.0f));
								vertex.Color = WallColor({ column.Base.x, std::lerp(column.High, column.Low - LedgeBury, t), column.Base.y }, vertex.Normal, 0.8f);
								vertices.push_back(vertex);
								low = glm::min(low, vertex.Position);
								high = glm::max(high, vertex.Position);
							}
						}
						for (size_t c = first; c < final; c++)
							for (int32_t r = 0; r < tiers; r++)
							{
								const uint32_t v00 = start + uint32_t((c - first) * size_t(tiers + 1) + size_t(r));
								const uint32_t v10 = v00 + uint32_t(tiers + 1);
								indices.insert(indices.end(), { v00, v10, v10 + 1, v00, v10 + 1, v00 + 1 });
							}
						meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(indices.size()) - firstIndex, .Material = material, .Layer = layer, .Flags = MeshSplat });
						bounds.push_back({ low, high });
					}
					for (size_t s = 0; s < run.Spots.size(); s++)
					{
						const LedgeSpot& spot = run.Spots[s];
						const Column& column = line[std::min(s * size_t(LedgeColumns), line.size() - 1)];
						const GroundCell* below = FindGround(spot.Low.X, spot.Low.Y);
						m_WallSpots.push_back({ .Base = column.Base, .Outward = column.Outward, .Top = column.High, .Bottom = column.Low, .HighStep = IsStepCell(spot.High.X, spot.High.Y), .LowStep = IsStepCell(spot.Low.X, spot.Low.Y), .LowPaved = below && below->Kind == GroundPaved });
					}
				}
				if (lakeside)
				{
					std::vector<float> shores(size_t(columns) * size_t(rows), FarAway);
					for (int32_t ey = 0; ey < rows; ey++)
						for (int32_t ex = 0; ex < columns; ex++)
						{
							float level = -FarAway;
							for (int32_t ny = std::max(ey - 2, 0); ny <= std::min(ey + 2, rows - 1); ny++)
								for (int32_t nx = std::max(ex - 2, 0); nx <= std::min(ex + 2, columns - 1); nx++)
									if (const float lake = lakes[size_t(ny) * size_t(columns) + size_t(nx)]; lake < FarAway)
										level = std::max(level, lake);
							if (level > -FarAway)
								shores[size_t(ey) * size_t(columns) + size_t(ex)] = level;
						}
					const glm::vec3 deep(0.18f, 0.36f, 0.5f);
					const glm::vec3 up(0.0f, 1.0f, 0.0f);
					const uint32_t flat = tangentOf(up, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
					const int32_t half = steps / 2;
					for (int32_t cy = 0; cy * ChunkSize < rows; cy++)
						for (int32_t cx = 0; cx * ChunkSize < columns; cx++)
						{
							const uint32_t firstIndex = uint32_t(indices.size());
							glm::vec3 low(1e9f);
							glm::vec3 high(-1e9f);
							for (int32_t ey = cy * ChunkSize; ey < std::min(cy * ChunkSize + ChunkSize, rows); ey++)
								for (int32_t ex = cx * ChunkSize; ex < std::min(cx * ChunkSize + ChunkSize, columns); ex++)
								{
									const float level = shores[size_t(ey) * size_t(columns) + size_t(ex)];
									if (level >= FarAway)
										continue;
									const uint32_t start = uint32_t(vertices.size());
									for (int32_t b = 0; b <= 2; b++)
										for (int32_t a = 0; a <= 2; a++)
										{
											const int32_t i = ex * steps + a * half;
											const int32_t j = ey * steps + b * half;
											const Vec2 point = pointOf(i, j);
											ModelVertex vertex;
											vertex.Position = { point.x, level, point.y };
											vertex.U = point.x;
											vertex.V = point.y;
											vertex.Normal = up;
											vertex.Weights = 255;
											vertex.Tangent = flat;
											vertex.Color = PackTint(glm::mix(deep, glm::vec3(1.0f), 1.0f - Ease(dry[size_t(j) * size_t(vw) + size_t(i)] / (1.6f * float(steps)))));
											vertices.push_back(vertex);
											low = glm::min(low, vertex.Position);
											high = glm::max(high, vertex.Position);
										}
									for (uint32_t b = 0; b < 2; b++)
										for (uint32_t a = 0; a < 2; a++)
										{
											const uint32_t v00 = start + b * 3u + a;
											indices.insert(indices.end(), { v00, v00 + 4u, v00 + 1u, v00, v00 + 3u, v00 + 4u });
										}
								}
							if (uint32_t(indices.size()) == firstIndex)
								continue;
							meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(indices.size()) - firstIndex, .Material = *water, .Layer = *water, .Flags = MeshWater | MeshVertexTint | MeshGround });
							bounds.push_back({ low, high });
						}
				}
				const uint32_t base = m_Renderer.AddLevelGeometry(vertices, indices, meshes);
				for (uint32_t index = 0; index < uint32_t(meshes.size()); index++)
				{
					const auto& [low, high] = bounds[index];
					Insert(base + index, m_Renderer.MakeModelInstance(base + index, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, low.y), low, high, false, low.y);
				}
				if (floating)
				{
					std::vector<uint8_t> openness(size_t(columns) * size_t(rows), 255);
					for (int32_t ey = 0; ey < rows; ey++)
						for (int32_t ex = 0; ex < columns; ex++)
						{
							float covered = 0.0f;
							for (int32_t ny = std::max(ey - 1, 0); ny <= std::min(ey + 1, rows - 1); ny++)
								for (int32_t nx = std::max(ex - 1, 0); nx <= std::min(ex + 1, columns - 1); nx++)
									covered += land[size_t(ny) * size_t(columns) + size_t(nx)] ? 1.0f : 0.0f;
							openness[size_t(ey) * size_t(columns) + size_t(ex)] = uint8_t(std::lround(255.0f * (1.0f - covered / 9.0f)));
						}
					m_Renderer.SetSkyMask(uint32_t(columns), uint32_t(rows), openness, glm::vec2(-float(TerrainMargin)), glm::vec2(float(columns), float(rows)));
					Islets(material, layer);
				}
				for (int32_t y = 0; y < height; y++)
					for (int32_t x = 0; x < width; x++)
					{
						const size_t cell = size_t(y + TerrainMargin) * size_t(columns) + size_t(x + TerrainMargin);
						const uint8_t kind = m_Ground[cell].Kind;
						if (kind == GroundOpen)
							m_Scene.Cells[CellIndex(x, y)] |= CellTop | CellSides | CellGround;
						else if (kind == GroundHill || (lakeside && lakes[cell] < FarAway))
							m_Scene.Cells[CellIndex(x, y)] |= CellTop | CellSides;
					}
				m_Scene.Ground = { std::move(heights), vw, vh };
			}

			void Islets(uint32_t material, uint32_t layer)
			{
				constexpr int32_t Count = 12;
				constexpr int32_t Segments = 28;
				constexpr int32_t Lip = 4;
				constexpr float Profile[][2] = {
					{ 0.0f, 0.22f }, { 0.35f, 0.18f }, { 0.65f, 0.1f }, { 0.88f, 0.03f }, { 1.0f, 0.0f },
					{ 1.0f, -0.4f }, { 0.94f, -1.1f }, { 0.82f, -2.2f }, { 0.64f, -3.4f }, { 0.44f, -4.4f }, { 0.24f, -5.3f }, { 0.08f, -6.0f }, { 0.0f, -6.3f },
				};
				constexpr int32_t Rings = int32_t(std::size(Profile));
				const PropId decorations[] = { PropId::OakTree, PropId::PineTree, PropId::MossBoulder, PropId::Crystal, PropId::Bush };
				const Vec2 middle(float(m_Map.GetWidth()) * 0.5f, float(m_Map.GetHeight()) * 0.5f);
				const float extent = 0.5f * float(std::max(m_Map.GetWidth(), m_Map.GetHeight()));
				const uint32_t grass = PackSplat({ 0.85f, 0.15f, 0.0f, 0.0f }, 0.85f);
				const uint32_t edge = PackSplat({ 0.2f, 0.5f, 0.3f, 0.0f }, 0.7f);
				const uint32_t stone = PackSplat({ 0.0f, 0.0f, 1.0f, 0.0f }, 0.55f);
				std::vector<ModelVertex> vertices;
				std::vector<uint32_t> indices;
				std::vector<LevelMeshDesc> meshes;
				struct Piece
				{
					glm::vec3 Low{ 1e9f };
					glm::vec3 High{ -1e9f };
					float Bob = 0.0f;
					float Phase = 0.0f;
				};
				std::vector<Piece> pieces;
				for (int32_t n = 0; n < Count; n++)
				{
					const float angle = TwoPi * (float(n) + 0.6f * Unit(n, 1, 701u)) / float(Count);
					const float distance = extent + 8.0f + 30.0f * Unit(n, 2, 701u);
					const float radius = 1.6f + 4.4f * Unit(n, 3, 701u) * Unit(n, 4, 701u) + 1.2f * Unit(n, 5, 701u);
					const Vec2 center = middle + FromAngle(angle) * distance;
					const float top = -4.0f - 0.7f * (distance - extent) - 12.0f * Unit(n, 6, 701u);
					const float depth = radius / 3.0f * (0.8f + 0.5f * Unit(n, 7, 701u));
					const float bob = 0.25f + 0.35f * Unit(n, 8, 701u);
					const float phase = TwoPi * Unit(n, 9, 701u);
					std::vector<glm::vec3> ring(size_t(Rings) * size_t(Segments));
					for (int32_t r = 0; r < Rings; r++)
						for (int32_t s = 0; s < Segments; s++)
						{
							const Vec2 direction = FromAngle(TwoPi * float(s) / float(Segments));
							const float wobble = 1.0f + 0.22f * (2.0f * Fbm(direction * 1.3f + Vec2(float(n) * 7.1f, 3.3f), 711u, 2) - 1.0f);
							const Vec2 spot = center + direction * (radius * wobble * Profile[r][0]);
							const float lump = r > Lip ? 0.6f * depth * (2.0f * Fbm(spot * 0.8f, 719u, 2) - 1.0f) : 0.08f * (2.0f * Fbm(spot * 1.1f, 727u, 2) - 1.0f);
							const float y = top + (r > Lip ? Profile[r][1] * depth * 1.4f : Profile[r][1] * radius * 0.35f) + lump;
							ring[size_t(r) * size_t(Segments) + size_t(s)] = { spot.x, y, spot.y };
						}
					const glm::vec3 axis(center.x, top - 0.45f * 6.3f * depth * 1.4f, center.y);
					for (int32_t part = 0; part < 2; part++)
					{
						const int32_t from = part == 0 ? 0 : Lip;
						const int32_t to = part == 0 ? Lip : Rings - 1;
						const uint32_t first = uint32_t(vertices.size());
						const uint32_t firstIndex = uint32_t(indices.size());
						Piece piece{ .Bob = bob, .Phase = phase };
						std::vector<glm::vec3> normals(size_t(to - from + 1) * size_t(Segments), glm::vec3(0.0f));
						for (int32_t r = from; r <= to; r++)
							for (int32_t s = 0; s < Segments; s++)
							{
								const glm::vec3& position = ring[size_t(r) * size_t(Segments) + size_t(s)];
								ModelVertex vertex;
								vertex.Position = position;
								vertex.U = position.x;
								vertex.V = position.z;
								vertex.Weights = 255;
								vertex.Tangent = PackTangent(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);
								vertex.Color = part == 1 ? stone : r >= Lip - 1 ? edge : grass;
								vertices.push_back(vertex);
								piece.Low = glm::min(piece.Low, position);
								piece.High = glm::max(piece.High, position);
							}
						for (int32_t r = from; r < to; r++)
							for (int32_t s = 0; s < Segments; s++)
							{
								const int32_t t = (s + 1) % Segments;
								const uint32_t corners[4] = {
									uint32_t((r - from) * Segments + s), uint32_t((r - from) * Segments + t),
									uint32_t((r + 1 - from) * Segments + t), uint32_t((r + 1 - from) * Segments + s),
								};
								for (const auto& triangle : { std::array<uint32_t, 3>{ corners[0], corners[1], corners[2] }, std::array<uint32_t, 3>{ corners[0], corners[2], corners[3] } })
								{
									const glm::vec3 a = vertices[first + triangle[0]].Position;
									const glm::vec3 b = vertices[first + triangle[1]].Position;
									const glm::vec3 c = vertices[first + triangle[2]].Position;
									glm::vec3 face = glm::cross(b - a, c - a);
									if (glm::dot(face, (a + b + c) / 3.0f - axis) < 0.0f)
									{
										face = -face;
										indices.insert(indices.end(), { first + triangle[0], first + triangle[2], first + triangle[1] });
									}
									else
										indices.insert(indices.end(), { first + triangle[0], first + triangle[1], first + triangle[2] });
									for (const uint32_t corner : triangle)
										normals[corner] += face;
								}
							}
						for (size_t v = 0; v < normals.size(); v++)
							vertices[first + v].Normal = glm::length(normals[v]) > 1e-9f ? glm::normalize(normals[v]) : glm::vec3(0.0f, part == 0 ? 1.0f : -1.0f, 0.0f);
						meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(indices.size()) - firstIndex, .Material = material, .Layer = layer, .Flags = part == 0 ? MeshSplat : MeshSplat | MeshVeins | MeshGround });
						pieces.push_back(piece);
					}
					if (!m_Roots.empty() && radius >= 2.0f)
						for (int32_t k = 0; k < 3; k++)
						{
							const glm::vec3 rim = ring[size_t(Lip) * size_t(Segments) + size_t(Hash(n, 20 + k, 701u) % uint32_t(Segments))];
							const Vec2 outward = Normalize(Vec2(rim.x, rim.z) - center);
							const WeightedMesh& root = m_Roots[Hash(n, 30 + k, 701u) % m_Roots.size()];
							const glm::mat4 hang = HangRoot(root, { rim.x - outward.x * 0.15f, rim.y - 0.1f, rim.z - outward.y * 0.15f }, outward, root.Scale * (0.8f + 0.5f * Unit(n, 40 + k, 701u)) * std::min(radius / 3.0f, 1.3f));
							AddFloating(root.Mesh, hang, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), bob, phase);
							for (uint32_t extra : root.Extras)
								AddFloating(extra, hang, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), bob, phase);
						}
					const ResolvedArt& art = m_PropArt[size_t(decorations[Hash(n, 11, 701u) % std::size(decorations)])];
					if (art.Variants.empty() || radius < 2.0f)
						continue;
					const std::vector<ResolvedPart>& variant = art.Variants[Hash(n, 12, 701u) % art.Variants.size()];
					const Vec2 offset = FromAngle(TwoPi * Unit(n, 13, 701u)) * (radius * 0.3f * Unit(n, 14, 701u));
					const glm::mat4 place = Translation({ center.x + offset.x, top + 0.18f * radius * 0.35f, center.y + offset.y }) * RotationY(TwoPi * Unit(n, 15, 701u)) * Scaling(Uniform(0.9f + 0.4f * Unit(n, 16, 701u)));
					for (const ResolvedPart& decoration : variant)
						if (decoration.Glow <= 0.0f && decoration.Fire < 0)
							AddFloating(decoration.Mesh, place * decoration.Local, decoration.Tint, bob, phase);
				}
				const uint32_t base = m_Renderer.AddLevelGeometry(vertices, indices, meshes);
				for (uint32_t index = 0; index < uint32_t(meshes.size()); index++)
					Insert(base + index, m_Renderer.MakeModelInstance(base + index, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, pieces[index].Low.y), pieces[index].Low, pieces[index].High, false, pieces[index].Low.y, pieces[index].Bob, pieces[index].Phase);
			}

			void RuinIslets()
			{
				constexpr int32_t Count = 16;
				constexpr int32_t Segments = 16;
				constexpr float Profile[][2] = { { 1.0f, 0.0f }, { 1.0f, -0.32f }, { 0.88f, -0.95f }, { 0.68f, -2.0f }, { 0.46f, -3.2f }, { 0.22f, -4.3f } };
				constexpr int32_t Rings = int32_t(std::size(Profile));
				const PropId decorations[] = { PropId::RuinColumn, PropId::RuinBrokenColumn, PropId::RuinArch, PropId::RuinWallShort, PropId::RuinStump, PropId::RuinBrokenColumn };
				const uint32_t material = m_Renderer.GetPlainMaterial();
				const Vec2 middle(float(m_Map.GetWidth()) * 0.5f, float(m_Map.GetHeight()) * 0.5f);
				std::vector<ModelVertex> vertices;
				std::vector<uint32_t> indices;
				std::vector<LevelMeshDesc> meshes;
				struct Piece
				{
					glm::vec3 Low{ 1e9f };
					glm::vec3 High{ -1e9f };
					float Bob = 0.0f;
					float Phase = 0.0f;
				};
				std::vector<Piece> pieces;
				const auto triangle = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, const glm::vec3& axis, const glm::vec3& color, Piece& piece)
				{
					glm::vec3 normal = glm::cross(b - a, c - a);
					if (glm::length(normal) < 1e-8f)
						return;
					if (glm::dot(normal, (a + b + c) / 3.0f - axis) < 0.0f)
					{
						std::swap(b, c);
						normal = -normal;
					}
					normal = glm::normalize(normal);
					for (const glm::vec3& point : { a, b, c })
					{
						indices.push_back(uint32_t(vertices.size()));
						ModelVertex vertex;
						vertex.Position = point;
						vertex.Normal = normal;
						vertex.U = point.x;
						vertex.V = point.z;
						vertex.Weights = 255;
						vertex.Tangent = PackTangent(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);
						vertex.Color = PackTint(color);
						vertices.push_back(vertex);
						piece.Low = glm::min(piece.Low, point);
						piece.High = glm::max(piece.High, point);
					}
				};
				const auto rock = [&](const glm::vec3& centroid, float facet)
				{
					const float band = 0.5f + 0.5f * std::sin(centroid.y * 3.3f + 5.0f * Fbm(Vec2(centroid.x, centroid.z) * 0.27f, 733u, 2));
					return glm::mix(ToColor(0x9A7F5E), ToColor(0xC2A783), band) * (0.84f + 0.24f * facet);
				};
				for (int32_t n = 0; n < Count; n++)
				{
					const float angle = TwoPi * (float(n) + 0.6f * Unit(n, 1, 911u)) / float(Count);
					const float distance = 31.0f + 22.0f * Unit(n, 2, 911u);
					const float radius = 1.3f + 2.6f * Unit(n, 3, 911u) * Unit(n, 4, 911u) + 0.8f * Unit(n, 5, 911u);
					const Vec2 center = middle + FromAngle(angle) * distance;
					const float top = -3.5f - 0.45f * (distance - 28.0f) - 8.0f * Unit(n, 6, 911u);
					const float depth = radius / 3.0f * (0.85f + 0.5f * Unit(n, 7, 911u));
					Piece piece{ .Bob = 0.2f + 0.3f * Unit(n, 8, 911u), .Phase = TwoPi * Unit(n, 9, 911u) };
					std::vector<glm::vec3> ring(size_t(Rings) * size_t(Segments));
					for (int32_t r = 0; r < Rings; r++)
						for (int32_t s = 0; s < Segments; s++)
						{
							const Vec2 direction = FromAngle(TwoPi * float(s) / float(Segments));
							const float wobble = 1.0f + 0.2f * (2.0f * Fbm(direction * 1.3f + Vec2(float(n) * 7.1f, 3.3f), 917u, 2) - 1.0f);
							const Vec2 spot = center + direction * (radius * wobble * Profile[r][0]);
							const float lump = r > 1 ? 0.5f * depth * (2.0f * Fbm(spot * 0.8f, 919u, 2) - 1.0f) : 0.0f;
							ring[size_t(r) * size_t(Segments) + size_t(s)] = { spot.x, top + Profile[r][1] * depth * 1.5f + lump, spot.y };
						}
					const glm::vec3 axis(center.x, top - 0.4f * depth, center.y);
					const glm::vec3 below(center.x, top - 20.0f, center.y);
					const glm::vec3 apex(center.x, top - 5.0f * depth * 1.5f, center.y);
					const uint32_t firstIndex = uint32_t(indices.size());
					const glm::vec3 paving = glm::mix(ToColor(0xC4AB84), ToColor(0xD6C6A6), Unit(n, 10, 911u));
					for (int32_t s = 0; s < Segments; s++)
					{
						const int32_t t = (s + 1) % Segments;
						triangle({ center.x, top, center.y }, ring[size_t(s)], ring[size_t(t)], below, paving, piece);
						for (int32_t r = 0; r + 1 < Rings; r++)
						{
							const glm::vec3& a = ring[size_t(r) * size_t(Segments) + size_t(s)];
							const glm::vec3& b = ring[size_t(r) * size_t(Segments) + size_t(t)];
							const glm::vec3& c = ring[size_t(r + 1) * size_t(Segments) + size_t(t)];
							const glm::vec3& d = ring[size_t(r + 1) * size_t(Segments) + size_t(s)];
							const glm::vec3 color = r == 0 ? ToColor(0xB89F7A) : rock((a + b + c + d) * 0.25f, Unit(n * 64 + s, r, 929u));
							triangle(a, b, c, axis, color, piece);
							triangle(a, c, d, axis, color, piece);
						}
						const glm::vec3& a = ring[size_t(Rings - 1) * size_t(Segments) + size_t(s)];
						const glm::vec3& b = ring[size_t(Rings - 1) * size_t(Segments) + size_t(t)];
						triangle(a, b, apex, axis, rock((a + b + apex) / 3.0f, Unit(n * 64 + s, Rings, 929u)), piece);
					}
					meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(indices.size()) - firstIndex, .Material = material, .Layer = material, .Flags = MeshVertexTint });
					pieces.push_back(piece);
					const ResolvedArt& art = m_PropArt[size_t(decorations[Hash(n, 11, 911u) % std::size(decorations)])];
					if (art.Variants.empty() || radius < 1.6f)
						continue;
					const std::vector<ResolvedPart>& variant = art.Variants[Hash(n, 12, 911u) % art.Variants.size()];
					const glm::mat4 place = Translation({ center.x, top, center.y }) * RotationY(TwoPi * Unit(n, 15, 911u)) * Scaling(Uniform(0.85f + 0.3f * Unit(n, 16, 911u)));
					for (const ResolvedPart& decoration : variant)
						if (decoration.Glow <= 0.0f && decoration.Fire < 0)
							AddFloating(decoration.Mesh, place * decoration.Local, decoration.Tint, piece.Bob, piece.Phase);
				}
				const uint32_t base = m_Renderer.AddLevelGeometry(vertices, indices, meshes);
				for (uint32_t index = 0; index < uint32_t(meshes.size()); index++)
					Insert(base + index, m_Renderer.MakeModelInstance(base + index, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, pieces[index].Low.y), pieces[index].Low, pieces[index].High, false, pieces[index].Low.y, pieces[index].Bob, pieces[index].Phase);
			}

			void Mountain()
			{
				const Elevation& relief = m_Level.Relief;
				const std::optional<uint32_t> layers[] = { m_Renderer.FindMaterialSlot("GroundSnow"), m_Renderer.FindMaterialSlot("GroundScree"), m_Renderer.FindMaterialSlot("GroundCliff"), m_Renderer.FindMaterialSlot("GroundAlpine") };
				const uint32_t plain = m_Renderer.GetPlainMaterial();
				const uint32_t material = layers[0].value_or(plain) | (layers[1].value_or(plain) << 16);
				const uint32_t layer = layers[2].value_or(plain) | (layers[3].value_or(plain) << 16);
				const MountainSurface surface{ relief };
				const int32_t columns = relief.Columns;
				const int32_t rows = relief.Rows;
				const auto node = [&](int32_t i, int32_t j) { return size_t(j) * size_t(columns) + size_t(i); };
				std::vector<std::array<float, MountainFields>> fields(size_t(columns) * size_t(rows));
				std::vector<std::array<float, MountainFields>> drops(fields.size());
				for (size_t k = 0; k < fields.size(); k++)
				{
					fields[k][0] = relief.Rim[k];
					drops[k][0] = 1.0f;
					for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
					{
						fields[k][size_t(cliff + 1)] = relief.Edges[size_t(cliff)][k];
						drops[k][size_t(cliff + 1)] = relief.Drops[size_t(cliff)][k];
					}
				}
				const auto tangentOf = [](const glm::vec3& normal)
				{
					glm::vec3 tangent = glm::vec3(1.0f, 0.0f, 0.0f) - normal * normal.x;
					tangent = glm::length(tangent) > 1e-6f ? glm::normalize(tangent) : glm::vec3(0.0f, 0.0f, 1.0f);
					return PackTangent(tangent, glm::dot(glm::cross(normal, tangent), glm::vec3(0.0f, 0.0f, -1.0f)) >= 0.0f ? 1.0f : -1.0f);
				};
				struct Segment
				{
					int32_t Field = 0;
					uint64_t KeyA = 0;
					uint64_t KeyB = 0;
					Vec2 A;
					Vec2 B;
					float TopA = 0.0f;
					float TopB = 0.0f;
					float BottomA = 0.0f;
					float BottomB = 0.0f;
				};
				std::vector<Segment> segments;
				std::vector<ModelVertex> vertices;
				std::vector<uint32_t> indices;
				std::vector<LevelMeshDesc> meshes;
				std::vector<std::pair<glm::vec3, glm::vec3>> bounds;
				std::unordered_map<uint64_t, uint32_t> lookup;
				std::vector<uint32_t> topIndices;
				std::vector<uint32_t> flankIndices;
				std::vector<std::vector<MountainPoint>> pieces;
				std::vector<std::vector<MountainPoint>> next;
				std::vector<MountainPoint> inside;
				std::vector<MountainPoint> outside;
				uint64_t interior = 0;
				const auto sidesOf = [](const MountainPoint& point, uint32_t piece)
				{
					uint32_t sides = 0;
					for (int32_t field = 0; field < MountainFields; field++)
					{
						const bool on = (point.On & (1u << field)) != 0;
						sides |= (on ? (piece & (1u << field)) != 0 : point.Fields[size_t(field)] >= 0.0f) ? 1u << field : 0u;
					}
					return sides;
				};
				for (int32_t cy = 0; cy * MountainChunk < rows - 1; cy++)
					for (int32_t cx = 0; cx * MountainChunk < columns - 1; cx++)
					{
						lookup.clear();
						topIndices.clear();
						flankIndices.clear();
						glm::vec3 topLow(1e9f);
						glm::vec3 topHigh(-1e9f);
						glm::vec3 flankLow(1e9f);
						glm::vec3 flankHigh(-1e9f);
						const auto vertexOf = [&](const MountainPoint& point, uint32_t sides)
						{
							const uint64_t key = point.Key | (uint64_t(sides) << 58);
							if (const auto found = lookup.find(key); found != lookup.end())
								return found->second;
							const float height = surface.Height(point.Position, sides);
							const glm::vec3 normal = surface.Normal(point.Position, sides);
							ModelVertex vertex;
							vertex.Position = { point.Position.x, height, point.Position.y };
							vertex.U = point.Position.x;
							vertex.V = point.Position.y;
							vertex.Normal = normal;
							vertex.Weights = 255;
							vertex.Tangent = tangentOf(normal);
							vertex.Color = (sides & 1u) != 0 ? surface.TopSplat(point.Position, sides, normal) : surface.FlankSplat(point.Position, normal);
							const uint32_t index = uint32_t(vertices.size());
							vertices.push_back(vertex);
							lookup.emplace(key, index);
							return index;
						};
						for (int32_t j = cy * MountainChunk; j < std::min((cy + 1) * MountainChunk, rows - 1); j++)
							for (int32_t i = cx * MountainChunk; i < std::min((cx + 1) * MountainChunk, columns - 1); i++)
							{
								const size_t corners[4] = { node(i, j), node(i + 1, j), node(i + 1, j + 1), node(i, j + 1) };
								const Vec2 places[4] = { relief.Origin + Vec2(float(i), float(j)) * relief.Spacing, relief.Origin + Vec2(float(i + 1), float(j)) * relief.Spacing, relief.Origin + Vec2(float(i + 1), float(j + 1)) * relief.Spacing, relief.Origin + Vec2(float(i), float(j + 1)) * relief.Spacing };
								const uint8_t touching[4] = { 0b1001, 0b0011, 0b0110, 0b1100 };
								const std::array<uint64_t, 4> edgeIds = { uint64_t(node(i, j)) * 2, uint64_t(node(i + 1, j)) * 2 + 1, uint64_t(node(i, j + 1)) * 2, uint64_t(node(i, j)) * 2 + 1 };
								std::vector<MountainPoint> cell(4);
								bool deep = true;
								for (size_t n = 0; n < 4; n++)
								{
									cell[n].Position = places[n];
									cell[n].Fields = fields[corners[n]];
									cell[n].Key = (1ull << 62) | uint64_t(corners[n]);
									cell[n].Edges = touching[n];
									deep = deep && fields[corners[n]][0] < -40.0f;
								}
								if (deep)
									continue;
								pieces.assign(1, cell);
								for (int32_t field = 0; field < MountainFields; field++)
								{
									bool positive = false;
									bool negative = false;
									float drop = 0.0f;
									for (size_t n = 0; n < 4; n++)
									{
										(cell[n].Fields[size_t(field)] >= 0.0f ? positive : negative) = true;
										drop = std::max(drop, drops[corners[n]][size_t(field)]);
									}
									if (!positive || !negative || drop < MountainActiveDrop)
										continue;
									next.clear();
									for (const std::vector<MountainPoint>& piece : pieces)
									{
										SplitPolygon(piece, field, edgeIds, interior, inside, outside);
										if (inside.size() >= 3)
											next.push_back(inside);
										if (outside.size() >= 3)
											next.push_back(outside);
									}
									pieces.swap(next);
								}
								for (const std::vector<MountainPoint>& piece : pieces)
								{
									std::array<float, MountainFields> middle{};
									for (const MountainPoint& point : piece)
										for (size_t f = 0; f < middle.size(); f++)
											middle[f] += point.Fields[f] / float(piece.size());
									uint32_t sides = 0;
									for (int32_t field = 0; field < MountainFields; field++)
										sides |= middle[size_t(field)] >= 0.0f ? 1u << field : 0u;
									std::vector<uint32_t>& target = (sides & 1u) != 0 ? topIndices : flankIndices;
									glm::vec3& low = (sides & 1u) != 0 ? topLow : flankLow;
									glm::vec3& high = (sides & 1u) != 0 ? topHigh : flankHigh;
									std::array<uint32_t, 12> ids{};
									const size_t count = std::min<size_t>(piece.size(), ids.size());
									for (size_t n = 0; n < count; n++)
									{
										ids[n] = vertexOf(piece[n], sidesOf(piece[n], sides));
										low = glm::min(low, vertices[ids[n]].Position);
										high = glm::max(high, vertices[ids[n]].Position);
									}
									for (size_t m = 1; m + 1 < count; m++)
										target.insert(target.end(), { ids[0], ids[m + 1], ids[m] });
									for (size_t n = 0; n < count; n++)
									{
										const MountainPoint& a = piece[n];
										const MountainPoint& b = piece[(n + 1) % count];
										const uint8_t shared = uint8_t(a.On & b.On);
										for (int32_t field = 0; field < MountainFields; field++)
										{
											if ((shared & (1u << field)) == 0 || (sides & (1u << field)) == 0 || LengthSquared(a.Position - b.Position) < 1e-10f)
												continue;
											const uint32_t upperA = sidesOf(a, sides);
											const uint32_t upperB = sidesOf(b, sides);
											segments.push_back({ field, a.Key, b.Key, a.Position, b.Position, surface.Height(a.Position, upperA), surface.Height(b.Position, upperB), surface.Height(a.Position, upperA & ~(1u << field)), surface.Height(b.Position, upperB & ~(1u << field)) });
										}
									}
								}
							}
						const auto flush = [&](const std::vector<uint32_t>& list, const glm::vec3& low, const glm::vec3& high, uint32_t flags)
						{
							if (list.empty())
								return;
							const uint32_t firstIndex = uint32_t(indices.size());
							indices.insert(indices.end(), list.begin(), list.end());
							meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(list.size()), .Material = material, .Layer = layer, .Flags = flags });
							bounds.push_back({ low, high });
						};
						flush(topIndices, topLow, topHigh, MeshSplat);
						flush(flankIndices, flankLow, flankHigh, MeshSplat | MeshCliff);
					}

				std::unordered_multimap<uint64_t, uint32_t> ends;
				for (uint32_t s = 0; s < uint32_t(segments.size()); s++)
				{
					ends.emplace(segments[s].KeyA, s);
					ends.emplace(segments[s].KeyB, s);
				}
				struct Column
				{
					Vec2 Base;
					float Top = 0.0f;
					float Bottom = 0.0f;
				};
				std::vector<uint8_t> used(segments.size(), 0);
				const auto follow = [&](uint32_t from, uint64_t key)
				{
					std::vector<Column> tail;
					uint32_t current = from;
					while (true)
					{
						uint32_t found = UINT32_MAX;
						const auto [begin, end] = ends.equal_range(key);
						for (auto it = begin; it != end; ++it)
							if (it->second != current && !used[it->second] && segments[it->second].Field == segments[from].Field)
							{
								found = it->second;
								break;
							}
						if (found == UINT32_MAX)
							break;
						used[found] = 1;
						const Segment& segment = segments[found];
						const bool atA = segment.KeyA == key;
						tail.push_back(atA ? Column{ segment.B, segment.TopB, segment.BottomB } : Column{ segment.A, segment.TopA, segment.BottomA });
						key = atA ? segment.KeyB : segment.KeyA;
						current = found;
					}
					return tail;
				};
				for (uint32_t s = 0; s < uint32_t(segments.size()); s++)
				{
					if (used[s])
						continue;
					used[s] = 1;
					const Segment& seed = segments[s];
					std::vector<Column> chain = follow(s, seed.KeyA);
					std::reverse(chain.begin(), chain.end());
					chain.push_back({ seed.A, seed.TopA, seed.BottomA });
					chain.push_back({ seed.B, seed.TopB, seed.BottomB });
					const std::vector<Column> after = follow(s, seed.KeyB);
					chain.insert(chain.end(), after.begin(), after.end());
					std::vector<Column> line;
					for (size_t c = 0; c + 1 < chain.size(); c++)
					{
						const float length = Length(chain[c + 1].Base - chain[c].Base);
						const int32_t steps = std::max(1, int32_t(std::ceil(length / MountainWallColumn)));
						for (int32_t k = 0; k < steps; k++)
						{
							const float f = float(k) / float(steps);
							line.push_back({ Lerp(chain[c].Base, chain[c + 1].Base, f), std::lerp(chain[c].Top, chain[c + 1].Top, f), std::lerp(chain[c].Bottom, chain[c + 1].Bottom, f) });
						}
					}
					line.push_back(chain.back());
					float tallest = 0.0f;
					for (const Column& column : line)
						tallest = std::max(tallest, column.Top - column.Bottom);
					if (tallest < 0.02f)
						continue;
					const int32_t tiers = std::clamp(int32_t(std::ceil((tallest + MountainBury) / MountainWallRow)), 3, 40);
					for (size_t c = 0; c < line.size(); c += 2)
						if (line[c].Top - line[c].Bottom > 0.25f)
							m_WallSpots.push_back({ .Base = line[c].Base, .Outward = surface.Downhill(seed.Field, line[c].Base), .Top = line[c].Top, .Bottom = line[c].Bottom - MountainBury, .Cliff = true, .Rim = seed.Field == 0 });
					float arc = 0.0f;
					for (size_t first = 0; first + 1 < line.size(); first += 48)
					{
						const size_t last = std::min(first + 48, line.size() - 1);
						const uint32_t start = uint32_t(vertices.size());
						const uint32_t firstIndex = uint32_t(indices.size());
						glm::vec3 low(1e9f);
						glm::vec3 high(-1e9f);
						for (size_t c = first; c <= last; c++)
						{
							const Column& column = line[c];
							if (c > first)
								arc += Length(column.Base - line[c - 1].Base);
							const Vec2 outward = surface.Downhill(seed.Field, column.Base);
							const Vec2 along = Perpendicular(outward);
							const float bottom = column.Bottom - MountainBury;
							for (int32_t r = 0; r <= tiers; r++)
							{
								const float t = float(r) / float(tiers);
								ModelVertex vertex;
								vertex.Position = MountainFace(column.Base, outward, column.Top, bottom, t);
								vertex.Normal = FaceNormal(MountainFace, column.Base, outward, column.Top, bottom, t);
								vertex.U = arc;
								vertex.V = -vertex.Position.y;
								vertex.Weights = 255;
								const glm::vec3 flat(along.x, 0.0f, along.y);
								const glm::vec3 tangent = glm::normalize(flat - vertex.Normal * glm::dot(vertex.Normal, flat));
								vertex.Tangent = PackTangent(tangent, glm::dot(glm::cross(vertex.Normal, tangent), glm::vec3(0.0f, 1.0f, 0.0f)) >= 0.0f ? 1.0f : -1.0f);
								vertex.Color = surface.WallSplat(vertex.Position, vertex.Normal);
								vertices.push_back(vertex);
								low = glm::min(low, vertex.Position);
								high = glm::max(high, vertex.Position);
							}
						}
						for (size_t c = first; c < last; c++)
						{
							const Vec2 outward = surface.Downhill(seed.Field, Lerp(line[c].Base, line[c + 1].Base, 0.5f));
							const bool forward = Dot(Perpendicular(outward), line[c + 1].Base - line[c].Base) >= 0.0f;
							for (int32_t r = 0; r < tiers; r++)
							{
								const uint32_t v00 = start + uint32_t((c - first) * size_t(tiers + 1) + size_t(r));
								const uint32_t v10 = v00 + uint32_t(tiers + 1);
								if (forward)
									indices.insert(indices.end(), { v00, v10, v10 + 1, v00, v10 + 1, v00 + 1 });
								else
									indices.insert(indices.end(), { v00, v10 + 1, v10, v00, v00 + 1, v10 + 1 });
							}
						}
						meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(indices.size()) - firstIndex, .Material = material, .Layer = layer, .Flags = MeshSplat });
						bounds.push_back({ low, high });
					}
				}
				const uint32_t base = m_Renderer.AddLevelGeometry(vertices, indices, meshes);
				for (uint32_t index = 0; index < uint32_t(meshes.size()); index++)
				{
					const auto& [low, high] = bounds[index];
					Insert(base + index, m_Renderer.MakeModelInstance(base + index, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, low.y), low, high, false, low.y);
				}

				const int32_t groundColumns = (m_Map.GetWidth() + 2 * TerrainMargin) * TerrainSteps + 1;
				const int32_t groundRows = (m_Map.GetHeight() + 2 * TerrainMargin) * TerrainSteps + 1;
				std::vector<float> ground(size_t(groundColumns) * size_t(groundRows), 0.0f);
				for (int32_t j = 0; j < groundRows; j++)
					for (int32_t i = 0; i < groundColumns; i++)
					{
						const Vec2 point(std::clamp(float(i) / float(TerrainSteps) - float(TerrainMargin), 0.0f, float(m_Map.GetWidth())), std::clamp(float(j) / float(TerrainSteps) - float(TerrainMargin), 0.0f, float(m_Map.GetHeight())));
						ground[size_t(j) * size_t(groundColumns) + size_t(i)] = surface.Height(point, surface.SidesAt(point));
					}
				m_Scene.Ground = { std::move(ground), groundColumns, groundRows };
				for (uint8_t& cell : m_Scene.Cells)
					cell |= CellGround;
			}

			void SummitStones()
			{
				constexpr float RingEdges[] = { 0.8f, 1.42f, 2.02f, 2.64f, 3.28f };
				constexpr float StoneLength = 0.78f;
				constexpr float Gap = 0.04f;
				constexpr int32_t StairSteps = 8;
				constexpr int32_t RoughSteps = 6;
				const std::optional<uint32_t> material = m_Renderer.FindMaterialSlot("PavingStone");
				if (!material)
					return;
				const Elevation& relief = m_Level.Relief;
				const Vec2 middle = relief.Summit;
				const float floor = relief.Height(middle) + 0.03f;
				const PavingStyle ritual{ .Shoulder = 0.86f, .Dome = 0.008f, .Rim = 0.014f, .Bed = 0.08f, .Tilt = 0.012f, .Lift = 0.008f, .Texture = 0.8f, .Light = { 0.86f, 0.88f, 0.92f }, .Dark = { 0.46f, 0.47f, 0.5f }, .Salt = 0xA17Au };
				PavingBatch batch;
				for (int32_t ring = 0; ring + 1 < int32_t(std::size(RingEdges)); ring++)
				{
					const float inner = RingEdges[ring] + Gap * 0.5f;
					const float outer = RingEdges[ring + 1] - Gap * 0.5f;
					const int32_t count = std::max(6, int32_t(std::lround(TwoPi * 0.5f * (inner + outer) / StoneLength)));
					const float turn = Unit(ring, 3, 0xA17Bu) * TwoPi;
					std::vector<float> cuts(size_t(count) + 1);
					for (int32_t k = 0; k <= count; k++)
						cuts[size_t(k)] = turn + TwoPi * (float(k % count) + (k % count == 0 ? 0.0f : 0.3f * (Unit(ring, k, 0xA17Cu) - 0.5f))) / float(count) + (k == count ? TwoPi : 0.0f);
					for (int32_t k = 0; k < count; k++)
					{
						const float from = cuts[size_t(k)] + Gap * 0.5f / inner;
						const float to = cuts[size_t(k + 1)] - Gap * 0.5f / inner;
						Outline outline;
						for (int32_t s = 0; s <= 2; s++)
							outline.push_back(middle + FromAngle(std::lerp(from, to, float(s) / 2.0f)) * inner);
						for (int32_t s = 2; s >= 0; s--)
							outline.push_back(middle + FromAngle(std::lerp(from, to, float(s) / 2.0f)) * outer);
						LayStone(batch, outline, ritual, floor, ring, k);
					}
				}
				const PavingStyle steps{ .Shoulder = 0.9f, .Dome = 0.004f, .Rim = 0.012f, .Bed = 0.2f, .Tilt = 0.008f, .Lift = 0.005f, .Texture = 0.8f, .Light = { 0.82f, 0.82f, 0.84f }, .Dark = { 0.42f, 0.42f, 0.44f }, .Salt = 0x57E9u };
				const PavingStyle rough{ .Shoulder = 0.8f, .Dome = 0.014f, .Rim = 0.022f, .Bed = 0.2f, .Tilt = 0.022f, .Lift = 0.012f, .Texture = 0.65f, .Light = { 0.74f, 0.74f, 0.76f }, .Dark = { 0.38f, 0.38f, 0.4f }, .Salt = 0x57EBu };
				for (const ElevationRamp& ramp : relief.Ramps)
				{
					const Vec2 side = Perpendicular(ramp.Uphill);
					const float reach = 1.25f;
					const int32_t count = ramp.Stair ? StairSteps : RoughSteps;
					const float depth = 2.0f * reach / float(count);
					const int32_t salt = int32_t(ramp.Cliff) * 31 + int32_t(std::lround(ramp.Foot.x * 7.0f)) + int32_t(std::lround(ramp.Foot.y * 13.0f));
					float below = relief.Height(ramp.Foot - ramp.Uphill * (reach + 0.2f));
					for (int32_t step = 0; step < count; step++)
					{
						const float start = -reach + float(step) * depth;
						const Vec2 front = ramp.Foot + ramp.Uphill * start;
						const float top = relief.Height(front + ramp.Uphill * (depth * 0.75f)) + 0.04f;
						if (ramp.Stair)
						{
							const Vec2 back = front + ramp.Uphill * (depth - Gap);
							const float width = ramp.Width * (0.96f - 0.06f * Unit(step, int32_t(ramp.Cliff), 0x57EAu));
							const float joint = width * 0.6f * (Unit(step, salt, 0x57EEu) - 0.5f);
							PavingStyle style = steps;
							style.Bed = std::max(top - below, 0.0f) + 0.16f;
							LayStone(batch, { front - side * width, front + side * (joint - Gap * 0.5f), back + side * (joint - Gap * 0.5f), back - side * width }, style, top, step * 2, salt);
							LayStone(batch, { front + side * (joint + Gap * 0.5f), front + side * width, back + side * width, back + side * (joint + Gap * 0.5f) }, style, top, step * 2 + 1, salt);
							below = top;
							continue;
						}
						const auto jitter = [&](int32_t k, uint32_t key) { return Unit(step * 7 + k, salt, 0x57ECu + key) - 0.5f; };
						const int32_t pieces = 2 + int32_t(Unit(step, salt, 0x57EDu) * 2.0f);
						std::vector<float> cuts = { -ramp.Width * (1.0f + 0.12f * jitter(0, 1u)) };
						for (int32_t k = 1; k < pieces; k++)
							cuts.push_back(ramp.Width * (2.0f * float(k) / float(pieces) - 1.0f + 0.35f * jitter(k, 2u) / float(pieces)));
						cuts.push_back(ramp.Width * (1.0f + 0.12f * jitter(pieces, 3u)));
						for (int32_t k = 0; k < pieces; k++)
						{
							const float left = cuts[size_t(k)] + Gap * 0.6f;
							const float right = cuts[size_t(k + 1)] - Gap * 0.6f;
							const float near = 0.06f * jitter(k, 4u);
							const float far = depth - Gap * 1.5f + 0.08f * jitter(k, 5u);
							const Outline outline = {
								front + side * left + ramp.Uphill * (near + 0.04f * jitter(k, 6u)),
								front + side * right + ramp.Uphill * (near + 0.04f * jitter(k, 7u)),
								front + side * right + ramp.Uphill * (far + 0.04f * jitter(k, 8u)),
								front + side * left + ramp.Uphill * (far + 0.04f * jitter(k, 9u)),
							};
							PavingStyle style = rough;
							style.Bed = std::max(top - below, 0.0f) + 0.18f;
							LayStone(batch, Rounded(outline, 0.16f), style, top, step * 4 + k, salt);
						}
						below = top;
					}
				}
				if (batch.Indices.empty())
					return;
				const uint32_t base = m_Renderer.AddLevelGeometry(batch.Vertices, batch.Indices, { { .FirstIndex = 0, .IndexCount = uint32_t(batch.Indices.size()), .Material = *material, .Layer = *material, .Flags = MeshVertexTint | MeshGround } });
				Insert(base, m_Renderer.MakeModelInstance(base, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, batch.Min.y), batch.Min, batch.Max, false, batch.Min.y);
			}

			void MountainSlabs()
			{
				constexpr float Pitch = 1.15f;
				constexpr float Gap = 0.06f;
				const std::optional<uint32_t> material = m_Renderer.FindMaterialSlot("PavingStone");
				if (!material)
					return;
				const Elevation& relief = m_Level.Relief;
				const MountainSurface surface{ relief };
				const PavingStyle style{ .Shoulder = 0.78f, .Dome = 0.016f, .Rim = 0.03f, .Bed = 0.32f, .Tilt = 0.03f, .Lift = 0.0f, .Texture = 0.6f, .Light = { 0.78f, 0.79f, 0.82f }, .Dark = { 0.4f, 0.4f, 0.43f }, .Salt = 0x51ABu };
				const auto seedAt = [&](int32_t row, int32_t column)
				{
					const float stagger = (row & 1) ? 0.5f : 0.0f;
					return Vec2((float(column) + stagger + 0.6f * (Unit(column, row, 0x51ACu) - 0.5f)) * Pitch, (float(row) + 0.6f * (Unit(column, row, 0x51ADu) - 0.5f)) * Pitch * 0.87f);
				};
				const auto weightOf = [&](int32_t row, int32_t column) { return 0.35f * Pitch * Pitch * Unit(column, row, 0x51AEu); };
				const auto nearRamp = [&](Vec2 point)
				{
					for (const ElevationRamp& ramp : relief.Ramps)
					{
						const Vec2 offset = point - ramp.Foot;
						if (std::abs(Dot(offset, Perpendicular(ramp.Uphill))) < ramp.Width + 0.45f && std::abs(Dot(offset, ramp.Uphill)) < 1.7f)
							return true;
					}
					return false;
				};
				std::vector<PavingBatch> batches(m_Scene.Chunks.size());
				const int32_t rows = int32_t(float(m_Map.GetHeight()) / (Pitch * 0.87f)) + 2;
				const int32_t columns = int32_t(float(m_Map.GetWidth()) / Pitch) + 2;
				for (int32_t row = 0; row < rows; row++)
					for (int32_t column = 0; column < columns; column++)
					{
						const Vec2 seed = seedAt(row, column);
						if (relief.RimDistance(seed) < 0.35f || Length(seed - relief.Summit) < 3.75f || nearRamp(seed))
							continue;
						const float summit = 1.0f - Ease((Length(seed - relief.Summit) - 4.0f) / 6.0f);
						if (Fbm(seed * 0.13f, 0x51AFu, 3) + 0.25f * (Unit(column, row, 0x51B0u) - 0.5f) + 0.2f * summit < 0.42f)
							continue;
						const uint32_t sides = surface.SidesAt(seed);
						if (surface.Cover(seed, sides)[PeakSnow] > 0.6f)
							continue;
						Outline outline = { seed + Vec2(-Pitch, -Pitch), seed + Vec2(Pitch, -Pitch), seed + Vec2(Pitch, Pitch), seed + Vec2(-Pitch, Pitch) };
						const float weight = weightOf(row, column);
						for (int32_t dr = -2; dr <= 2; dr++)
							for (int32_t dc = -2; dc <= 2; dc++)
							{
								if (dr == 0 && dc == 0)
									continue;
								const Vec2 other = seedAt(row + dr, column + dc);
								KeepBelow(outline, other - seed, 0.5f * (Dot(other, other) - Dot(seed, seed) + weight - weightOf(row + dr, column + dc)));
							}
						const Elevation::Spot spot = relief.Locate(seed);
						for (int32_t field = 0; field < MountainFields && outline.size() >= 3; field++)
						{
							if (field > 0 && relief.Read(relief.Drops[size_t(field - 1)], spot) < MountainActiveDrop)
								continue;
							const std::vector<float>& grid = field == 0 ? relief.Rim : relief.Edges[size_t(field - 1)];
							const float value = relief.Read(grid, spot);
							const Vec2 rise = relief.Gradient(grid, seed);
							const float steep = Length(rise);
							if (std::abs(value) > 2.5f || steep < 1e-4f)
								continue;
							const Vec2 up = rise / steep;
							if (value >= 0.0f)
								KeepBelow(outline, up * -1.0f, Dot(seed, up * -1.0f) + (value - 0.14f) / steep);
							else
								KeepBelow(outline, up, Dot(seed, up) + (-0.16f - value) / steep);
						}
						if (outline.size() < 3 || SignedArea(outline) < 0.06f)
							continue;
						Inset(outline, Gap * (0.6f + 0.8f * Unit(column, row, 0x51B1u)));
						outline = Cleaned(outline);
						if (outline.size() < 3 || SignedArea(outline) < 0.05f)
							continue;
						Vec2 center;
						for (const Vec2 point : outline)
							center += point;
						center = center / float(outline.size());
						const float h = 0.2f;
						const Vec2 grade(surface.Height(center + Vec2(h, 0.0f), sides) - surface.Height(center - Vec2(h, 0.0f), sides), surface.Height(center + Vec2(0.0f, h), sides) - surface.Height(center - Vec2(0.0f, h), sides));
						const float poke = Unit(column, row, 0x51B2u);
						PavingStyle shaped = style;
						float lift = -0.03f + 0.09f * poke * poke;
						if (poke > 0.9f)
						{
							lift = 0.1f + 0.08f * Unit(column, row, 0x51B3u);
							shaped.Tilt = 0.07f;
						}
						const size_t chunk = size_t(std::clamp(int32_t(center.y) / ChunkSize, 0, m_ChunksY - 1)) * size_t(m_ChunksX) + size_t(std::clamp(int32_t(center.x) / ChunkSize, 0, m_ChunksX - 1));
						LayStone(batches[chunk], Rounded(outline, 0.18f + 0.12f * Unit(column, row, 0x51B4u)), shaped, surface.Height(center, sides) + lift, row, column, grade / (2.0f * h));
					}
				std::vector<ModelVertex> vertices;
				std::vector<uint32_t> indices;
				std::vector<LevelMeshDesc> meshes;
				std::vector<std::pair<glm::vec3, glm::vec3>> bounds;
				for (const PavingBatch& batch : batches)
				{
					if (batch.Indices.empty())
						continue;
					const uint32_t firstIndex = uint32_t(indices.size());
					const uint32_t baseVertex = uint32_t(vertices.size());
					vertices.insert(vertices.end(), batch.Vertices.begin(), batch.Vertices.end());
					for (const uint32_t index : batch.Indices)
						indices.push_back(baseVertex + index);
					meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(batch.Indices.size()), .Material = *material, .Layer = *material, .Flags = MeshVertexTint | MeshGround });
					bounds.push_back({ batch.Min, batch.Max });
				}
				if (meshes.empty())
					return;
				const uint32_t base = m_Renderer.AddLevelGeometry(vertices, indices, meshes);
				for (uint32_t i = 0; i < uint32_t(meshes.size()); i++)
				{
					const auto& [low, high] = bounds[i];
					Insert(base + i, m_Renderer.MakeModelInstance(base + i, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, low.y), low, high, false, low.y);
				}
			}

			void MountainDressing()
			{
				const glm::vec4 plain(1.0f, 1.0f, 1.0f, 0.0f);
				const glm::vec4 stone(0.9f, 0.9f, 0.92f, 0.0f);
				const glm::vec3 up(0.0f, 1.0f, 0.0f);
				const Elevation& relief = m_Level.Relief;
				const MountainSurface surface{ relief };
				const auto rock = [&](const std::vector<WeightedMesh>& kit, uint32_t pick, const glm::vec3& center, const glm::vec3& axis, float spin, float radius, const glm::vec4& tint)
				{
					if (kit.empty())
						return;
					const WeightedMesh& mesh = kit[pick % kit.size()];
					Add(mesh, Translation(center) * AlignUp(axis, spin) * Scaling(Uniform(mesh.Scale * radius / 0.4f)), tint, center.y, false, center.y);
				};
				for (const WallSpot& spot : m_WallSpots)
				{
					const int32_t hx = int32_t(std::lround(spot.Base.x * 8.0f));
					const int32_t hy = int32_t(std::lround(spot.Base.y * 8.0f));
					const auto roll = [&](uint32_t salt) { return Unit(hx, hy, salt); };
					const glm::vec3 outward(spot.Outward.x, 0.0f, spot.Outward.y);
					const float drop = spot.Top - spot.Bottom;
					const float grass = surface.Cover(spot.Base - spot.Outward * 0.3f, surface.SidesAt(spot.Base - spot.Outward * 0.3f))[PeakGrass];
					if (!spot.Rim && roll(1301u) < 0.32f)
					{
						const float radius = 0.1f + 0.17f * roll(1302u);
						const glm::vec3 center = glm::vec3(spot.Base.x, spot.Bottom + MountainBury - radius * 0.4f, spot.Base.y) + outward * (0.06f + 0.16f * roll(1303u));
						rock(m_CliffRocks, Hash(hx, hy, 1304u), center, glm::normalize(up + outward * (0.5f * roll(1305u))), roll(1306u) * TwoPi, radius, stone);
					}
					if (drop > 0.6f && roll(1311u) < (spot.Rim ? 0.1f : 0.14f))
					{
						const float radius = 0.16f + 0.18f * roll(1312u);
						const glm::vec3 axis = glm::normalize(outward * 0.85f + up * (0.35f + 0.45f * roll(1313u)));
						const glm::vec3 center = MountainFace(spot.Base, spot.Outward, spot.Top, spot.Bottom, 0.25f + 0.45f * roll(1314u)) - outward * (radius * 0.3f) - axis * (radius * 0.35f);
						rock(m_CliffRocks, Hash(hx, hy, 1315u), center, axis, roll(1316u) * TwoPi, radius, stone);
					}
					if (roll(1321u) < 0.16f)
					{
						const float radius = 0.1f + 0.12f * roll(1322u);
						const glm::vec3 center = glm::vec3(spot.Base.x, spot.Top - radius * 0.45f, spot.Base.y) - outward * (0.05f + 0.1f * roll(1323u));
						rock(m_CliffRocks, Hash(hx, hy, 1324u), center, glm::normalize(up + outward * (0.4f + 0.5f * roll(1325u))), roll(1326u) * TwoPi, radius, stone);
					}
					if (!m_Grass.empty() && roll(1331u) < 0.1f + 0.5f * grass)
					{
						const WeightedMesh& tuft = m_Grass[Hash(hx, hy, 1332u) % m_Grass.size()];
						const glm::vec3 base = glm::vec3(spot.Base.x, spot.Top - 0.02f, spot.Base.y) - outward * (0.04f + 0.12f * roll(1333u));
						const glm::vec3 lean = glm::normalize(up + outward * (0.3f + 0.35f * roll(1334u)));
						Add(tuft, Translation(base) * AlignUp(lean, roll(1335u) * TwoPi) * Scaling(Uniform(tuft.Scale * (0.55f + 0.35f * roll(1336u)))), plain, base.y, false, base.y);
					}
				}
				const auto nearRamp = [&](Vec2 point)
				{
					for (const ElevationRamp& ramp : relief.Ramps)
					{
						const Vec2 offset = point - ramp.Foot;
						if (std::abs(Dot(offset, Perpendicular(ramp.Uphill))) < ramp.Width + 0.2f && std::abs(Dot(offset, ramp.Uphill)) < 1.4f)
							return true;
					}
					return false;
				};
				constexpr float Spacing = 0.55f;
				const int32_t count = int32_t(float(m_Map.GetWidth()) / Spacing);
				for (int32_t j = 0; j < count; j++)
					for (int32_t i = 0; i < count; i++)
					{
						const auto roll = [&](uint32_t salt) { return Unit(i, j, salt); };
						const Vec2 point = Vec2(float(i) + roll(1341u), float(j) + roll(1342u)) * Spacing;
						if (relief.RimDistance(point) < 0.3f || Length(point - relief.Summit) < 3.9f || nearRamp(point))
							continue;
						const uint32_t sides = surface.SidesAt(point);
						const std::array<float, 4> cover = surface.Cover(point, sides);
						const float ground = surface.Height(point, sides);
						if (!m_Grass.empty() && roll(1343u) < 0.45f * Ease((cover[PeakGrass] - 0.25f) / 0.4f))
						{
							const WeightedMesh& tuft = m_Grass[Hash(i, j, 1344u) % m_Grass.size()];
							const glm::vec3 base(point.x, ground - 0.02f, point.y);
							Add(tuft, Translation(base) * RotationY(roll(1345u) * TwoPi) * Scaling(Uniform(tuft.Scale * (0.5f + 0.4f * roll(1346u)))), plain, base.y, false, base.y);
						}
						else if (roll(1347u) < 0.04f * (1.0f - cover[PeakSnow]))
						{
							const float radius = 0.05f + 0.07f * roll(1348u);
							rock(m_CliffRocks, Hash(i, j, 1349u), glm::vec3(point.x, ground - radius * 0.3f, point.y), up, roll(1350u) * TwoPi, radius, stone);
						}
					}
			}

			void AddFloating(uint32_t mesh, const glm::mat4& transform, const glm::vec4& tint, float bob, float phase)
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
				Insert(mesh, m_Renderer.MakeModelInstance(mesh, transform, NoPalette, tint, 0.0f, low.y), low, high, false, low.y, bob, phase);
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
							if (first.Terrain != TerrainId::Flagstone || first.Slope != Direction::None || m_Paved[CellIndex(x, y)])
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
						if (!def.Walkable || def.Style == TerrainStyle::Void)
							continue;
						if (const GroundCell* ground = FindGround(x, y); ground && ground->Kind == GroundOpen)
							continue;
						uint8_t& flags = m_Scene.Cells[CellIndex(x, y)];
						const float top = CellHeight(x, y);
						const bool built = IsBuilt(cell.Terrain);
						const Vec2 center = { float(x) + 0.5f, float(y) + 0.5f };
						if (cell.Slope != Direction::None && cell.Climb > 0)
						{
							if (!m_Stairs || IsStepCell(x, y) || def.Style == TerrainStyle::Ground && cell.Terrain != TerrainId::Dirt && cell.Terrain != TerrainId::CaveFloor)
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
						if (m_Paved[CellIndex(x, y)])
						{
							flags |= CellTop;
							if (Foundation(x, y, top - FloorBottom, top, m_Terrain))
								flags |= CellSides;
							continue;
						}
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
								const TerrainId terrain = m_Map.GetTerrain(x + dx, y + dy);
								const TerrainDef& neighbor = GetTerrain(terrain);
								const bool molten = neighbor.Style == TerrainStyle::Molten || IsBrawlFloor(terrain);
								if (m_Terrain ? molten : (neighbor.Walkable || neighbor.Style == TerrainStyle::Liquid))
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
							Add(rock, transform, glm::vec4(0.95f, 0.95f, 0.95f, 0.0f), base, true, base);
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
					Add(bush, transform, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), wall.Base, true, wall.Base);
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
					const float base = art.Natural ? NaturalBase(prop.Position, prop.Base) : prop.Base;
					const glm::mat4 place = Translation({ prop.Position.x, base, prop.Position.y }) * RotationY(prop.Yaw) * Scaling(Uniform(jitter));
					const bool tall = GetProp(prop.Id).Tall;
					glm::vec3 low(1e9f);
					glm::vec3 high(-1e9f);
					for (const ResolvedPart& part : variant)
						if (part.Fire < 0 && part.Glow <= 0.0f)
							Bounds(part.Mesh, place * part.Local, low, high);
					for (const ResolvedPart& part : variant)
					{
						if (part.Fire >= 0)
						{
							m_Scene.Fires.push_back({ glm::vec3((place * part.Local)[3]), part.FireScale * jitter, FireKind(part.Fire) });
							continue;
						}
						if (part.Glow > 0.0f)
						{
							const glm::vec3 flame = glm::vec3((place * part.Local)[3]);
							AddBox({ .Mesh = MeshKind::Sphere, .Center = flame, .Half = glm::vec3(part.Glow * jitter), .Color = ToColor(PropData::Flame), .Emission = 1.0f, .Base = base, .Reach = part.Glow });
							continue;
						}
						const ModelInstance instance = m_Renderer.MakeModelInstance(part.Mesh, place * part.Local, NoPalette, part.Tint, 0.0f, base);
						if ((m_Level.Skyborne && m_Level.Relief.IsEmpty()) || (m_Level.Kind == MapKind::FrozenLake && prop.Id == PropId::IcePillar))
							m_Scene.Loose.push_back({ MakeItem(part.Mesh, instance, low, high, tall, base), prop.Position });
						else
							Insert(part.Mesh, instance, low, high, tall, base);
					}
					if (art.KeepAbove >= 0.0f)
					{
						const Vec2 axis = FromAngle(prop.Yaw);
						for (const PropPart& part : GetProp(prop.Id).Parts)
						{
							if (part.Base < art.KeepAbove)
								continue;
							const Vec2 center = prop.Position + Rotate(part.Offset, axis);
							AddBox({ .Mesh = part.Mesh, .Center = { center.x, base + part.Base + part.Height * 0.5f, center.y }, .Half = { part.Half.x, part.Height * 0.5f, part.Half.y }, .Color = ToColor(part.Color), .Yaw = prop.Yaw + part.Yaw, .Emission = part.Emission, .Base = base, .Reach = Length(part.Half), .Cut = tall });
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

			void Roots()
			{
				if (!m_Floating || m_Roots.empty() || m_Rim.Heights.empty() || m_Scene.Ground.Heights.empty())
					return;
				for (int32_t y = 0; y < m_Map.GetHeight(); y++)
					for (int32_t x = 0; x < m_Map.GetWidth(); x++)
					{
						const TerrainDef& def = GetTerrain(m_Map.Get(x, y).Terrain);
						if (!def.Walkable || def.Style == TerrainStyle::Void)
							continue;
						const float top = CellHeight(x, y);
						for (uint32_t side = 0; side < 4; side++)
						{
							int32_t nx = x;
							int32_t ny = y;
							TileMap::Step(Direction(side), nx, ny);
							if (!m_Map.IsAbyss(nx, ny) || Hash(x, y, 300u + side) % 100u >= RootChance)
								continue;
							const Vec2 outward(float(nx - x), float(ny - y));
							Vec2 rim = Vec2(float(x) + 0.5f, float(y) + 0.5f) + Perpendicular(outward) * ((Unit(x, y, 310u + side) - 0.5f) * 0.7f);
							float previous = SampleField(m_Rim, rim);
							bool found = false;
							for (int32_t step = 0; step < 40 && previous > 0.0f; step++)
							{
								const float next = SampleField(m_Rim, rim + outward * 0.05f);
								if (next <= 0.0f)
								{
									rim += outward * (0.05f * previous / (previous - next));
									found = true;
									break;
								}
								rim += outward * 0.05f;
								previous = next;
							}
							if (!found)
								continue;
							const WeightedMesh& root = m_Roots[Hash(x, y, 320u + side) % m_Roots.size()];
							const float hang = std::min(top, SampleField(m_Scene.Ground, rim - outward * 0.25f)) - 0.08f - 0.12f * Unit(x, y, 330u + side);
							const glm::mat4 transform = HangRoot(root, { rim.x - outward.x * 0.1f, hang, rim.y - outward.y * 0.1f }, outward, root.Scale * (0.75f + 0.5f * Unit(x, y, 340u + side)));
							Add(root, transform, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), hang - 3.0f, false, hang - 3.0f);
						}
					}
			}

			void WallDressing()
			{
				const glm::vec4 plain(1.0f, 1.0f, 1.0f, 0.0f);
				for (const WallSpot& spot : m_WallSpots)
				{
					const int32_t hx = int32_t(std::lround(spot.Base.x * 8.0f));
					const int32_t hy = int32_t(std::lround(spot.Base.y * 8.0f));
					const auto roll = [&](uint32_t salt) { return Unit(hx, hy, salt); };
					const auto face = [&](float t) { return spot.Cliff ? CliffFace(spot.Base, spot.Outward, spot.Top, spot.Bottom, t) : LedgeFace(spot.Base, spot.Outward, spot.Top, spot.Bottom, t); };
					const glm::vec3 outward(spot.Outward.x, 0.0f, spot.Outward.y);
					const glm::vec3 up(0.0f, 1.0f, 0.0f);
					const float drop = spot.Top - spot.Bottom;
					const TileCoord inner = m_Map.CellAt(spot.Base - spot.Outward * 0.12f);
					const GroundCell* brink = FindGround(inner.X, inner.Y);
					const bool paved = brink && brink->Kind == GroundPaved;
					if (!m_CliffRocks.empty() && drop > 0.45f && roll(901u) < (spot.Cliff ? 0.05f : 0.08f))
					{
						const float radius = spot.Cliff ? 0.18f + 0.2f * roll(903u) : 0.11f + 0.09f * roll(903u);
						const WeightedMesh& rock = m_CliffRocks[Hash(hx, hy, 904u) % m_CliffRocks.size()];
						const glm::vec3 axis = glm::normalize(outward * 0.85f + up * (0.35f + 0.45f * roll(905u)));
						const glm::vec3 center = face(0.3f + 0.4f * roll(902u)) - outward * (radius * 0.3f) - axis * (radius * 0.35f);
						Add(rock, Translation(center) * AlignUp(axis, roll(906u) * TwoPi) * Scaling(Uniform(rock.Scale * radius / 0.4f)), glm::vec4(0.92f, 0.92f, 0.92f, 0.0f), center.y, false, center.y);
					}
					if (!m_CliffRocks.empty() && !spot.HighStep && !paved && roll(951u) < 0.06f)
					{
						const float radius = 0.1f + 0.1f * roll(952u);
						const WeightedMesh& rock = m_CliffRocks[Hash(hx, hy, 953u) % m_CliffRocks.size()];
						const glm::vec3 axis = glm::normalize(up + outward * (0.4f + 0.5f * roll(954u)));
						const glm::vec3 center = glm::vec3(spot.Base.x, spot.Top - radius * 0.45f, spot.Base.y) - outward * (0.05f + 0.08f * roll(955u));
						Add(rock, Translation(center) * AlignUp(axis, roll(956u) * TwoPi) * Scaling(Uniform(rock.Scale * radius / 0.4f)), glm::vec4(0.95f, 0.95f, 0.95f, 0.0f), center.y, false, center.y);
					}
					if (!m_Grass.empty() && !spot.HighStep && roll(911u) < (paved ? 0.08f : spot.Cliff ? 0.3f : 0.34f))
					{
						const WeightedMesh& tuft = m_Grass[Hash(hx, hy, 914u) % m_Grass.size()];
						const glm::vec3 base = glm::vec3(spot.Base.x, spot.Top - 0.02f, spot.Base.y) - outward * (0.04f + 0.1f * roll(912u));
						const glm::vec3 lean = glm::normalize(up + outward * (0.3f + 0.35f * roll(913u)));
						Add(tuft, Translation(base) * AlignUp(lean, roll(915u) * TwoPi) * Scaling(Uniform(tuft.Scale * (0.65f + 0.35f * roll(916u)))), plain, base.y, false, base.y);
					}
					if (!spot.Cliff && !spot.LowStep && !m_Grass.empty() && roll(921u) < (spot.LowPaved ? 0.12f : 0.45f))
					{
						const WeightedMesh& tuft = m_Grass[Hash(hx, hy, 924u) % m_Grass.size()];
						const glm::vec3 base = glm::vec3(spot.Base.x, spot.Bottom - 0.01f, spot.Base.y) + outward * (0.08f + 0.18f * roll(922u));
						Add(tuft, Translation(base) * AlignUp(glm::normalize(up + outward * 0.15f), roll(925u) * TwoPi) * Scaling(Uniform(tuft.Scale * (0.7f + 0.4f * roll(926u)))), plain, base.y, false, base.y);
						if (!spot.LowPaved && !m_SmallBushes.empty() && roll(927u) < 0.08f)
						{
							const WeightedMesh& bush = m_SmallBushes[Hash(hx, hy, 928u) % m_SmallBushes.size()];
							const glm::vec3 spotBase = glm::vec3(spot.Base.x, spot.Bottom - 0.02f, spot.Base.y) + outward * 0.22f;
							Add(bush, Translation(spotBase) * RotationY(roll(929u) * TwoPi) * Scaling(Uniform(bush.Scale * (0.6f + 0.3f * roll(930u)))), plain, spotBase.y, false, spotBase.y);
						}
					}
					if (!m_Grass.empty() && drop > 0.55f && roll(931u) < (spot.Cliff ? 0.07f : 0.1f))
					{
						const WeightedMesh& tuft = m_Grass[Hash(hx, hy, 934u) % m_Grass.size()];
						const glm::vec3 base = face(0.25f + 0.45f * roll(932u)) - outward * 0.03f;
						const glm::vec3 lean = glm::normalize(outward * 0.9f + up * (0.35f + 0.3f * roll(933u)));
						Add(tuft, Translation(base) * AlignUp(lean, roll(935u) * TwoPi) * Scaling(Uniform(tuft.Scale * (0.5f + 0.3f * roll(936u)))), plain, base.y, false, base.y);
					}
					if (!spot.Cliff && !spot.HighStep && !m_Roots.empty() && drop > 0.8f && roll(941u) < 0.025f)
					{
						const WeightedMesh& root = m_Roots[Hash(hx, hy, 942u) % m_Roots.size()];
						const glm::mat4 transform = HangRoot(root, { spot.Base.x - spot.Outward.x * 0.05f, spot.Top - 0.04f, spot.Base.y - spot.Outward.y * 0.05f }, spot.Outward, root.Scale * (0.35f + 0.12f * roll(943u)));
						Add(root, transform, plain, spot.Bottom, false, spot.Bottom);
					}
				}
			}

			void RoundedRailings()
			{
				const auto fenced = [&](TileCoord cell)
				{
					for (const TileRect& area : m_Level.Railings)
						if (cell.X >= area.X && cell.X < area.Right() && cell.Y >= area.Y && cell.Y < area.Top())
							return true;
					return false;
				};
				const auto fence = [&](const std::vector<Vec2>& line)
				{
					if (line.size() < 2)
						return;
					std::vector<Vec2> corners = { line.front() };
					for (size_t n = 1; n + 1 < line.size(); n++)
						if (Dot(Normalize(line[n] - corners.back()), Normalize(line[n + 1] - line[n])) < 0.999f)
							corners.push_back(line[n]);
					corners.push_back(line.back());
					std::vector<glm::vec3> posts;
					for (size_t n = 0; n + 1 < corners.size(); n++)
					{
						const int32_t pieces = std::max(1, int32_t(std::ceil(Length(corners[n + 1] - corners[n]) / 1.1f)));
						for (int32_t p = 0; p < pieces; p++)
						{
							const Vec2 point = Lerp(corners[n], corners[n + 1], float(p) / float(pieces));
							posts.push_back({ point.x, m_Map.GetHeight(point), point.y });
						}
					}
					posts.push_back({ corners.back().x, m_Map.GetHeight(corners.back()), corners.back().y });
					for (size_t n = 0; n < posts.size(); n++)
					{
						const glm::vec3& base = posts[n];
						const int32_t hx = int32_t(std::lround(base.x * 8.0f));
						const int32_t hy = int32_t(std::lround(base.z * 8.0f));
						const WeightedMesh& mesh = m_FencePosts[Hash(hx, hy, 230u) % m_FencePosts.size()];
						Add(mesh, Translation({ base.x, base.y - 0.1f, base.z }) * RotationY(Unit(hx, hy, 231u) * 0.3f - 0.15f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), base.y, false, base.y);
						if (n + 1 == posts.size())
							break;
						const glm::vec3 along = base - posts[n + 1];
						const glm::vec3 side = glm::normalize(glm::cross(along, glm::vec3(0.0f, 1.0f, 0.0f)));
						const glm::vec3 middle = (base + posts[n + 1]) * 0.5f + side * FenceRailOffset;
						glm::mat4 transform(1.0f);
						transform[0] = glm::vec4(along, 0.0f);
						transform[2] = glm::vec4(side, 0.0f);
						transform[3] = glm::vec4(middle.x, middle.y + FenceRailBase, middle.z, 1.0f);
						Add(m_FenceRails[Hash(hx, hy, 240u) % m_FenceRails.size()], transform, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), middle.y, false, middle.y);
					}
				};
				for (const LedgeRun& run : m_LedgeRuns)
				{
					const auto inset = [&](const LedgeSpot& spot) { return spot.Position - spot.Outward * (FenceInset * spot.Miter); };
					std::vector<Vec2> line;
					for (size_t s = 0; s + 1 < run.Spots.size(); s++)
					{
						if (!fenced(run.Spots[s].High))
						{
							fence(line);
							line.clear();
							continue;
						}
						if (line.empty())
							line.push_back(inset(run.Spots[s]));
						line.push_back(inset(run.Spots[s + 1]));
					}
					fence(line);
				}
			}

			void Steps()
			{
				if (!m_Terrain || m_StepBlocks.empty())
					return;
				for (int32_t y = 0; y < m_Map.GetHeight(); y++)
					for (int32_t x = 0; x < m_Map.GetWidth(); x++)
					{
						const Cell& cell = m_Map.Get(x, y);
						const Direction down = Direction((uint32_t(cell.Slope) + 2u) % 4u);
						if (cell.Slope == Direction::None || cell.Climb == 0 || cell.Terrain != TerrainId::Dirt || ContinuesStairs(x, y, down))
							continue;
						int32_t endX = x;
						int32_t endY = y;
						int32_t depth = 1;
						for (int32_t nextX = x, nextY = y;; depth++)
						{
							TileMap::Step(cell.Slope, nextX, nextY);
							if (!m_Map.IsInside(nextX, nextY) || !ContinuesStairs(nextX, nextY, down))
								break;
							endX = nextX;
							endY = nextY;
						}
						const Cell& last = m_Map.Get(endX, endY);
						const float low = CellHeight(x, y);
						const float rise = float(int32_t(last.Level) + int32_t(last.Climb) - int32_t(cell.Level)) * HeightUnit;
						int32_t stepX = 0;
						int32_t stepY = 0;
						TileMap::Step(cell.Slope, stepX, stepY);
						for (int32_t d = 0; d < depth; d++)
							m_Scene.Cells[CellIndex(x + stepX * d, y + stepY * d)] |= CellTop | CellSides;
						const glm::vec3 up(float(stepX), 0.0f, float(stepY));
						const glm::vec3 across(-up.z, 0.0f, up.x);
						const glm::vec3 start = glm::vec3(float(x) + 0.5f, 0.0f, float(y) + 0.5f) - up * 0.5f;
						const int32_t count = std::max(2, int32_t(std::lround(float(depth) / StepDepth)));
						const float tread = float(depth) / float(count);
						const float bottom = low - 0.3f;
						const int32_t ax = int32_t(std::lround(across.x));
						const int32_t ay = int32_t(std::lround(across.z));
						const auto exposed = [&](int32_t nx, int32_t ny) { return !m_Map.IsInside(nx, ny) || !IsStepCell(nx, ny) || m_Map.Get(nx, ny).Slope != cell.Slope; };
						const auto hollow = [&](int32_t nx, int32_t ny) { return !m_Map.IsInside(nx, ny) || GetTerrain(m_Map.GetTerrain(nx, ny)).Style == TerrainStyle::Void; };
						for (int32_t i = 0; i < count; i++)
						{
							const float top = low + rise * float(i + 1) / float(count) + 0.015f;
							const glm::vec3 center = start + up * (tread * (float(i) + 0.5f));
							const int32_t cx = int32_t(std::floor(center.x));
							const int32_t cy = int32_t(std::floor(center.z));
							const bool plus = exposed(cx + ax, cy + ay);
							const bool minus = exposed(cx - ax, cy - ay);
							const bool plusVoid = plus && hollow(cx + ax, cy + ay);
							const bool minusVoid = minus && hollow(cx - ax, cy - ay);
							const bool shallow = (plusVoid || minusVoid) && plus == plusVoid && minus == minusVoid;
							const float base = shallow ? std::max(bottom, top - rise / float(count) - StepSkirt) : bottom;
							const float wide = 1.0f + (plus ? StepLip : 0.0f) + (minus ? StepLip : 0.0f);
							const glm::vec3 middle = center + across * (0.5f * ((plus ? StepLip : 0.0f) - (minus ? StepLip : 0.0f)));
							const float flip = (Hash(x, y, 240u + uint32_t(i)) & 1u) != 0u ? -1.0f : 1.0f;
							glm::mat4 transform(1.0f);
							transform[0] = glm::vec4(up * (flip * tread / 0.5f), 0.0f);
							transform[1] = glm::vec4(0.0f, (top - base) / 0.5f, 0.0f, 0.0f);
							transform[2] = glm::vec4(across * (flip * wide), 0.0f);
							transform[3] = glm::vec4(middle.x, base, middle.z, 1.0f);
							const float shade = 0.94f + 0.12f * Unit(x * 31 + i, y, 210u);
							Add(m_StepBlocks[Hash(x, y, 220u + uint32_t(i)) % m_StepBlocks.size()], transform, glm::vec4(shade, shade, shade, 0.0f), base, false, base);
						}
					}
			}

			void Railings()
			{
				if (m_FencePosts.empty() || m_FenceRails.empty())
					return;
				if (m_Level.Rounded && !m_LedgeRuns.empty())
				{
					RoundedRailings();
					return;
				}
				struct Edge
				{
					int32_t X = 0;
					int32_t Y = 0;
					Direction Side = Direction::None;
				};
				std::vector<Edge> edges;
				std::unordered_map<uint64_t, uint8_t> corners;
				const auto key = [](int32_t x, int32_t y) { return (uint64_t(uint32_t(x)) << 32) | uint64_t(uint32_t(y)); };
				const auto ends = [](const Edge& edge)
				{
					switch (edge.Side)
					{
					case Direction::North:
						return std::array<std::pair<int32_t, int32_t>, 2>{ { { edge.X, edge.Y + 1 }, { edge.X + 1, edge.Y + 1 } } };
					case Direction::East:
						return std::array<std::pair<int32_t, int32_t>, 2>{ { { edge.X + 1, edge.Y }, { edge.X + 1, edge.Y + 1 } } };
					case Direction::South:
						return std::array<std::pair<int32_t, int32_t>, 2>{ { { edge.X, edge.Y }, { edge.X + 1, edge.Y } } };
					default:
						return std::array<std::pair<int32_t, int32_t>, 2>{ { { edge.X, edge.Y }, { edge.X, edge.Y + 1 } } };
					}
				};
				for (const TileRect& area : m_Level.Railings)
					for (int32_t y = area.Y; y < area.Top(); y++)
						for (int32_t x = area.X; x < area.Right(); x++)
						{
							if (!m_Map.IsInside(x, y) || !m_Map.IsWalkable(x, y))
								continue;
							for (uint32_t side = 0; side < 4; side++)
							{
								int32_t nx = x;
								int32_t ny = y;
								TileMap::Step(Direction(side), nx, ny);
								if (!m_Map.IsInside(nx, ny) || !m_Map.IsWalkable(nx, ny) || m_Map.IsConnected(x, y, Direction(side)))
									continue;
								const Vec2 middle = (Vec2(float(x) + 0.5f, float(y) + 0.5f) + Vec2(float(nx) + 0.5f, float(ny) + 0.5f)) * 0.5f;
								const float own = m_Map.GetHeightAt(x, y, std::clamp(middle.x - float(x), 0.0f, 1.0f), std::clamp(middle.y - float(y), 0.0f, 1.0f));
								const float other = m_Map.GetHeightAt(nx, ny, std::clamp(middle.x - float(nx), 0.0f, 1.0f), std::clamp(middle.y - float(ny), 0.0f, 1.0f));
								if (own - other <= MaxStepHeight)
									continue;
								const Edge edge{ x, y, Direction(side) };
								edges.push_back(edge);
								const uint8_t inward = uint8_t(1u << ((side + 2u) % 4u));
								for (const auto& [cx, cy] : ends(edge))
									corners[key(cx, cy)] |= inward;
							}
						}
				const auto post = [&](int32_t cx, int32_t cy)
				{
					const uint8_t mask = corners[key(cx, cy)];
					Vec2 offset;
					for (uint32_t side = 0; side < 4; side++)
						if (mask & (1u << side))
						{
							int32_t sx = 0;
							int32_t sy = 0;
							TileMap::Step(Direction(side), sx, sy);
							offset += Vec2(float(sx), float(sy));
						}
					const Vec2 point = Vec2(float(cx), float(cy)) + offset * FenceInset;
					const int32_t px = int32_t(std::floor(point.x));
					const int32_t py = int32_t(std::floor(point.y));
					const float height = m_Map.IsInside(px, py) ? m_Map.GetHeightAt(px, py, point.x - float(px), point.y - float(py)) : 0.0f;
					return glm::vec3(point.x, height, point.y);
				};
				for (const auto& [corner, mask] : corners)
				{
					const glm::vec3 base = post(int32_t(corner >> 32), int32_t(uint32_t(corner)));
					const WeightedMesh& mesh = m_FencePosts[Hash(int32_t(base.x * 8.0f), int32_t(base.z * 8.0f), 230u) % m_FencePosts.size()];
					Add(mesh, Translation({ base.x, base.y - 0.1f, base.z }) * RotationY(Unit(int32_t(base.x * 8.0f), int32_t(base.z * 8.0f), 231u) * 0.3f - 0.15f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), base.y, false, base.y);
				}
				for (const Edge& edge : edges)
				{
					const auto [first, second] = ends(edge);
					const glm::vec3 a = post(first.first, first.second);
					const glm::vec3 b = post(second.first, second.second);
					int32_t ox = 0;
					int32_t oy = 0;
					TileMap::Step(edge.Side, ox, oy);
					const glm::vec3 outward(float(ox), 0.0f, float(oy));
					glm::vec3 along = b - a;
					glm::vec3 side = glm::normalize(glm::cross(along, glm::vec3(0.0f, 1.0f, 0.0f)));
					if (glm::dot(side, outward) < 0.0f)
					{
						along = -along;
						side = -side;
					}
					const glm::vec3 middle = (a + b) * 0.5f + outward * FenceRailOffset;
					glm::mat4 transform(1.0f);
					transform[0] = glm::vec4(along, 0.0f);
					transform[2] = glm::vec4(side, 0.0f);
					transform[3] = glm::vec4(middle.x, middle.y + FenceRailBase, middle.z, 1.0f);
					Add(m_FenceRails[Hash(edge.X, edge.Y, 240u + uint32_t(edge.Side)) % m_FenceRails.size()], transform, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), middle.y, false, middle.y);
				}
			}

			glm::mat4 HangRoot(const WeightedMesh& root, const glm::vec3& anchor, Vec2 outward, float scale) const
			{
				const ModelMesh& mesh = m_Models.GetMesh(root.Mesh);
				const glm::vec3 knot(0.5f * (mesh.Min.x + mesh.Max.x), mesh.Max.y - 0.06f, mesh.Min.z + 0.14f);
				const glm::mat4 turn = RotationY(std::atan2(-outward.x, outward.y)) * Scaling(Uniform(scale));
				return Translation(anchor - glm::vec3(turn * glm::vec4(knot, 1.0f))) * turn;
			}

			void Tethers()
			{
				if (!m_ChainLink)
					return;
				const auto anchor = [&](Vec2 point, float lift)
				{
					const int32_t x = std::clamp(int32_t(std::floor(point.x)), 0, m_Map.GetWidth() - 1);
					const int32_t y = std::clamp(int32_t(std::floor(point.y)), 0, m_Map.GetHeight() - 1);
					return glm::vec3(point.x, NaturalBase(point, CellHeight(x, y)) + lift, point.y);
				};
				for (const Tether& tether : m_Level.Tethers)
				{
					const glm::vec3 from = anchor(tether.From, tether.Lift);
					const glm::vec3 to = anchor(tether.To, tether.Lift);
					const auto along = [&](float s) { return glm::mix(from, to, s) - glm::vec3(0.0f, 4.0f * tether.Sag * s * (1.0f - s), 0.0f); };
					float length = 0.0f;
					for (int32_t i = 0; i < 16; i++)
						length += glm::length(along(float(i + 1) / 16.0f) - along(float(i) / 16.0f));
					const int32_t count = std::max(2, int32_t(std::ceil(length / (ChainPitch * tether.Scale))));
					for (int32_t i = 0; i < count; i++)
					{
						const float s = (float(i) + 0.5f) / float(count);
						const glm::vec3 point = along(s);
						const glm::vec3 tangent = glm::normalize(to - from - glm::vec3(0.0f, 4.0f * tether.Sag * (1.0f - 2.0f * s), 0.0f));
						glm::vec3 side = glm::cross(tangent, glm::vec3(0.0f, 1.0f, 0.0f));
						side = glm::length(side) > 1e-4f ? glm::normalize(side) : glm::vec3(1.0f, 0.0f, 0.0f);
						const glm::vec3 up = glm::cross(side, tangent);
						const bool turned = (i & 1) != 0;
						glm::mat4 transform(1.0f);
						transform[0] = glm::vec4(tangent * tether.Scale, 0.0f);
						transform[1] = glm::vec4((turned ? side : up) * tether.Scale, 0.0f);
						transform[2] = glm::vec4((turned ? -up : side) * tether.Scale, 0.0f);
						transform[3] = glm::vec4(point, 1.0f);
						Add(*m_ChainLink, transform, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), point.y, false, point.y);
					}
				}
			}

			void Scatter(const std::vector<WeightedMesh>& meshes, int32_t x, int32_t y, uint32_t salt, float height, const glm::vec4& tint)
			{
				if (meshes.empty())
					return;
				const Vec2 point = { float(x) + 0.2f + 0.6f * Unit(x, y, salt + 1), float(y) + 0.2f + 0.6f * Unit(x, y, salt + 2) };
				if (const TileCoord cell = m_Map.CellAt(point); cell.X != x || cell.Y != y || !IsClear(point, 0.3f))
					return;
				const float ground = TerrainHeight(point).value_or(height + 0.01f) - 0.01f;
				const WeightedMesh& mesh = meshes[Hash(x, y, salt + 3) % meshes.size()];
				const float scale = mesh.Scale * (0.8f + 0.4f * Unit(x, y, salt + 4));
				const glm::mat4 transform = Translation({ point.x, ground, point.y }) * RotationY(Unit(x, y, salt + 5) * TwoPi) * Scaling(Uniform(scale));
				Add(mesh, transform, tint, ground, false, ground);
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
						if (!def.Walkable || def.Style == TerrainStyle::Void || cell.Slope != Direction::None)
							continue;
						const float top = CellHeight(x, y);
						const uint32_t roll = Hash(x, y, 61) % 1000u;
						switch (cell.Terrain)
						{
						case TerrainId::Grass:
							if (roll < 330)
								Scatter(m_Grass, x, y, 70, top, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
							else if (roll < 380)
								Scatter(m_SmallBushes, x, y, 80, top, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
							else if (roll < 400)
								Scatter(m_Pebbles, x, y, 90, top, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
							break;
						case TerrainId::Dirt:
						case TerrainId::Sand:
							if (roll < 50)
								Scatter(m_Pebbles, x, y, 90, top, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
							break;
						case TerrainId::Ash:
							if (roll < 40)
								Scatter(m_Pebbles, x, y, 90, top, glm::vec4(0.72f, 0.7f, 0.68f, 0.0f));
							else if (roll < 60)
								Scatter(m_Saplings, x, y, 100, top, glm::vec4(0.45f, 0.4f, 0.38f, 0.0f));
							break;
						case TerrainId::CaveFloor:
							if (roll < 70)
								Scatter(m_Pebbles, x, y, 90, top, glm::vec4(0.88f, 0.88f, 0.94f, 0.0f));
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
							if (const GroundCell* neighbor = FindGround(nx, ny); neighbor && neighbor->Kind != GroundNone)
								continue;
							const Vec2 outward = Vec2(float(nx - x), float(ny - y));
							const Vec2 along = Perpendicular(outward);
							const Vec2 point = Vec2(float(x) + 0.5f, float(y) + 0.5f) + outward * 0.5f + along * ((Unit(x, y, 130 + side) - 0.5f) * 0.6f);
							const WeightedMesh& rock = m_CliffRocks[Hash(x, y, 140 + side) % m_CliffRocks.size()];
							const float scale = rock.Scale * (0.85f + 0.3f * Unit(x, y, 150 + side));
							const float base = std::max(edge, top - 1.2f);
							const glm::mat4 transform = Translation({ point.x, base, point.y }) * RotationY(Unit(x, y, 160 + side) * TwoPi) * Scaling(Uniform(scale));
							Add(rock, transform, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), base, false, base);
						}
					}
			}

			const Level& m_Level;
			const TileMap& m_Map;
			WorldRenderer& m_Renderer;
			const ModelLibrary& m_Models;
			LevelScene& m_Scene;
			int32_t m_ChunksX = 0;
			int32_t m_ChunksY = 0;
			std::vector<std::string> m_Missing;
			std::vector<ResolvedArt> m_PropArt;
			std::vector<uint8_t> m_Paved;
			std::vector<GroundCell> m_Ground;
			int32_t m_GroundColumns = 0;
			int32_t m_GroundRows = 0;
			bool m_Terrain = false;
			bool m_Floating = false;
			std::array<uint32_t, 4> m_Splat{};
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
			std::optional<uint32_t> m_ChainLink;
			std::vector<WeightedMesh> m_Hedge;
			std::vector<WeightedMesh> m_EdgeRocks;
			std::vector<WeightedMesh> m_CliffRocks;
			std::vector<WeightedMesh> m_Pebbles;
			std::vector<WeightedMesh> m_Grass;
			std::vector<WeightedMesh> m_SmallBushes;
			std::vector<WeightedMesh> m_Saplings;
			std::vector<WeightedMesh> m_Roots;
			std::vector<WeightedMesh> m_StepBlocks;
			std::vector<WeightedMesh> m_FencePosts;
			std::vector<WeightedMesh> m_FenceRails;
			GroundField m_Rim;
			std::vector<LedgeRun> m_LedgeRuns;
			std::vector<std::vector<std::pair<Vec2, Vec2>>> m_Coast;
			std::vector<WallSpot> m_WallSpots;
		};
	}

	LevelScene BuildLevelScene(const Level& level, WorldRenderer& renderer)
	{
		const Rdn::Timer timer;
		LevelScene scene;
		SceneBuilder(level, renderer, scene).Build();
		RDN_LOG("Built the level scene in {:.0f} ms", timer.ElapsedMilliseconds());
		return scene;
	}

	std::optional<float> SampleGround(const LevelScene& scene, Vec2 position)
	{
		const int32_t x = int32_t(std::floor(position.x));
		const int32_t y = int32_t(std::floor(position.y));
		if (scene.Ground.Heights.empty() || x < 0 || y < 0 || x >= scene.Width)
			return std::nullopt;
		const size_t index = size_t(y) * size_t(scene.Width) + size_t(x);
		if (index >= scene.Cells.size() || (scene.Cells[index] & CellGround) == 0)
			return std::nullopt;
		return SampleField(scene.Ground, position);
	}
}
