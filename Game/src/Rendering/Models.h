#pragma once
#include <GameShared/Content/Items.h>
#include <GameShared/Core/Math.h>
#include <GameShared/Net/Protocol.h>
#include <glm/glm.hpp>
#include <cstdint>

namespace Game
{
	class WorldRenderer;

	inline constexpr float HalfPi = 1.5707963f;

	inline glm::vec3 ToWorld(Vec2 position, float height = 0.0f)
	{
		return { position.x, height, position.y };
	}

	glm::vec3 Heading(float facing);
	glm::vec3 ElementColor(Element element);
	glm::vec3 SkillColor(AbilityId ability);
	void DrawRing(WorldRenderer& renderer, const glm::vec3& center, float radius, uint32_t segments, const glm::vec3& color, float emission, float thickness);
	void DrawArc(WorldRenderer& renderer, const glm::vec3& center, float facing, float halfArc, float radius, uint32_t segments, const glm::vec3& color, float emission, float thickness = 0.05f);
	void DrawWeapon(WorldRenderer& renderer, ItemBase type, Rarity rarity, const glm::vec3& grip, float facing, float time);
	void DrawCharacter(WorldRenderer& renderer, const PlayerLook& look, const glm::vec3& base, float facing, const glm::vec3& teamColor, float flash, float emission, float time);
	void DrawItemDrop(WorldRenderer& renderer, ItemBase base, Rarity rarity, AbilityId skill, const glm::vec3& position, float time, float phase);
}
