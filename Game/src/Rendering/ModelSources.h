#pragma once
#include <cstdint>
#include <span>

namespace Game
{
	inline constexpr const char* ModelLibraryPath = "res/Models/Models.bin";
	inline constexpr uint32_t ModelTextureMaxSize = 256;
	inline constexpr float ModelClipFrameRate = 30.0f;

	enum class ModelSourceKind : uint8_t
	{
		Character,
		Animations,
		Prop,
		PropFolder,
		Scan,
		Material,
		Environment,
	};

	struct ModelSource
	{
		const char* Path;
		ModelSourceKind Kind;
		uint32_t TextureSize = 0;
		float Size = 0.0f;
		bool SplitNodes = false;
		bool KeepOrigin = false;
	};

	struct ModelPack
	{
		const char* Path;
		std::span<const ModelSource> Sources;
		bool Required = true;
	};

	inline constexpr ModelSource CharacterSources[] = {
		{ "KayKit/Animations/Rig_Medium_General.glb", ModelSourceKind::Animations },
		{ "KayKit/Animations/Rig_Medium_MovementBasic.glb", ModelSourceKind::Animations },
		{ "KayKit/Animations/Rig_Medium_MovementAdvanced.glb", ModelSourceKind::Animations },
		{ "KayKit/Animations/Rig_Medium_CombatMelee.glb", ModelSourceKind::Animations },
		{ "KayKit/Animations/Rig_Medium_CombatRanged.glb", ModelSourceKind::Animations },
		{ "KayKit/Animations/Rig_Medium_Special.glb", ModelSourceKind::Animations },
		{ "KayKit/Adventurers/Knight.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/Mage.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/Ranger.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/Rogue.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/Rogue_Hooded.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/Barbarian.glb", ModelSourceKind::Character },
		{ "KayKit/Skeletons/Skeleton_Minion.glb", ModelSourceKind::Character },
		{ "KayKit/Skeletons/Skeleton_Rogue.glb", ModelSourceKind::Character },
		{ "KayKit/Skeletons/Skeleton_Warrior.glb", ModelSourceKind::Character },
		{ "KayKit/Skeletons/Skeleton_Mage.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/wand.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/staff.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/sword_1handed.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/axe_1handed.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/dagger.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/bow_withString.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/crossbow_2handed.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/spellbook_open.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/arrow_bow.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/arrow_crossbow.gltf", ModelSourceKind::Prop },
		{ "KayKit/FantasyWeapons/spear_A.gltf", ModelSourceKind::Prop },
		{ "KayKit/Skeletons/Skeleton_Axe.gltf", ModelSourceKind::Prop },
		{ "KayKit/Skeletons/Skeleton_Blade.gltf", ModelSourceKind::Prop },
		{ "KayKit/Skeletons/Skeleton_Crossbow.gltf", ModelSourceKind::Prop },
		{ "KayKit/Skeletons/Skeleton_Staff.gltf", ModelSourceKind::Prop },
		{ "KayKit/Forest", ModelSourceKind::PropFolder },
		{ "KayKit/Dungeon", ModelSourceKind::PropFolder },
		{ "Reference/LightingTest.gltf", ModelSourceKind::Prop },
	};

	inline constexpr ModelSource AbbeySources[] = {
		{ .Path = "PolyHaven/Models/large_iron_gate", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "PolyHaven/Models/large_castle_door", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "PolyHaven/Models/stone_fire_pit", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/gothic_statue", .Kind = ModelSourceKind::Scan, .TextureSize = 1024 },
		{ .Path = "PolyHaven/Models/wine_barrel_01", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/barrel_03", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/wooden_crate_01", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/wooden_crate_02", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/wooden_bucket_02", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/treasure_chest", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/wooden_lantern_01", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/vintage_oil_lamp", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/brass_candleholders", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "PolyHaven/Models/brass_goblets", .Kind = ModelSourceKind::Scan, .TextureSize = 256 },
		{ .Path = "PolyHaven/Models/brass_vase_01", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/brass_pot_01", .Kind = ModelSourceKind::Scan, .TextureSize = 256 },
		{ .Path = "PolyHaven/Models/metal_jug", .Kind = ModelSourceKind::Scan, .TextureSize = 256 },
		{ .Path = "PolyHaven/Models/antique_ceramic_vase_01", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/marble_bust_01", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/kite_shield", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/ornate_war_hammer", .Kind = ModelSourceKind::Scan, .TextureSize = 256 },
		{ .Path = "PolyHaven/Models/antique_estoc", .Kind = ModelSourceKind::Scan, .TextureSize = 256 },
		{ .Path = "PolyHaven/Models/WoodenTable_01", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/folding_wooden_stool", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/rock_moss_set_02", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "PolyHaven/Models/tree_stump_01", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/dead_tree_trunk", .Kind = ModelSourceKind::Scan, .TextureSize = 512 },
		{ .Path = "PolyHaven/Models/fern_02", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "PolyHaven/Models/dry_branches_medium_01", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "PolyHaven/Models/weed_plant_02", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "PolyHaven/Models/grass_medium_01", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "PolyHaven/Textures/cobblestone_floor_08", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.8f },
		{ .Path = "PolyHaven/Textures/mud_forest", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.35f },
		{ .Path = "PolyHaven/Textures/forest_leaves_02", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 3.0f },
		{ .Path = "PolyHaven/Textures/large_floor_tiles_02", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 3.0f },
		{ .Path = "PolyHaven/Textures/castle_wall_varriation", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "PolyHaven/Textures/old_stone_wall", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "PolyHaven/Textures/weathered_planks", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "PolyHaven/HDRIs/kiara_8_sunset_2k.hdr", .Kind = ModelSourceKind::Environment, .TextureSize = 512 },
	};

	inline constexpr ModelSource OriginalSources[] = {
		{ .Path = "Original/ObsidianCluster", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Fountain", .Kind = ModelSourceKind::Scan, .TextureSize = 1024 },
		{ .Path = "Original/Critters", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/HubProps", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/Stations", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/Arcana", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/Nature", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Emberhold", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Market", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/Chess", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/Hero", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Armory", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true, .KeepOrigin = true },
		{ .Path = "Original/Glacier", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Ruins", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Summit", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Materials/PavingStone", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 1.0f },
		{ .Path = "Original/Materials/PavingBed", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 1.0f },
		{ .Path = "Original/Materials/GroundGrass", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundDirt", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundRock", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 3.0f },
		{ .Path = "Original/Materials/GroundSand", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundSnow", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundWater", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundCliff", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 3.0f },
		{ .Path = "Original/Materials/GroundScree", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundAlpine", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
	};

	inline constexpr ModelPack ModelPacks[] = {
		{ ModelLibraryPath, CharacterSources },
		{ "res/Models/Original.bin", OriginalSources },
		{ "res/Models/Abbey.bin", AbbeySources, false },
	};
}
