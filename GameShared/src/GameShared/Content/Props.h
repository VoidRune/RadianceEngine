#pragma once
#include "GameShared/World/Shapes.h"
#include <cstdint>
#include <iterator>
#include <span>

namespace Game
{
	enum class MeshKind : uint8_t
	{
		Box,
		Cylinder,
		CylinderX,
		Cone,
		Sphere,
		Wedge,
		Roof,
		Count,
	};

	struct PropPart
	{
		MeshKind Mesh = MeshKind::Box;
		Vec2 Offset;
		float Base = 0.0f;
		Vec2 Half;
		float Height = 1.0f;
		float Yaw = 0.0f;
		uint32_t Color = 0xFFFFFF;
		float Emission = 0.0f;
	};

	struct PropCollider
	{
		ShapeKind Kind = ShapeKind::Box;
		Vec2 Offset;
		Vec2 Half;
		float Radius = 0.0f;
		float Yaw = 0.0f;
		Block Blocks = Block::All;
	};

	enum class PropId : uint8_t
	{
		Barrel,
		Crate,
		CrateStack,
		StonePillar,
		MarblePillar,
		BrokenPillar,
		Bookshelf,
		DoubleBookshelf,
		ReadingTable,
		RoundTable,
		Chair,
		Bench,
		Candelabra,
		LampPost,
		Fountain,
		Well,
		MarketStall,
		OakTree,
		PineTree,
		Bush,
		Boulder,
		Rock,
		FallenLog,
		Flowers,
		Mushrooms,
		Crystal,
		Statue,
		Globe,
		Desk,
		Armchair,
		Rug,
		Banner,
		PottedPlant,
		Door,
		Cart,
		Rubble,
		Bones,
		Brazier,
		House,
		LongHouse,
		Signpost,
		WallTorch,
		Gravestone,
		Window,
		Lectern,
		Fireplace,
		StashChest,
		Anvil,
		Forge,
		GemAltar,
		MissionBoard,
		WineBarrel,
		OldBarrel,
		LowCrate,
		LongCrate,
		Bucket,
		TreasureChest,
		LanternPost,
		OilLamp,
		CandleStand,
		FirePit,
		BronzeStatue,
		PlankTable,
		Stool,
		MossRock,
		MossBoulder,
		TreeStump,
		DeadLog,
		Fern,
		Weeds,
		GrassTuft,
		DryBranches,
		IronGate,
		CryptDoor,
		Sarcophagus,
		Altar,
		CryptPillar,
		MarbleBust,
		FireBowl,
		CalderaGate,
		ObsidianCluster,
		NoticeBoard,
		OfferingBowl,
		Telescope,
		MageTower,
		PierPost,
		ChainAnchor,
		ChessTable,
		IcePillar,
		SnowRock,
		RuinColumn,
		RuinBrokenColumn,
		RuinStump,
		RuinDrum,
		RuinCapital,
		RuinArch,
		RuinWall,
		RuinWallShort,
		RuinPedestal,
		RuinRubble,
		StormSpire,
		Cairn,
		StandingStone,
		GlyphStone,
		PrayerPole,
		StoneGate,
		GraniteBoulder,
		Count,
	};

	struct PropDef
	{
		PropId Id = PropId::Barrel;
		const char* Name = "";
		std::span<const PropCollider> Colliders;
		std::span<const PropPart> Parts;
		bool Tall = false;
	};

	namespace PropShapes
	{
		constexpr PropPart Part(MeshKind mesh, Vec2 offset, float base, Vec2 half, float height, uint32_t color, float yaw, float emission)
		{
			return { mesh, offset, base, half, height, yaw, color, emission };
		}

		constexpr PropPart Box(Vec2 offset, float base, Vec2 half, float height, uint32_t color, float yaw = 0.0f, float emission = 0.0f) { return Part(MeshKind::Box, offset, base, half, height, color, yaw, emission); }
		constexpr PropPart Cylinder(Vec2 offset, float base, Vec2 half, float height, uint32_t color, float yaw = 0.0f, float emission = 0.0f) { return Part(MeshKind::Cylinder, offset, base, half, height, color, yaw, emission); }
		constexpr PropPart CylinderX(Vec2 offset, float base, Vec2 half, float height, uint32_t color, float yaw = 0.0f, float emission = 0.0f) { return Part(MeshKind::CylinderX, offset, base, half, height, color, yaw, emission); }
		constexpr PropPart Cone(Vec2 offset, float base, Vec2 half, float height, uint32_t color, float yaw = 0.0f, float emission = 0.0f) { return Part(MeshKind::Cone, offset, base, half, height, color, yaw, emission); }
		constexpr PropPart Sphere(Vec2 offset, float base, Vec2 half, float height, uint32_t color, float yaw = 0.0f, float emission = 0.0f) { return Part(MeshKind::Sphere, offset, base, half, height, color, yaw, emission); }
		constexpr PropPart Wedge(Vec2 offset, float base, Vec2 half, float height, uint32_t color, float yaw = 0.0f, float emission = 0.0f) { return Part(MeshKind::Wedge, offset, base, half, height, color, yaw, emission); }
		constexpr PropPart Roof(Vec2 offset, float base, Vec2 half, float height, uint32_t color, float yaw = 0.0f, float emission = 0.0f) { return Part(MeshKind::Roof, offset, base, half, height, color, yaw, emission); }

		constexpr PropCollider Circle(float radius, Block blocks = Block::All, Vec2 offset = {}) { return { ShapeKind::Circle, offset, { radius, radius }, radius, 0.0f, blocks }; }
		constexpr PropCollider Rect(Vec2 half, Block blocks = Block::All, Vec2 offset = {}, float yaw = 0.0f) { return { ShapeKind::Box, offset, half, 0.0f, yaw, blocks }; }
	}

	namespace PropData
	{
		using namespace PropShapes;

		inline constexpr uint32_t WoodDark = 0x4A2F1D;
		inline constexpr uint32_t WoodMid = 0x7B5434;
		inline constexpr uint32_t WoodLight = 0xA67C52;
		inline constexpr uint32_t Iron = 0x34343A;
		inline constexpr uint32_t Stone = 0x8A8378;
		inline constexpr uint32_t StoneDark = 0x5E584F;
		inline constexpr uint32_t Marble = 0xDED8CD;
		inline constexpr uint32_t MarbleShade = 0xB9B2A5;
		inline constexpr uint32_t Gold = 0xC9A23A;
		inline constexpr uint32_t Leaves = 0x3E6E2C;
		inline constexpr uint32_t LeavesLight = 0x5B8C3A;
		inline constexpr uint32_t LeavesDark = 0x2C5222;
		inline constexpr uint32_t Pine = 0x2B5230;
		inline constexpr uint32_t PineLight = 0x376640;
		inline constexpr uint32_t Trunk = 0x5A4128;
		inline constexpr uint32_t Flame = 0xFFA640;
		inline constexpr uint32_t Candle = 0xEEE6D0;
		inline constexpr uint32_t Water = 0x2E6B9A;
		inline constexpr uint32_t Plaster = 0xCFC0A2;
		inline constexpr uint32_t RoofRed = 0x8C3B2E;
		inline constexpr uint32_t RoofSlate = 0x4A5260;
		inline constexpr uint32_t ClothRed = 0x8E2B33;
		inline constexpr uint32_t ClothBlue = 0x2E4F8C;
		inline constexpr uint32_t Paper = 0xE8E0C8;
		inline constexpr uint32_t Terracotta = 0x9C5A3C;
		inline constexpr uint32_t Bone = 0xE6DFCB;
		inline constexpr uint32_t CrystalBlue = 0x7FD4FF;
		inline constexpr uint32_t Fungus = 0x4FD1C5;
		inline constexpr uint32_t Glass = 0x9FC4E8;
		inline constexpr uint32_t LampGlow = 0xFFD27A;
		inline constexpr uint32_t Sack = 0xB89B6A;
		inline constexpr uint32_t BookRed = 0x8C2A2A;
		inline constexpr uint32_t BookBlue = 0x2A4F8C;
		inline constexpr uint32_t BookGreen = 0x2E6B3A;
		inline constexpr uint32_t BookOchre = 0xA6822E;
		inline constexpr uint32_t BookPurple = 0x5B2E73;
		inline constexpr uint32_t BookBrown = 0x7A4A2A;

		constexpr PropPart Books(float side, int row, int column, uint32_t color)
		{
			return Box({ -0.62f + 0.62f * float(column), side }, 0.12f + 0.5f * float(row), { 0.29f, 0.14f }, 0.4f, color);
		}

		inline constexpr PropCollider BarrelColliders[] = { Circle(0.33f) };
		inline constexpr PropPart BarrelParts[] = {
			Cylinder({}, 0.0f, { 0.32f, 0.32f }, 0.9f, WoodMid),
			Cylinder({}, 0.14f, { 0.335f, 0.335f }, 0.07f, Iron),
			Cylinder({}, 0.69f, { 0.335f, 0.335f }, 0.07f, Iron),
			Cylinder({}, 0.9f, { 0.27f, 0.27f }, 0.02f, WoodLight),
		};

		inline constexpr PropCollider CrateColliders[] = { Rect({ 0.42f, 0.42f }) };
		inline constexpr PropPart CrateParts[] = {
			Box({}, 0.0f, { 0.4f, 0.4f }, 0.8f, WoodLight),
			Box({}, 0.74f, { 0.42f, 0.42f }, 0.08f, WoodMid),
			Box({ 0.0f, -0.41f }, 0.3f, { 0.36f, 0.02f }, 0.07f, WoodMid),
			Box({ 0.41f, 0.0f }, 0.3f, { 0.02f, 0.36f }, 0.07f, WoodMid),
		};

		inline constexpr PropCollider CrateStackColliders[] = { Rect({ 0.47f, 0.47f }) };
		inline constexpr PropPart CrateStackParts[] = {
			Box({ -0.05f, 0.05f }, 0.0f, { 0.42f, 0.42f }, 0.8f, WoodLight),
			Box({ -0.05f, 0.05f }, 0.74f, { 0.44f, 0.44f }, 0.07f, WoodMid),
			Box({ 0.05f, -0.05f }, 0.81f, { 0.32f, 0.32f }, 0.6f, WoodLight, 0.35f),
			Box({ 0.05f, -0.05f }, 1.38f, { 0.34f, 0.34f }, 0.06f, WoodMid, 0.35f),
		};

