#pragma once
#include <cstdint>

namespace Game
{
	inline constexpr uint32_t SwatchGrid = 8;
	inline constexpr uint32_t SwatchCells = SwatchGrid * SwatchGrid;

	struct SwatchMaterial
	{
		char Key;
		float Roughness;
		float Metallic;
	};

	struct SwatchTable
	{
		const char* Texture;
		const char* Rows[SwatchGrid];
	};

	struct SwatchOverride
	{
		const char* Mesh;
		char Key;
	};

	inline constexpr SwatchMaterial SwatchMaterials[] = {
		{ '.', 0.6f, 0.0f },
		{ 'k', 0.55f, 0.0f },
		{ 'h', 0.5f, 0.0f },
		{ 'c', 0.85f, 0.0f },
		{ 'l', 0.6f, 0.0f },
		{ 'w', 0.7f, 0.0f },
		{ 's', 0.8f, 0.0f },
		{ 'm', 0.45f, 0.6f },
		{ 'i', 0.55f, 0.5f },
		{ 'g', 0.35f, 0.85f },
		{ 'b', 0.55f, 0.0f },
		{ 'p', 0.85f, 0.0f },
		{ 'j', 0.15f, 0.0f },
		{ 'f', 0.65f, 0.0f },
		{ 'x', 0.4f, 0.0f },
		{ 'o', 0.45f, 0.0f },
	};

	inline constexpr SwatchTable SwatchTables[] = {
		{ "knight_texture", {
			"khhmi.lm",
			"khhmi.lm",
			"ccmm.l.i",
			"ccmm.l.i",
			"........",
			"........",
			"........",
			"........" } },
		{ "mage_texture", {
			"khhmml..",
			"khhmml..",
			"cccm.w.c",
			"cccm.w.c",
			"pljl..j.",
			"pljl..jk",
			"........",
			"........" } },
		{ "ranger_texture", {
			"khhc.ccl",
			"khhc.ccl",
			"cl.m.wwl",
			"cl.m.wwl",
			".cbl...l",
			".cbl...l",
			"........",
			"........" } },
		{ "rogue_texture", {
			"khhm.ll.",
			"khhm.ll.",
			"ccmm.wwl",
			"ccmm.wwl",
			"wc.l...l",
			"wc.l...l",
			"........",
			"........" } },
		{ "barbarian_texture", {
			"khhm..lc",
			"khhm..lc",
			"c.cm.llc",
			"c.cm.llc",
			"..ml...c",
			"..ml...c",
			".c......",
			".c......" } },
		{ "skeleton_texture", {
			"..liiclc",
			"..liiclc",
			".b.m.wwj",
			".b.m.ww.",
			"cccw.cw.",
			"cccw.cw.",
			"........",
			"........" } },
		{ "weapons_bits_texture", {
			"........",
			"........",
			"......m.",
			"......m.",
			"....l..w",
			"....l..w",
			"c.......",
			"c......." } },
		{ "forest_texture", {
			"f.......",
			"f.......",
			"w.......",
			"w.......",
			"s.......",
			"s.......",
			"........",
			"........" } },
		{ "dungeon_texture", {
			"sswiwsws",
			"sswiwsws",
			"cw......",
			"cw......",
			"fcocc.cg",
			"fcocc.cg",
			"x.......",
			"x......." } },
	};

	inline constexpr SwatchOverride SwatchOverrides[] = {
		{ "candle_lit", 'x' },
		{ "candle_thin_lit", 'x' },
		{ "candle_triple", 'x' },
		{ "torch_lit", 'w' },
	};
}
