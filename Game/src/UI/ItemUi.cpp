#include "ItemUi.h"
#include "Rendering/Models.h"
#include "UI/Palette.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace Game
{
	namespace
	{
		const UiColor AffixColor = Hex(0x8FB8FF);
		const UiColor ImplicitColor = Hex(0xE4E0F0);
		const UiColor BetterColor = Hex(0x7FD884);
		const UiColor WorseColor = Hex(0xF06A5E);
		const UiColor CorruptColor = Hex(0xE45AA9);

		struct TraitLine
		{
			const char* Label = "";
			const char* Format = "";
			float Value = 0.0f;
		};

		float ArmorReduction(float armor)
		{
			return std::min(armor / (armor + ArmorConstant), MaxArmorReduction);
		}

		std::array<TraitLine, 17> DescribeTraits(const PlayerTraits& traits)
		{
			const AttackSummary attack = SummarizeAttack(traits);
			return { {
				{ "Attack damage per second", "%.0f", attack.DamagePerSecond },
				{ "Damage per hit", "%.0f", attack.DamagePerHit },
				{ "Attacks per second", "%.2f", attack.AttacksPerSecond },
				{ "Max health", "%.0f", float(traits.MaxHealth) },
				{ "Max mana", "%.0f", traits.MaxMana },
				{ "Mana per second", "%.1f", traits.ManaRegen },
				{ "Armor", "%.0f", traits.Armor },
				{ "Physical damage blocked", "%.0f%%", ArmorReduction(traits.Armor) * 100.0f },
				{ "Fire resistance", "%.0f%%", traits.Resist[size_t(Element::Fire)] * 100.0f },
				{ "Cold resistance", "%.0f%%", traits.Resist[size_t(Element::Cold)] * 100.0f },
				{ "Lightning resistance", "%.0f%%", traits.Resist[size_t(Element::Lightning)] * 100.0f },
				{ "Poison resistance", "%.0f%%", traits.Resist[size_t(Element::Poison)] * 100.0f },
				{ "Move speed", "%.2f", traits.MoveSpeed },
				{ "Critical chance", "%.0f%%", traits.CritChance * 100.0f },
				{ "Critical damage", "%.0f%%", traits.CritMultiplier * 100.0f },
				{ "Life steal", "%.1f%%", traits.LifeSteal * 100.0f },
				{ "Health per second", "%.1f", traits.Regeneration },
			} };
		}

		std::string Format(const TraitLine& line)
		{
			char text[32];
			std::snprintf(text, sizeof(text), line.Format, double(line.Value));
			return text;
		}

		template<typename... Arguments>
		std::string Format(const char* format, Arguments... arguments)
		{
			char text[160];
			std::snprintf(text, sizeof(text), format, arguments...);
			return text;
		}

		const char* DescribeMovement(float moveScale)
		{
			if (moveScale <= 0.01f)
				return "roots you while casting";
			if (moveScale < 0.5f)
				return "slows you a lot while casting";
			if (moveScale < 0.95f)
				return "slows you a little while casting";
			return "you move freely while casting";
		}

		std::string DescribeAllowedGems(uint8_t affinity)
		{
			std::string allows;
			const auto add = [&](const char* name)
			{
				allows += std::string(allows.empty() ? "" : " and ") + name;
			};
			if (affinity & Affinity::Caster)
				add("spell");
			if (affinity & Affinity::Melee)
				add("melee");
			if (affinity & Affinity::Bow)
				add("bow");
			return allows;
		}
	}

	AttackSummary SummarizeAttack(const PlayerTraits& traits)
	{
		AttackSummary summary;
		const SkillTraits& skill = traits.Skills[0];
		summary.Ability = skill.Ability;
		const AbilityDef& ability = GetAbility(skill.Ability);
		summary.DamagePerHit = float(ability.Damage) * skill.DamageScale;
		const float interval = skill.CastTime + skill.Cooldown;
		summary.AttacksPerSecond = interval > 0.0f ? 1.0f / interval : 0.0f;
		if (ability.Kind == AbilityKind::Projectile)
			summary.Projectiles = GetProjectilePattern(ability, traits.ExtraProjectiles, traits.ExtraPierce, traits.ProjectileSpeedScale).Count;
		const float critFactor = 1.0f + traits.CritChance * (traits.CritMultiplier - 1.0f);
		summary.DamagePerSecond = summary.DamagePerHit * float(summary.Projectiles) * summary.AttacksPerSecond * critFactor;
		return summary;
	}

	float RateItem(const PlayerTraits& traits)
	{
		const float survival = float(traits.MaxHealth) / (1.0f - ArmorReduction(traits.Armor) * 0.6f);
		float resist = 0.0f;
		for (const float value : traits.Resist)
			resist += value;
		return SummarizeAttack(traits).DamagePerSecond * 1.5f + survival * 0.5f + traits.MaxMana * 0.25f + traits.ManaRegen * 4.0f + resist * 40.0f + traits.MoveSpeed * 8.0f;
	}

	UiColor ItemColor(ItemBase base, Rarity rarity, AbilityId skill)
	{
		return WithAlpha(base == ItemBase::Gem ? SkillColor(skill) : RarityColor(rarity));
	}

	UiColor ItemColor(const Item& item)
	{
		return ItemColor(item.Base, item.Quality, item.Skill);
	}

	void DrawItemIcon(UiDrawList& draw, const Item& item, const UiRect& rect, float alpha)
	{
		const UiColor color = ItemColor(item);
		if (item.IsGem())
		{
			draw.Shadow(rect.Shrink(rect.W * 0.12f), rect.W * 0.4f, rect.W * 0.22f, WithAlpha(color, 0.35f * alpha), { 0.0f, 0.0f });
			draw.Icon(IconFor(item.Skill), rect, { .Color = WithAlpha(Lighten(color, 0.55f), alpha), .ColorBottom = WithAlpha(color, alpha), .Outline = 0.8f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f * alpha), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.55f * alpha) });
			return;
		}
		const UiColor top = item.Quality == Rarity::Common ? Hex(0xEEECF6) : Lighten(color, 0.5f);
		const UiColor bottom = item.Quality == Rarity::Common ? Hex(0xA4A0B6) : color;
		draw.Icon(IconFor(item.Base), rect, { .Color = WithAlpha(top, alpha), .ColorBottom = WithAlpha(bottom, alpha), .Outline = 0.8f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f * alpha), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f * alpha) });
	}

	void DrawItemSlot(Ui& ui, const UiRect& rect, const Item& item, const SlotLook& look)
	{
		UiDrawList& draw = ui.Draw();
		const bool valid = item.IsValid();
		const UiColor color = valid ? ItemColor(item) : Theme::Edge;
		const Rarity quality = valid && !item.IsGem() ? item.Quality : Rarity::Common;
		const float pulse = 0.5f + 0.5f * std::sin(ui.GetTime() * 3.0f + rect.X * 0.05f);
		if (valid && quality >= Rarity::Legendary)
			draw.Rect(rect.Expand(2.0f), { .Fill = WithAlpha(color, (0.3f + 0.2f * pulse) * (look.Dim ? 0.4f : 1.0f)), .Softness = 7.0f, .Chamfer = 9.0f, .Additive = 0.6f });
		if (look.Hovered || look.Target)
			draw.Rect(rect.Expand(2.0f), { .Fill = WithAlpha(look.Target ? Theme::Accent : Theme::Astral, look.Target ? 0.25f + 0.25f * pulse : 0.22f), .Softness = 6.0f, .Chamfer = 9.0f, .Additive = 0.6f });

		UiColor border = valid ? WithAlpha(color, look.Dim ? 0.35f : 0.85f) : WithAlpha(Theme::Edge, 0.55f);
		if (look.Hovered)
			border = Lighten(border, 0.35f);
		if (look.Target)
			border = glm::mix(border, Theme::AccentLight, pulse);
		if (look.Highlight)
			border = Theme::AccentLight;
		const UiColor glow = valid && quality >= Rarity::Rare ? WithAlpha(color, 0.14f + (quality >= Rarity::Legendary ? 0.08f * pulse : 0.0f)) : UiColor(0.0f);
		draw.Rect(rect, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Border = border, .BorderWidth = look.Highlight ? 2.0f : 1.4f, .Glow = glow, .GlowSize = rect.W * 0.22f, .Chamfer = std::min(8.0f, rect.W * 0.14f), .Seed = rect.X * 0.013f });
		draw.Rect(rect.Shrink(3.0f), { .Border = Rgba(0.85f, 0.8f, 1.0f, 0.04f), .BorderWidth = 1.0f, .Chamfer = std::min(6.0f, rect.W * 0.11f) });

		if (valid)
		{
			DrawItemIcon(draw, item, rect.Shrink(rect.W * 0.17f), look.Dim ? 0.45f : 1.0f);
			if (const UiImage* gem = RarityGem(quality))
				draw.Image(*gem, { rect.X + 3.0f, rect.Y + 3.0f, rect.W * 0.27f, rect.W * 0.27f });
			const TextStyle level = Theme::Floating(12.0f, Theme::Muted, UiFont::Bold);
			draw.TextAligned(rect.Shrink(5.0f, 3.0f), std::to_string(unsigned(item.Level)), level, { 1.0f, 1.0f });
			if (look.Better)
				draw.Triangle({ rect.Right() - 9.0f, rect.Y + 9.0f }, { 11.0f, 9.0f }, 0.0f, BetterColor, Rgba(0.0f, 0.15f, 0.0f, 0.9f), 1.0f);
			if (look.Dim)
				draw.Icon(UiIcon::Padlock, { rect.X + 4.0f, rect.Bottom() - 20.0f, 16.0f, 16.0f }, { .Color = WorseColor, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f) });
		}
		else
		{
			if (look.EmptyIcon)
				draw.Icon(*look.EmptyIcon, rect.Shrink(rect.W * 0.25f), { .Color = Rgba(0.62f, 0.57f, 0.9f, 0.22f) });
			if (!look.EmptyLabel.empty())
				draw.TextAligned(rect.Shrink(4.0f, 3.0f), look.EmptyLabel, Theme::Body(12.0f, WithAlpha(Theme::Faint, 0.9f)), { 0.5f, 1.0f });
		}
		if (!look.KeyLabel.empty())
		{
			const TextStyle key = Theme::Bold(11.0f, Theme::AccentLight);
			const float width = draw.MeasureWidth(look.KeyLabel, key) + 9.0f;
			const UiRect badge{ rect.X + 3.0f, rect.Bottom() - 17.0f, width, 14.0f };
			draw.Rect(badge, { .Fill = Rgba(0.03f, 0.025f, 0.06f, 0.94f), .Border = WithAlpha(Theme::Edge, 0.9f), .BorderWidth = 1.0f, .Chamfer = 3.0f });
			draw.TextAligned(badge, look.KeyLabel, key, { 0.5f, 0.5f });
		}
	}

	void DescribeItem(UiTooltip& tooltip, const Item& item, uint8_t weaponAffinity)
	{
		const UiColor color = ItemColor(item);
		tooltip.Accent = WithAlpha(color, 0.85f);
		if (item.IsGem())
		{
			const AbilityDef& ability = GetAbility(item.Skill);
			tooltip.Header(GetItemName(item), color, std::nullopt, IconFor(item.Skill));
			tooltip.Muted(Format("Level %u skill gem, %s%s%s", unsigned(item.Level), ability.Spell ? "spell" : "attack", ability.Damages != Element::Physical ? ", " : "", ability.Damages != Element::Physical ? GetElementName(ability.Damages) : ""));
			if (item.GemQuality > 0)
				tooltip.Pair("Quality", Format("%u%%, +%u%% damage", unsigned(item.GemQuality), unsigned(item.GemQuality)), Theme::Muted, AffixColor);
			if (item.Mod != GemMod::None)
				tooltip.Text(Format("%s: %s", GetGemMod(item.Mod).Name, GetGemMod(item.Mod).Description), CorruptColor);
			if (item.Corrupted)
				tooltip.Text("Corrupted, it can't be changed any more", CorruptColor, 14.0f);
			tooltip.Space(2.0f).Text(ability.Description, Theme::Text, 15.0f).Space(4.0f);
			const float levelScale = 1.0f + 0.06f * float(item.Level - 1);
			if (ability.Damage > 0)
				tooltip.Pair("Base damage", Format("%.0f", double(float(ability.Damage) * levelScale)));
			tooltip.Pair("Mana", Format("%.0f", double(ability.ManaCost * (1.0f + 0.03f * float(item.Level - 1)))));
			tooltip.Pair("Cast time", Format("%.2f s", double(ability.CastTime)));
			tooltip.Pair("Cooldown", Format("%.1f s", double(ability.Cooldown)));
			tooltip.Space(2.0f).Muted(Format("Needs %s, %s", DescribeRequirement(ability.Needs), DescribeMovement(ability.CastMove)));
			if (!MeetsRequirement(ability.Needs, weaponAffinity))
				tooltip.Text("Your weapon can't use this gem", WorseColor);
			return;
		}

		const ItemBaseDef& base = GetItemBase(item.Base);
		const UiImage* gem = RarityGem(item.Quality);
		tooltip.Header(GetItemName(item), color, gem ? std::optional<UiImage>(*gem) : std::nullopt, gem ? std::nullopt : std::optional<UiIcon>(IconFor(item.Base)));
		tooltip.Muted(Format("Level %u %s %s", unsigned(item.Level), GetRarity(item.Quality).Name, base.Name));
		if (base.Slot == EquipSlot::Weapon)
		{
			const AbilityDef& attack = GetAbility(base.Attack);
			const float interval = attack.CastTime + attack.Cooldown;
			tooltip.Space(2.0f).Text(Format("%s: %.0f %s damage, %.1f per second", attack.Name, double(float(attack.Damage) * item.Power), GetElementName(attack.Damages), double(interval > 0.0f ? 1.0f / interval : 0.0f)));
			tooltip.Muted(Format("Uses %s skill gems, %.0f%% power for every skill", DescribeAllowedGems(base.Affinity).c_str(), double(item.Power) * 100.0));
		}
		if (base.ImplicitCount > 0)
		{
			tooltip.Separator();
			for (uint32_t i = 0; i < base.ImplicitCount; i++)
				tooltip.Text(DescribeAffix({ base.Implicits[i].Kind, GetImplicitValue(item, i) }), ImplicitColor);
		}
		const uint8_t slots = GetAffixSlots(item.Quality);
		if (item.AffixCount > 0 || slots > 0)
			tooltip.Separator();
		for (uint8_t i = 0; i < item.AffixCount; i++)
			tooltip.Pair(DescribeAffix(item.Affixes[i]), Format("T%u", unsigned(item.Affixes[i].Tier)), AffixColor, TierColor(item.Affixes[i].Tier));
		if (item.AffixCount < slots)
			tooltip.Muted(Format("%u open affix slot%s", unsigned(slots - item.AffixCount), slots - item.AffixCount == 1 ? "" : "s"), 13.5f);
		tooltip.Muted(Format("Item level %u allows affix tiers up to %u", unsigned(item.Level), unsigned(MaxAffixTier(item.Level))), 13.5f);
	}

	UiColor TierColor(uint8_t tier)
	{
		return WithAlpha(RarityColor(Rarity(std::clamp<int>(int(tier) - 1, 0, int(Rarity::Count) - 1))));
	}

	UiColor MaterialColor(Material material)
	{
		const uint32_t rgb = GetMaterial(material).Color;
		return Hex(rgb);
	}

	std::string FormatAmount(uint32_t amount)
	{
		if (amount >= 1000000)
			return Format("%.1fm", double(amount) / 1000000.0);
		if (amount >= 10000)
			return Format("%.0fk", double(amount) / 1000.0);
		if (amount >= 1000)
			return Format("%.1fk", double(amount) / 1000.0);
		return std::to_string(amount);
	}

	float DrawMaterial(UiDrawList& draw, glm::vec2 position, Material material, std::string_view text, float size, UiColor textColor)
	{
		const UiColor color = MaterialColor(material);
		draw.Icon(IconFor(material), { position.x, position.y - size * 0.5f, size, size }, { .Color = Lighten(color, 0.35f), .ColorBottom = color, .Outline = 0.8f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f) });
		const TextStyle style = Theme::Bold(size * 0.82f, textColor);
		draw.TextAligned({ position.x + size + 4.0f, position.y - size * 0.5f, 200.0f, size }, text, style, { 0.0f, 0.5f });
		return size + 4.0f + draw.MeasureWidth(text, style);
	}

	float DrawCost(UiDrawList& draw, const UiRect& area, const MaterialStock& cost, const MaterialStock& stock, bool unlimited, float alignX)
	{
		std::vector<std::pair<Material, std::string>> parts;
		for (size_t i = 0; i < cost.size(); i++)
			if (cost[i] > 0)
				parts.push_back({ Material(i), FormatAmount(cost[i]) });
		const float size = std::min(area.H, 18.0f);
		const TextStyle style = Theme::Bold(size * 0.82f, Theme::Text);
		float width = 0.0f;
		for (const auto& [material, text] : parts)
			width += size + 4.0f + draw.MeasureWidth(text, style) + 10.0f;
		width = std::max(width - 10.0f, 0.0f);
		float x = area.X + (area.W - width) * alignX;
		for (const auto& [material, text] : parts)
		{
			const bool enough = unlimited || stock[size_t(material)] >= cost[size_t(material)];
			x += DrawMaterial(draw, { x, area.Center().y }, material, text, size, enough ? Theme::Text : Theme::Danger) + 10.0f;
		}
		return width;
	}

	void DescribeMaterial(UiTooltip& tooltip, Material material, uint32_t amount, bool unlimited)
	{
		const MaterialDef& def = GetMaterial(material);
		tooltip.Accent = WithAlpha(MaterialColor(material), 0.85f);
		tooltip.Header(def.Name, MaterialColor(material), std::nullopt, IconFor(material));
		tooltip.Text(def.Description);
		tooltip.Muted(unlimited ? "Unlimited for testing, crafting won't spend it" : Format("You have %u", unsigned(amount)));
	}

	void DescribeSkill(UiTooltip& tooltip, const SkillTraits& skill, const PlayerTraits& traits)
	{
		if (skill.IsEmpty())
		{
			tooltip.Muted("Empty, drag a skill gem here");
			return;
		}
		const AbilityDef& ability = GetAbility(skill.Ability);
		const UiColor color = WithAlpha(SkillColor(skill.Ability));
		tooltip.Accent = WithAlpha(color, 0.85f);
		tooltip.Header(Format("%s  (level %u)", ability.Name, unsigned(skill.Level)), color, std::nullopt, IconFor(skill.Ability));
		tooltip.Space(2.0f).Text(ability.Description, Theme::Text, 15.0f).Space(4.0f);
		if (ability.Damage > 0)
		{
			uint32_t count = 1;
			if (ability.Kind == AbilityKind::Projectile)
				count = GetProjectilePattern(ability, traits.ExtraProjectiles, traits.ExtraPierce, traits.ProjectileSpeedScale).Count;
			const std::string damage = count > 1 ? Format("%.0f x %u", double(float(ability.Damage) * skill.DamageScale), count) : Format("%.0f", double(float(ability.Damage) * skill.DamageScale));
			std::string label = std::string(GetElementName(ability.Damages)) + " damage";
			label[0] = char(std::toupper(uint8_t(label[0])));
			tooltip.Pair(label, damage, Theme::Muted, WithAlpha(ElementColor(ability.Damages)));
		}
		if (skill.ManaCost > 0.0f)
			tooltip.Pair("Mana", Format("%.0f", double(skill.ManaCost)), Theme::Muted, Lighten(Theme::Mana, 0.3f));
		if (ability.Kind == AbilityKind::Projectile)
			tooltip.Pair("Range", Format("%.1f", double(ability.ProjectileRange)));
		else if (ability.Kind == AbilityKind::Ground && ability.Range > 0.0f)
			tooltip.Pair("Range", Format("%.1f", double(ability.Range)));
		else if ((ability.Kind == AbilityKind::Melee || ability.Kind == AbilityKind::Nova) && ability.Reach > 0.0f)
			tooltip.Pair("Reach", Format("%.1f", double(ability.Reach * traits.AreaScale * skill.AreaScale)));
		if (ability.Lunge > 0.0f)
			tooltip.Pair("Lunge", Format("%.1f", double(ability.Lunge)));
		tooltip.Pair("Cast time", Format("%.2f s", double(skill.CastTime)));
		tooltip.Pair("Cooldown", Format("%.1f s", double(skill.Cooldown)));
		tooltip.Space(2.0f).Muted(DescribeMovement(skill.MoveScale));
		if (!skill.Usable)
			tooltip.Text(Format("Needs %s", DescribeRequirement(ability.Needs)), WorseColor);
	}

	void CompareTraits(UiTooltip& tooltip, const PlayerTraits& current, const PlayerTraits& candidate)
	{
		const auto before = DescribeTraits(current);
		const auto after = DescribeTraits(candidate);
		bool changed = false;
		for (size_t i = 0; i < before.size(); i++)
		{
			const float delta = after[i].Value - before[i].Value;
			if (std::abs(delta) <= 0.005f * std::max(1.0f, std::abs(before[i].Value)))
				continue;
			tooltip.Pair(before[i].Label, Format("%s  \xE2\x86\x92  %s", Format(before[i]).c_str(), Format(after[i]).c_str()), Theme::Muted, delta > 0.0f ? BetterColor : WorseColor);
			changed = true;
		}
		for (uint32_t slot = 1; slot < AbilitySlots; slot++)
			if (current.Skills[slot].Usable != candidate.Skills[slot].Usable && !candidate.Skills[slot].IsEmpty())
			{
				tooltip.Text(Format("%s %s", GetAbility(candidate.Skills[slot].Ability).Name, candidate.Skills[slot].Usable ? "becomes usable" : "stops working"), candidate.Skills[slot].Usable ? BetterColor : WorseColor);
				changed = true;
			}
		if (!changed)
			tooltip.Muted("Same stats as what you wear now");
	}

	float DrawTraitsTable(Ui& ui, const UiRect& area, const PlayerTraits& traits)
	{
		UiDrawList& draw = ui.Draw();
		const auto lines = DescribeTraits(traits);
		constexpr float RowHeight = 22.0f;
		const float columnWidth = (area.W - 24.0f) * 0.5f;
		for (size_t i = 0; i < lines.size(); i++)
		{
			const float x = area.X + float(i % 2) * (columnWidth + 24.0f);
			const float y = area.Y + float(i / 2) * RowHeight;
			const UiRect row{ x, y, columnWidth, RowHeight };
			if ((i / 2) % 2 == 0)
				draw.Rect(row.Expand(1.0f), { .Fill = Rgba(0.8f, 0.75f, 1.0f, 0.035f), .Radius = 3.0f });
			draw.TextAligned(row.Shrink(6.0f, 0.0f), lines[i].Label, Theme::Body(14.5f, Theme::Muted), { 0.0f, 0.5f });
			draw.TextAligned(row.Shrink(6.0f, 0.0f), Format(lines[i]), Theme::Bold(14.5f, Theme::Text), { 1.0f, 0.5f });
		}
		return float((lines.size() + 1) / 2) * RowHeight;
	}
}