		inline constexpr PropCollider StonePillarColliders[] = { Circle(0.42f) };
		inline constexpr PropPart StonePillarParts[] = {
			Box({}, 0.0f, { 0.45f, 0.45f }, 0.3f, StoneDark),
			Cylinder({}, 0.3f, { 0.32f, 0.32f }, 2.3f, Stone),
			Box({}, 2.6f, { 0.46f, 0.46f }, 0.25f, StoneDark),
		};

		inline constexpr PropCollider MarblePillarColliders[] = { Circle(0.44f) };
		inline constexpr PropPart MarblePillarParts[] = {
			Box({}, 0.0f, { 0.48f, 0.48f }, 0.35f, MarbleShade),
			Cylinder({}, 0.35f, { 0.33f, 0.33f }, 2.75f, Marble),
			Box({}, 3.1f, { 0.5f, 0.5f }, 0.3f, MarbleShade),
		};

		inline constexpr PropCollider BrokenPillarColliders[] = { Circle(0.42f), Rect({ 0.5f, 0.3f }, Block::Movement, { 0.75f, 0.35f }, 0.6f) };
		inline constexpr PropPart BrokenPillarParts[] = {
			Box({}, 0.0f, { 0.45f, 0.45f }, 0.3f, StoneDark),
			Cylinder({}, 0.3f, { 0.32f, 0.32f }, 0.9f, Stone),
			CylinderX({ 0.75f, 0.35f }, 0.0f, { 0.5f, 0.3f }, 0.6f, Stone, 0.6f),
		};

		inline constexpr PropCollider BookshelfColliders[] = { Rect({ 1.0f, 0.27f }) };
		inline constexpr PropPart BookshelfParts[] = {
			Box({}, 0.0f, { 1.0f, 0.25f }, 2.2f, WoodDark),
			Books(-0.13f, 0, 0, BookRed), Books(-0.13f, 0, 1, BookBlue), Books(-0.13f, 0, 2, BookOchre),
			Books(-0.13f, 1, 0, BookGreen), Books(-0.13f, 1, 1, BookPurple), Books(-0.13f, 1, 2, BookBrown),
			Books(-0.13f, 2, 0, BookBlue), Books(-0.13f, 2, 1, BookBrown), Books(-0.13f, 2, 2, BookRed),
			Books(-0.13f, 3, 0, BookPurple), Books(-0.13f, 3, 1, BookOchre), Books(-0.13f, 3, 2, BookGreen),
		};

		inline constexpr PropCollider DoubleBookshelfColliders[] = { Rect({ 1.0f, 0.42f }) };
		inline constexpr PropPart DoubleBookshelfParts[] = {
			Box({}, 0.0f, { 1.0f, 0.4f }, 2.2f, WoodDark),
			Books(-0.27f, 0, 0, BookRed), Books(-0.27f, 0, 1, BookBlue), Books(-0.27f, 0, 2, BookOchre),
			Books(-0.27f, 1, 0, BookGreen), Books(-0.27f, 1, 1, BookPurple), Books(-0.27f, 1, 2, BookBrown),
			Books(-0.27f, 2, 0, BookBlue), Books(-0.27f, 2, 1, BookBrown), Books(-0.27f, 2, 2, BookRed),
			Books(-0.27f, 3, 0, BookPurple), Books(-0.27f, 3, 1, BookOchre), Books(-0.27f, 3, 2, BookGreen),
			Books(0.27f, 0, 0, BookBrown), Books(0.27f, 0, 1, BookGreen), Books(0.27f, 0, 2, BookPurple),
			Books(0.27f, 1, 0, BookOchre), Books(0.27f, 1, 1, BookRed), Books(0.27f, 1, 2, BookBlue),
			Books(0.27f, 2, 0, BookGreen), Books(0.27f, 2, 1, BookOchre), Books(0.27f, 2, 2, BookPurple),
			Books(0.27f, 3, 0, BookRed), Books(0.27f, 3, 1, BookBlue), Books(0.27f, 3, 2, BookBrown),
		};

		inline constexpr PropCollider ReadingTableColliders[] = { Rect({ 1.22f, 0.52f }, Block::Movement) };
		inline constexpr PropPart ReadingTableParts[] = {
			Box({}, 0.72f, { 1.2f, 0.5f }, 0.08f, WoodMid),
			Box({ -1.1f, -0.42f }, 0.0f, { 0.06f, 0.06f }, 0.72f, WoodDark),
			Box({ 1.1f, -0.42f }, 0.0f, { 0.06f, 0.06f }, 0.72f, WoodDark),
			Box({ -1.1f, 0.42f }, 0.0f, { 0.06f, 0.06f }, 0.72f, WoodDark),
			Box({ 1.1f, 0.42f }, 0.0f, { 0.06f, 0.06f }, 0.72f, WoodDark),
			Box({ -0.55f, 0.1f }, 0.8f, { 0.18f, 0.13f }, 0.06f, BookRed, 0.3f),
			Box({ 0.1f, -0.15f }, 0.8f, { 0.22f, 0.15f }, 0.02f, Paper, -0.2f),
			Cylinder({ 0.65f, 0.1f }, 0.8f, { 0.04f, 0.04f }, 0.16f, Candle),
			Sphere({ 0.65f, 0.1f }, 0.96f, { 0.045f, 0.045f }, 0.1f, Flame, 0.0f, 1.0f),
		};

		inline constexpr PropCollider RoundTableColliders[] = { Circle(0.56f, Block::Movement) };
		inline constexpr PropPart RoundTableParts[] = {
			Cylinder({}, 0.7f, { 0.55f, 0.55f }, 0.07f, WoodMid),
			Cylinder({}, 0.0f, { 0.08f, 0.08f }, 0.7f, WoodDark),
			Cylinder({}, 0.0f, { 0.3f, 0.3f }, 0.05f, WoodDark),
			Cylinder({ 0.1f, 0.05f }, 0.77f, { 0.04f, 0.04f }, 0.16f, Candle),
			Sphere({ 0.1f, 0.05f }, 0.93f, { 0.045f, 0.045f }, 0.1f, Flame, 0.0f, 1.0f),
		};

		inline constexpr PropPart ChairParts[] = {
			Box({}, 0.42f, { 0.22f, 0.22f }, 0.06f, WoodMid),
			Box({ -0.18f, -0.18f }, 0.0f, { 0.03f, 0.03f }, 0.42f, WoodDark),
			Box({ 0.18f, -0.18f }, 0.0f, { 0.03f, 0.03f }, 0.42f, WoodDark),
			Box({ -0.18f, 0.18f }, 0.0f, { 0.03f, 0.03f }, 0.42f, WoodDark),
			Box({ 0.18f, 0.18f }, 0.0f, { 0.03f, 0.03f }, 0.42f, WoodDark),
			Box({ 0.0f, 0.2f }, 0.48f, { 0.22f, 0.03f }, 0.5f, WoodMid),
		};

		inline constexpr PropCollider BenchColliders[] = { Rect({ 0.72f, 0.22f }, Block::Movement) };
		inline constexpr PropPart BenchParts[] = {
			Box({}, 0.4f, { 0.7f, 0.2f }, 0.07f, WoodMid),
			Box({ -0.58f, 0.0f }, 0.0f, { 0.05f, 0.17f }, 0.4f, Iron),
			Box({ 0.58f, 0.0f }, 0.0f, { 0.05f, 0.17f }, 0.4f, Iron),
		};

		inline constexpr PropCollider CandelabraColliders[] = { Circle(0.18f) };
		inline constexpr PropPart CandelabraParts[] = {
			Cylinder({}, 0.0f, { 0.18f, 0.18f }, 0.05f, Gold),
			Cylinder({}, 0.0f, { 0.04f, 0.04f }, 1.3f, Gold),
			Box({}, 1.25f, { 0.25f, 0.03f }, 0.04f, Gold),
			Cylinder({ -0.22f, 0.0f }, 1.29f, { 0.03f, 0.03f }, 0.14f, Candle),
			Cylinder({ 0.0f, 0.0f }, 1.29f, { 0.03f, 0.03f }, 0.14f, Candle),
			Cylinder({ 0.22f, 0.0f }, 1.29f, { 0.03f, 0.03f }, 0.14f, Candle),
			Sphere({ -0.22f, 0.0f }, 1.43f, { 0.04f, 0.04f }, 0.09f, Flame, 0.0f, 1.0f),
			Sphere({ 0.0f, 0.0f }, 1.43f, { 0.04f, 0.04f }, 0.09f, Flame, 0.0f, 1.0f),
			Sphere({ 0.22f, 0.0f }, 1.43f, { 0.04f, 0.04f }, 0.09f, Flame, 0.0f, 1.0f),
		};

		inline constexpr PropCollider LampPostColliders[] = { Circle(0.16f) };
		inline constexpr PropPart LampPostParts[] = {
			Cylinder({}, 0.0f, { 0.14f, 0.14f }, 0.25f, Iron),
			Cylinder({}, 0.25f, { 0.07f, 0.07f }, 2.3f, Iron),
			Box({}, 2.45f, { 0.17f, 0.17f }, 0.35f, LampGlow, 0.0f, 0.9f),
			Box({}, 2.8f, { 0.22f, 0.22f }, 0.06f, Iron),
		};

		inline constexpr PropCollider FountainColliders[] = { Circle(1.8f, Block::Movement), Circle(0.5f) };
		inline constexpr PropPart FountainParts[] = {
			Cylinder({}, 0.0f, { 1.8f, 1.8f }, 0.45f, Stone),
			Cylinder({}, 0.45f, { 1.6f, 1.6f }, 0.04f, Water, 0.0f, 0.2f),
			Cylinder({}, 0.45f, { 0.45f, 0.45f }, 0.6f, StoneDark),
			Cylinder({}, 1.05f, { 0.85f, 0.85f }, 0.18f, Stone),
			Cylinder({}, 1.23f, { 0.72f, 0.72f }, 0.03f, Water, 0.0f, 0.25f),
			Cylinder({}, 1.23f, { 0.18f, 0.18f }, 0.7f, Marble),
			Sphere({}, 1.9f, { 0.22f, 0.22f }, 0.4f, Marble),
		};

