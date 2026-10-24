#pragma once
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class Outfit : uint8_t
	{
		Wanderer,
		Count,
	};

	enum class Headwear : uint8_t
	{
		Hood,
		PointedHat,
		Bare,
		Count,
	};

	struct Cosmetics
	{
		Outfit Body = Outfit::Wanderer;
		Headwear Head = Headwear::Hood;

		bool operator==(const Cosmetics&) const = default;
	};

	struct OutfitDef
	{
		Outfit Id = Outfit::Wanderer;
		const char* Key = "";
		const char* Name = "";
	};

	struct HeadwearDef
	{
		Headwear Id = Headwear::Hood;
		const char* Key = "";
		const char* Name = "";
	};

	inline constexpr OutfitDef Outfits[] = {
		{ Outfit::Wanderer, "wanderer", "Wanderer's Garb" },
	};
	static_assert(std::size(Outfits) == size_t(Outfit::Count));

	inline constexpr HeadwearDef Headwears[] = {
		{ Headwear::Hood, "hood", "Deep Hood" },
		{ Headwear::PointedHat, "hat", "Pointed Hat" },
		{ Headwear::Bare, "bare", "Bare Head" },
	};
	static_assert(std::size(Headwears) == size_t(Headwear::Count));

	inline const OutfitDef& GetOutfit(Outfit id)
	{
		return Outfits[size_t(id) < std::size(Outfits) ? size_t(id) : 0];
	}

	inline const HeadwearDef& GetHeadwear(Headwear id)
	{
		return Headwears[size_t(id) < std::size(Headwears) ? size_t(id) : 0];
	}

	inline constexpr Cosmetics SanitizeCosmetics(Cosmetics look)
	{
		if (look.Body >= Outfit::Count)
			look.Body = Outfit::Wanderer;
		if (look.Head >= Headwear::Count)
			look.Head = Headwear::Hood;
		return look;
	}

	inline constexpr uint16_t PackCosmetics(Cosmetics look)
	{
		return uint16_t(uint32_t(look.Body) | uint32_t(look.Head) << 8);
	}

	inline constexpr Cosmetics UnpackCosmetics(uint32_t packed)
	{
		return SanitizeCosmetics({ Outfit(packed & 0xFFu), Headwear((packed >> 8) & 0xFFu) });
	}
}
