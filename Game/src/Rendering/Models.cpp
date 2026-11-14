#include "Models.h"
#include "Rendering/Characters.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include "UI/Palette.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <iterator>

namespace Game
{
	namespace
	{
		constexpr glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
		constexpr glm::vec3 Arcane{ 0.72f, 0.55f, 1.0f };
		constexpr glm::vec3 Gold{ 0.95f, 0.75f, 0.3f };
	}

	glm::vec3 Heading(float facing)
	{
		return { std::cos(facing), 0.0f, std::sin(facing) };
	}

	glm::vec3 ElementColor(Element element)
	{
		switch (element)
		{
		case Element::Fire:
			return { 1.0f, 0.55f, 0.2f };
		case Element::Cold:
			return { 0.5f, 0.85f, 1.0f };
		case Element::Lightning:
			return { 1.0f, 0.94f, 0.58f };
		case Element::Poison:
			return { 0.5f, 0.95f, 0.3f };
		default:
			return { 0.95f, 0.95f, 0.95f };
		}
	}

	glm::vec3 SkillColor(AbilityId ability)
	{
		ability = CreditOf(ability);
		switch (ability)
		{
		case AbilityId::ChainHook:
			return { 1.0f, 0.68f, 0.34f };
		case AbilityId::ObsidianBlades:
			return { 0.7f, 0.45f, 0.92f };
		case AbilityId::Firebomb:
			return { 1.0f, 0.44f, 0.16f };
		case AbilityId::Gust:
			return { 0.62f, 1.0f, 0.74f };
		case AbilityId::GravityWell:
			return { 0.45f, 0.42f, 1.0f };
		case AbilityId::DiamondWard:
			return { 1.0f, 0.86f, 0.48f };
		case AbilityId::EarthSpike:
			return { 0.93f, 0.6f, 0.3f };
		case AbilityId::PurpleFireball:
			return { 0.74f, 0.3f, 1.0f };
		case AbilityId::HexShield:
			return { 0.36f, 0.82f, 1.0f };
		case AbilityId::Resonance:
			return { 0.95f, 0.8f, 0.5f };
		default:
			break;
		}
		const AbilityDef& def = GetAbility(ability);
		if (def.Damages != Element::Physical)
			return ElementColor(def.Damages);
		return def.Kind == AbilityKind::Heal ? glm::vec3(0.4f, 1.0f, 0.55f) : Arcane;
	}

	void DrawRing(WorldRenderer& renderer, const glm::vec3& center, float radius, uint32_t segments, const glm::vec3& color, float emission, float thickness)
	{
		const float halfLength = Pi * radius / float(segments);
		for (uint32_t i = 0; i < segments; i++)
		{
			const float angle = 2.0f * Pi * float(i) / float(segments);
			renderer.DrawBox(center + Heading(angle) * radius, { thickness, 0.03f, halfLength }, color, angle, emission);
		}
	}

}