		inline constexpr PropCollider WellColliders[] = { Circle(0.78f) };
		inline constexpr PropPart WellParts[] = {
			Cylinder({}, 0.0f, { 0.75f, 0.75f }, 0.8f, Stone),
			Cylinder({}, 0.8f, { 0.6f, 0.6f }, 0.02f, Water, 0.0f, 0.15f),
			Box({ -0.7f, 0.0f }, 0.8f, { 0.06f, 0.06f }, 1.2f, WoodDark),
			Box({ 0.7f, 0.0f }, 0.8f, { 0.06f, 0.06f }, 1.2f, WoodDark),
			Box({}, 1.85f, { 0.75f, 0.04f }, 0.06f, WoodDark),
			Roof({}, 2.0f, { 0.95f, 0.7f }, 0.55f, WoodDark),
		};

		inline constexpr PropCollider MarketStallColliders[] = { Rect({ 1.36f, 0.95f }, Block::Movement) };
		inline constexpr PropPart MarketStallParts[] = {
			Box({ 0.0f, -0.55f }, 0.0f, { 1.3f, 0.3f }, 0.9f, WoodMid),
			Box({ -0.7f, -0.55f }, 0.9f, { 0.25f, 0.18f }, 0.15f, 0xC0392B),
			Box({ 0.0f, -0.55f }, 0.9f, { 0.25f, 0.18f }, 0.15f, 0xE6A23C),
			Box({ 0.7f, -0.55f }, 0.9f, { 0.25f, 0.18f }, 0.15f, 0x6E9A3A),
			Box({ -1.3f, -0.85f }, 0.0f, { 0.06f, 0.06f }, 2.3f, WoodDark),
			Box({ 1.3f, -0.85f }, 0.0f, { 0.06f, 0.06f }, 2.3f, WoodDark),
			Box({ -1.3f, 0.85f }, 0.0f, { 0.06f, 0.06f }, 2.3f, WoodDark),
			Box({ 1.3f, 0.85f }, 0.0f, { 0.06f, 0.06f }, 2.3f, WoodDark),
			Roof({}, 2.3f, { 1.55f, 1.15f }, 0.45f, 0xA33A2E),
			Box({ 0.6f, 0.45f }, 0.0f, { 0.35f, 0.35f }, 0.7f, WoodLight),
			Sphere({ -0.6f, 0.45f }, 0.0f, { 0.3f, 0.3f }, 0.6f, Sack),
		};

		inline constexpr PropCollider OakTreeColliders[] = { Circle(0.32f) };
		inline constexpr PropPart OakTreeParts[] = {
			Cylinder({}, 0.0f, { 0.28f, 0.28f }, 1.8f, Trunk),
			Sphere({}, 1.4f, { 1.3f, 1.3f }, 2.0f, Leaves),
			Sphere({ 0.6f, 0.4f }, 2.1f, { 0.95f, 0.95f }, 1.5f, LeavesLight),
			Sphere({ -0.55f, -0.35f }, 1.9f, { 0.85f, 0.85f }, 1.4f, LeavesDark),
		};

		inline constexpr PropCollider PineTreeColliders[] = { Circle(0.28f) };
		inline constexpr PropPart PineTreeParts[] = {
			Cylinder({}, 0.0f, { 0.22f, 0.22f }, 1.0f, Trunk),
			Cone({}, 0.7f, { 1.15f, 1.15f }, 1.6f, Pine),
			Cone({}, 1.7f, { 0.9f, 0.9f }, 1.4f, Pine),
			Cone({}, 2.6f, { 0.6f, 0.6f }, 1.2f, PineLight),
		};

		inline constexpr PropPart BushParts[] = {
			Sphere({}, 0.0f, { 0.6f, 0.5f }, 0.8f, Leaves),
			Sphere({ 0.35f, 0.25f }, 0.0f, { 0.4f, 0.38f }, 0.6f, LeavesLight),
			Sphere({ -0.3f, 0.1f }, 0.0f, { 0.35f, 0.35f }, 0.5f, LeavesDark),
		};

		inline constexpr PropCollider BoulderColliders[] = { Circle(0.75f) };
		inline constexpr PropPart BoulderParts[] = {
			Sphere({}, -0.1f, { 0.8f, 0.65f }, 1.2f, Stone, 0.4f),
			Sphere({ 0.5f, 0.3f }, -0.05f, { 0.45f, 0.4f }, 0.65f, StoneDark),
		};

		inline constexpr PropCollider RockColliders[] = { Circle(0.36f, Block::Movement) };
		inline constexpr PropPart RockParts[] = {
			Sphere({}, -0.05f, { 0.4f, 0.32f }, 0.55f, Stone, 0.7f),
		};

		inline constexpr PropCollider FallenLogColliders[] = { Rect({ 1.12f, 0.3f }, Block::Movement) };
		inline constexpr PropPart FallenLogParts[] = {
			CylinderX({}, 0.0f, { 1.1f, 0.28f }, 0.56f, Trunk),
			Box({ 0.2f, 0.0f }, 0.48f, { 0.5f, 0.16f }, 0.1f, LeavesDark),
		};

		inline constexpr PropPart FlowersParts[] = {
			Sphere({}, -0.05f, { 0.35f, 0.3f }, 0.15f, LeavesLight),
			Sphere({ 0.0f, 0.0f }, 0.05f, { 0.08f, 0.08f }, 0.16f, 0xE04F6A),
			Sphere({ 0.2f, 0.12f }, 0.05f, { 0.08f, 0.08f }, 0.16f, 0xF2C94C),
			Sphere({ -0.15f, 0.18f }, 0.05f, { 0.08f, 0.08f }, 0.16f, 0xB57EDC),
			Sphere({ 0.1f, -0.2f }, 0.05f, { 0.08f, 0.08f }, 0.16f, 0xF5F5F5),
			Sphere({ -0.22f, -0.1f }, 0.05f, { 0.08f, 0.08f }, 0.16f, 0xE04F6A),
		};

		inline constexpr PropPart MushroomsParts[] = {
			Cylinder({ 0.0f, 0.0f }, 0.0f, { 0.04f, 0.04f }, 0.25f, 0xD9D0BC),
			Cylinder({ 0.25f, 0.1f }, 0.0f, { 0.03f, 0.03f }, 0.18f, 0xD9D0BC),
			Cylinder({ -0.2f, 0.15f }, 0.0f, { 0.03f, 0.03f }, 0.2f, 0xD9D0BC),
			Sphere({ 0.0f, 0.0f }, 0.2f, { 0.14f, 0.14f }, 0.12f, Fungus, 0.0f, 0.6f),
			Sphere({ 0.25f, 0.1f }, 0.14f, { 0.1f, 0.1f }, 0.09f, Fungus, 0.0f, 0.6f),
			Sphere({ -0.2f, 0.15f }, 0.16f, { 0.11f, 0.11f }, 0.1f, Fungus, 0.0f, 0.6f),
		};

		inline constexpr PropCollider CrystalColliders[] = { Circle(0.3f) };
		inline constexpr PropPart CrystalParts[] = {
			Cone({}, 0.0f, { 0.22f, 0.22f }, 1.1f, CrystalBlue, 0.0f, 0.7f),
			Cone({ 0.25f, 0.1f }, 0.0f, { 0.15f, 0.15f }, 0.7f, CrystalBlue, 0.5f, 0.6f),
			Cone({ -0.18f, 0.15f }, 0.0f, { 0.12f, 0.12f }, 0.55f, CrystalBlue, 0.2f, 0.6f),
		};

		inline constexpr PropCollider StatueColliders[] = { Rect({ 0.62f, 0.62f }) };
		inline constexpr PropPart StatueParts[] = {
			Box({}, 0.0f, { 0.6f, 0.6f }, 0.8f, StoneDark),
			Box({}, 0.8f, { 0.5f, 0.5f }, 0.1f, Stone),
			Cylinder({}, 0.9f, { 0.28f, 0.24f }, 1.2f, Marble),
			Sphere({}, 2.1f, { 0.2f, 0.2f }, 0.38f, Marble),
			Box({ 0.3f, 0.0f }, 1.6f, { 0.1f, 0.08f }, 0.5f, Marble),
		};

		inline constexpr PropCollider GlobeColliders[] = { Circle(0.5f) };
		inline constexpr PropPart GlobeParts[] = {
			Cylinder({}, 0.0f, { 0.45f, 0.45f }, 0.08f, Gold),
			Cylinder({}, 0.08f, { 0.1f, 0.1f }, 0.8f, Gold),
			Sphere({}, 0.85f, { 0.5f, 0.5f }, 1.0f, 0x2A5C8C),
			Sphere({ 0.15f, -0.2f }, 1.2f, { 0.25f, 0.18f }, 0.32f, 0x4E8A3A),
		};

		inline constexpr PropCollider DeskColliders[] = { Rect({ 1.36f, 0.56f }, Block::Movement) };
		inline constexpr PropPart DeskParts[] = {
			Box({}, 0.0f, { 1.3f, 0.5f }, 0.85f, WoodDark),
			Box({}, 0.85f, { 1.35f, 0.55f }, 0.05f, WoodMid),
			Box({ -0.4f, 0.0f }, 0.9f, { 0.25f, 0.18f }, 0.01f, Paper, 0.2f),
			Cylinder({ 0.2f, 0.1f }, 0.9f, { 0.05f, 0.05f }, 0.08f, 0x1A1A1A),
			Cylinder({ 0.9f, 0.2f }, 0.9f, { 0.04f, 0.04f }, 0.16f, Candle),
			Sphere({ 0.9f, 0.2f }, 1.06f, { 0.045f, 0.045f }, 0.1f, Flame, 0.0f, 1.0f),
		};

		inline constexpr PropCollider ArmchairColliders[] = { Rect({ 0.44f, 0.4f }, Block::Movement) };
		inline constexpr PropPart ArmchairParts[] = {
			Box({}, 0.0f, { 0.42f, 0.38f }, 0.45f, ClothRed),
			Box({ 0.0f, 0.3f }, 0.45f, { 0.42f, 0.1f }, 0.55f, ClothRed),
			Box({ -0.37f, 0.0f }, 0.45f, { 0.06f, 0.38f }, 0.2f, 0x6E2027),
			Box({ 0.37f, 0.0f }, 0.45f, { 0.06f, 0.38f }, 0.2f, 0x6E2027),
		};

