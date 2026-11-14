#include "Traits.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	float DamageTakenScale(const PlayerTraits& traits, Element element, float maxArmor)
	{
		if (element == Element::Physical)
			return 1.0f - std::min(traits.Armor / (traits.Armor + ArmorConstant), maxArmor);
		return 1.0f - std::clamp(traits.Resist[size_t(element)], -0.5f, MaxResist);
	}

	ProjectilePattern GetProjectilePattern(const AbilityDef& ability, uint8_t extraProjectiles, uint8_t extraPierce, float speedScale)
	{
		ProjectilePattern pattern;
		pattern.Count = uint32_t(ability.ProjectileCount) + extraProjectiles;
		pattern.Spread = std::max(ability.ProjectileSpread, 0.13f * float(pattern.Count - 1));
		pattern.Speed = ability.ProjectileSpeed * speedScale;
		pattern.Pierce = uint8_t(std::min(ability.Pierce + extraPierce, 8));
		return pattern;
	}

	float ProjectileOffset(const ProjectilePattern& pattern, uint32_t index)
	{
		if (pattern.Count <= 1)
			return 0.0f;
		const float t = float(index) / float(pattern.Count - 1) - 0.5f;
		if (pattern.Flare == 1.0f)
			return pattern.Spread * t;
		return 0.5f * pattern.Spread * std::copysign(std::pow(2.0f * std::abs(t), pattern.Flare), t);
	}

	Vec2 ProjectileDirection(const ProjectilePattern& pattern, Vec2 aim, uint32_t index)
	{
		if (pattern.Count <= 1)
			return aim;
		return FromAngle(AngleOf(aim) + ProjectileOffset(pattern, index));
	}
}
