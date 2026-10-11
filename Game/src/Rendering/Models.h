#pragma once
#include <GameShared/Content/Items.h>
#include <GameShared/Core/Math.h>
#include <GameShared/Net/Protocol.h>
#include <glm/glm.hpp>
#include <cstdint>

namespace Game
{
	class WorldRenderer;
	class CharacterLibrary;

	inline constexpr float HalfPi = 1.5707963f;

	inline glm::vec3 ToWorld(Vec2 position, float height = 0.0f)
	{
		return { position.x, height, position.y };
	}

	glm::vec3 Heading(float facing);
	glm::vec3 ElementColor(Element element);
	glm::vec3 SkillColor(AbilityId ability);
	void DrawRing(WorldRenderer& renderer, const glm::vec3& center, float radius, uint32_t segments, const glm::vec3& color, float emission, float thickness);
	void DrawItemDrop(WorldRenderer& renderer, const CharacterLibrary& characters, ItemBase base, Rarity rarity, AbilityId skill, const glm::vec3& position, float time, float phase);
}