		inline constexpr PropPart RugParts[] = {
			Box({}, 0.0f, { 1.3f, 0.9f }, 0.02f, Gold),
			Box({}, 0.0f, { 1.2f, 0.8f }, 0.03f, ClothRed),
			Box({}, 0.0f, { 0.55f, 0.32f }, 0.04f, ClothBlue),
		};

		inline constexpr PropPart BannerParts[] = {
			Box({}, 2.3f, { 0.55f, 0.04f }, 0.08f, WoodDark),
			Box({}, 0.9f, { 0.45f, 0.03f }, 1.4f, ClothRed),
			Box({ 0.0f, -0.02f }, 1.3f, { 0.14f, 0.03f }, 0.5f, Gold),
		};

		inline constexpr PropCollider PottedPlantColliders[] = { Circle(0.24f, Block::Movement) };
		inline constexpr PropPart PottedPlantParts[] = {
			Cylinder({}, 0.0f, { 0.22f, 0.22f }, 0.4f, Terracotta),
			Sphere({}, 0.3f, { 0.35f, 0.35f }, 0.8f, Leaves),
			Sphere({ 0.15f, 0.1f }, 0.6f, { 0.22f, 0.22f }, 0.5f, LeavesLight),
		};

		inline constexpr PropCollider DoorColliders[] = { Rect({ 1.0f, 0.26f }) };
		inline constexpr PropPart DoorParts[] = {
			Box({ -0.88f, 0.0f }, 0.0f, { 0.12f, 0.26f }, 2.5f, Stone),
			Box({ 0.88f, 0.0f }, 0.0f, { 0.12f, 0.26f }, 2.5f, Stone),
			Box({}, 2.4f, { 1.0f, 0.26f }, 0.35f, StoneDark),
			Box({}, 0.0f, { 0.76f, 0.08f }, 2.4f, WoodDark),
			Box({ -0.38f, -0.085f }, 0.05f, { 0.02f, 0.01f }, 2.3f, WoodMid),
			Box({ 0.38f, -0.085f }, 0.05f, { 0.02f, 0.01f }, 2.3f, WoodMid),
			Sphere({ 0.5f, -0.12f }, 1.05f, { 0.05f, 0.05f }, 0.1f, Gold, 0.0f, 0.3f),
		};

		inline constexpr PropCollider CartColliders[] = { Rect({ 1.05f, 0.68f }, Block::Movement), Rect({ 0.5f, 0.35f }, Block::Movement, { 1.5f, 0.0f }) };
		inline constexpr PropPart CartParts[] = {
			Box({}, 0.45f, { 1.0f, 0.6f }, 0.35f, WoodMid),
			Box({ 0.0f, -0.58f }, 0.8f, { 1.0f, 0.04f }, 0.25f, WoodDark),
			Box({ 0.0f, 0.58f }, 0.8f, { 1.0f, 0.04f }, 0.25f, WoodDark),
			CylinderX({ -0.55f, -0.66f }, 0.0f, { 0.05f, 0.42f }, 0.84f, WoodDark, 1.5707963f),
			CylinderX({ 0.55f, -0.66f }, 0.0f, { 0.05f, 0.42f }, 0.84f, WoodDark, 1.5707963f),
			CylinderX({ -0.55f, 0.66f }, 0.0f, { 0.05f, 0.42f }, 0.84f, WoodDark, 1.5707963f),
			CylinderX({ 0.55f, 0.66f }, 0.0f, { 0.05f, 0.42f }, 0.84f, WoodDark, 1.5707963f),
			Box({ 1.5f, -0.3f }, 0.5f, { 0.5f, 0.04f }, 0.06f, WoodDark),
			Box({ 1.5f, 0.3f }, 0.5f, { 0.5f, 0.04f }, 0.06f, WoodDark),
			Sphere({ -0.3f, 0.0f }, 0.8f, { 0.35f, 0.3f }, 0.45f, Sack),
			Box({ 0.4f, 0.1f }, 0.8f, { 0.3f, 0.3f }, 0.5f, WoodLight, 0.3f),
		};

		inline constexpr PropPart RubbleParts[] = {
			Box({}, 0.0f, { 0.2f, 0.15f }, 0.18f, StoneDark, 0.4f),
			Box({ 0.25f, 0.15f }, 0.0f, { 0.14f, 0.12f }, 0.14f, Stone, 1.1f),
			Box({ -0.2f, 0.2f }, 0.0f, { 0.12f, 0.1f }, 0.12f, Stone, 0.7f),
			Box({ 0.05f, -0.22f }, 0.0f, { 0.16f, 0.1f }, 0.1f, StoneDark, 2.0f),
		};

		inline constexpr PropPart BonesParts[] = {
			Box({}, 0.0f, { 0.25f, 0.04f }, 0.05f, Bone, 0.5f),
			Box({ 0.1f, 0.1f }, 0.0f, { 0.2f, 0.03f }, 0.04f, Bone, 2.1f),
			Sphere({ -0.2f, 0.1f }, 0.0f, { 0.1f, 0.09f }, 0.16f, Bone),
		};

		inline constexpr PropCollider BrazierColliders[] = { Circle(0.32f) };
		inline constexpr PropPart BrazierParts[] = {
			Cylinder({}, 0.0f, { 0.12f, 0.12f }, 0.6f, Iron),
			Cylinder({}, 0.6f, { 0.32f, 0.32f }, 0.22f, Iron),
			Sphere({}, 0.72f, { 0.26f, 0.26f }, 0.35f, Flame, 0.0f, 1.0f),
			Cone({}, 0.9f, { 0.16f, 0.16f }, 0.45f, LampGlow, 0.0f, 1.0f),
		};

		inline constexpr PropCollider HouseColliders[] = { Rect({ 2.0f, 2.0f }) };
		inline constexpr PropPart HouseParts[] = {
			Box({}, 0.0f, { 2.0f, 2.0f }, 2.4f, Plaster),
			Box({ -2.0f, -2.0f }, 0.0f, { 0.1f, 0.1f }, 2.4f, WoodDark),
			Box({ 2.0f, -2.0f }, 0.0f, { 0.1f, 0.1f }, 2.4f, WoodDark),
			Box({ -2.0f, 2.0f }, 0.0f, { 0.1f, 0.1f }, 2.4f, WoodDark),
			Box({ 2.0f, 2.0f }, 0.0f, { 0.1f, 0.1f }, 2.4f, WoodDark),
			Box({ 0.0f, -2.02f }, 2.25f, { 2.05f, 0.04f }, 0.15f, WoodDark),
			Box({ 0.0f, 2.02f }, 2.25f, { 2.05f, 0.04f }, 0.15f, WoodDark),
			Roof({}, 2.4f, { 2.35f, 2.35f }, 1.6f, RoofRed),
			Box({ 0.6f, -2.02f }, 0.0f, { 0.4f, 0.05f }, 1.7f, WoodDark),
			Box({ -1.1f, -2.03f }, 1.1f, { 0.32f, 0.04f }, 0.55f, LampGlow, 0.0f, 0.6f),
			Box({ 1.2f, 0.9f }, 2.6f, { 0.25f, 0.25f }, 1.7f, StoneDark),
		};

		inline constexpr PropCollider LongHouseColliders[] = { Rect({ 3.0f, 2.0f }) };
		inline constexpr PropPart LongHouseParts[] = {
			Box({}, 0.0f, { 3.0f, 2.0f }, 2.6f, Plaster),
			Box({ -3.0f, -2.0f }, 0.0f, { 0.1f, 0.1f }, 2.6f, WoodDark),
			Box({ 3.0f, -2.0f }, 0.0f, { 0.1f, 0.1f }, 2.6f, WoodDark),
			Box({ -3.0f, 2.0f }, 0.0f, { 0.1f, 0.1f }, 2.6f, WoodDark),
			Box({ 3.0f, 2.0f }, 0.0f, { 0.1f, 0.1f }, 2.6f, WoodDark),
			Box({ 0.0f, -2.02f }, 2.45f, { 3.05f, 0.04f }, 0.15f, WoodDark),
			Box({ 0.0f, -2.02f }, 0.0f, { 0.08f, 0.04f }, 2.5f, WoodDark),
			Roof({}, 2.6f, { 3.35f, 2.35f }, 1.7f, RoofSlate),
			Box({ -1.6f, -2.02f }, 0.0f, { 0.42f, 0.05f }, 1.8f, WoodDark),
			Box({ 1.0f, -2.03f }, 1.1f, { 0.34f, 0.04f }, 0.6f, LampGlow, 0.0f, 0.6f),
			Box({ 2.2f, -2.03f }, 1.1f, { 0.34f, 0.04f }, 0.6f, LampGlow, 0.0f, 0.6f),
			Box({ -2.2f, 0.8f }, 2.8f, { 0.28f, 0.28f }, 1.8f, StoneDark),
		};

		inline constexpr PropCollider SignpostColliders[] = { Circle(0.1f, Block::Movement) };
		inline constexpr PropPart SignpostParts[] = {
			Cylinder({}, 0.0f, { 0.06f, 0.06f }, 1.7f, WoodDark),
			Box({ 0.25f, 0.0f }, 1.35f, { 0.35f, 0.04f }, 0.25f, WoodLight, 0.15f),
			Box({ -0.2f, 0.0f }, 1.05f, { 0.3f, 0.04f }, 0.22f, WoodLight, -0.25f),
		};

		inline constexpr PropPart WallTorchParts[] = {
			Box({ 0.0f, 0.05f }, 1.5f, { 0.05f, 0.12f }, 0.08f, Iron),
			Cylinder({}, 1.45f, { 0.04f, 0.04f }, 0.4f, WoodDark),
			Sphere({}, 1.85f, { 0.09f, 0.09f }, 0.2f, Flame, 0.0f, 1.0f),
		};

		inline constexpr PropCollider GravestoneColliders[] = { Rect({ 0.36f, 0.12f }, Block::Movement) };
		inline constexpr PropPart GravestoneParts[] = {
			Box({}, 0.0f, { 0.45f, 0.25f }, 0.08f, StoneDark),
			Box({}, 0.0f, { 0.35f, 0.1f }, 0.8f, Stone),
		};

