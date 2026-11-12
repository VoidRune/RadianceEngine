#pragma once
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class Outfit : uint8_t
	{
		Wanderer,
		Frostwarden,
		Voidcaller,
		Count,
	};

	enum class Focus : uint8_t
	{
		Wand,
		Staff,
		Hands,
		Count,
	};

	enum class Aura : uint8_t
	{
		None,
		Ember,
		Astral,
		Frost,
		Count,
	};

	enum class Headpiece : uint8_t
	{
		None,
		ShatteredCrown,
		FrostCrown,
		Count,
	};

	struct Cosmetics
	{
		Outfit Body = Outfit::Wanderer;
		Focus Cast = Focus::Wand;
		Aura Glow = Aura::None;
		Headpiece Crown = Headpiece::None;

		bool operator==(const Cosmetics&) const = default;
	};

	template <typename T>
	struct CosmeticDef
	{
		T Id{};
		const char* Key = "";
		const char* Name = "";
	};

	using OutfitDef = CosmeticDef<Outfit>;
	using FocusDef = CosmeticDef<Focus>;
	using AuraDef = CosmeticDef<Aura>;
	using HeadpieceDef = CosmeticDef<Headpiece>;

	inline constexpr OutfitDef Outfits[] = {
		{ Outfit::Wanderer, "wanderer", "Wanderer's Garb" },
		{ Outfit::Frostwarden, "frostwarden", "Frostwarden" },
		{ Outfit::Voidcaller, "voidcaller", "Voidcaller" },
	};
	static_assert(std::size(Outfits) == size_t(Outfit::Count));

	inline constexpr FocusDef Foci[] = {
		{ Focus::Wand, "wand", "Wand" },
		{ Focus::Staff, "staff", "Staff" },
		{ Focus::Hands, "hands", "Bare Hands" },
	};
	static_assert(std::size(Foci) == size_t(Focus::Count));

	inline constexpr AuraDef Auras[] = {
		{ Aura::None, "none", "No Aura" },
		{ Aura::Ember, "ember", "Emberheart" },
		{ Aura::Astral, "astral", "Astral Orbit" },
		{ Aura::Frost, "frost", "Hoarfrost" },
	};
	static_assert(std::size(Auras) == size_t(Aura::Count));

	inline constexpr HeadpieceDef Headpieces[] = {
		{ Headpiece::None, "none", "Bare Brow" },
		{ Headpiece::ShatteredCrown, "shattered_crown", "Shattered Crown" },
		{ Headpiece::FrostCrown, "frost_crown", "Frost Crown" },
	};
	static_assert(std::size(Headpieces) == size_t(Headpiece::Count));

	template <typename T, size_t N>
	const CosmeticDef<T>& FindCosmetic(const CosmeticDef<T> (&defs)[N], T id)
	{
		return defs[size_t(id) < N ? size_t(id) : 0];
	}

	inline const OutfitDef& GetOutfit(Outfit id) { return FindCosmetic(Outfits, id); }
	inline const FocusDef& GetFocus(Focus id) { return FindCosmetic(Foci, id); }
	inline const AuraDef& GetAura(Aura id) { return FindCosmetic(Auras, id); }
	inline const HeadpieceDef& GetHeadpiece(Headpiece id) { return FindCosmetic(Headpieces, id); }

	inline constexpr Cosmetics SanitizeCosmetics(Cosmetics look)
	{
		if (look.Body >= Outfit::Count)
			look.Body = Outfit::Wanderer;
		if (look.Cast >= Focus::Count)
			look.Cast = Focus::Wand;
		if (look.Glow >= Aura::Count)
			look.Glow = Aura::None;
		if (look.Crown >= Headpiece::Count)
			look.Crown = Headpiece::None;
		return look;
	}

	inline constexpr uint32_t PackCosmetics(Cosmetics look)
	{
		return uint32_t(look.Body) | uint32_t(look.Cast) << 8 | uint32_t(look.Glow) << 16 | uint32_t(look.Crown) << 24;
	}

	inline constexpr Cosmetics UnpackCosmetics(uint32_t packed)
	{
		return SanitizeCosmetics({ Outfit(packed & 0xFFu), Focus((packed >> 8) & 0xFFu), Aura((packed >> 16) & 0xFFu), Headpiece((packed >> 24) & 0xFFu) });
	}
}
