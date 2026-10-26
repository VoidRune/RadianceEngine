#pragma once
#include <GameShared/Content/Items.h>
#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace Game
{
	inline constexpr glm::vec3 PlayerColors[] = {
		{ 0.239f, 0.545f, 1.000f },
		{ 1.000f, 0.239f, 0.431f },
		{ 0.545f, 0.910f, 0.290f },
		{ 1.000f, 0.788f, 0.235f },
		{ 0.655f, 0.455f, 1.000f },
		{ 0.133f, 0.827f, 0.773f },
		{ 0.941f, 0.310f, 0.847f },
		{ 0.910f, 0.925f, 0.961f },
		{ 0.510f, 0.847f, 1.000f },
		{ 0.384f, 0.949f, 0.682f },
		{ 1.000f, 0.620f, 0.769f },
		{ 0.804f, 0.690f, 1.000f },
	};
	static_assert(std::size(PlayerColors) >= MaxPlayers);
	inline constexpr uint32_t RingPatterns = 6;

	inline constexpr glm::vec3 TeamColors[] = {
		{ 0.25f, 0.55f, 1.00f },
		{ 1.00f, 0.32f, 0.25f },
		{ 0.40f, 0.88f, 0.36f },
		{ 1.00f, 0.76f, 0.22f },
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

	inline glm::vec3 PlayerTone(uint32_t playerId)
	{
		return glm::pow(PlayerColor(playerId), glm::vec3(2.2f));
	}

	inline uint32_t PlayerPattern(uint32_t playerId)
	{
		return playerId < std::size(PlayerColors) ? playerId % RingPatterns : 0u;
	}

	inline glm::vec3 TeamColor(uint8_t team)
	{
		return team < std::size(TeamColors) ? TeamColors[team] : glm::vec3(0.8f);
	}

	inline glm::vec3 RarityColor(Rarity rarity)
	{
		return RarityColors[size_t(rarity) < std::size(RarityColors) ? size_t(rarity) : 0];
	}

	inline constexpr glm::vec3 ApexHues[] = {
		{ 1.000f, 0.420f, 0.660f },
		{ 1.000f, 0.600f, 0.450f },
		{ 1.000f, 0.900f, 0.940f },
		{ 1.000f, 0.500f, 0.860f },
	};

	inline glm::vec3 ApexColor(float time)
	{
		const float cycle = time * 0.3f - std::floor(time * 0.3f);
		const float scaled = cycle * float(std::size(ApexHues));
		const size_t index = size_t(scaled) % std::size(ApexHues);
		const float blend = scaled - std::floor(scaled);
		return glm::mix(ApexHues[index], ApexHues[(index + 1) % std::size(ApexHues)], blend * blend * (3.0f - 2.0f * blend));
	}
}