		inline constexpr PropPart WindowParts[] = {
			Box({}, 0.9f, { 0.55f, 0.06f }, 1.5f, WoodDark),
			Box({ 0.0f, -0.01f }, 0.98f, { 0.47f, 0.06f }, 1.34f, Glass, 0.0f, 0.55f),
			Box({ 0.0f, -0.02f }, 0.98f, { 0.03f, 0.06f }, 1.34f, WoodDark),
			Box({ 0.0f, -0.02f }, 1.62f, { 0.47f, 0.06f }, 0.04f, WoodDark),
		};

		inline constexpr PropCollider LecternColliders[] = { Circle(0.3f, Block::Movement) };
		inline constexpr PropPart LecternParts[] = {
			Box({}, 0.0f, { 0.25f, 0.25f }, 0.08f, WoodDark),
			Box({}, 0.08f, { 0.1f, 0.1f }, 0.92f, WoodDark),
			Wedge({}, 1.0f, { 0.35f, 0.28f }, 0.22f, WoodMid),
			Box({ 0.0f, -0.02f }, 1.13f, { 0.26f, 0.16f }, 0.03f, Paper),
		};

		inline constexpr PropCollider FireplaceColliders[] = { Rect({ 1.02f, 0.42f }) };
		inline constexpr PropPart FireplaceParts[] = {
			Box({}, 0.0f, { 1.0f, 0.4f }, 1.6f, StoneDark),
			Box({ 0.0f, -0.32f }, 0.12f, { 0.62f, 0.12f }, 0.85f, 0x1A1410),
			Sphere({ 0.0f, -0.34f }, 0.15f, { 0.38f, 0.14f }, 0.45f, Flame, 0.0f, 1.0f),
			Box({}, 1.6f, { 1.15f, 0.48f }, 0.12f, WoodDark),
			Box({}, 1.72f, { 0.7f, 0.3f }, 1.2f, StoneDark),
		};

		inline constexpr PropCollider StashChestColliders[] = { Rect({ 0.66f, 0.44f }) };
		inline constexpr PropPart StashChestParts[] = {
			Box({}, 0.0f, { 0.62f, 0.4f }, 0.55f, WoodMid),
			Box({}, 0.55f, { 0.64f, 0.42f }, 0.2f, WoodDark),
			Box({ -0.42f, 0.0f }, 0.0f, { 0.05f, 0.43f }, 0.76f, Iron),
			Box({ 0.42f, 0.0f }, 0.0f, { 0.05f, 0.43f }, 0.76f, Iron),
			Box({ 0.0f, -0.43f }, 0.38f, { 0.09f, 0.03f }, 0.17f, Gold, 0.0f, 0.25f),
			Box({}, 0.75f, { 0.5f, 0.3f }, 0.04f, Gold),
		};

		inline constexpr PropCollider AnvilColliders[] = { Circle(0.48f) };
		inline constexpr PropPart AnvilParts[] = {
			Cylinder({}, 0.0f, { 0.32f, 0.32f }, 0.42f, Trunk),
			Box({}, 0.42f, { 0.13f, 0.11f }, 0.2f, Iron),
			Box({}, 0.62f, { 0.42f, 0.17f }, 0.16f, Iron),
			Box({ 0.5f, 0.0f }, 0.66f, { 0.12f, 0.08f }, 0.09f, Iron),
			Box({ -0.15f, 0.05f }, 0.78f, { 0.18f, 0.04f }, 0.05f, 0x5A5A63),
		};

		inline constexpr PropCollider ForgeColliders[] = { Rect({ 1.05f, 0.85f }) };
		inline constexpr PropPart ForgeParts[] = {
			Box({}, 0.0f, { 1.0f, 0.8f }, 1.25f, Stone),
			Box({ 0.0f, -0.81f }, 0.22f, { 0.48f, 0.03f }, 0.6f, 0x1A1410),
			Sphere({ 0.0f, -0.6f }, 0.2f, { 0.42f, 0.25f }, 0.4f, Flame, 0.0f, 1.0f),
			Cone({ 0.0f, -0.7f }, 0.3f, { 0.22f, 0.12f }, 0.45f, LampGlow, 0.0f, 1.0f),
			Box({}, 1.25f, { 0.9f, 0.7f }, 0.22f, StoneDark),
			Box({ 0.4f, 0.3f }, 1.47f, { 0.26f, 0.26f }, 1.7f, StoneDark),
			Box({ 0.4f, 0.3f }, 3.17f, { 0.3f, 0.3f }, 0.12f, Stone),
		};

		inline constexpr PropCollider GemAltarColliders[] = { Circle(0.58f) };
		inline constexpr PropPart GemAltarParts[] = {
			Cylinder({}, 0.0f, { 0.58f, 0.58f }, 0.25f, Marble),
			Cylinder({}, 0.25f, { 0.3f, 0.3f }, 0.8f, MarbleShade),
			Cylinder({}, 1.05f, { 0.48f, 0.48f }, 0.12f, Marble),
			Sphere({}, 1.2f, { 0.13f, 0.13f }, 0.22f, 0xE7D3FF, 0.0f, 1.0f),
			Cone({}, 1.38f, { 0.19f, 0.19f }, 0.6f, 0xC79BFF, 0.0f, 0.9f),
			Cone({ 0.3f, 0.1f }, 1.17f, { 0.08f, 0.08f }, 0.28f, 0x9CC8FF, 0.0f, 0.7f),
			Cone({ -0.26f, -0.16f }, 1.17f, { 0.07f, 0.07f }, 0.24f, 0xE45AA9, 0.0f, 0.7f),
		};

		inline constexpr PropCollider MissionBoardColliders[] = { Rect({ 1.22f, 0.22f }) };
		inline constexpr PropPart MissionBoardParts[] = {
			Box({ -1.12f, 0.0f }, 0.0f, { 0.09f, 0.09f }, 2.3f, WoodDark),
			Box({ 1.12f, 0.0f }, 0.0f, { 0.09f, 0.09f }, 2.3f, WoodDark),
			Box({}, 0.75f, { 1.08f, 0.06f }, 1.3f, WoodMid),
			Box({ -0.58f, -0.08f }, 0.95f, { 0.22f, 0.02f }, 0.32f, Paper),
			Box({ 0.05f, -0.08f }, 1.18f, { 0.26f, 0.02f }, 0.38f, 0xD9C79A),
			Box({ 0.62f, -0.08f }, 0.92f, { 0.2f, 0.02f }, 0.3f, Paper),
			Box({ -0.2f, -0.08f }, 1.62f, { 0.38f, 0.02f }, 0.3f, 0xB89B6A),
			Box({ 0.0f, -0.1f }, 1.75f, { 0.05f, 0.02f }, 0.05f, ClothRed),
			Roof({}, 2.3f, { 1.35f, 0.42f }, 0.38f, RoofRed),
		};

