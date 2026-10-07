#pragma once
#include "UI/Ui.h"
#include <GameShared/Content/Items.h>
#include <GameShared/Content/Traits.h>
#include <cstdint>

namespace Game
{
	struct AttackSummary
	{
		AbilityId Ability = AbilityId::Bolt;
		float DamagePerHit = 0.0f;
		float AttacksPerSecond = 0.0f;
		uint32_t Projectiles = 1;
		float DamagePerSecond = 0.0f;
	};

	struct SlotLook
	{
		bool Hovered = false;
		bool Dim = false;
		bool Better = false;
		bool Highlight = false;
		bool Target = false;
		std::optional<UiIcon> EmptyIcon;
		std::string_view EmptyLabel;
		std::string_view KeyLabel;
	};

	AttackSummary SummarizeAttack(const PlayerTraits& traits);
	float RateItem(const PlayerTraits& traits);
	UiColor ItemColor(const Item& item);
	UiColor ItemColor(ItemBase base, Rarity rarity, AbilityId skill);
	void DrawItemIcon(UiDrawList& draw, const Item& item, const UiRect& rect, float alpha = 1.0f);
	void DrawItemSlot(Ui& ui, const UiRect& rect, const Item& item, const SlotLook& look);
	void DescribeItem(UiTooltip& tooltip, const Item& item, uint8_t weaponAffinity);
	void DescribeSkill(UiTooltip& tooltip, const SkillTraits& skill, const PlayerTraits& traits);
	void CompareTraits(UiTooltip& tooltip, const PlayerTraits& current, const PlayerTraits& candidate);
	float DrawTraitsTable(Ui& ui, const UiRect& area, const PlayerTraits& traits);
	UiColor TierColor(uint8_t tier);
	UiColor MaterialColor(Material material);
	std::string FormatAmount(uint32_t amount);
	float DrawMaterial(UiDrawList& draw, glm::vec2 position, Material material, std::string_view text, float size, UiColor textColor);
	float DrawCost(UiDrawList& draw, const UiRect& area, const MaterialStock& cost, const MaterialStock& stock, bool unlimited, float alignX = 0.0f);
	void DescribeMaterial(UiTooltip& tooltip, Material material, uint32_t amount, bool unlimited);
}
