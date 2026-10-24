#pragma once
#include <cstdint>
#include <initializer_list>
#include <iterator>

namespace Game
{
	enum class Honor : uint8_t
	{
		Skydiver,
		Stargazer,
		Faithful,
		Count,
	};

	struct HonorDef
	{
		Honor Id = Honor::Skydiver;
		const char* Key = "";
		const char* Title = "";
		const char* Feat = "";
	};

	inline constexpr HonorDef Honors[] = {
		{ Honor::Skydiver, "skydiver", "Skydiver", "Fell off Emberhold 25 times" },
		{ Honor::Stargazer, "stargazer", "Stargazer", "Lit every constellation above the observatory" },
		{ Honor::Faithful, "faithful", "Faithful", "Walked where the eye sees nothing" },
	};
	static_assert(std::size(Honors) == size_t(Honor::Count));

	inline const HonorDef& GetHonor(Honor id)
	{
		return Honors[size_t(id) < std::size(Honors) ? size_t(id) : 0];
	}

	inline constexpr uint8_t HonorBit(Honor id) { return uint8_t(1u << uint32_t(id)); }
	inline constexpr uint8_t AllHonors = uint8_t((1u << uint32_t(Honor::Count)) - 1u);

	inline const HonorDef* ShownHonor(uint8_t honors)
	{
		for (const Honor id : { Honor::Stargazer, Honor::Skydiver, Honor::Faithful })
			if ((honors & HonorBit(id)) != 0)
				return &GetHonor(id);
		return nullptr;
	}

	inline constexpr uint32_t SkydiverFalls = 25;
	inline constexpr float WishCooldownSeconds = 1.5f;
	inline constexpr uint32_t WishCost = 1;
}