		inline constexpr PropCollider WineBarrelColliders[] = { Circle(0.37f) };
		inline constexpr PropPart WineBarrelParts[] = { Cylinder({}, 0.0f, { 0.36f, 0.36f }, 0.87f, WoodMid) };
		inline constexpr PropCollider OldBarrelColliders[] = { Circle(0.32f) };
		inline constexpr PropPart OldBarrelParts[] = { Cylinder({}, 0.0f, { 0.31f, 0.31f }, 0.93f, WoodDark) };
		inline constexpr PropCollider LowCrateColliders[] = { Rect({ 0.42f, 0.21f }) };
		inline constexpr PropPart LowCrateParts[] = { Box({}, 0.0f, { 0.41f, 0.2f }, 0.35f, WoodLight) };
		inline constexpr PropCollider LongCrateColliders[] = { Rect({ 0.27f, 0.58f }) };
		inline constexpr PropPart LongCrateParts[] = { Box({}, 0.0f, { 0.26f, 0.58f }, 0.46f, WoodLight) };
		inline constexpr PropCollider BucketColliders[] = { Circle(0.3f, Block::Movement) };
		inline constexpr PropPart BucketParts[] = { Cylinder({}, 0.0f, { 0.3f, 0.3f }, 0.35f, WoodMid) };
		inline constexpr PropCollider TreasureChestColliders[] = { Rect({ 0.48f, 0.26f }) };
		inline constexpr PropPart TreasureChestParts[] = { Box({}, 0.0f, { 0.48f, 0.26f }, 0.62f, WoodMid) };
		inline constexpr PropCollider LanternPostColliders[] = { Circle(0.13f) };
		inline constexpr PropPart LanternPostParts[] = {
			Box({}, 0.0f, { 0.07f, 0.07f }, 2.15f, WoodDark),
			Box({}, 2.15f, { 0.11f, 0.11f }, 0.45f, LampGlow, 0.0f, 0.8f),
		};
		inline constexpr PropPart OilLampParts[] = { Cylinder({}, 0.0f, { 0.1f, 0.1f }, 0.47f, Gold) };
		inline constexpr PropCollider CandleStandColliders[] = { Circle(0.18f) };
		inline constexpr PropPart CandleStandParts[] = {
			Cylinder({}, 0.0f, { 0.12f, 0.12f }, 0.75f, Gold),
			Sphere({}, 0.78f, { 0.03f, 0.03f }, 0.06f, Flame, 0.0f, 1.0f),
		};
		inline constexpr PropCollider FirePitColliders[] = { Circle(0.72f, Block::Movement) };
		inline constexpr PropPart FirePitParts[] = {
			Cylinder({}, 0.0f, { 0.72f, 0.72f }, 0.39f, Stone),
			Cone({}, 0.2f, { 0.35f, 0.35f }, 0.6f, Flame, 0.0f, 1.0f),
		};
		inline constexpr PropCollider BronzeStatueColliders[] = { Circle(0.82f) };
		inline constexpr PropPart BronzeStatueParts[] = {
			Box({}, 0.0f, { 0.85f, 0.85f }, 0.4f, StoneDark),
			Cylinder({}, 0.4f, { 0.5f, 0.5f }, 1.7f, 0x6E5A38),
		};
		inline constexpr PropCollider PlankTableColliders[] = { Rect({ 0.9f, 0.33f }) };
		inline constexpr PropPart PlankTableParts[] = { Box({}, 0.0f, { 0.9f, 0.33f }, 0.55f, WoodMid) };
		inline constexpr PropCollider StoolColliders[] = { Circle(0.24f, Block::Movement) };
		inline constexpr PropPart StoolParts[] = { Cylinder({}, 0.0f, { 0.24f, 0.24f }, 0.44f, WoodLight) };
		inline constexpr PropCollider MossRockColliders[] = { Circle(0.6f) };
		inline constexpr PropPart MossRockParts[] = { Sphere({}, 0.0f, { 0.65f, 0.65f }, 0.7f, Stone) };
		inline constexpr PropCollider MossBoulderColliders[] = { Circle(0.95f) };
		inline constexpr PropPart MossBoulderParts[] = { Sphere({}, 0.0f, { 1.0f, 1.0f }, 1.2f, StoneDark) };
		inline constexpr PropCollider TreeStumpColliders[] = { Circle(0.62f) };
		inline constexpr PropPart TreeStumpParts[] = { Cylinder({}, 0.0f, { 0.6f, 0.6f }, 0.57f, Trunk) };
		inline constexpr PropCollider DeadLogColliders[] = { Rect({ 1.45f, 0.15f }, Block::Movement) };
		inline constexpr PropPart DeadLogParts[] = { CylinderX({}, 0.0f, { 1.5f, 0.15f }, 0.3f, Trunk) };
		inline constexpr PropPart FernParts[] = { Cone({}, 0.0f, { 0.45f, 0.45f }, 0.4f, LeavesDark) };
		inline constexpr PropPart WeedsParts[] = { Cone({}, 0.0f, { 0.12f, 0.12f }, 0.25f, Leaves) };
		inline constexpr PropPart GrassTuftParts[] = { Cone({}, 0.0f, { 0.12f, 0.12f }, 0.25f, LeavesLight) };
		inline constexpr PropPart DryBranchesParts[] = { CylinderX({}, 0.0f, { 0.6f, 0.04f }, 0.08f, Trunk) };
		inline constexpr PropCollider IronGateColliders[] = {
			Rect({ 0.04f, 0.72f }, Block::Movement, { -1.42f, -0.72f }),
			Rect({ 0.04f, 0.72f }, Block::Movement, { 1.42f, -0.72f }),
		};
		inline constexpr PropPart IronGateParts[] = {
			Box({ -1.42f, -0.72f }, 0.0f, { 0.03f, 0.72f }, 2.9f, Iron),
			Box({ 1.42f, -0.72f }, 0.0f, { 0.03f, 0.72f }, 2.9f, Iron),
		};
		inline constexpr PropCollider CryptDoorColliders[] = {
			Rect({ 0.08f, 0.48f }, Block::Movement, { -0.92f, 0.5f }),
			Rect({ 0.08f, 0.48f }, Block::Movement, { 0.92f, 0.5f }),
		};
		inline constexpr PropPart CryptDoorParts[] = {
			Box({ -0.92f, 0.5f }, 0.0f, { 0.06f, 0.48f }, 2.8f, WoodDark),
			Box({ 0.92f, 0.5f }, 0.0f, { 0.06f, 0.48f }, 2.8f, WoodDark),
		};
		inline constexpr PropCollider SarcophagusColliders[] = { Rect({ 1.05f, 0.48f }) };
		inline constexpr PropPart SarcophagusParts[] = { Box({}, 0.0f, { 1.05f, 0.48f }, 0.9f, Stone) };
		inline constexpr PropCollider AltarColliders[] = { Rect({ 1.0f, 0.45f }) };
		inline constexpr PropPart AltarParts[] = { Box({}, 0.0f, { 1.0f, 0.45f }, 1.0f, StoneDark) };
		inline constexpr PropCollider CryptPillarColliders[] = { Circle(0.36f) };
		inline constexpr PropPart CryptPillarParts[] = { Box({}, 0.0f, { 0.34f, 0.34f }, 3.6f, Stone) };
		inline constexpr PropCollider MarbleBustColliders[] = { Circle(0.3f) };
		inline constexpr PropPart MarbleBustParts[] = {
			Box({}, 0.0f, { 0.25f, 0.25f }, 1.05f, StoneDark),
			Sphere({}, 1.05f, { 0.15f, 0.15f }, 0.5f, Marble),
		};
		inline constexpr PropCollider FireBowlColliders[] = { Circle(0.34f) };
		inline constexpr PropPart FireBowlParts[] = {
			Box({}, 0.0f, { 0.24f, 0.24f }, 0.86f, StoneDark),
			Cylinder({}, 0.86f, { 0.34f, 0.34f }, 0.16f, Stone),
			Cone({}, 0.98f, { 0.2f, 0.2f }, 0.4f, Flame, 0.0f, 1.0f),
		};

		inline constexpr uint32_t Basalt = 0x2E2724;
		inline constexpr uint32_t BasaltEdge = 0x4A3F38;
		inline constexpr uint32_t Ember = 0xFF6A1F;

		inline constexpr PropCollider CalderaGateColliders[] = { Rect({ 1.75f, 0.42f }) };
		inline constexpr PropPart CalderaGateParts[] = {
			Box({}, 0.0f, { 2.05f, 0.7f }, 0.16f, StoneDark),
			Box({ -1.38f, 0.0f }, 0.16f, { 0.34f, 0.34f }, 2.9f, Basalt),
			Box({ 1.38f, 0.0f }, 0.16f, { 0.34f, 0.34f }, 2.9f, Basalt),
			Box({ -1.38f, 0.0f }, 0.16f, { 0.4f, 0.4f }, 0.3f, BasaltEdge),
			Box({ 1.38f, 0.0f }, 0.16f, { 0.4f, 0.4f }, 0.3f, BasaltEdge),
			Box({}, 3.06f, { 1.95f, 0.44f }, 0.46f, Basalt),
			Box({}, 3.52f, { 1.6f, 0.36f }, 0.12f, BasaltEdge),
			Box({}, 0.16f, { 1.04f, 0.06f }, 2.9f, Ember, 0.0f, 0.75f),
			Box({ 0.0f, -0.08f }, 0.16f, { 0.86f, 0.02f }, 2.6f, Flame, 0.0f, 1.0f),
			Sphere({}, 3.64f, { 0.22f, 0.22f }, 0.4f, Flame, 0.0f, 1.0f),
			Cone({ -1.25f, 0.0f }, 3.64f, { 0.13f, 0.13f }, 0.32f, Ember, 0.0f, 0.9f),
			Cone({ 1.25f, 0.0f }, 3.64f, { 0.13f, 0.13f }, 0.32f, Ember, 0.0f, 0.9f),
		};

		inline constexpr uint32_t Obsidian = 0x17141B;

		inline constexpr PropCollider ObsidianClusterColliders[] = { Circle(0.8f) };
		inline constexpr PropPart ObsidianClusterParts[] = {
			Cone({}, 0.0f, { 0.36f, 0.36f }, 2.9f, Obsidian),
			Cone({ 0.45f, 0.18f }, 0.0f, { 0.26f, 0.26f }, 1.8f, Obsidian, 0.4f),
			Cone({ -0.4f, 0.3f }, 0.0f, { 0.22f, 0.22f }, 1.4f, Obsidian, 0.2f),
			Cone({ 0.08f, -0.48f }, 0.0f, { 0.18f, 0.18f }, 1.0f, Obsidian, 0.7f),
		};

		inline constexpr PropCollider NoticeBoardColliders[] = { Rect({ 0.76f, 0.1f }) };
		inline constexpr PropPart NoticeBoardParts[] = {
			Box({ -0.68f, 0.0f }, 0.0f, { 0.05f, 0.05f }, 2.15f, WoodDark),
			Box({ 0.68f, 0.0f }, 0.0f, { 0.05f, 0.05f }, 2.15f, WoodDark),
			Box({}, 0.88f, { 0.7f, 0.04f }, 1.0f, WoodMid),
			Roof({}, 2.15f, { 0.82f, 0.36f }, 0.2f, WoodDark),
		};

		inline constexpr PropCollider OfferingBowlColliders[] = { Circle(0.34f) };
		inline constexpr PropPart OfferingBowlParts[] = {
			Box({}, 0.0f, { 0.29f, 0.29f }, 0.14f, Stone),
			Cylinder({}, 0.14f, { 0.14f, 0.14f }, 0.36f, Stone),
			Cylinder({}, 0.5f, { 0.34f, 0.34f }, 0.22f, Stone),
		};

		inline constexpr PropCollider TelescopeColliders[] = { Circle(0.42f) };
		inline constexpr PropPart TelescopeParts[] = {
			Cone({}, 0.0f, { 0.45f, 0.45f }, 1.25f, WoodDark),
			CylinderX({ 0.0f, 0.1f }, 1.3f, { 0.4f, 0.06f }, 0.12f, Gold),
		};

		inline constexpr PropCollider MageTowerColliders[] = { Circle(1.95f) };
		inline constexpr PropPart MageTowerParts[] = {
			Cylinder({}, 0.0f, { 1.9f, 1.9f }, 0.38f, StoneDark),
			Cylinder({}, 0.38f, { 1.55f, 1.55f }, 5.5f, Stone),
			Cone({}, 5.88f, { 1.95f, 1.95f }, 2.95f, RoofSlate),
		};

		inline constexpr PropCollider PierPostColliders[] = { Circle(0.17f) };
		inline constexpr PropPart PierPostParts[] = {
			Cylinder({}, 0.0f, { 0.15f, 0.15f }, 0.95f, StoneDark),
			Cylinder({}, 0.95f, { 0.17f, 0.17f }, 0.12f, Stone),
		};

		inline constexpr PropCollider ChainAnchorColliders[] = { Rect({ 0.29f, 0.25f }) };
		inline constexpr PropPart ChainAnchorParts[] = {
			Box({}, 0.0f, { 0.29f, 0.25f }, 0.34f, Basalt),
		};

		inline constexpr PropCollider ChessTableColliders[] = { Rect({ 0.98f, 0.98f }), Circle(0.2f, Block::Movement, { 1.4f, 0.0f }), Circle(0.2f, Block::Movement, { -1.4f, 0.0f }) };
		inline constexpr PropPart ChessTableParts[] = {
			Cylinder({}, 0.0f, { 0.4f, 0.4f }, 0.54f, Stone),
			Box({}, 0.54f, { 0.98f, 0.98f }, 0.08f, Marble),
			Cylinder({ 1.4f, 0.0f }, 0.0f, { 0.18f, 0.18f }, 0.44f, StoneDark),
			Cylinder({ -1.4f, 0.0f }, 0.0f, { 0.18f, 0.18f }, 0.44f, Stone),
		};

		inline constexpr uint32_t GlacierIce = 0xA9DDF2;

