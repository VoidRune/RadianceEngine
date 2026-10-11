#pragma once
#include "GameShared/World/Shapes.h"
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class TerrainId : uint8_t
	{
		Rock,
		Void,
		Water,
		CaveFloor,
		Flagstone,
		Cobblestone,
		Marble,
		WoodFloor,
		Carpet,
		Grass,
		Dirt,
		Sand,
		Ash,
		Count,
	};

	enum class TerrainStyle : uint8_t
	{
		Solid,
		Void,
		Liquid,
		Ground,
		Slabs,
		Planks,
	};

	struct TerrainDef
	{
		TerrainId Id = TerrainId::Rock;
		const char* Name = "";
		bool Walkable = false;
		bool BlocksProjectiles = false;
		TerrainStyle Style = TerrainStyle::Ground;
		uint32_t Color = 0;
		uint32_t Accent = 0;
	};

	inline constexpr TerrainDef Terrains[] = {
		{ TerrainId::Rock, "Rock", false, true, TerrainStyle::Solid, 0x38332D, 0x62594E },
		{ TerrainId::Void, "Void", false, false, TerrainStyle::Void, 0x07080B, 0x07080B },
		{ TerrainId::Water, "Water", false, false, TerrainStyle::Liquid, 0x1F4A73, 0x2E6B9A },
		{ TerrainId::CaveFloor, "Cave floor", true, false, TerrainStyle::Ground, 0xA9A292, 0x989183 },
		{ TerrainId::Flagstone, "Flagstone", true, false, TerrainStyle::Slabs, 0xA39C8E, 0x6F695F },
		{ TerrainId::Cobblestone, "Cobblestone", true, false, TerrainStyle::Slabs, 0x7D7A72, 0x55524C },
		{ TerrainId::Marble, "Marble", true, false, TerrainStyle::Slabs, 0xDAD5CA, 0xA8A193 },
		{ TerrainId::WoodFloor, "Wood floor", true, false, TerrainStyle::Planks, 0x8A5D3A, 0x4A3020 },
		{ TerrainId::Carpet, "Carpet", true, false, TerrainStyle::Ground, 0x7A2430, 0x8A2E38 },
		{ TerrainId::Grass, "Grass", true, false, TerrainStyle::Ground, 0x4E7A36, 0x5E8C3E },
		{ TerrainId::Dirt, "Dirt", true, false, TerrainStyle::Ground, 0x7B6145, 0x6E563C },
		{ TerrainId::Sand, "Sand", true, false, TerrainStyle::Ground, 0xC2AE84, 0xB39F76 },
		{ TerrainId::Ash, "Ash", true, false, TerrainStyle::Ground, 0x4D4844, 0x5A5450 },
	};

	constexpr bool TerrainsMatchIds()
	{
		for (size_t i = 0; i < std::size(Terrains); i++)
			if (size_t(Terrains[i].Id) != i)
				return false;
		return std::size(Terrains) == size_t(TerrainId::Count);
	}
	static_assert(TerrainsMatchIds());

	inline const TerrainDef& GetTerrain(TerrainId id)
	{
		return Terrains[size_t(id) < std::size(Terrains) ? size_t(id) : 0];
	}

	enum class WallId : uint8_t
	{
		Stone,
		Library,
		Balustrade,
		WoodRailing,
		WoodFence,
		IronFence,
		Hedge,
		AbbeyWall,
		CryptWall,
		Count,
	};

	struct WallDef
	{
		WallId Id = WallId::Stone;
		const char* Name = "";
		float Thickness = 0.3f;
		float Height = 2.0f;
		Block Blocks = Block::All;
		uint32_t Color = 0;
		uint32_t Top = 0;
	};

	inline constexpr WallDef Walls[] = {
		{ WallId::Stone, "Stone wall", 0.4f, 2.2f, Block::All, 0x6E665C, 0x857C70 },
		{ WallId::Library, "Library wall", 0.4f, 2.8f, Block::All, 0xB59C7A, 0xCDB591 },
		{ WallId::Balustrade, "Balustrade", 0.3f, 0.75f, Block::Movement, 0xD0C8B8, 0xE4DDCF },
		{ WallId::WoodRailing, "Wooden railing", 0.12f, 0.9f, Block::Movement, 0x5C3A22, 0x6E4A2E },
		{ WallId::WoodFence, "Wooden fence", 0.12f, 0.85f, Block::Movement, 0x8A6A44, 0x9C7B52 },
		{ WallId::IronFence, "Iron fence", 0.08f, 1.2f, Block::Movement, 0x2A2A2E, 0x3A3A40 },
		{ WallId::Hedge, "Hedge", 0.7f, 1.3f, Block::All, 0x2F5A26, 0x3E7032 },
		{ WallId::AbbeyWall, "Abbey wall", 0.6f, 2.4f, Block::All, 0x6A645B, 0x7E776C },
		{ WallId::CryptWall, "Crypt wall", 0.8f, 3.0f, Block::All, 0x5E5850, 0x726B61 },
	};

	constexpr bool WallsMatchIds()
	{
		for (size_t i = 0; i < std::size(Walls); i++)
			if (size_t(Walls[i].Id) != i)
				return false;
		return std::size(Walls) == size_t(WallId::Count);
	}
	static_assert(WallsMatchIds());

	inline const WallDef& GetWall(WallId id)
	{
		return Walls[size_t(id) < std::size(Walls) ? size_t(id) : 0];
	}
}
