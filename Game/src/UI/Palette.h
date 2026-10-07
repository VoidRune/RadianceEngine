#pragma once
#include <GameShared/Content/Items.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <iterator>

namespace Game
{
	inline constexpr glm::vec3 PlayerColors[] = {
		{ 0.20f, 0.45f, 0.95f },
		{ 0.95f, 0.30f, 0.25f },
		{ 0.25f, 0.80f, 0.35f },
		{ 0.95f, 0.80f, 0.20f },
		{ 0.70f, 0.35f, 0.95f },
		{ 0.20f, 0.85f, 0.85f },
		{ 1.00f, 0.55f, 0.15f },
		{ 0.95f, 0.45f, 0.75f },
		{ 0.55f, 0.85f, 0.25f },
		{ 0.85f, 0.85f, 0.85f },
		{ 0.45f, 0.30f, 0.20f },
		{ 0.30f, 0.30f, 0.60f },
	};
	static_assert(std::size(PlayerColors) >= MaxPlayers);

	inline constexpr glm::vec3 TeamColors[] = {
		{ 0.25f, 0.55f, 1.00f },
		{ 1.00f, 0.32f, 0.25f },
	};

	inline constexpr glm::vec3 RarityColors[] = {
		{ 0.85f, 0.85f, 0.85f },
		{ 0.35f, 0.90f, 0.35f },
		{ 0.35f, 0.60f, 1.00f },
		{ 1.00f, 0.60f, 0.15f },
		{ 1.00f, 0.30f, 0.55f },
	};
	static_assert(std::size(RarityColors) == size_t(Rarity::Count));

	inline constexpr glm::vec3 LootColors[] = {
		{ 0.95f, 0.25f, 0.30f },
		{ 1.00f, 0.55f, 0.15f },
		{ 1.00f, 0.90f, 0.25f },
		{ 0.30f, 0.90f, 1.00f },
		{ 0.35f, 0.95f, 0.40f },
		{ 0.25f, 0.45f, 1.00f },
	};

	inline glm::vec3 PlayerColor(uint32_t playerId)
	{
		return playerId < std::size(PlayerColors) ? PlayerColors[playerId] : glm::vec3(0.8f);
	}

	inline glm::vec3 TeamColor(uint8_t team)
	{
		return team < std::size(TeamColors) ? TeamColors[team] : glm::vec3(0.8f);
	}

	inline glm::vec3 RarityColor(Rarity rarity)
	{
		return RarityColors[size_t(rarity) < std::size(RarityColors) ? size_t(rarity) : 0];
	}
}