		inline constexpr PropCollider IcePillarColliders[] = { Circle(0.55f) };
		inline constexpr PropPart IcePillarParts[] = {
			Cone({}, 0.0f, { 0.55f, 0.55f }, 2.6f, GlacierIce, 0.0f, 0.12f),
			Cone({ 0.3f, 0.22f }, 0.0f, { 0.28f, 0.28f }, 1.5f, GlacierIce, 0.5f, 0.12f),
		};

		inline constexpr PropCollider SnowRockColliders[] = { Circle(0.7f) };
		inline constexpr PropPart SnowRockParts[] = {
			Sphere({}, 0.0f, { 0.75f, 0.75f }, 0.6f, StoneDark),
			Sphere({}, 0.38f, { 0.62f, 0.62f }, 0.24f, 0xE8F0F6),
		};

		inline constexpr uint32_t Limestone = 0xD9CCB0;
		inline constexpr uint32_t LimestoneShade = 0xB6A787;

		inline constexpr PropCollider RuinColumnColliders[] = { Circle(0.45f) };
		inline constexpr PropPart RuinColumnParts[] = {
			Box({}, 0.0f, { 0.47f, 0.47f }, 0.3f, LimestoneShade),
			Cylinder({}, 0.3f, { 0.32f, 0.32f }, 2.7f, Limestone),
			Box({}, 3.0f, { 0.44f, 0.44f }, 0.3f, LimestoneShade),
		};

		inline constexpr PropPart RuinBrokenColumnParts[] = {
			Box({}, 0.0f, { 0.47f, 0.47f }, 0.3f, LimestoneShade),
			Cylinder({}, 0.3f, { 0.32f, 0.32f }, 1.3f, Limestone),
		};

		inline constexpr PropCollider RuinStumpColliders[] = { Circle(0.45f) };
		inline constexpr PropPart RuinStumpParts[] = {
			Box({}, 0.0f, { 0.47f, 0.47f }, 0.3f, LimestoneShade),
			Cylinder({}, 0.3f, { 0.32f, 0.32f }, 0.42f, Limestone),
		};

		inline constexpr PropCollider RuinDrumColliders[] = { Rect({ 0.41f, 0.34f }) };
		inline constexpr PropPart RuinDrumParts[] = {
			CylinderX({}, 0.0f, { 0.4f, 0.34f }, 0.68f, Limestone),
		};

		inline constexpr PropCollider RuinCapitalColliders[] = { Rect({ 0.5f, 0.52f }, Block::Movement) };
		inline constexpr PropPart RuinCapitalParts[] = {
			Box({}, 0.0f, { 0.45f, 0.45f }, 0.45f, LimestoneShade, 0.12f),
		};

		inline constexpr PropCollider RuinArchColliders[] = { Rect({ 0.32f, 0.32f }, Block::All, { -1.25f, 0.0f }), Rect({ 0.32f, 0.32f }, Block::All, { 1.25f, 0.0f }) };
		inline constexpr PropPart RuinArchParts[] = {
			Box({ -1.25f, 0.0f }, 0.0f, { 0.3f, 0.3f }, 1.9f, Limestone),
			Box({ 1.25f, 0.0f }, 0.0f, { 0.3f, 0.3f }, 1.9f, Limestone),
			Box({}, 1.9f, { 1.55f, 0.3f }, 1.0f, LimestoneShade),
		};

		inline constexpr PropCollider RuinWallColliders[] = { Rect({ 1.3f, 0.26f }, Block::All, { 0.0f, 0.07f }) };
		inline constexpr PropPart RuinWallParts[] = {
			Box({ -0.4f, 0.0f }, 0.0f, { 0.9f, 0.25f }, 1.4f, Limestone),
			Box({ 0.85f, 0.0f }, 0.0f, { 0.45f, 0.25f }, 0.9f, LimestoneShade),
		};

		inline constexpr PropCollider RuinWallShortColliders[] = { Rect({ 0.81f, 0.26f }, Block::All, { 0.0f, 0.35f }), Rect({ 0.26f, 0.35f }, Block::All, { 0.54f, -0.25f }) };
		inline constexpr PropPart RuinWallShortParts[] = {
			Box({ 0.0f, 0.35f }, 0.0f, { 0.8f, 0.25f }, 1.0f, Limestone),
			Box({ 0.54f, -0.25f }, 0.0f, { 0.25f, 0.35f }, 0.8f, LimestoneShade),
		};

		inline constexpr PropCollider RuinPedestalColliders[] = { Rect({ 0.6f, 0.6f }) };
		inline constexpr PropPart RuinPedestalParts[] = {
			Box({}, 0.0f, { 0.6f, 0.6f }, 0.35f, LimestoneShade),
			Box({}, 0.35f, { 0.45f, 0.45f }, 0.45f, Limestone),
		};

		inline constexpr PropPart RuinRubbleParts[] = {
			Box({ -0.3f, 0.1f }, 0.0f, { 0.22f, 0.16f }, 0.2f, Limestone, 0.4f),
			Box({ 0.25f, -0.15f }, 0.0f, { 0.16f, 0.14f }, 0.16f, LimestoneShade, 1.1f),
		};

		inline constexpr uint32_t StormGlow = 0x9FD8FF;

		inline constexpr PropCollider StormSpireColliders[] = { Circle(0.62f) };
		inline constexpr PropPart StormSpireParts[] = {
			Box({}, 0.0f, { 0.6f, 0.6f }, 0.78f, LimestoneShade),
			Cylinder({}, 0.78f, { 0.32f, 0.32f }, 2.5f, Limestone),
			Cone({}, 3.28f, { 0.24f, 0.24f }, 0.9f, StormGlow, 0.0f, 1.0f),
		};

		inline constexpr PropCollider CairnColliders[] = { Circle(0.42f) };
		inline constexpr PropPart CairnParts[] = {
			Sphere({}, 0.0f, { 0.42f, 0.38f }, 0.34f, StoneDark),
			Sphere({ 0.04f, -0.02f }, 0.3f, { 0.3f, 0.28f }, 0.26f, StoneDark, 0.6f),
			Sphere({ -0.02f, 0.02f }, 0.53f, { 0.2f, 0.18f }, 0.2f, StoneDark, 1.2f),
		};

		inline constexpr uint32_t Granite = 0x6E6F72;
		inline constexpr uint32_t GraniteDark = 0x4C4E53;

		inline constexpr PropCollider StandingStoneColliders[] = { Rect({ 0.44f, 0.26f }) };
		inline constexpr PropPart StandingStoneParts[] = {
			Box({}, 0.0f, { 0.42f, 0.24f }, 2.4f, Granite),
			Box({ 0.0f, -0.25f }, 1.2f, { 0.12f, 0.02f }, 0.8f, StormGlow, 0.0f, 0.6f),
		};

		inline constexpr PropCollider GlyphStoneColliders[] = { Circle(0.36f) };
		inline constexpr PropPart GlyphStoneParts[] = {
			Box({}, 0.0f, { 0.3f, 0.2f }, 1.0f, Granite),
			Box({ 0.0f, -0.21f }, 0.45f, { 0.1f, 0.02f }, 0.36f, StormGlow, 0.0f, 0.6f),
		};

		inline constexpr PropCollider PrayerPoleColliders[] = { Circle(0.22f) };
		inline constexpr PropPart PrayerPoleParts[] = {
			Cylinder({}, 0.0f, { 0.06f, 0.06f }, 2.6f, WoodDark),
			Box({ 0.0f, 0.22f }, 1.7f, { 0.02f, 0.2f }, 0.7f, ClothBlue),
		};

		inline constexpr PropCollider StoneGateColliders[] = { Circle(0.34f, Block::All, { -1.15f, 0.0f }), Circle(0.34f, Block::All, { 1.15f, 0.0f }) };
		inline constexpr PropPart StoneGateParts[] = {
			Box({ -1.15f, 0.0f }, 0.0f, { 0.3f, 0.3f }, 2.5f, Granite),
			Box({ 1.15f, 0.0f }, 0.0f, { 0.3f, 0.3f }, 2.5f, Granite),
			Box({}, 2.5f, { 1.65f, 0.34f }, 0.45f, GraniteDark),
		};

		inline constexpr PropCollider GraniteBoulderColliders[] = { Circle(0.62f) };
		inline constexpr PropPart GraniteBoulderParts[] = {
			Sphere({}, -0.1f, { 0.7f, 0.6f }, 1.0f, Granite, 0.4f),
		};
	}

	inline constexpr PropDef Props[] = {
		{ PropId::Barrel, "Barrel", PropData::BarrelColliders, PropData::BarrelParts },
		{ PropId::Crate, "Crate", PropData::CrateColliders, PropData::CrateParts },
		{ PropId::CrateStack, "Crate stack", PropData::CrateStackColliders, PropData::CrateStackParts },
		{ PropId::StonePillar, "Stone pillar", PropData::StonePillarColliders, PropData::StonePillarParts, true },
		{ PropId::MarblePillar, "Marble pillar", PropData::MarblePillarColliders, PropData::MarblePillarParts, true },
		{ PropId::BrokenPillar, "Broken pillar", PropData::BrokenPillarColliders, PropData::BrokenPillarParts },
		{ PropId::Bookshelf, "Bookshelf", PropData::BookshelfColliders, PropData::BookshelfParts, true },
		{ PropId::DoubleBookshelf, "Double bookshelf", PropData::DoubleBookshelfColliders, PropData::DoubleBookshelfParts, true },
		{ PropId::ReadingTable, "Reading table", PropData::ReadingTableColliders, PropData::ReadingTableParts },
		{ PropId::RoundTable, "Round table", PropData::RoundTableColliders, PropData::RoundTableParts },
		{ PropId::Chair, "Chair", {}, PropData::ChairParts },
		{ PropId::Bench, "Bench", PropData::BenchColliders, PropData::BenchParts },
		{ PropId::Candelabra, "Candelabra", PropData::CandelabraColliders, PropData::CandelabraParts },
		{ PropId::LampPost, "Lamp post", PropData::LampPostColliders, PropData::LampPostParts, true },
		{ PropId::Fountain, "Fountain", PropData::FountainColliders, PropData::FountainParts },
		{ PropId::Well, "Well", PropData::WellColliders, PropData::WellParts },
		{ PropId::MarketStall, "Market stall", PropData::MarketStallColliders, PropData::MarketStallParts, true },
		{ PropId::OakTree, "Oak tree", PropData::OakTreeColliders, PropData::OakTreeParts, true },
		{ PropId::PineTree, "Pine tree", PropData::PineTreeColliders, PropData::PineTreeParts, true },
		{ PropId::Bush, "Bush", {}, PropData::BushParts },
		{ PropId::Boulder, "Boulder", PropData::BoulderColliders, PropData::BoulderParts },
		{ PropId::Rock, "Rock", PropData::RockColliders, PropData::RockParts },
		{ PropId::FallenLog, "Fallen log", PropData::FallenLogColliders, PropData::FallenLogParts },
		{ PropId::Flowers, "Flowers", {}, PropData::FlowersParts },
		{ PropId::Mushrooms, "Mushrooms", {}, PropData::MushroomsParts },
		{ PropId::Crystal, "Crystal", PropData::CrystalColliders, PropData::CrystalParts },
		{ PropId::Statue, "Statue", PropData::StatueColliders, PropData::StatueParts, true },
		{ PropId::Globe, "Globe", PropData::GlobeColliders, PropData::GlobeParts },
		{ PropId::Desk, "Desk", PropData::DeskColliders, PropData::DeskParts },
		{ PropId::Armchair, "Armchair", PropData::ArmchairColliders, PropData::ArmchairParts },
		{ PropId::Rug, "Rug", {}, PropData::RugParts },
		{ PropId::Banner, "Banner", {}, PropData::BannerParts, true },
		{ PropId::PottedPlant, "Potted plant", PropData::PottedPlantColliders, PropData::PottedPlantParts },
		{ PropId::Door, "Door", PropData::DoorColliders, PropData::DoorParts, true },
		{ PropId::Cart, "Cart", PropData::CartColliders, PropData::CartParts },
		{ PropId::Rubble, "Rubble", {}, PropData::RubbleParts },
		{ PropId::Bones, "Bones", {}, PropData::BonesParts },
		{ PropId::Brazier, "Brazier", PropData::BrazierColliders, PropData::BrazierParts },
		{ PropId::House, "House", PropData::HouseColliders, PropData::HouseParts, true },
		{ PropId::LongHouse, "Long house", PropData::LongHouseColliders, PropData::LongHouseParts, true },
		{ PropId::Signpost, "Signpost", PropData::SignpostColliders, PropData::SignpostParts },
		{ PropId::WallTorch, "Wall torch", {}, PropData::WallTorchParts, true },
		{ PropId::Gravestone, "Gravestone", PropData::GravestoneColliders, PropData::GravestoneParts },
		{ PropId::Window, "Window", {}, PropData::WindowParts, true },
		{ PropId::Lectern, "Lectern", PropData::LecternColliders, PropData::LecternParts },
		{ PropId::Fireplace, "Fireplace", PropData::FireplaceColliders, PropData::FireplaceParts, true },
		{ PropId::StashChest, "Stash chest", PropData::StashChestColliders, PropData::StashChestParts },
		{ PropId::Anvil, "Anvil", PropData::AnvilColliders, PropData::AnvilParts },
		{ PropId::Forge, "Forge", PropData::ForgeColliders, PropData::ForgeParts, true },
		{ PropId::GemAltar, "Gem altar", PropData::GemAltarColliders, PropData::GemAltarParts },
		{ PropId::MissionBoard, "Mission board", PropData::MissionBoardColliders, PropData::MissionBoardParts, true },
		{ PropId::WineBarrel, "Wine barrel", PropData::WineBarrelColliders, PropData::WineBarrelParts },
		{ PropId::OldBarrel, "Old barrel", PropData::OldBarrelColliders, PropData::OldBarrelParts },
		{ PropId::LowCrate, "Low crate", PropData::LowCrateColliders, PropData::LowCrateParts },
		{ PropId::LongCrate, "Long crate", PropData::LongCrateColliders, PropData::LongCrateParts },
		{ PropId::Bucket, "Bucket", PropData::BucketColliders, PropData::BucketParts },
		{ PropId::TreasureChest, "Treasure chest", PropData::TreasureChestColliders, PropData::TreasureChestParts },
		{ PropId::LanternPost, "Lantern post", PropData::LanternPostColliders, PropData::LanternPostParts, true },
		{ PropId::OilLamp, "Oil lamp", {}, PropData::OilLampParts },
		{ PropId::CandleStand, "Candle stand", PropData::CandleStandColliders, PropData::CandleStandParts },
		{ PropId::FirePit, "Fire pit", PropData::FirePitColliders, PropData::FirePitParts },
		{ PropId::BronzeStatue, "Bronze statue", PropData::BronzeStatueColliders, PropData::BronzeStatueParts, true },
		{ PropId::PlankTable, "Plank table", PropData::PlankTableColliders, PropData::PlankTableParts },
		{ PropId::Stool, "Stool", PropData::StoolColliders, PropData::StoolParts },
		{ PropId::MossRock, "Moss rock", PropData::MossRockColliders, PropData::MossRockParts },
		{ PropId::MossBoulder, "Moss boulder", PropData::MossBoulderColliders, PropData::MossBoulderParts },
		{ PropId::TreeStump, "Tree stump", PropData::TreeStumpColliders, PropData::TreeStumpParts },
		{ PropId::DeadLog, "Dead log", PropData::DeadLogColliders, PropData::DeadLogParts },
		{ PropId::Fern, "Fern", {}, PropData::FernParts },
		{ PropId::Weeds, "Weeds", {}, PropData::WeedsParts },
		{ PropId::GrassTuft, "Grass tuft", {}, PropData::GrassTuftParts },
		{ PropId::DryBranches, "Dry branches", {}, PropData::DryBranchesParts },
		{ PropId::IronGate, "Iron gate", PropData::IronGateColliders, PropData::IronGateParts, true },
		{ PropId::CryptDoor, "Crypt door", PropData::CryptDoorColliders, PropData::CryptDoorParts, true },
		{ PropId::Sarcophagus, "Sarcophagus", PropData::SarcophagusColliders, PropData::SarcophagusParts },
		{ PropId::Altar, "Altar", PropData::AltarColliders, PropData::AltarParts },
		{ PropId::CryptPillar, "Crypt pillar", PropData::CryptPillarColliders, PropData::CryptPillarParts, true },
		{ PropId::MarbleBust, "Marble bust", PropData::MarbleBustColliders, PropData::MarbleBustParts },
		{ PropId::FireBowl, "Fire bowl", PropData::FireBowlColliders, PropData::FireBowlParts },
		{ PropId::CalderaGate, "Caldera gate", PropData::CalderaGateColliders, PropData::CalderaGateParts, true },
		{ PropId::ObsidianCluster, "Obsidian cluster", PropData::ObsidianClusterColliders, PropData::ObsidianClusterParts },
		{ PropId::NoticeBoard, "Notice board", PropData::NoticeBoardColliders, PropData::NoticeBoardParts, true },
		{ PropId::OfferingBowl, "Offering bowl", PropData::OfferingBowlColliders, PropData::OfferingBowlParts },
		{ PropId::Telescope, "Telescope", PropData::TelescopeColliders, PropData::TelescopeParts },
		{ PropId::MageTower, "Mage tower", PropData::MageTowerColliders, PropData::MageTowerParts, true },
		{ PropId::PierPost, "Pier post", PropData::PierPostColliders, PropData::PierPostParts },
		{ PropId::ChainAnchor, "Chain anchor", PropData::ChainAnchorColliders, PropData::ChainAnchorParts },
		{ PropId::ChessTable, "Chess table", PropData::ChessTableColliders, PropData::ChessTableParts },
		{ PropId::IcePillar, "Ice pillar", PropData::IcePillarColliders, PropData::IcePillarParts, true },
		{ PropId::SnowRock, "Snowy rock", PropData::SnowRockColliders, PropData::SnowRockParts },
		{ PropId::RuinColumn, "Ruined column", PropData::RuinColumnColliders, PropData::RuinColumnParts, true },
		{ PropId::RuinBrokenColumn, "Broken ruin column", PropData::RuinColumnColliders, PropData::RuinBrokenColumnParts, true },
		{ PropId::RuinStump, "Column stump", PropData::RuinStumpColliders, PropData::RuinStumpParts },
		{ PropId::RuinDrum, "Fallen column drum", PropData::RuinDrumColliders, PropData::RuinDrumParts },
		{ PropId::RuinCapital, "Fallen capital", PropData::RuinCapitalColliders, PropData::RuinCapitalParts },
		{ PropId::RuinArch, "Ruined arch", PropData::RuinArchColliders, PropData::RuinArchParts, true },
		{ PropId::RuinWall, "Ruined wall", PropData::RuinWallColliders, PropData::RuinWallParts, true },
		{ PropId::RuinWallShort, "Ruined wall fragment", PropData::RuinWallShortColliders, PropData::RuinWallShortParts },
		{ PropId::RuinPedestal, "Ruined pedestal", PropData::RuinPedestalColliders, PropData::RuinPedestalParts },
		{ PropId::RuinRubble, "Ruin rubble", {}, PropData::RuinRubbleParts },
		{ PropId::StormSpire, "Storm spire", PropData::StormSpireColliders, PropData::StormSpireParts, true },
		{ PropId::Cairn, "Cairn", PropData::CairnColliders, PropData::CairnParts },
		{ PropId::StandingStone, "Standing stone", PropData::StandingStoneColliders, PropData::StandingStoneParts, true },
		{ PropId::GlyphStone, "Glyph stone", PropData::GlyphStoneColliders, PropData::GlyphStoneParts },
		{ PropId::PrayerPole, "Prayer pole", PropData::PrayerPoleColliders, PropData::PrayerPoleParts, true },
		{ PropId::StoneGate, "Pilgrim gate", PropData::StoneGateColliders, PropData::StoneGateParts, true },
		{ PropId::GraniteBoulder, "Granite boulder", PropData::GraniteBoulderColliders, PropData::GraniteBoulderParts },
	};

	constexpr bool PropsMatchIds()
	{
		for (size_t i = 0; i < std::size(Props); i++)
			if (size_t(Props[i].Id) != i)
				return false;
		return std::size(Props) == size_t(PropId::Count);
	}
	static_assert(PropsMatchIds());

	const PropDef& GetProp(PropId id);
}
