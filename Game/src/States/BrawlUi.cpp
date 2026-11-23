#include "SessionState.h"
#include "App/GameApp.h"
#include "Audio/GameAudio.h"
#include "Rendering/LevelArt.h"
#include "Rendering/Models.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include "UI/Palette.h"
#include "UI/Ui.h"
#include <GameShared/Content/Fighters.h>
#include <RadianceEngine/Window/Input.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace Game
{
	namespace
	{
		constexpr glm::vec3 LavaGlow{ 1.0f, 0.42f, 0.12f };
		constexpr glm::vec3 FrostGlow{ 0.3f, 0.62f, 1.0f };
		constexpr glm::vec3 StormBurnGlow{ 0.4f, 0.55f, 1.0f };
		constexpr float StormArcRate = 9.0f;
		constexpr glm::vec3 MarkGlow{ 0.66f, 0.4f, 1.0f };
		constexpr float CardBurnDelay = 0.2f;
		constexpr float CardBurnSeconds = 0.85f;
		constexpr const char* SpellKeys[] = { "RMB", "Q", "R" };
		static_assert(std::size(SpellKeys) == BrawlSlots);
		constexpr Rdn::KeyCode CardKeys[] = { Rdn::KeyCode::Key1, Rdn::KeyCode::Key2, Rdn::KeyCode::Key3 };
		static_assert(std::size(CardKeys) == CardChoices);
		constexpr float CardNewsSeconds = 9.0f;
		constexpr float ApexNewsSeconds = 4.5f;
		constexpr glm::vec3 ArchmageGlow{ 1.0f, 0.78f, 0.46f };
		constexpr glm::vec3 BloodShieldGlow{ 0.95f, 0.06f, 0.14f };
		constexpr float CrumbleRange = 18.0f;
		constexpr float StrikeSight = 24.0f;
		constexpr float StrikeRumble = 0.5f;
		constexpr float HeartRateCalm = 0.3f;
		constexpr float HeartRateFury = 0.75f;
		constexpr float PourRumble = 0.35f;
		constexpr float FloodRumble = 0.45f;
		constexpr float GateRumble = 0.2f;
		constexpr float PourFury = 0.85f;
		constexpr float SizzleRange = 13.0f;
		constexpr float CrumbleRumble = 0.22f;
		constexpr float SpireTop = 4.5f;
		constexpr float SpireSlot = 1.0f;
		constexpr float RingLead = 1.5f;
		constexpr float BoltFlash = 0.4f;
		constexpr float LeaderSeconds = 0.35f;
		constexpr float PeakBoltScale = 1.3f;
		constexpr float SpireBoltScale = 1.5f;
		constexpr glm::vec3 PeakLight{ 5.9f, 4.2f, 14.0f };
		constexpr float ScorchSeconds = 7.0f;
		constexpr float ScorchHeatSeconds = 4.5f;
		constexpr float RingSegment = 0.9f;
		constexpr glm::vec3 BoltBlue{ 0.55f, 0.78f, 1.0f };
		constexpr glm::vec3 RitualBlue{ 0.42f, 0.62f, 1.0f };
		constexpr glm::vec3 StormViolet{ 0.46f, 0.38f, 1.0f };
		constexpr glm::vec3 HallFieldGlow{ 0.12f, 0.6f, 0.5f };

		std::vector<float> SpireStrikes(const CollapsePlan& plan, float from, float to)
		{
			std::vector<float> strikes;
			if (plan.Shape != BrawlShape::Peak || to <= from)
				return strikes;
			for (int32_t slot = std::max(int32_t(std::floor(from / SpireSlot)), 1); slot <= int32_t(std::floor(to / SpireSlot)); slot++)
			{
				const float when = (float(slot) + 0.15f + 0.7f * CollapseNoise(slot, 0x5F17, plan.Seed)) * SpireSlot;
				if (when > from && when <= to && CollapseNoise(slot, 0x5F19, plan.Seed) < 0.06f + 0.3f * StormBuild(plan, when))
					strikes.push_back(when);
			}
			return strikes;
		}

		uint32_t BoltSeed(const LightningBolt& bolt)
		{
			return uint32_t(bolt.Time * 977.0f) * 2654435761u ^ uint32_t(bolt.Center.x * 131.0f) * 40503u;
		}
		constexpr glm::vec3 StarlightPale{ 0.55f, 0.66f, 1.0f };
		constexpr float SigilScale = 1.05f;
		constexpr const char* BotLevelNames[] = { "Easy", "Normal", "Hard" };
		constexpr const char* ArenaNames[] = { "Random", "Molten Heart", "Frozen Lake", "Sky Observatory", "Storm Peak" };
		constexpr const char* ModeNames[] = { "Damage", "Knockback" };
		constexpr const char* GateTabs[] = { "Match", "Custom", "Teams" };
		constexpr const char* RunOnlyNames[] = { "Every kind", "Quartz", "Jade", "Sapphire", "Ruby", "Amethyst", "Prismatic", "Duelist", "Archer", "Prism Mage", "Geode Brute", "Mirror Moth", "Lens Wisp", "Obelisk" };
		static_assert(std::size(RunOnlyNames) == std::size(RunOnlyKinds) + 1);
		constexpr size_t StragglerMarkers = 2;
		constexpr float StragglerDelay = 25.0f;

		std::string Upper(std::string text);

		struct RunItemLook
		{
			std::string Name;
			std::string Text;
			std::string Tag;
			std::string Footer;
			UiColor Color;
			UiIcon Icon = UiIcon::Emerald;
			uint32_t Rarity = 0;
		};

		constexpr UiIcon SigilIcons[] = { UiIcon::EarthCrack, UiIcon::Whirlwind, UiIcon::Teleport, UiIcon::MeteorImpact, UiIcon::ArrowCluster, UiIcon::Hourglass, UiIcon::Gems, UiIcon::Sparkles, UiIcon::SkullCrossedBones, UiIcon::Crown, UiIcon::Dodge, UiIcon::HeartPlus, UiIcon::Sprint, UiIcon::Fireball };
		static_assert(std::size(SigilIcons) == size_t(SigilId::Count));
		constexpr UiIcon CurseIcons[] = { UiIcon::Drop, UiIcon::Backpack, UiIcon::Dodge, UiIcon::Hourglass };
		static_assert(std::size(CurseIcons) == size_t(CurseId::Count));
		constexpr UiIcon ServiceIcons[] = { UiIcon::Healing, UiIcon::Sparkles, UiIcon::Padlock };
		static_assert(std::size(ServiceIcons) == size_t(ServiceId::Count));
		constexpr uint32_t SigilColor = 0x7FD8FF;
		constexpr uint32_t DuoColor = 0xFFD36B;
		constexpr uint32_t CurseColor = 0xE0485A;
		constexpr uint32_t ServiceColor = 0xB9C2D8;
		constexpr UiIcon RuneIcons[] = { UiIcon::ArrowCluster, UiIcon::ThornedArrow, UiIcon::Cycle, UiIcon::Aura, UiIcon::BurningEmbers, UiIcon::Stopwatch, UiIcon::SplitArrows, UiIcon::OnTarget, UiIcon::EchoRipples, UiIcon::ArcingBolt, UiIcon::StarSwirl, UiIcon::PsychicWaves, UiIcon::FireRing };
		static_assert(std::size(RuneIcons) == size_t(RuneId::Count));
		constexpr uint32_t RuneColor = 0x6FE3C4;
		constexpr uint32_t TriggerRuneColor = 0xFFB45E;

		UiColor RuneTone(RuneId rune)
		{
			return Hex(GetRune(rune).Kind == RuneKind::Trigger ? TriggerRuneColor : RuneColor);
		}

		UiIcon RuneIcon(RuneId rune)
		{
			return RuneIcons[std::min<size_t>(size_t(rune), std::size(RuneIcons) - 1)];
		}

		std::string UpperCase(std::string text)
		{
			for (char& letter : text)
				letter = char(std::toupper(static_cast<unsigned char>(letter)));
			return text;
		}

		RunItemLook ItemLookOf(const RunCard& card)
		{
			switch (card.Kind)
			{
			case RunCardKind::Rune:
			{
				const RuneDef& rune = GetRune(card.Rune());
				return { rune.Name, rune.Text, rune.Kind == RuneKind::Trigger ? "TRIGGER RUNE" : "RUNE", "FITS " + UpperCase(rune.Needs), RuneTone(rune.Id), RuneIcon(rune.Id), rune.Rarity >= CardRarity::Rare ? 1u : 0u };
			}
			case RunCardKind::Sigil:
			{
				const SigilDef& sigil = GetSigil(SigilId(card.Id));
				return { sigil.Name, sigil.Text, "SIGIL", *sigil.Twice ? std::string("SLOT 1 + LEFTMOST: ") + sigil.Twice : std::string("ORDERS YOUR SIGILS"), Hex(SigilColor), SigilIcons[std::min<size_t>(card.Id, std::size(SigilIcons) - 1)], 1 };
			}
			case RunCardKind::Duo:
			{
				const DuoDef& duo = GetDuo(DuoId(card.Id));
				return { duo.Name, duo.Text, "DUO", Upper(GetAbility(duo.First).Name) + " + " + Upper(GetAbility(duo.Second).Name), Hex(DuoColor), IconFor(duo.First), 2 };
			}
			case RunCardKind::Curse:
			{
				const CurseDef& curse = GetCurse(CurseId(card.Id));
				return { curse.Name, curse.Text, "CURSED", "CLEANSE AT A SHRINE", Hex(CurseColor), CurseIcons[std::min<size_t>(card.Id, std::size(CurseIcons) - 1)], 0 };
			}
			case RunCardKind::Service:
			{
				const ServiceDef& service = GetService(ServiceId(card.Id));
				return { service.Name, service.Text, "SERVICE", "", Hex(ServiceColor), ServiceIcons[std::min<size_t>(card.Id, std::size(ServiceIcons) - 1)], 0 };
			}
			default:
				return { "", "", "", "", Theme::Faint, UiIcon::Emerald, 0 };
			}
		}
		constexpr const char* RunAffixNames[] = { "None", "Hasted", "Warded", "Splitting", "Volatile", "Vampiric", "Reflective", "Random" };
		static_assert(std::size(RunAffixNames) == size_t(RandomAffix) + 1);
		constexpr const char* TeamChoices[] = { "Off", "2 teams", "3 teams", "4 teams" };
		static_assert(std::size(BotLevelNames) == BotLevels);
		static_assert(std::size(ModeNames) == size_t(BrawlMode::Count));
		static_assert(std::size(TeamChoices) == MaxTeams);

		struct Sigil
		{
			AbilityId Spell = AbilityId::EmberBolt;
			uint8_t Stars = 0;
			Vec2 Points[8];
			uint8_t Lines = 0;
			uint8_t Links[8][2];
		};

		constexpr Sigil Sigils[] = {
			{ AbilityId::EmberBolt, 6, { { 0.9f, 0.0f }, { 0.2f, 0.4f }, { 0.2f, -0.4f }, { -0.35f, 0.2f }, { -0.35f, -0.2f }, { -0.85f, 0.0f } }, 6, { { 0, 1 }, { 0, 2 }, { 1, 3 }, { 2, 4 }, { 3, 5 }, { 4, 5 } } },
			{ AbilityId::Comet, 6, { { 0.6f, 0.6f }, { 0.15f, 0.2f }, { -0.35f, -0.25f }, { -0.85f, -0.7f }, { 0.3f, 0.7f }, { 0.7f, 0.3f } }, 5, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 4, 1 }, { 5, 1 } } },
			{ AbilityId::Warp, 6, { { -0.85f, -0.55f }, { -0.25f, 0.3f }, { 0.1f, -0.3f }, { 0.8f, 0.55f }, { 0.4f, 0.62f }, { 0.75f, 0.15f } }, 5, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 3, 5 } } },
			{ AbilityId::GlacialNova, 7, { { 0.0f, 0.0f }, { 0.8f, 0.0f }, { 0.4f, 0.69f }, { -0.4f, 0.69f }, { -0.8f, 0.0f }, { -0.4f, -0.69f }, { 0.4f, -0.69f } }, 6, { { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 }, { 6, 1 } } },
			{ AbilityId::GravityWell, 7, { { 0.85f, 0.05f }, { 0.4f, 0.65f }, { -0.35f, 0.55f }, { -0.55f, -0.1f }, { -0.1f, -0.45f }, { 0.25f, -0.1f }, { 0.0f, 0.1f } }, 6, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 } } },
			{ AbilityId::ArcLightning, 6, { { -0.15f, 0.9f }, { 0.3f, 0.25f }, { -0.2f, 0.05f }, { 0.25f, -0.5f }, { -0.1f, -0.9f }, { 0.65f, 0.55f } }, 5, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 1, 5 } } },
			{ AbilityId::DiamondWard, 6, { { 0.0f, 0.92f }, { 0.58f, -0.12f }, { 0.0f, -0.62f }, { -0.58f, -0.12f }, { 0.32f, 0.22f }, { -0.32f, 0.22f } }, 8, { { 0, 4 }, { 4, 1 }, { 1, 2 }, { 2, 3 }, { 3, 5 }, { 5, 0 }, { 1, 3 }, { 4, 5 } } },
			{ AbilityId::FrostSpear, 6, { { -0.85f, -0.62f }, { -0.3f, -0.22f }, { 0.25f, 0.18f }, { 0.82f, 0.6f }, { 0.4f, 0.57f }, { 0.67f, 0.21f } }, 6, { { 0, 1 }, { 1, 2 }, { 2, 4 }, { 4, 3 }, { 2, 5 }, { 5, 3 } } },
		};

		const Sigil* FindSigil(AbilityId spell)
		{
			for (const Sigil& sigil : Sigils)
				if (sigil.Spell == spell)
					return &sigil;
			return nullptr;
		}

		float Smooth(float edge0, float edge1, float value)
		{
			const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float Twinkle(uint32_t x, uint32_t y)
		{
			uint32_t hash = x * 73856093u ^ y * 19349663u;
			hash = (hash ^ (hash >> 13)) * 1274126177u;
			return float(hash & 0xFFFFu) / 65535.0f;
		}

		template<typename... Arguments>
		std::string Format(const char* format, Arguments... arguments)
		{
			char text[192];
			std::snprintf(text, sizeof(text), format, arguments...);
			return text;
		}

		uint16_t WinPoints(BrawlMode mode)
		{
			return mode == BrawlMode::Damage ? PlacementPoints[0] : RoundWinPoints;
		}

		struct Stepper
		{
			const char* Label = "";
			std::string Value;
			int Low = 0;
			int High = 0;
			int Step = 1;
			bool Enabled = true;
			std::string Note;
			const char* Tip = nullptr;
			float LabelWidth = 170.0f;
			float ValueWidth = 100.0f;
		};

		bool StepRow(Ui& ui, UiRect& area, UiId id, int& current, const Stepper& stepper)
		{
			UiDrawList& draw = ui.Draw();
			UiRect row = area.CutTop(40.0f);
			const UiRect label = row.CutLeft(stepper.LabelWidth);
			draw.TextAligned(label, stepper.Label, Theme::Bold(16.0f, stepper.Enabled ? Theme::Text : Theme::Faint), { 0.0f, 0.5f });
			if (stepper.Tip && ui.Interact(id ^ 0x7105C0DEull, label).Hovered)
				ui.Tooltip().Header(stepper.Label, Theme::AccentLight).Text(stepper.Tip);
			const int before = current;
			if (ui.Button(id, row.CutLeft(40.0f), "-", ButtonKind::Normal, stepper.Enabled && current > stepper.Low))
				current = std::max(current - stepper.Step, stepper.Low);
			TextStyle number = Theme::Title(22.0f);
			if (!stepper.Enabled)
			{
				number.Color = Theme::Faint;
				number.ColorBottom = Theme::Faint;
			}
			draw.TextAligned(row.CutLeft(stepper.ValueWidth), stepper.Value, number, { 0.5f, 0.5f });
			if (ui.Button(id + 1, row.CutLeft(40.0f), "+", ButtonKind::Normal, stepper.Enabled && current < stepper.High))
				current = std::min(current + stepper.Step, stepper.High);
			if (!stepper.Note.empty())
				draw.TextFit(row.Shrink(12.0f, 0.0f), stepper.Note, Theme::Body(13.5f, Theme::Faint));
			area.CutTop(6.0f);
			return current != before;
		}

		std::string Percent(int value)
		{
			return std::to_string(value) + "%";
		}

		bool ToggleRow(Ui& ui, UiRect& area, UiId id, const char* label, bool& value, bool enabled, const char* note)
		{
			UiDrawList& draw = ui.Draw();
			const UiRect row = area.CutTop(32.0f);
			bool changed = false;
			if (enabled)
				changed = ui.Checkbox(id, row, label, value);
			else
			{
				draw.Diamond({ row.X + 10.0f, row.Center().y }, 19.0f, Theme::SlotBottom, WithAlpha(Theme::Edge, 0.45f), 1.3f);
				draw.TextAligned({ row.X + 28.0f, row.Y, row.W - 28.0f, row.H }, label, Theme::Body(16.0f, Theme::Faint), { 0.0f, 0.5f });
			}
			if (note)
			{
				const UiRect line = area.CutTop(18.0f);
				draw.TextAligned({ line.X + 28.0f, line.Y, line.W - 28.0f, line.H }, note, Theme::Body(13.5f, Theme::Faint), { 0.0f, 0.5f });
			}
			area.CutTop(4.0f);
			return changed;
		}

		std::string CustomSummary(const MatchRules& rules)
		{
			const MatchRules defaults;
			const bool damage = rules.Mode == BrawlMode::Damage;
			std::vector<std::string> parts;
			if (rules.Health != defaults.Health)
				parts.push_back(std::to_string(rules.Health) + " life");
			if (damage && rules.Mana != defaults.Mana)
				parts.push_back(std::to_string(rules.Mana) + " mana");
			if (damage && rules.ManaRegen != defaults.ManaRegen)
				parts.push_back(std::to_string(rules.ManaRegen) + "% mana regen");
			if (damage && rules.Damage != defaults.Damage)
				parts.push_back(std::to_string(rules.Damage) + "% damage");
			if (rules.CastSpeed != defaults.CastSpeed)
				parts.push_back(std::to_string(rules.CastSpeed) + "% cast speed");
			if (rules.CooldownSpeed != defaults.CooldownSpeed)
				parts.push_back(std::to_string(rules.CooldownSpeed) + "% cooldown speed");
			if (rules.MoveSpeed != defaults.MoveSpeed)
				parts.push_back(std::to_string(rules.MoveSpeed) + "% movement");
			if (rules.Knockback != defaults.Knockback)
				parts.push_back(std::to_string(rules.Knockback) + "% knockback");
			if (rules.RandomSpells)
				parts.push_back("random spells every round");
			if (damage && !rules.Apex)
				parts.push_back("no apex cards");
			else if (damage && rules.ApexDraft)
				parts.push_back("an apex pick at the start");
			if (!rules.Crumble)
				parts.push_back("no crumbling");
			if (!rules.WinnerCard)
				parts.push_back("no card for the round winner");
			std::string text;
			for (size_t i = 0; i < parts.size(); i++)
				text += (i == 0 ? "" : i + 1 == parts.size() ? " and " : ", ") + parts[i];
			return text;
		}

		std::string Seconds(float seconds)
		{
			return std::to_string(int(std::ceil(std::max(seconds, 0.0f))));
		}

		bool IsChoosing(const GameStatus& status)
		{
			return status.Mode == GameMode::Brawl && status.Match != MatchPhase::Ended && (status.Brawl.Phase == BrawlPhase::Draft || status.Brawl.Phase == BrawlPhase::Pick);
		}

		float FinalCollapse(const CollapsePlan& plan)
		{
			if (plan.Shape == BrawlShape::Ruins)
				return HallRingClose;
			if (plan.Shape == BrawlShape::Peak)
				return RingClose;
			return std::max(CollapseGrace + (plan.StartRadius - CollapseMinRadius) / CollapseRate, FinalCollapseStart);
		}

		std::string SpellDamage(AbilityId id, float scale = 1.0f)
		{
			const AbilityDef& def = GetAbility(id);
			const auto scaled = [scale](float damage) { return int(std::lround(damage * scale)); };
			const int base = scaled(float(def.BrawlDamage));
			switch (id)
			{
			case AbilityId::Firebomb:
				return Format("%d, then %d on the skip", base, scaled(float(GetAbility(AbilityId::FirebombSkip).BrawlDamage)));
			case AbilityId::StormStrike:
				return Format("%d, then %d on the jump", base, scaled(float(GetAbility(AbilityId::StormJump).BrawlDamage)));
			case AbilityId::ObsidianBlades:
				return Format("%d per blade, %d if all %u land", base, base * int(ObsidianVolley), unsigned(ObsidianVolley));
			case AbilityId::FrostSpear:
				return Format("%d tapped, up to %d charged, %d burst", base, scaled(float(SpearFullDamage)), scaled(float(SpearBurstDamage)));
			case AbilityId::PurpleFireball:
				return Format("%d tapped, up to %d charged, to everyone in the blast", base, scaled(float(FireballFullDamage)));
			default:
				return def.Chain > 0 ? Format("%d per rival", base) : std::to_string(base);
			}
		}

		std::string SpellSummary(AbilityId id, BrawlMode mode)
		{
			const AbilityDef& def = GetAbility(id);
			if (def.Kind == AbilityKind::Shield)
				return mode == BrawlMode::Damage ? Format("Absorbs %.0f, %.0f mana   %.0f s if broken", double(HexShieldDurability), double(def.BrawlMana), double(HexShieldBreakCooldown)) : Format("Blocks shoves   %.0f s if broken", double(HexShieldBreakCooldown));
			if (mode == BrawlMode::Damage)
			{
				const std::string cost = def.BrawlMana > 0.0f ? Format("%.0f mana", double(def.BrawlMana)) : std::string("free");
				const std::string effect = id == AbilityId::ObsidianBlades ? Format("%d dmg x%u", int(def.BrawlDamage), unsigned(ObsidianVolley)) : def.ChargeTime > 0.0f ? Format("%d-%d dmg", int(def.BrawlDamage), int(ChargedShotOf(def, 1.0f).DamageScale * float(def.BrawlDamage) + 0.5f)) : def.BrawlDamage > 0 ? Format("%d dmg", int(def.BrawlDamage)) : def.Ward > 0.0f ? Format("Holds %.0f", double(DiamondWardDurability)) : std::string("Teleports");
				return Format("%s, %s   %g s", effect.c_str(), cost.c_str(), double(def.BrawlCooldown > 0.0f ? def.BrawlCooldown : def.Cooldown));
			}
			switch (id)
			{
			case AbilityId::ChainHook:
				return Format("Hooks and pulls   %.0f s", double(def.Cooldown));
			case AbilityId::Firebomb:
				return Format("Shove %.0f twice   %.1f s", double(def.Push), double(def.Cooldown));
			case AbilityId::ObsidianBlades:
				return Format("Shove %.1f x%u   %.1f s", double(def.Push), unsigned(ObsidianVolley), double(def.Cooldown));
			case AbilityId::FrostSpear:
			case AbilityId::PurpleFireball:
				return Format("Shove %.0f-%.0f, charge   %.0f s", double(def.Push), double(ChargedShotOf(def, 1.0f).Push), double(def.Cooldown));
			case AbilityId::StormStrike:
				return Format("Shove %.0f, jumps   %.1f s", double(def.Push), double(def.Cooldown));
			default:
				break;
			}
			if (def.Ward > 0.0f)
				return Format("Reflects for %.1f s   %.0f s", double(def.Ward), double(def.Cooldown));
			if (def.Push < 0.0f)
				return Format("Pulls inward   %.0f s", double(def.Cooldown));
			if (def.Kind == AbilityKind::Dash)
				return Format("Teleport %.1f   %.0f s", double(def.Range), double(def.Cooldown));
			return Format("Shove %.0f   %.1f s", double(def.Push), double(def.Cooldown));
		}

		std::string Upper(std::string text)
		{
			for (char& c : text)
				c = char(std::toupper(static_cast<unsigned char>(c)));
			return text;
		}

		UiColor RarityColor(CardRarity rarity)
		{
			switch (rarity)
			{
			case CardRarity::Rare:
				return Hex(0xB58CFF);
			case CardRarity::Legendary:
				return Hex(0xFFCF5A);
			case CardRarity::Apex:
				return Hex(0xFF7AC9);
			default:
				return Hex(0x7FD6FF);
			}
		}

		const char* RarityName(CardRarity rarity)
		{
			switch (rarity)
			{
			case CardRarity::Rare:
				return "Rare";
			case CardRarity::Legendary:
				return "Legendary";
			case CardRarity::Apex:
				return "Apex";
			default:
				return "Common";
			}
		}

		UiColor CardTint(const CardDef& card)
		{
			return card.Kind == CardKind::Upgrade ? WithAlpha(SkillColor(card.Spell)) : RarityColor(card.Rarity);
		}

		const char* ActionWord(AbilityId id)
		{
			switch (id)
			{
			case AbilityId::EmberBolt:
				return "Bolt";
			case AbilityId::Comet:
				return "Zone";
			case AbilityId::Warp:
				return "Blink";
			case AbilityId::GlacialNova:
				return "Burst";
			case AbilityId::GravityWell:
				return "Pull";
			case AbilityId::ArcLightning:
				return "Chain";
			case AbilityId::DiamondWard:
				return "Block";
			case AbilityId::FrostSpear:
				return "Charge";
			case AbilityId::ChainHook:
				return "Hook";
			case AbilityId::Firebomb:
				return "Lob";
			case AbilityId::ObsidianBlades:
				return "Volley";
			case AbilityId::StormStrike:
				return "Strike";
			case AbilityId::EarthSpike:
				return "Rupture";
			case AbilityId::PurpleFireball:
				return "Blast";
			case AbilityId::HexShield:
				return "Shield";
			default:
				return "";
			}
		}

		UiColor TagColor(CardTag tag)
		{
			constexpr uint32_t Colors[] = { 0xFF8A4A, 0x8FE3FF, 0xC9A6FF, 0x7FA8FF, 0xFF6F7F, 0xFFD36A };
			static_assert(std::size(Colors) == size_t(CardTag::Count));
			return Hex(Colors[size_t(tag)]);
		}

		std::string TagNames(TagMask tags)
		{
			std::string names;
			for (size_t tag = 0; tag < size_t(CardTag::Count); tag++)
				if (tags & TagBit(CardTag(tag)))
					names += (names.empty() ? "" : ", ") + std::string(CardTagNames[tag]);
			return names;
		}

		void DescribeCard(UiTooltip& tooltip, CardId id, BrawlMode mode)
		{
			const CardDef& card = GetCard(id);
			const UiColor color = RarityColor(card.Rarity);
			tooltip.Accent = WithAlpha(color, 0.85f);
			tooltip.Header(card.Name, color, std::nullopt, IconFor(id));
			tooltip.Space(2.0f).Text(CardText(card, mode), Theme::Text, 15.0f).Space(4.0f);
			if (card.Tags != 0)
				tooltip.Pair("Tags", TagNames(card.Tags));
			tooltip.Muted(card.Kind == CardKind::Upgrade ? std::string(RarityName(card.Rarity)) + " upgrade for " + GetAbility(card.Spell).Name : card.Rarity == CardRarity::Apex ? std::string("Apex card, one per player and announced to everyone") : std::string(RarityName(card.Rarity)) + " wild card");
		}

		void DescribeSpell(UiTooltip& tooltip, AbilityId id, BrawlMode mode, const SkillTraits* run = nullptr)
		{
			const AbilityDef& def = GetAbility(id);
			const UiColor color = WithAlpha(SkillColor(id));
			tooltip.Accent = WithAlpha(color, 0.85f);
			tooltip.Header(def.Name, color, std::nullopt, IconFor(id));
			tooltip.Space(2.0f).Text(def.Description, Theme::Text, 15.0f).Space(4.0f);
			tooltip.Pair("Role", ActionWord(id));
			const bool damage = mode == BrawlMode::Damage;
			if (def.Kind == AbilityKind::Shield)
			{
				if (damage)
				{
					if (!run)
						tooltip.Pair("Mana", Format("%.0f to raise, %.1f per point it soaks up", double(def.BrawlMana), double(HexShieldManaPerPoint)));
					tooltip.Pair("Holds", Format("%.0f damage, then it shatters", double(run ? run->Durability : HexShieldDurability)));
				}
				else
					tooltip.Pair("Holds", "a few hard shoves, then it shatters");
				tooltip.Pair("Covers", Format("%.0f degrees in front of you", double(2.0f * def.HalfArc * 57.2958f)));
				tooltip.Pair("Overflow", "a blow it can't hold gets through in part");
				tooltip.Pair("Mends", Format("%.0f%% a second, %.0f s after you lower it", double(HexShieldMendRate * 100.0f), double(HexShieldMendDelay)));
				tooltip.Pair("Raise again", Format("%.1f s after lowering", double(def.Cooldown)));
				tooltip.Pair("Shattered", Format("reforms after %.0f s", double(HexShieldBreakCooldown)));
				tooltip.Space(2.0f).Muted("Hold to keep it up. You walk slowly behind it and can't cast until you let go, and a dodge drops it");
				return;
			}
			if (def.Kind == AbilityKind::Ward)
			{
				const float cooldown = run ? run->Cooldown : damage && def.BrawlCooldown > 0.0f ? def.BrawlCooldown : def.Cooldown;
				if (damage)
				{
					if (!run)
						tooltip.Pair("Mana", Format("%.0f", double(def.BrawlMana)));
					tooltip.Pair("Holds", Format("%.0f damage, from every side", double(run ? run->Durability : DiamondWardDurability)));
				}
				else
					tooltip.Pair("Holds", "a few hard shoves, from every side");
				tooltip.Pair("Lasts", Format("%.1f s, or until it shatters", double(def.Ward)));
				tooltip.Pair("Reflects", "spells it can hold fly back at their caster");
				tooltip.Pair("Overflow", "a blow it can't hold shatters it and the rest gets through");
				tooltip.Pair("Cast time", "instant");
				tooltip.Pair("Cooldown", Format("%.1f s", double(cooldown)));
				tooltip.Pair("Shattered", Format("%.1f s before you can recast it", double(cooldown * DiamondWardBreakCooldown)));
				tooltip.Space(2.0f).Muted("Cast it on the move");
				return;
			}
			if (damage)
			{
				if (def.BrawlDamage > 0)
					tooltip.Pair("Damage", SpellDamage(id, run ? run->DamageScale : 1.0f));
				if (!run)
					tooltip.Pair("Mana", def.BrawlMana > 0.0f ? Format("%.0f", double(def.BrawlMana)) : std::string("free"));
			}
			const float shove = damage ? DamagePushScale : 1.0f;
			if (id == AbilityId::PurpleFireball)
			{
				tooltip.Pair("Shove", Format(damage ? "%.1f to %.1f, outward" : "%.0f to %.0f, outward", double(def.Push * shove), double(FireballFullPush * shove)));
				tooltip.Pair("Range", Format("%.1f to %.1f", double(def.ProjectileRange), double(FireballFullRange)));
				tooltip.Pair("Blast", Format("%.1f to %.1f across", double(2.0f * def.ImpactRadius), double(2.0f * FireballFullBlast)));
				tooltip.Pair("Full charge", Format("%.1f s", double(def.ChargeTime)));
				tooltip.Pair("Hold longer", Format("it flies at full power after %.0f s", double(ChargeHoldSeconds)));
			}
			else if (id == AbilityId::ObsidianBlades)
			{
				tooltip.Pair("Shove", Format("%.1f per blade", double(def.Push * shove)));
				tooltip.Pair("Blades", Format("%u in a %.0f degree fan", unsigned(ObsidianVolley), double(ObsidianSpread * 57.2958f)));
				tooltip.Pair("Point blank", Format("all %u land within %.1f", unsigned(ObsidianVolley), double((PlayerRadius + def.ProjectileRadius) / std::sin(0.5f * ObsidianSpread))));
				tooltip.Pair("Range", Format("%.1f", double(def.ProjectileRange)));
			}
			else if (def.ChargeTime > 0.0f)
			{
				tooltip.Pair("Shove", Format(damage ? "%.1f to %.1f" : "%.0f to %.0f", double(def.Push * shove), double(SpearFullPush * shove)));
				tooltip.Pair("Range", Format("%.1f to %.1f", double(def.ProjectileRange), double(SpearFullRange)));
				tooltip.Pair("Full charge", Format("%.1f s, pierces %u rivals", double(def.ChargeTime), unsigned(SpearFullPierce)));
				tooltip.Pair("Hold longer", Format("it flies at full power after %.0f s", double(ChargeHoldSeconds)));
				tooltip.Pair("Burst", Format(damage ? "%.1f around where it stops, shove %.1f" : "%.1f around where it stops, shove %.0f", double(SpearBurstRadius), double(SpearBurstPush * shove)));
			}
			else if (id == AbilityId::GlacialNova)
			{
				const AbilityDef& eruption = GetAbility(AbilityId::GlacialEruption);
				tooltip.Pair("Slows", Format("rivals in the frost for %.1f s", double(PvpChillDuration(def.Chill))));
				if (damage)
					tooltip.Pair("Press again", Format("within %.1f s: the spikes erupt for %d, shove %.1f", double(FrostNovaWindow), int(std::lround(float(eruption.BrawlDamage) * (run ? run->DamageScale : 1.0f))), double(eruption.Push * shove)));
				else
					tooltip.Pair("Press again", Format("within %.1f s: the spikes erupt, shove %.0f", double(FrostNovaWindow), double(eruption.Push * shove)));
				tooltip.Pair("Unripe", Format("before %.1f s: %.0f%% strength", double(FrostNovaUnripe), double(FrostNovaUnripeScale * 100.0f)));
				tooltip.Pair("Brittle", Format("after %.1f s: shove +%.0f%%", double(FrostNovaRipe), double((FrostNovaBrittlePush - 1.0f) * 100.0f)));
			}
			else if (def.Push > 0.0f)
				tooltip.Pair("Shove", Format(damage ? "%.1f" : "%.0f", double(def.Push * shove)));
			else if (def.Push < 0.0f)
				tooltip.Pair("Pull", "through the center");
			else if (id == AbilityId::ChainHook)
				tooltip.Pair("Pull", "to you and past you");
			if (id == AbilityId::ChainHook)
				tooltip.Pair("Press again", "yank yourself to the hook");
			else if (id == AbilityId::Firebomb)
				tooltip.Pair("Skips", Format(damage ? "%.1f further, shove %.1f" : "%.1f further, shove %.0f", double(FirebombSkipDistance), double(GetAbility(AbilityId::FirebombSkip).Push * shove)));
			else if (id == AbilityId::StormStrike)
				tooltip.Pair("Jumps", Format("once, up to %.1f", double(GetAbility(AbilityId::StormJump).Range)));
			else if (id == AbilityId::EarthSpike)
			{
				tooltip.Pair("Steer", Format("%.1f s after you pick the spot", double(def.CastTime)));
				tooltip.Pair("Spike", Format("%.1f long, skewers every rival on it", double(def.Reach)));
			}
			if (def.Kind == AbilityKind::Projectile && def.ChargeTime <= 0.0f)
				tooltip.Pair("Range", Format("%.1f", double(def.ProjectileRange)));
			else if (def.Kind == AbilityKind::Ground || def.Kind == AbilityKind::Dash)
				tooltip.Pair("Range", Format("%.1f", double(def.Range)));
			else if (def.Reach > 0.0f)
				tooltip.Pair("Reach", Format("%.1f", double(def.Reach)));
			if (def.Chain > 0)
				tooltip.Pair("Jumps", std::to_string(def.Chain));
			const float castTime = run ? run->CastTime : def.CastTime;
			const float castMove = run ? run->MoveScale : def.CastMove;
			tooltip.Pair("Cast time", castTime > 0.0f ? Format("%.2f s", double(castTime)) : std::string("instant"));
			tooltip.Pair("Cooldown", Format("%.1f s", double(run ? run->Cooldown : damage && def.BrawlCooldown > 0.0f ? def.BrawlCooldown : def.Cooldown)));
			tooltip.Space(2.0f).Muted(id == AbilityId::PurpleFireball ? "You walk slowly while charging, and a dodge snuffs the fireball out" : def.ChargeTime > 0.0f ? "You walk at half speed while charging, and a dodge drops the spear" : castMove <= 0.01f ? "You stand still while casting" : castMove >= 0.99f ? "Cast it on the move" : "You slow down while casting");
		}
	}

	void SessionState::UpdateArena(float dt)
	{
		m_RestoreAge += dt;
		m_CrumbleCooldown = std::max(m_CrumbleCooldown - dt, 0.0f);
		m_Flash = std::max(m_Flash - dt * 4.5f, 0.0f);
		m_RitualPulse = std::max(m_RitualPulse - dt * 2.5f, 0.0f);
		const Level* level = m_Client.GetLevel();
		if (!level || m_Client.GetSession().Mode != GameMode::Brawl)
			return;
		const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
		UpdateCardNews(dt);
		if (level->Kind == MapKind::StormPeak)
			UpdateStorm(brawl);
		if (level->Kind == MapKind::Caldera)
			UpdateCaldera(brawl, dt);
		const TileMap& map = level->Map;
		const size_t cells = size_t(map.GetWidth()) * size_t(map.GetHeight());
		const bool changed = !m_HasPlan || brawl.Plan.Seed != m_SeenPlan.Seed || brawl.Plan.StartRadius != m_SeenPlan.StartRadius || brawl.Plan.Center != m_SeenPlan.Center;
		if (changed || m_Fallen.size() != cells)
		{
			m_Risen.assign(cells, 0);
			m_Fallen.assign(cells, 0);
			if (m_HasPlan)
			{
				for (int32_t y = 0; y < map.GetHeight(); y++)
					for (int32_t x = 0; x < map.GetWidth(); x++)
						if (IsBrawlFloor(map.GetTerrain(x, y)) && TileCollapseTime(m_SeenPlan, x, y) <= m_SeenClock && TileCollapseTime(brawl.Plan, x, y) > brawl.Clock)
							m_Risen[size_t(y) * size_t(map.GetWidth()) + size_t(x)] = 1;
				m_RestoreAge = 0.0f;
			}
			m_SeenPlan = brawl.Plan;
			m_HasPlan = true;
		}
		m_SeenClock = brawl.Clock;
		if (brawl.Clock < 0.0f)
			return;
		if (!level->Hall.IsEmpty() || UsesForge(*level))
			return;
		const Vec2 focus = { m_CameraTarget.x, m_CameraTarget.z };
		const int32_t minX = std::max(int32_t(focus.x - CrumbleRange), 0);
		const int32_t maxX = std::min(int32_t(focus.x + CrumbleRange), map.GetWidth() - 1);
		const int32_t minY = std::max(int32_t(focus.y - CrumbleRange), 0);
		const int32_t maxY = std::min(int32_t(focus.y + CrumbleRange), map.GetHeight() - 1);
		for (int32_t y = minY; y <= maxY; y++)
			for (int32_t x = minX; x <= maxX; x++)
			{
				const size_t index = size_t(y) * size_t(map.GetWidth()) + size_t(x);
				if (m_Fallen[index] || !IsBrawlFloor(map.GetTerrain(x, y)))
					continue;
				const float collapse = TileCollapseTime(brawl.Plan, x, y);
				if (collapse > brawl.Clock)
					continue;
				m_Fallen[index] = 1;
				if (brawl.Clock - collapse > 0.5f)
					continue;
				const Vec2 seed = CellSeed({ x, y });
				const glm::vec3 center(seed.x, level->Skyborne ? 0.05f : LavaSurface + 0.1f, seed.y);
				if (level->Kind == MapKind::FrozenLake)
				{
					m_Effects->Burst(Fx::IceBreak, glm::vec3(seed.x, LakeIceTop + 0.05f, seed.y), glm::vec3(0.0f, 1.0f, 0.0f), 0.85f);
					for (const PlacedProp& prop : level->Props)
						if (const CellCoord cell = CellAt(prop.Position); prop.Id == PropId::IcePillar && cell.X == x && cell.Y == y)
						{
							m_Effects->Burst(Fx::IceCrumble, glm::vec3(prop.Position.x, prop.Base + 0.9f, prop.Position.y), glm::vec3(0.0f, 1.0f, 0.0f), 2.4f);
							m_Effects->Burst(Fx::FrostBurst, glm::vec3(prop.Position.x, prop.Base + 0.3f, prop.Position.y), glm::vec3(0.0f, 1.0f, 0.0f), 1.3f);
							m_App.GetGameAudio().PlayCrumble(prop.Position, true);
						}
				}
				else if (level->Skyborne)
					m_Effects->Burst(Fx::Crumble, center, glm::vec3(0.0f, 1.0f, 0.0f), 0.5f);
				else
				{
					m_Effects->Burst(Fx::FireBurst, center, glm::vec3(0.0f, 1.0f, 0.0f), 0.55f);
					m_Effects->Burst(Fx::Debris, center + glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.5f);
				}
				if (m_CrumbleCooldown <= 0.0f)
				{
					m_App.GetGameAudio().PlayCrumble(Vec2(center.x, center.z), level->Kind == MapKind::FrozenLake);
					Rumble(center, CrumbleRumble);
					m_CrumbleCooldown = 0.35f;
				}
			}
	}

	void SessionState::DrawHallRing(WorldRenderer& renderer) const
	{
		const Level* level = m_Client.GetLevel();
		if (!level || level->Hall.IsEmpty() || m_Client.GetSession().Mode != GameMode::Brawl)
			return;
		const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
		if (brawl.Phase != BrawlPhase::Fight && brawl.Phase != BrawlPhase::Over)
			return;
		const float clock = m_Client.GetArenaClock(m_Client.GetPresentTick());
		const StormRing ring = RingAt(brawl.Plan, clock);
		if (!ring.Active || ring.Radius <= 0.05f)
			return;
		GroundDecal edge;
		edge.Center = glm::vec3(ring.Center.x, -1.2f, ring.Center.y);
		edge.Extent = glm::vec2(ring.Radius + 0.6f);
		edge.Depth = 4.0f;
		edge.Shape = DecalShape::Ring;
		edge.Color = glm::vec4(0.1f, 0.5f, 0.42f, 1.0f);
		edge.Tint = glm::vec4(0.0f);
		edge.Params = glm::vec4(0.12f, 0.0f, 0.0f, ring.Radius / (ring.Radius + 0.6f));
		edge.Seed = 2;
		renderer.DrawDecal(edge);
	}

	void SessionState::UpdateStorm(const BrawlStatus& brawl)
	{
		const float clock = m_Client.GetArenaClock(m_Client.GetPresentTick());
		std::erase_if(m_BoltMarks, [clock](const BoltMark& mark) { return clock < mark.Time || clock - mark.Time >= ScorchSeconds; });
		const Level* level = m_Client.GetLevel();
		if (level && clock > m_StormClock && clock - m_StormClock < 0.5f && (brawl.Phase == BrawlPhase::Fight || brawl.Phase == BrawlPhase::Over))
		{
			const auto thunder = [&](const glm::vec3& point, float size)
			{
				const float distance = Length(Vec2(point.x, point.z) - Vec2(m_CameraTarget.x, m_CameraTarget.z));
				if (distance < StrikeSight)
				{
					m_Effects->Burst(Fx::LightningBurst, point + glm::vec3(0.0f, 0.3f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.4f * size);
					m_Effects->Burst(Fx::Crumble, point + glm::vec3(0.0f, 0.05f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.4f * size, glm::vec3(0.7f, 0.74f, 0.8f));
				}
				m_Flash = std::max(m_Flash, std::clamp(1.0f - distance / 30.0f, 0.12f, 0.9f) * size);
				Rumble(point, StrikeRumble * size);
				m_App.GetGameAudio().PlayLightning(Vec2(point.x, point.z), distance);
			};
			for (const LightningBolt& bolt : BoltsOf(brawl.Plan))
				if (bolt.Time > m_StormClock && bolt.Time <= clock)
				{
					thunder(Ground(bolt.Center), 1.0f);
					m_BoltMarks.push_back({ bolt.Center, bolt.Time, BoltSeed(bolt) });
				}
			for (const float strike : SpireStrikes(brawl.Plan, m_StormClock, clock))
			{
				thunder(Ground(level->Peak.Center, SpireTop), 0.8f);
				m_RitualPulse = 1.0f;
				m_LastSpireStrike = strike;
			}
		}
		m_StormClock = clock;
	}

	void SessionState::UpdateCaldera(const BrawlStatus& brawl, float dt)
	{
		const bool live = brawl.Phase == BrawlPhase::Fight || brawl.Phase == BrawlPhase::Over;
		const float clock = live ? m_Client.GetArenaClock(m_Client.GetPresentTick()) : 0.0f;
		const ForgeArt& forge = m_LevelRenderer.GetForge();
		const float fury = live ? std::max(CalderaFury(brawl.Plan, clock), forge.IsPouring(clock) ? PourFury : 0.0f) : 0.15f;
		const float rate = HeartRateCalm + (HeartRateFury - HeartRateCalm) * fury;
		const float before = m_HeartPhase;
		m_HeartPhase += dt * rate;
		if (std::floor(m_HeartPhase) > std::floor(before))
		{
			m_App.GetGameAudio().PlayHeartbeat(Vec2(m_CameraTarget.x, m_CameraTarget.z), 0.35f + 0.65f * fury);
			if (fury > 0.55f)
				m_Trauma = std::max(m_Trauma, 0.12f * (fury - 0.55f) / 0.45f);
		}
		if (!live)
		{
			m_CalderaClock = clock;
			return;
		}
		const glm::vec3 foot = forge.PourFoot();
		const Vec2 camera(m_CameraTarget.x, m_CameraTarget.z);
		if (clock > m_CalderaClock && clock - m_CalderaClock < 0.5f)
		{
			for (const Crust& crust : CrustsOf(brawl.Plan))
				if (crust.Time > m_CalderaClock && crust.Time <= clock)
				{
					const glm::vec3 point = Ground(crust.Center) + glm::vec3(0.0f, 0.05f, 0.0f);
					m_Effects->Burst(Fx::GeyserBlast, point, glm::vec3(0.0f, 1.0f, 0.0f), 0.45f);
					m_Effects->Burst(Fx::IceBreak, point, glm::vec3(0.0f, 1.0f, 0.0f), 1.0f);
					m_App.GetGameAudio().PlayCrumble(crust.Center, true);
					m_App.GetGameAudio().PlaySizzle(crust.Center, 1.0f);
				}
			for (uint32_t pour = 0; pour < ForgePourCount(clock); pour++)
				if (const float landing = PourLanding(pour); landing > m_CalderaClock && landing <= clock)
				{
					m_Effects->Burst(Fx::LavaSplash, foot + glm::vec3(0.0f, 0.3f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.6f);
					Rumble(foot, PourRumble);
					m_App.GetGameAudio().PlayPour(Vec2(foot.x, foot.z), Length(Vec2(foot.x, foot.z) - camera));
				}
			if (const float open = forge.GetGateOpen(); open > m_CalderaClock && open <= clock)
			{
				const glm::vec3 gate = forge.GetGatePosition();
				const Vec2 spot(gate.x, gate.z);
				Rumble(gate, GateRumble);
				m_App.GetGameAudio().PlayEruption(spot, Length(spot - camera), true);
			}
			if (!brawl.Plan.Steady)
				for (uint32_t band = 0; band < ForgeBands; band++)
					if (FloodTimes[band] > m_CalderaClock && FloodTimes[band] <= clock)
					{
						const Vec2 rim = ForgeCenter - Vec2(0.0f, 0.5f * (ForgeBandEdges[band] + ForgeBandEdges[band + 1]));
						Rumble(Ground(rim), FloodRumble);
						m_App.GetGameAudio().PlayEruption(rim, Length(rim - camera), true);
					}
			for (const ForgeAnchor& anchor : forge.GetAnchors())
			{
				const Vec2 spot(anchor.Position.x, anchor.Position.z);
				if (LengthSquared(spot - camera) > SizzleRange * SizzleRange)
					continue;
				for (uint32_t pour = 0; pour < ForgePourCount(clock); pour++)
					if (const float front = PourFrontTime(pour, ForgeLocal(spot), anchor.Meters); front > m_CalderaClock && front <= clock)
					{
						m_App.GetGameAudio().PlaySizzle(spot, 1.0f - Length(spot - camera) / SizzleRange);
						m_Effects->Burst(Fx::FireBurst, anchor.Position, glm::vec3(0.0f, 1.0f, 0.0f), 0.3f);
					}
			}
		}
		if (forge.IsPouring(clock))
			m_Effects->Trail(Fx::LavaFountain, 16ull << 40, foot + glm::vec3(0.0f, 0.2f, 0.0f), dt, 0.6f);
		m_CalderaClock = clock;
	}

	void SessionState::DrawPeak(WorldRenderer& renderer) const
	{
		const Level* level = m_Client.GetLevel();
		if (!level || level->Peak.IsEmpty() || m_Client.GetSession().Mode != GameMode::Brawl)
			return;
		const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
		const Plateau& peak = level->Peak;
		const bool live = brawl.Phase == BrawlPhase::Fight || brawl.Phase == BrawlPhase::Over;
		const float clock = live ? m_Client.GetArenaClock(m_Client.GetPresentTick()) : 0.0f;
		const float build = live ? StormBuild(brawl.Plan, clock) : 0.2f;
		const float awake = live ? std::clamp((clock - RingStart + RingWarning) / (RingSettle - RingStart + RingWarning), 0.0f, 1.0f) : 0.0f;
		const glm::vec3 summit = Ground(peak.Center);
		const float glow = 0.16f + 0.25f * build + 0.9f * awake + 1.2f * m_RitualPulse;
		const float charge = std::clamp(0.25f * build + awake + m_RitualPulse, 0.0f, 1.0f);
		const SummitCourse border = SummitBorderCourse();
		DrawSpellMark(renderer, { .Center = summit, .Radius = border.Outer, .Yaw = border.Turn, .Color = RitualBlue, .Intensity = 2.0f * glow, .Progress = charge, .Kind = SpellMarkKind::Ritual, .Seed = level->Seed % 7u + 3u, .Inner = border.Inner, .Cells = uint32_t(border.Count) });
		DrawSpellMark(renderer, { .Center = summit, .Radius = peak.Ritual + 1.65f, .Color = RitualBlue * 0.85f, .Intensity = 1.5f * glow, .Progress = charge, .Kind = SpellMarkKind::Ritual, .Seed = level->Seed % 5u + 11u });
		if (m_RitualPulse > 0.0f)
		{
			const glm::vec3 spire = summit + glm::vec3(0.0f, SpireTop, 0.0f);
			const float age = clock - m_LastSpireStrike;
			const float stroke = StormStroke(age);
			DrawStormBolt(renderer, spire, uint32_t(m_LastSpireStrike * 977.0f), age, SpireBoltScale);
			if (stroke > 0.01f)
			{
				DrawGlow(renderer, spire, 0.4f + 0.9f * stroke, StormGlow * (0.6f * stroke));
				renderer.DrawLight({ .Position = spire, .Radius = 8.0f, .Color = PeakLight * stroke });
			}
		}
		if (!live)
			return;
		for (const LightningBolt& bolt : BoltsOf(brawl.Plan))
		{
			const float until = bolt.Time - clock;
			const glm::vec3 point = Ground(bolt.Center);
			const uint32_t seed = BoltSeed(bolt);
			if (until > 0.0f && until <= BoltWarning)
			{
				const float progress = 1.0f - until / BoltWarning;
				GroundDecal charge;
				charge.Center = point;
				charge.Extent = glm::vec2(BoltRadius / 0.86f);
				charge.Depth = 1.4f;
				charge.Shape = DecalShape::Charge;
				charge.Color = glm::vec4(BoltBlue * (0.7f + 1.5f * progress), 1.0f);
				charge.Tint = glm::vec4(0.04f, 0.06f, 0.11f, 0.7f);
				charge.Roughness = 0.35f;
				charge.Params = glm::vec4(progress, 0.0f, 0.0f, 0.0f);
				charge.Seed = seed;
				renderer.DrawDecal(charge);
				if (until < LeaderSeconds)
					DrawStormLeader(renderer, point, BoltRadius, seed, 1.0f - until / LeaderSeconds, m_Time);
			}
			else if (until <= 0.0f && until > -BoltFlash)
			{
				StormStrikeLook look;
				look.Ground = point;
				look.Radius = BoltRadius;
				look.Age = -until;
				look.Seed = seed;
				look.Scale = PeakBoltScale;
				for (uint32_t i = 0; i < StormGroundArcs; i++)
				{
					const glm::vec2 offset = StormArcOffset(seed, i, BoltRadius);
					look.Ends[i] = Ground(bolt.Center + Vec2(offset.x, offset.y));
				}
				DrawStormStrike(renderer, look, m_Time);
				const float light = 1.3f * std::exp(-look.Age / 0.06f) + 0.9f * StormStroke(look.Age);
				if (light > 0.02f)
					renderer.DrawLight({ .Position = point + glm::vec3(0.0f, 1.4f, 0.0f), .Radius = 7.0f, .Color = PeakLight * light });
			}
		}
		for (const BoltMark& mark : m_BoltMarks)
		{
			const float age = std::max(clock - mark.Time, 0.0f);
			const float fade = 1.0f - age / ScorchSeconds;
			const float heat = std::max(1.0f - age / ScorchHeatSeconds, 0.0f);
			GroundDecal scorch;
			scorch.Center = Ground(mark.Center);
			scorch.Extent = glm::vec2(BoltRadius * 0.85f);
			scorch.Depth = 1.2f;
			scorch.Shape = DecalShape::Scorch;
			scorch.Color = glm::vec4(BoltBlue * 1.4f, 1.0f);
			scorch.Tint = glm::vec4(0.012f, 0.012f, 0.016f, std::min(1.25f * fade * (2.0f - fade), 1.0f));
			scorch.Roughness = 0.98f;
			scorch.Params = glm::vec4(heat * (2.0f - heat) * heat, 0.0f, 0.0f, 0.0f);
			scorch.Seed = mark.Seed;
			renderer.DrawDecal(scorch);
		}
		const StormRing ring = RingAt(brawl.Plan, clock);
		if (!ring.Active || ring.Radius <= 0.05f)
			return;
		const glm::vec3 middle(ring.Center.x, summit.y - 1.5f, ring.Center.y);
		GroundDecal edge;
		edge.Center = middle;
		edge.Extent = glm::vec2(ring.Radius + 0.6f);
		edge.Depth = 4.0f;
		edge.Shape = DecalShape::Ring;
		edge.Color = glm::vec4(StormViolet * 1.4f, 1.0f);
		edge.Tint = glm::vec4(0.0f);
		edge.Params = glm::vec4(0.12f, 0.0f, 0.0f, ring.Radius / (ring.Radius + 0.6f));
		edge.Seed = 2;
		renderer.DrawDecal(edge);
		const Vec2 focus = { m_CameraTarget.x, m_CameraTarget.z };
		const uint32_t beat = uint32_t(m_Time * 3.0f);
		const uint32_t count = std::clamp(uint32_t(2.0f * Pi * ring.Radius / RingSegment), 8u, 256u);
		for (uint32_t k = 0; k < 2; k++)
		{
			const uint32_t pick = (beat * 2654435761u + k * 40503u) % count;
			const float angle = 2.0f * Pi * float(pick) / float(count);
			const Vec2 a = ring.Center + FromAngle(angle) * ring.Radius;
			const Vec2 b = ring.Center + FromAngle(angle + RingSegment * 2.5f / ring.Radius) * ring.Radius;
			if (LengthSquared(a - focus) > 22.0f * 22.0f)
				continue;
			DrawStormJump(renderer, Ground(a, 2.4f), Ground(b, 0.3f), beat * 7919u + k, std::fmod(m_Time * 3.0f, 1.0f) / 3.0f, m_Time);
		}
	}

	void SessionState::DrawBrawlPlayer(WorldRenderer& renderer, const EntityState& entity, const glm::vec3& base) const
	{
		if (entity.Has(EntityFlags::Burning) && InStorm())
		{
			const uint32_t beat = uint32_t(m_Time * StormArcRate);
			const float age = m_Time - float(beat) / StormArcRate;
			for (uint32_t k = 0; k < 2; k++)
			{
				const uint32_t seed = (beat * 2654435761u) ^ (entity.NetId * 40503u) ^ (k * 977u);
				const float angle = float(seed % 6283u) * 0.001f;
				const glm::vec3 foot = base + glm::vec3(std::cos(angle), 0.05f, std::sin(angle)) * 0.55f;
				const glm::vec3 body = base + glm::vec3(0.0f, 0.55f + 0.9f * float((seed >> 13) % 100u) * 0.01f, 0.0f) + glm::vec3(-std::sin(angle), 0.0f, std::cos(angle)) * 0.12f;
				DrawStormJump(renderer, foot, body, seed, age + 0.02f * float(k), m_Time);
			}
			DrawGroundGlow(renderer, base, 0.85f, StormBurnGlow, 0.7f + 0.3f * std::sin(m_Time * 23.0f + float(entity.NetId)));
		}
		else if (entity.Has(EntityFlags::Burning))
		{
			const float flicker = 0.7f + 0.3f * std::sin(m_Time * 17.0f + float(entity.NetId) * 1.3f);
			const MapKind map = m_Client.GetSession().Map;
			const glm::vec3 glow = map == MapKind::FrozenLake ? FrostGlow : map == MapKind::SkyRuins ? HallFieldGlow : LavaGlow;
			DrawGroundGlow(renderer, base, 0.9f, glow, 0.9f * flicker);
			DrawGlow(renderer, base + glm::vec3(0.0f, 0.35f, 0.0f), 0.7f, glow * (0.6f * flicker));
		}
		if (entity.Has(EntityFlags::Ignited))
		{
			const float flicker = 0.65f + 0.35f * std::sin(m_Time * 21.0f + float(entity.NetId) * 0.7f);
			DrawGlow(renderer, base + glm::vec3(0.0f, 0.75f, 0.0f), 0.65f, LavaGlow * (0.75f * flicker));
			DrawGlow(renderer, base + glm::vec3(0.0f, 1.25f, 0.0f), 0.4f, LavaGlow * (0.5f * flicker));
		}
		if (entity.Has(EntityFlags::Marked))
		{
			const float pulse = 0.7f + 0.3f * std::sin(m_Time * 12.0f + float(entity.NetId));
			DrawFloatingRing(renderer, base + glm::vec3(0.0f, 1.95f, 0.0f), 0.32f, MarkGlow * (1.0f * pulse), 0.025f);
			DrawGlow(renderer, base + glm::vec3(0.0f, 1.95f, 0.0f), 0.3f, MarkGlow * (0.2f * pulse));
		}
		if (entity.Has(EntityFlags::BloodShield) && !entity.Has(EntityFlags::Dead))
		{
			const float pulse = 0.75f + 0.25f * std::sin(m_Time * 4.5f + float(entity.NetId) * 0.8f);
			const auto fx = m_Fx.find(entity.NetId);
			const float flash = fx != m_Fx.end() ? fx->second.BloodFlash : 0.0f;
			float strength = 0.85f;
			if (entity.NetId == m_Client.GetPlayerNetId())
			{
				const LocalPlayerState& local = m_Client.GetLocalPlayer();
				const float cap = std::max(float(local.MaxHealth) / (1.0f + BloodMagicHealth) * BloodMagicOverheal, 1.0f);
				strength = 0.45f + 0.55f * std::clamp(float(local.BloodShield) / cap, 0.0f, 1.0f);
			}
			DrawBloodBubble(renderer, base + glm::vec3(0.0f, 0.92f, 0.0f), m_Camera.Eye, 0.98f, BloodShieldGlow * (0.75f + 0.25f * pulse), strength, flash, entity.NetId);
		}
		const bool crescendo = entity.Has(EntityFlags::Archmage);
		const uint32_t weave = crescendo ? ArchmageStacks : entity.NetId == m_Client.GetPlayerNetId() ? m_Client.GetPredictedState().Weave : 0u;
		if (weave > 0)
		{
			const float spin = m_Time * (crescendo ? 3.2f : 1.9f) + float(entity.NetId) * 0.9f;
			for (uint32_t mote = 0; mote < weave; mote++)
			{
				const float angle = spin + 2.0f * Pi * float(mote) / float(ArchmageStacks);
				const glm::vec3 point = base + glm::vec3(0.62f * std::cos(angle), 1.05f + 0.08f * std::sin(angle * 2.0f + m_Time * 3.0f), 0.62f * std::sin(angle));
				DrawGlow(renderer, point, crescendo ? 0.2f : 0.15f, ArchmageGlow * (crescendo ? 1.3f : 0.8f), 0.35f);
			}
		}
		if (crescendo)
		{
			const float pulse = 0.75f + 0.25f * std::sin(m_Time * 9.0f + float(entity.NetId));
			DrawGroundGlow(renderer, base, 0.95f, ArchmageGlow, 0.55f * pulse);
			DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.06f, 0.0f), 0.8f, ArchmageGlow * (0.9f * pulse), 0.03f);
			DrawGlow(renderer, base + glm::vec3(0.0f, 1.05f, 0.0f), 0.8f, ArchmageGlow * (0.12f * pulse), 0.5f);
		}
		if (entity.Has(EntityFlags::Barrier))
			if (const auto fx = m_Fx.find(entity.NetId); fx != m_Fx.end())
				DrawCrystalWard(renderer, base, m_Camera.Eye, fx->second.CrystalForm, m_Time, entity.NetId);
	}

	void SessionState::DrawSkyWarnings()
	{
		const Level* level = m_Client.GetLevel();
		if (!level || !level->Skyborne || m_Client.GetSession().Mode != GameMode::Brawl)
			return;
		const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
		if (brawl.Phase != BrawlPhase::Fight)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const float clock = m_Client.GetArenaClock(m_Client.GetPresentTick());
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (!self || !m_Client.GetPredictedState().Alive || m_Client.GetPredictedState().IsFalling())
			return;
		const float throb = 0.7f + 0.3f * std::sin(m_Time * 7.0f);
		const bool peak = level->Kind == MapKind::StormPeak || level->Kind == MapKind::SkyRuins;
		if (peak && brawl.Phase == BrawlPhase::Fight)
		{
			const StormRing ring = RingAt(brawl.Plan, clock + RingLead);
			if (ring.Active && Length(self->Position - ring.Center) > ring.Radius - 0.6f && !self->Has(EntityFlags::Burning))
			{
				const float y = ui.GetSize().y * 0.15f;
				draw.TextAligned({ 0.0f, y - 12.0f, ui.GetSize().x * 0.5f - 6.0f, 24.0f }, "Get inside the circle", Theme::Bold(18.0f, Hex(0xC9D8FF)), { 1.0f, 0.5f });
				const std::optional<glm::vec2> from = ProjectToScreen(Ground(self->Position));
				const std::optional<glm::vec2> to = ProjectToScreen(Ground(ring.Center));
				if (from && to && glm::length(*to - *from) > 1.0f)
				{
					const glm::vec2 heading = glm::normalize(ui.ToLogical(*to) - ui.ToLogical(*from));
					const glm::vec2 middle = { ui.GetSize().x * 0.5f + 40.0f, y };
					draw.Circle(middle, 26.0f, Rgba(0.05f, 0.06f, 0.16f, 0.45f));
					for (int32_t i = -1; i <= 1; i++)
						draw.Triangle(middle + heading * (float(i) * 12.0f), { 14.0f, 11.0f }, std::atan2(heading.x, -heading.y), Hex(0xC9D8FF, 0.45f + 0.25f * float(i + 1)));
				}
			}
			if (BoltThreat(brawl.Plan, self->Position, clock, 0.2f))
			{
				draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.3f, 0.45f, 0.9f, 0.06f), .Glow = Rgba(0.55f, 0.75f, 1.0f, 0.55f * throb), .GlowSize = 160.0f });
				draw.TextAligned({ 0.0f, ui.GetSize().y * 0.2f, ui.GetSize().x, 24.0f }, "Lightning! Move", Theme::Bold(18.0f, Hex(0xCFE4FF)), { 0.5f, 0.5f });
				return;
			}
		}
		const float left = GroundLeft(*level, brawl.Plan, clock, self->Position);
		if (left <= 0.0f || left > CollapseWarning)
			return;
		draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.45f, 0.6f, 0.9f, 0.05f), .Glow = Rgba(0.8f, 0.9f, 1.0f, 0.45f * throb), .GlowSize = 160.0f });
		const char* warning = peak ? "The ground is giving way! Move" : TermsFor(level->Kind).Danger;
		if (UsesForge(*level))
			warning = InTrench(ForgeLocal(self->Position)) && !OnBridge(ForgeLocal(self->Position)) && ForgeL1(ForgeLocal(self->Position)) < FloodLine(clock + left + 0.05f) ? "Molten metal is coming! Get out of the trench" : "The forge is flooding! Move inward";
		draw.TextAligned({ 0.0f, ui.GetSize().y * 0.2f, ui.GetSize().x, 24.0f }, warning, Theme::Bold(18.0f, Hex(0xEAF5FF)), { 0.5f, 0.5f });
	}

	void SessionState::DrawLavaWarning()
	{
		if (m_Client.GetSession().Mode != GameMode::Brawl || !m_Client.GetPredictedState().Alive)
			return;
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (!self || !self->Has(EntityFlags::Burning))
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const float throb = 0.7f + 0.3f * std::sin(m_Time * 7.0f);
		const MapKind map = m_Client.GetSession().Map;
		if (map == MapKind::FrozenLake)
			draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.0f, 0.08f, 0.2f, 0.08f), .Glow = Rgba(0.35f, 0.75f, 1.0f, 0.5f * throb), .GlowSize = 160.0f });
		else if (map == MapKind::SkyRuins)
			draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.0f, 0.1f, 0.1f, 0.04f), .Glow = Rgba(0.15f, 0.8f, 0.68f, 0.45f * throb), .GlowSize = 160.0f });
		else if (map == MapKind::StormPeak)
			draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.05f, 0.04f, 0.18f, 0.12f), .Glow = Rgba(0.45f, 0.5f, 1.0f, 0.55f * throb), .GlowSize = 160.0f });
		else
			draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.4f, 0.08f, 0.0f, 0.07f), .Glow = Rgba(1.0f, 0.3f, 0.05f, 0.55f * throb), .GlowSize = 160.0f });
		draw.TextAligned({ 0.0f, ui.GetSize().y * 0.2f, ui.GetSize().x, 24.0f }, TermsFor(map).Danger, Theme::Bold(18.0f, map == MapKind::FrozenLake ? Hex(0xA8E4FF) : map == MapKind::StormPeak ? Hex(0xC9D8FF) : Hex(0xFFC08A)), { 0.5f, 0.5f });
	}

	void SessionState::UpdateBrawlFeedback()
	{
		const GameStatus& status = m_Client.GetStatus();
		const BrawlStatus& brawl = status.Brawl;
		GameAudio& audio = m_App.GetGameAudio();
		if (brawl.Phase == BrawlPhase::Fight && m_Client.GetPredictedState().Alive)
			m_FoughtRound = brawl.Round;
		if (status.Match == MatchPhase::Ended)
			return;
		if (brawl.Phase == BrawlPhase::Countdown)
		{
			const int32_t second = int32_t(std::ceil(brawl.Timer));
			if (second != m_SeenCountdown && second > 0 && second <= 3)
				audio.PlayEvent(AudioEvent::Countdown);
			m_SeenCountdown = second;
		}
		if (brawl.Phase == m_SeenBrawlPhase && brawl.Round == m_SeenRound)
			return;
		if (brawl.Phase == BrawlPhase::Countdown && m_SeenBrawlPhase != BrawlPhase::Countdown)
			for (auto& [netId, fx] : m_Fx)
				if (fx.Last.Kind == EntityKind::Player)
					fx.SpawnAge = 0.0f;
		const uint32_t self = m_Client.GetPlayerId();
		const bool teams = m_Client.GetSession().Rules.Teams >= 2;
		switch (brawl.Phase)
		{
		case BrawlPhase::Fight:
			m_BannerTitle = Format("Round %u", unsigned(brawl.Round));
			if (teams)
				m_BannerText = m_Client.GetSession().Rules.Mode == BrawlMode::Damage ? (brawl.Round == brawl.Rounds ? "The last round. Be the last team standing" : "Be the last team standing") : Format(brawl.Round == brawl.Rounds ? "The last round. Shove the other teams into the %s" : "Shove the other teams into the %s", TermsFor(status.Map).Hazard);
			else if (m_Client.GetSession().Rules.Mode == BrawlMode::Damage)
				m_BannerText = brawl.Round == brawl.Rounds ? "The last round. Outlast them all" : "Outlast them all";
			else
				m_BannerText = Format(brawl.Round == brawl.Rounds ? "The last round. Shove them all into the %s" : "Shove them into the %s", TermsFor(status.Map).Hazard);
			m_BannerColor = Hex(0xFF8A4A);
			m_BannerTime = 2.6f;
			audio.PlayEvent(AudioEvent::FightStart);
			break;
		case BrawlPhase::Over:
		{
			const bool won = WonBrawlRound(self);
			if (teams)
			{
				const bool nobody = brawl.RoundTeam >= MaxTeams;
				m_BannerTitle = won ? "Round Won" : nobody ? "No Survivors" : std::string(GetTeamName(brawl.RoundTeam)) + " Team";
				m_BannerText = won ? Format("Your team takes round %u", unsigned(brawl.Round)) : nobody ? Format("The %s took everyone", TermsFor(status.Map).Place) : Format("takes round %u", unsigned(brawl.Round));
				m_BannerColor = won ? Hex(0xFFD36A) : nobody ? Hex(0xC8C0B4) : Lighten(WithAlpha(TeamColor(brawl.RoundTeam)), 0.35f);
			}
			else
			{
				m_BannerTitle = won ? "Round Won" : brawl.RoundWinner == NoPlayer ? "No Survivors" : PlayerName(brawl.RoundWinner);
				m_BannerText = won ? Format("+%u points", unsigned(WinPoints(m_Client.GetSession().Rules.Mode))) : brawl.RoundWinner == NoPlayer ? Format("The %s took everyone", TermsFor(status.Map).Place) : Format("takes round %u", unsigned(brawl.Round));
				m_BannerColor = won ? Hex(0xFFD36A) : Hex(0xC8C0B4);
			}
			m_BannerTime = 3.5f;
			audio.PlayEvent(won ? AudioEvent::RoundWon : AudioEvent::RoundLost);
			break;
		}
		case BrawlPhase::Pick:
			m_LoadoutOpen = true;
			m_ChosenCard.reset();
			m_CardReveal = 0.0f;
			m_PickingSlot.reset();
			m_SelectedSpell.reset();
			audio.PlayUi(Cue::UiOpen, 0.8f);
			break;
		default:
			break;
		}
		m_SeenBrawlPhase = brawl.Phase;
		m_SeenRound = brawl.Round;
	}

	void SessionState::DrawBrawlStatus()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const GameStatus& status = m_Client.GetStatus();
		const BrawlStatus& brawl = status.Brawl;
		const uint32_t self = m_Client.GetPlayerId();
		const float width = ui.GetSize().x;
		const float center = width * 0.5f;
		std::string subtitle;
		UiColor subtitleColor = Theme::Text;
		if (status.Match == MatchPhase::Ended)
		{
			std::string outcome;
			subtitleColor = DescribeOutcome(outcome, subtitle) ? Theme::Success : Theme::Danger;
			subtitle = outcome + ". " + subtitle;
		}
		else
			switch (brawl.Phase)
			{
			case BrawlPhase::Draft:
				subtitle = brawl.Players < 2 ? std::string("Ready check, the clock starts when a rival joins") : "Ready check, the first round starts in " + Seconds(brawl.Timer);
				subtitleColor = Theme::AccentLight;
				break;
			case BrawlPhase::Countdown:
				subtitle = Format("Round %u of %u", unsigned(brawl.Round + 1), unsigned(brawl.Rounds));
				subtitleColor = Theme::AccentLight;
				break;
			case BrawlPhase::Fight:
				subtitle = Format("Round %u of %u   %u of %u still standing", unsigned(brawl.Round), unsigned(brawl.Rounds), unsigned(status.Alive), unsigned(status.Entrants));
				break;
			case BrawlPhase::Over:
				if (m_Client.GetSession().Rules.Teams >= 2)
					subtitle = brawl.RoundTeam >= MaxTeams ? Format("Nobody survived round %u", unsigned(brawl.Round)) : WonBrawlRound(self) ? Format("Your team won round %u", unsigned(brawl.Round)) : Format("The %s team takes round %u", GetTeamName(brawl.RoundTeam), unsigned(brawl.Round));
				else
					subtitle = brawl.RoundWinner == NoPlayer ? Format("Nobody survived round %u", unsigned(brawl.Round)) : brawl.RoundWinner == self ? Format("You won round %u", unsigned(brawl.Round)) : PlayerName(brawl.RoundWinner) + Format(" takes round %u", unsigned(brawl.Round));
				subtitleColor = Theme::Gold;
				break;
			case BrawlPhase::Pick:
				subtitle = Format("Round %u of %u starts in %s", unsigned(brawl.Round + 1), unsigned(brawl.Rounds), Seconds(brawl.Timer).c_str());
				subtitleColor = Theme::AccentLight;
				break;
			}
		TextStyle titleStyle = Theme::Title(28.0f);
		titleStyle.Tracking = 2.0f;
		const char* title = "Spellbrawl";
		const float titleWidth = draw.MeasureWidth(title, titleStyle);
		draw.Shadow(UiRect::Centered({ center, 42.0f }, { titleWidth + 160.0f, 52.0f }), 26.0f, 30.0f, Rgba(0.0f, 0.0f, 0.0f, 0.55f), { 0.0f, 0.0f });
		draw.TextAligned({ 0.0f, 16.0f, width, 34.0f }, title, titleStyle, { 0.5f, 0.5f });
		DrawTitleOrnament(center, 32.0f, titleWidth);
		draw.TextAligned({ 0.0f, 54.0f, width, 22.0f }, subtitle, Theme::Floating(16.0f, subtitleColor, UiFont::Body), { 0.5f, 0.5f });
		const uint8_t teams = m_Client.GetSession().Rules.Teams;
		float pillY = 97.0f;
		if (teams >= 2)
		{
			const uint8_t mine = m_Client.GetTeam(self);
			constexpr float Chip = 112.0f;
			constexpr float Gap = 8.0f;
			const float total = float(teams) * Chip + float(teams - 1) * Gap;
			for (uint8_t team = 0; team < teams; team++)
			{
				const UiRect box{ center - total * 0.5f + float(team) * (Chip + Gap), 80.0f, Chip, 26.0f };
				const UiColor color = WithAlpha(TeamColor(team));
				draw.Rect(box, { .Fill = WithAlpha(Darken(color, 0.7f), 0.85f), .Border = team == mine ? Lighten(color, 0.35f) : WithAlpha(color, 0.6f), .BorderWidth = team == mine ? 1.8f : 1.0f, .Chamfer = 7.0f });
				draw.TextAligned(box.Shrink(10.0f, 0.0f), GetTeamName(team), Theme::Bold(14.0f, Lighten(color, 0.45f)), { 0.0f, 0.5f });
				draw.TextAligned(box.Shrink(10.0f, 0.0f), std::to_string(status.TeamScores[team]), Theme::Title(19.0f), { 1.0f, 0.5f });
			}
			pillY += 30.0f;
		}
		if (status.Match == MatchPhase::Ended)
			return;

		std::string line;
		UiColor color = status.Map == MapKind::FrozenLake ? Hex(0x9BE7FF) : Hex(0xFFB27A);
		if (m_Client.IsSpectating())
		{
			line = !m_Client.GetPredictedState().Alive && m_Client.GetLocalPlayer().Watching != NoPlayer ? "Spectating " + PlayerName(m_Client.GetLocalPlayer().Watching) + ", click to switch" : std::string("Spectating");
			color = Theme::Astral;
		}
		else if (IsChoosing(status))
		{
			line = Format("%u of %u ready", unsigned(brawl.Ready), unsigned(std::max<uint8_t>(brawl.Players, 1)));
			if (!m_LoadoutOpen)
				line += "   I opens your spellbook";
			color = Theme::AccentLight;
		}
		else if (brawl.Phase == BrawlPhase::Fight && brawl.Plan.Shape == BrawlShape::Caldera)
		{
			const BrawlTerms& terms = TermsFor(status.Map);
			const bool steady = brawl.Plan.Steady;
			const uint32_t pours = ForgePourCount(brawl.Clock);
			const auto seconds = [](float time)
			{
				const std::string left = Seconds(time);
				return left + (left == "1" ? " second" : " seconds");
			};
			if (!steady && brawl.Clock >= FloodTimes[ForgeBands - 1])
			{
				line = terms.Sudden;
				color = Hex(0xFF7A5A);
			}
			else if (pours > 0 && brawl.Clock < PourLanding(pours - 1))
			{
				line = "The crucible tips! Get out of the trench";
				color = Hex(0xFFB35A);
			}
			else if (!steady && brawl.Clock >= FloodTimes[0] - FloodWarning)
				line = terms.Breaking;
			else if (pours == 0)
				line = "The crucible pours in " + seconds(ForgePourAt(0).Tilt + PourTiltSeconds - brawl.Clock);
			else
				line = steady ? "The forge holds its level this match" : "The forge overflows in " + seconds(FloodTimes[0] - brawl.Clock);
		}
		else if (brawl.Phase == BrawlPhase::Fight && brawl.Plan.Steady)
			line = brawl.Plan.Shape == BrawlShape::Peak ? "The storm holds off this match" : brawl.Plan.Shape == BrawlShape::Ruins ? "The core's field holds off this match" : std::string("The ") + TermsFor(status.Map).Floor + " holds this match";
		else if (brawl.Phase == BrawlPhase::Fight)
		{
			const float final = FinalCollapse(brawl.Plan);
			const BrawlTerms& terms = TermsFor(status.Map);
			const bool peak = brawl.Plan.Shape == BrawlShape::Peak;
			const bool hallRing = brawl.Plan.Shape == BrawlShape::Ruins;
			const float grace = hallRing ? HallRingStart : peak ? RingStart : CollapseGrace;
			if (peak && brawl.Clock < RingStart - RingWarning)
				line = "Lightning strikes where the ground crackles";
			else if (brawl.Clock < grace)
			{
				const std::string left = Seconds(grace - brawl.Clock);
				line = hallRing ? "The core's field closes in " + left + (left == "1" ? " second" : " seconds") : peak ? "The storm closes in " + left + (left == "1" ? " second" : " seconds") : std::string("The ") + terms.Floor + " holds for " + left + (left == "1" ? " more second" : " more seconds");
			}
			else if (brawl.Clock < final)
				line = terms.Breaking;
			else
			{
				line = terms.Sudden;
				color = Hex(0xFF7A5A);
			}
		}
		if (!line.empty())
		{
			const TextStyle style = Theme::Floating(15.0f, color, UiFont::Bold);
			const float lineWidth = draw.MeasureWidth(line, style) + 54.0f;
			const UiRect pill = UiRect::Centered({ center, pillY }, { lineWidth, 28.0f });
			const bool frozen = status.Map == MapKind::FrozenLake;
			const bool watching = m_Client.IsSpectating();
			draw.Rect(pill, { .Fill = watching ? Rgba(0.04f, 0.06f, 0.12f, 0.8f) : frozen ? Rgba(0.03f, 0.08f, 0.16f, 0.78f) : Rgba(0.16f, 0.05f, 0.03f, 0.78f), .Border = WithAlpha(color, 0.7f), .BorderWidth = 1.0f, .Chamfer = 8.0f });
			draw.Icon(watching ? UiIcon::Eye : IsChoosing(status) ? UiIcon::ScrollUnfurled : frozen ? UiIcon::Snowflake : status.Map == MapKind::StormPeak ? UiIcon::LightningArc : UiIcon::Fireball, { pill.X + 10.0f, pill.Center().y - 9.0f, 18.0f, 18.0f }, { .Color = color });
			draw.TextAligned({ pill.X + 34.0f, pill.Y, pill.W - 44.0f, pill.H }, line, style, { 0.0f, 0.5f });
		}
		if (brawl.Phase == BrawlPhase::Countdown && brawl.Timer > 0.0f)
		{
			const float fraction = brawl.Timer - std::floor(brawl.Timer);
			TextStyle digits = Theme::Title(96.0f * (0.8f + 0.35f * fraction));
			digits.GlowColor = WithAlpha(Hex(0xFF8A4A), 0.8f);
			digits.GlowBlur = 18.0f;
			digits.Color = Hex(0xFFF2D8);
			digits.ColorBottom = Hex(0xFF9A4A);
			draw.TextAligned({ 0.0f, std::max(ui.GetSize().y * 0.12f, 84.0f), width, 110.0f }, Seconds(brawl.Timer), digits, { 0.5f, 0.5f });
		}
	}

	void SessionState::UpdateCardNews(float dt)
	{
		for (CardNews& news : m_CardNews)
			news.Age += dt;
		std::erase_if(m_CardNews, [](const CardNews& news) { return news.Age > CardNewsSeconds; });
		for (CardNews& news : m_ApexNews)
			news.Age += dt;
		std::erase_if(m_ApexNews, [](const CardNews& news) { return news.Age > ApexNewsSeconds; });
		const uint32_t self = m_Client.GetPlayerId();
		const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
		const bool drafting = brawl.Phase == BrawlPhase::Draft || (brawl.Phase == BrawlPhase::Countdown && brawl.Round == 0);
		for (const PlayerInfo& player : m_Client.GetPlayers())
		{
			if (player.PlayerId >= MaxPlayers)
				continue;
			const CardMask fresh = player.Cards & ~m_KnownCards[player.PlayerId];
			if (m_CardsKnown)
				for (uint32_t bit = 0; bit < uint32_t(CardId::Count); bit++)
				{
					if (!HasCard(fresh, CardId(bit)))
						continue;
					if (IsApex(CardId(bit)) && !drafting)
					{
						m_ApexNews.push_back({ player.PlayerId, CardId(bit), 0.0f });
						m_App.GetGameAudio().PlayEvent(AudioEvent::ApexClaimed);
					}
					if (player.PlayerId == self)
						continue;
					m_CardNews.push_back({ player.PlayerId, CardId(bit), 0.0f });
					if (!IsApex(CardId(bit)))
						m_App.GetGameAudio().PlayUi(Cue::UiNotify, 0.45f);
				}
			m_KnownCards[player.PlayerId] = player.Cards;
		}
		m_CardsKnown = true;
	}

	void SessionState::TakeCard(uint32_t choice, CardId card)
	{
		m_Client.ChooseCard(choice);
		m_ChosenCard = choice;
		m_TakenAge = 0.0f;
		if (!IsApex(card))
			m_App.GetGameAudio().PlayUi(GetCard(card).Rarity == CardRarity::Common ? Cue::UiReward : Cue::UiRare);
	}

	void SessionState::DrawApexNews()
	{
		if (m_ApexNews.empty() || m_Client.GetSession().Mode != GameMode::Brawl)
			return;
		const CardNews& news = m_ApexNews.back();
		const CardDef& card = GetCard(news.Card);
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const float fade = std::clamp(news.Age / 0.25f, 0.0f, 1.0f) * std::clamp((ApexNewsSeconds - news.Age) / 0.25f, 0.0f, 1.0f);
		const float solid = std::min(1.0f, fade * 2.0f);
		const UiColor apex = WithAlpha(ApexColor(m_Time), fade);
		const UiRect panel = UiRect::Centered({ ui.GetSize().x * 0.5f, 66.0f + 10.0f * (1.0f - fade) }, { 720.0f, 104.0f });
		draw.Shadow(panel, 22.0f, 28.0f, WithAlpha(apex, 0.4f * fade), { 0.0f, 0.0f });
		draw.Rect(panel, { .Fill = Rgba(0.1f, 0.05f, 0.16f, solid), .FillBottom = Rgba(0.04f, 0.02f, 0.07f, solid), .Border = apex, .BorderWidth = 2.0f, .Glow = WithAlpha(apex, 0.22f * fade), .GlowSize = 22.0f, .Chamfer = 14.0f, .Stars = 0.5f * fade });
		draw.Rect(panel.Shrink(5.0f), { .Border = WithAlpha(apex, 0.35f * fade), .BorderWidth = 1.0f, .Chamfer = 11.0f });
		const UiRect badge = UiRect::Centered({ panel.X + 56.0f, panel.Center().y }, { 64.0f, 64.0f });
		draw.Diamond(badge.Center(), 74.0f, Rgba(0.06f, 0.03f, 0.1f, 0.95f * fade), apex, 1.6f);
		draw.Icon(IconFor(news.Card), badge.Shrink(12.0f), { .Color = WithAlpha(Lighten(apex, 0.5f), fade), .ColorBottom = apex, .Outline = 1.0f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f * fade) });
		UiRect text{ panel.X + 104.0f, panel.Y + 10.0f, panel.W - 120.0f, panel.H - 20.0f };
		const bool you = news.PlayerId == m_Client.GetPlayerId();
		TextStyle claim = Theme::Bold(12.5f, WithAlpha(you ? Theme::GoldLight : Lighten(WithAlpha(ColorOf(news.PlayerId)), 0.3f), fade));
		claim.Tracking = 1.6f;
		draw.TextAligned(text.CutTop(16.0f), Upper(you ? std::string("You claimed an apex") : PlayerName(news.PlayerId) + " claimed an apex"), claim, { 0.0f, 0.5f });
		TextStyle name = Theme::Title(24.0f);
		name.Color = WithAlpha(Lighten(apex, 0.35f), fade);
		name.ColorBottom = apex;
		draw.TextAligned(text.CutTop(30.0f), card.Name, name, { 0.0f, 0.5f });
		draw.TextWrapped(text, CardText(card, BrawlMode::Damage), Theme::Body(13.0f, WithAlpha(Theme::Muted, fade)), 0.0f, 1.0f);
	}

	void SessionState::DrawCardFace(const UiRect& rect, CardId id, float hover, float fade, std::optional<uint32_t> key, bool taken, float reveal, float burst, float burnAway)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const CardDef& card = GetCard(id);
		const bool apex = card.Rarity == CardRarity::Apex;
		const bool legendary = card.Rarity == CardRarity::Legendary;
		const UiColor rarity = apex ? WithAlpha(ApexColor(m_Time)) : RarityColor(card.Rarity);
		const UiColor tint = apex ? rarity : CardTint(card);
		const float lift = taken ? 1.0f : hover;
		const UiRect face = rect.Translate({ 0.0f, -8.0f * lift });
		const bool small = face.W < 200.0f;
		UiRect body = face.Shrink(18.0f, 16.0f);
		const UiRect tagRect = body.CutTop(18.0f);
		const float iconSize = std::clamp(face.H * 0.24f, small ? 44.0f : 52.0f, 80.0f);
		body.CutTop(face.H * 0.04f);
		const UiRect iconRect = UiRect::Centered({ body.Center().x, body.Y + iconSize * 0.5f + 6.0f }, { iconSize, iconSize });
		const float margin = burst > 0.0f ? 120.0f : legendary ? 58.0f : apex ? 52.0f : card.Rarity == CardRarity::Rare ? 36.0f : 28.0f;
		draw.Card(face, { .Color = rarity, .Rarity = uint32_t(card.Rarity), .Margin = margin, .Hover = lift, .Fade = fade, .Taken = burst, .Reveal = reveal, .Dissolve = burnAway, .Mouse = ui.GetMouse(), .MedallionY = iconRect.Center().y, .MedallionRadius = iconSize * 0.68f, .Seed = float(id) * 0.6180339f });
		fade *= Smooth(0.0f, 0.35f, reveal) * (1.0f - Smooth(0.0f, 0.55f, burnAway));
		TextStyle tagStyle = Theme::Bold(12.0f, WithAlpha(apex || legendary ? Lighten(rarity, 0.25f) : rarity, fade));
		tagStyle.Tracking = apex ? 4.0f : 1.8f;
		const std::string tag = card.Kind == CardKind::Upgrade ? "UPGRADE  \xC2\xB7  " + Upper(GetAbility(card.Spell).Name) : apex ? std::string("APEX") : std::string("WILD CARD");
		draw.TextAligned(tagRect, tag, tagStyle, { 0.5f, 0.5f });
		draw.Icon(IconFor(id), iconRect, { .Color = WithAlpha(Lighten(tint, 0.55f), fade), .ColorBottom = WithAlpha(tint, fade), .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f * fade), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f * fade), .GlowColor = WithAlpha(tint, (apex || legendary ? 0.35f : 0.18f) * fade) });
		body.CutTop(iconSize + 22.0f);
		TextStyle nameStyle = Theme::Title(small ? 17.0f : 21.0f);
		nameStyle.Color = WithAlpha(Theme::Text, fade);
		nameStyle.ColorBottom = WithAlpha(nameStyle.ColorBottom, fade);
		nameStyle.GlowColor = WithAlpha(nameStyle.GlowColor, fade);
		nameStyle.ShadowColor = WithAlpha(nameStyle.ShadowColor, fade);
		if (apex)
		{
			nameStyle.Color = WithAlpha(Rgba(1.0f, 0.96f, 0.98f), fade);
			nameStyle.ColorBottom = WithAlpha(Lighten(rarity, 0.3f), fade);
			nameStyle.GlowColor = WithAlpha(rarity, 0.45f * fade);
			nameStyle.Shimmer = 0.8f;
		}
		else if (legendary)
		{
			nameStyle.Color = WithAlpha(Theme::GoldLight, fade);
			nameStyle.ColorBottom = WithAlpha(Theme::Gold, fade);
			nameStyle.GlowColor = WithAlpha(Theme::Gold, 0.4f * fade);
			nameStyle.Shimmer = 0.6f;
		}
		else if (card.Rarity == CardRarity::Rare)
		{
			nameStyle.ColorBottom = WithAlpha(Lighten(rarity, 0.45f), fade);
			nameStyle.GlowColor = WithAlpha(rarity, 0.3f * fade);
		}
		const bool run = m_Client.GetSession().Mode == GameMode::Shardfall;
		const RunLoadout& held = m_Client.GetLocalPlayer().Run;
		const uint8_t stack = run && IsStackable(id) ? StackOf(held.Cards, held.Second, held.Third, id) : uint8_t(0);
		constexpr const char* StackNames[] = { "", " II", " III" };
		draw.TextAligned(body.CutTop(small ? 22.0f : 28.0f), std::string(card.Name) + (stack > 0 && stack < MaxCardStack ? StackNames[stack] : ""), nameStyle, { 0.5f, 0.5f });
		body.CutTop(small ? 4.0f : 8.0f);
		const UiRect footer = body.CutBottom(18.0f);
		draw.TextWrapped(body, run ? RunCardDescription(id) : CardText(card, ShownBrawlMode()), Theme::Body(small ? 12.0f : 14.5f, WithAlpha(Theme::Muted, fade)), 0.5f, 1.05f);
		TextStyle footStyle = Theme::Bold(11.5f, WithAlpha(rarity, 0.9f * fade));
		footStyle.Tracking = 1.4f;
		if (taken)
			draw.TextAligned(footer, "TAKEN", Theme::Bold(12.5f, WithAlpha(Theme::Success, fade)), { 0.5f, 0.5f });
		else
		{
			std::vector<std::pair<std::string, TextStyle>> parts{ { Upper(RarityName(card.Rarity)), footStyle } };
			for (size_t tag = 0; tag < size_t(CardTag::Count); tag++)
				if (card.Tags & TagBit(CardTag(tag)))
				{
					TextStyle tagStyle = footStyle;
					tagStyle.Color = WithAlpha(TagColor(CardTag(tag)), 0.9f * fade);
					parts.push_back({ "\xC2\xB7", Theme::Bold(11.5f, WithAlpha(Theme::Faint, fade)) });
					parts.push_back({ Upper(CardTagNames[tag]), tagStyle });
				}
			constexpr float Gap = 7.0f;
			float width = -Gap;
			for (const auto& [text, style] : parts)
				width += draw.MeasureWidth(text, style) + Gap;
			float x = footer.Center().x - width * 0.5f;
			for (const auto& [text, style] : parts)
			{
				const float advance = draw.MeasureWidth(text, style);
				draw.TextAligned({ x, footer.Y, advance, footer.H }, text, style, { 0.0f, 0.5f });
				x += advance + Gap;
			}
		}
		if (key)
		{
			const UiRect badge{ face.Right() - 32.0f, face.Y + 10.0f, 22.0f, 20.0f };
			draw.Rect(badge, { .Fill = Rgba(0.04f, 0.035f, 0.07f, 0.95f * fade), .Border = WithAlpha(Theme::Edge, 0.9f * fade), .BorderWidth = 1.0f, .Chamfer = 4.0f });
			draw.TextAligned(badge, std::to_string(*key + 1), Theme::Bold(12.5f, WithAlpha(Theme::AccentLight, fade)), { 0.5f, 0.5f });
		}
	}

	void SessionState::DrawCardRow(UiRect area, CardMask cards, float size, const char* empty)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		if (cards == 0)
		{
			if (empty)
				draw.TextAligned(area, empty, Theme::Body(13.5f, Theme::Faint), { 0.0f, 0.5f });
			return;
		}
		const float count = float(CountCards(cards));
		size = std::max(std::min(size, (area.W + 6.0f) / count - 6.0f), 16.0f);
		for (const CardDef& card : Cards)
		{
			if (!HasCard(cards, card.Id) || area.W < size)
				continue;
			const UiRect tile = UiRect::Centered({ area.X + size * 0.5f, area.Center().y }, { size, size });
			area.CutLeft(size + 6.0f);
			const bool apex = card.Rarity == CardRarity::Apex;
			const UiColor rarity = apex ? WithAlpha(ApexColor(m_Time)) : RarityColor(card.Rarity);
			const UiColor tint = apex ? rarity : CardTint(card);
			draw.Rect(tile, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Border = WithAlpha(rarity, apex ? 1.0f : 0.85f), .BorderWidth = apex ? 1.8f : 1.2f, .Glow = WithAlpha(rarity, apex ? 0.35f : 0.0f), .GlowSize = 10.0f, .Chamfer = 6.0f });
			draw.Icon(IconFor(card.Id), tile.Shrink(size * 0.17f), { .Color = Lighten(tint, 0.5f), .ColorBottom = tint, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			if (ui.Interact(Ui::MakeId("ownedcard", uint64_t(card.Id) + uint64_t(size * 1000.0f)), tile).Hovered)
				DescribeCard(ui.Tooltip(), card.Id, ShownBrawlMode());
		}
	}

	void SessionState::ShowCardTooltip(CardId card)
	{
		DescribeCard(m_App.GetUi().Tooltip(), card, ShownBrawlMode());
	}

	void SessionState::DrawSpellStrip(const UiRect& area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const BrawlSpells& spells = m_Client.GetPreferredSpells();
		ui.Panel(area, 12.0f);
		UiRect content = area.Shrink(16.0f, 10.0f);
		TextStyle labelStyle = Theme::Bold(12.0f, Theme::Muted);
		labelStyle.Tracking = 1.6f;
		draw.TextAligned(content.CutTop(16.0f), m_SelectedSpell ? "NOW CLICK A KEY BELOW, OR DRAG THE SPELL ONTO IT" : "SPELLBOOK  \xC2\xB7  DRAG A SPELL ONTO RMB, Q OR R BELOW", labelStyle, { 0.5f, 0.5f });
		content.CutTop(8.0f);
		constexpr float Gap = 10.0f;
		const float count = float(std::size(BrawlSpellPool));
		const float size = std::min({ content.H, 58.0f, (content.W - (count - 1.0f) * Gap) / count });
		const float total = float(std::size(BrawlSpellPool)) * size + float(std::size(BrawlSpellPool) - 1) * Gap;
		float x = content.Center().x - total * 0.5f;
		for (size_t i = 0; i < std::size(BrawlSpellPool); i++)
		{
			const AbilityId spell = BrawlSpellPool[i];
			const UiRect tile{ x, content.Y, size, size };
			x += size + Gap;
			const UiId id = Ui::MakeId("stripspell", i);
			const UiInteraction interaction = ui.Interact(id, tile);
			ui.OfferDrag(id, SpellPayload, int(spell));
			const auto equipped = std::find(spells.begin(), spells.end(), spell);
			const bool owned = equipped != spells.end();
			const bool selected = m_SelectedSpell == spell;
			const float hover = ui.Animate(id, interaction.Hovered || selected);
			if (interaction.Clicked)
				m_SelectedSpell = selected ? std::nullopt : std::optional<AbilityId>(spell);
			const UiColor color = WithAlpha(SkillColor(spell));
			draw.Rect(tile.Translate({ 0.0f, -3.0f * hover }), { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Border = selected ? Theme::Astral : WithAlpha(color, owned ? 0.95f : 0.45f + 0.4f * hover), .BorderWidth = selected ? 2.2f : 1.3f, .Glow = WithAlpha(color, owned ? 0.18f : 0.08f * hover), .GlowSize = 12.0f, .Chamfer = 9.0f });
			draw.Icon(IconFor(spell), tile.Translate({ 0.0f, -3.0f * hover }).Shrink(size * 0.16f), { .Color = Lighten(color, 0.5f), .ColorBottom = color, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			if (owned)
			{
				const char* key = SpellKeys[equipped - spells.begin()];
				const TextStyle keyStyle = Theme::Bold(10.5f, Theme::GoldLight);
				const UiRect badge{ tile.Right() - draw.MeasureWidth(key, keyStyle) - 12.0f, tile.Bottom() - 15.0f, draw.MeasureWidth(key, keyStyle) + 9.0f, 14.0f };
				draw.Rect(badge, { .Fill = Rgba(0.08f, 0.06f, 0.02f, 0.94f), .Border = WithAlpha(Theme::Gold, 0.9f), .BorderWidth = 1.0f, .Chamfer = 3.0f });
				draw.TextAligned(badge, key, keyStyle, { 0.5f, 0.5f });
			}
			if (interaction.Hovered && !ui.GetDrag())
				DescribeSpell(ui.Tooltip(), spell, ShownBrawlMode());
		}
	}

	bool SessionState::AcceptSpellDrop(const UiRect& rect, uint32_t slot, bool clicked)
	{
		Ui& ui = m_App.GetUi();
		const BrawlSpells& spells = m_Client.GetPreferredSpells();
		std::optional<AbilityId> spell;
		if (const std::optional<int> dropped = ui.AcceptDrop(rect, SpellPayload))
			spell = AbilityId(*dropped);
		else if (const std::optional<int> moved = ui.AcceptDrop(rect, KeyPayload); moved && uint32_t(*moved) < BrawlSlots)
			spell = spells[size_t(*moved)];
		else if (clicked && m_SelectedSpell)
			spell = m_SelectedSpell;
		if (!spell || slot >= BrawlSlots)
			return false;
		if (spells[slot] != *spell)
		{
			m_Client.PreferSpell(slot, *spell);
			m_App.GetGameAudio().PlayUi(Cue::UiConfirm, 0.7f);
		}
		m_SelectedSpell.reset();
		m_PickingSlot.reset();
		return true;
	}

	bool SessionState::CanEditSpells() const
	{
		const SessionInfo& session = m_Client.GetSession();
		return (session.Mode == GameMode::Brawl && IsChoosing(m_Client.GetStatus()) && !session.Rules.RandomSpells) || (session.Mode == GameMode::Hub && m_Station == StationKind::Spellbook);
	}

	void SessionState::DrawDraggedSpell()
	{
		Ui& ui = m_App.GetUi();
		const UiPayload* drag = ui.GetDrag();
		if (!drag || (drag->Type != SpellPayload && drag->Type != KeyPayload))
			return;
		const AbilityId spell = drag->Type == SpellPayload ? AbilityId(drag->Value) : m_Client.GetPreferredSpells()[size_t(std::clamp(drag->Value, 0, int(BrawlSlots) - 1))];
		const UiColor color = WithAlpha(SkillColor(spell));
		ui.SetLayer(UiLayer::Overlay);
		UiDrawList& draw = ui.Draw();
		const UiRect ghost = UiRect::Centered(ui.GetMouse(), { 54.0f, 54.0f });
		draw.Shadow(ghost, 12.0f, 16.0f, WithAlpha(color, 0.45f), { 0.0f, 4.0f });
		draw.Rect(ghost, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Border = Theme::Astral, .BorderWidth = 2.0f, .Chamfer = 9.0f });
		draw.Icon(IconFor(spell), ghost.Shrink(9.0f), { .Color = Lighten(color, 0.5f), .ColorBottom = color, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f) });
		ui.SetLayer(UiLayer::Windows);
	}

	void SessionState::DrawBrawlLoadout()
	{
		const GameStatus& status = m_Client.GetStatus();
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (!IsChoosing(status) || !local.Valid || m_Paused || m_Client.IsSpectating())
			return;
		Ui& ui = m_App.GetUi();
		const BrawlStatus& brawl = status.Brawl;
		const BrawlLoadout& loadout = local.Brawl;
		const MatchRules& rules = m_Client.GetSession().Rules;
		const bool picking = brawl.Phase == BrawlPhase::Pick;
		const uint32_t self = m_Client.GetPlayerId();
		const bool champion = picking && !rules.WinnerCard && WonBrawlRound(self);
		const bool cardsUp = loadout.Offer[0] != CardId::Count && (loadout.OfferPending || m_ChosenCard.has_value());
		const bool apexPick = !picking && cardsUp;
		const bool offered = loadout.OfferPending && !m_ChosenCard;
		if (offered && !m_App.UiWantsKeyboard())
			for (uint32_t i = 0; i < CardChoices; i++)
				if (Rdn::Input::IsKeyPressed(CardKeys[i]) && loadout.Offer[i] != CardId::Count)
					TakeCard(i, loadout.Offer[i]);
		if (!offered && !m_App.UiWantsKeyboard() && Rdn::Input::IsKeyPressed(Rdn::KeyCode::Enter))
			m_Client.SetBrawlReady(!loadout.Ready);
		if (!m_LoadoutOpen)
			return;

		const glm::vec2 size = ui.GetSize();
		constexpr float HudBand = 172.0f;
		ui.SetLayer(UiLayer::World);
		ui.Draw().Rect(ui.GetScreen(), { .Fill = Rgba(0.02f, 0.012f, 0.05f, 0.64f), .FillBottom = Rgba(0.02f, 0.012f, 0.05f, 0.74f) });
		ui.SetLayer(UiLayer::Windows);
		UiDrawList& draw = ui.Draw();
		ui.Block({ 0.0f, 112.0f, size.x, size.y - HudBand - 112.0f });
		const float center = size.x * 0.5f;
		const float bottom = size.y - HudBand;
		const UiRect strip = UiRect::Centered({ center, bottom - 52.0f }, { std::min(size.x - 40.0f, 880.0f), 92.0f });
		const UiRect readyRow{ center - 380.0f, strip.Y - 62.0f, 760.0f, 46.0f };
		const float headingY = 118.0f;
		const float feedY = readyRow.Y - 32.0f;
		const float cardsTop = headingY + 84.0f;
		const float cardHeight = std::clamp(feedY - 14.0f - cardsTop, 200.0f, 330.0f);
		const float cardWidth = std::min(cardHeight * 0.8f, 270.0f);

		TextStyle heading = Theme::Title(30.0f);
		heading.Tracking = 3.0f;
		heading.Color = Theme::GoldLight;
		heading.ColorBottom = Theme::Gold;
		std::string title = "READY CHECK";
		std::string hint = rules.RandomSpells ? "Random spells: everyone gets three new spells when each round starts. Ready up when you are." : "These are the spells from your Spellbook in the Sun's Cradle. Drag a different one onto a key below if you want, then ready up.";
		if (champion)
		{
			title = "CHAMPION";
			hint = rules.Teams >= 2 ? Format("Your team won round %u. The winners sit this pick out, the others are choosing their cards.", unsigned(brawl.Round)) : Format("You won round %u. No card for the champion, the others are choosing theirs.", unsigned(brawl.Round));
		}
		else if (apexPick)
		{
			title = offered ? "CHOOSE YOUR APEX" : "APEX TAKEN";
			hint = offered ? "Every mage starts this match with an apex card, and it lasts all match. Press 1, 2 or 3." : rules.RandomSpells ? "It lasts for the whole match. Ready up when you are." : "It lasts for the whole match. Check your spells below, then ready up.";
		}
		else if (picking && offered)
		{
			title = "CHOOSE A CARD";
			hint = WonBrawlRound(self) ? "You won the round, and the winner picks a card too. Press 1, 2 or 3." : "Upgrades change how one of your spells works. Wild cards bend the rules. Press 1, 2 or 3.";
		}
		else if (picking)
		{
			title = m_ChosenCard ? "CARD TAKEN" : "BETWEEN ROUNDS";
			if (rules.RandomSpells)
				hint = m_ChosenCard ? "It lasts for the rest of the match. New spells come with the next round." : "No cards left to offer. New spells come with the next round.";
			else
				hint = m_ChosenCard ? "It lasts for the rest of the match. Swap spells below if you like, then ready up." : "No cards left to offer. Swap spells below if you like, then ready up.";
		}
		if ((picking || apexPick) && !champion && cardsUp)
		{
			constexpr float Gap = 40.0f;
			m_CardReveal += ui.GetDeltaTime();
			m_TakenAge += ui.GetDeltaTime();
			uint32_t count = 0;
			for (const CardId card : loadout.Offer)
				count += card != CardId::Count ? 1u : 0u;
			float x = center - (float(count) * cardWidth + float(count - 1) * Gap) * 0.5f;
			for (uint32_t i = 0; i < CardChoices; i++)
			{
				const CardId card = loadout.Offer[i];
				if (card == CardId::Count)
					continue;
				const float reveal = std::clamp((m_CardReveal - 0.12f * float(i)) / 0.55f, 0.0f, 1.0f);
				const float rise = 1.0f - reveal;
				const UiRect rect{ x, cardsTop + 42.0f * rise * rise * rise, cardWidth, cardHeight };
				x += cardWidth + Gap;
				const UiId id = Ui::MakeId("cardpick", i);
				const UiInteraction interaction = ui.Interact(id, rect);
				const bool taken = m_ChosenCard == i;
				const float hover = ui.Animate(id, offered && interaction.Hovered);
				const float burnAway = m_ChosenCard && !taken ? std::clamp((m_TakenAge - CardBurnDelay) / CardBurnSeconds, 0.0f, 1.0f) : 0.0f;
				if (burnAway >= 1.0f)
					continue;
				if (offered && interaction.Clicked)
					TakeCard(i, card);
				DrawCardFace(rect, card, hover, 1.0f, offered ? std::optional<uint32_t>(i) : std::nullopt, taken, reveal, taken ? std::max(1.0f - m_TakenAge / 0.9f, 0.0f) : 0.0f, burnAway);
			}
		}
		else if (champion)
		{
			const UiRect crown = UiRect::Centered({ center, cardsTop + cardHeight * 0.38f }, { 120.0f, 120.0f });
			draw.Circle(crown.Center(), 86.0f, WithAlpha(Theme::Gold, 0.08f), WithAlpha(Theme::Gold, 0.45f), 1.5f);
			draw.Icon(UiIcon::Crown, crown, { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .Outline = 1.0f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextAligned({ 0.0f, crown.Bottom() + 26.0f, size.x, 24.0f }, Format("+%u points", unsigned(WinPoints(m_Client.GetSession().Rules.Mode))), Theme::Bold(19.0f, Theme::Gold), { 0.5f, 0.5f });
		}
		else if (!picking)
		{
			const BrawlTerms& terms = TermsFor(m_Client.GetSession().Map);
			const bool damage = rules.Mode == BrawlMode::Damage;
			std::string scoring;
			if (rules.Teams >= 2)
				scoring = std::string(damage ? terms.DamageRules : terms.Rules) + Format(" The last team standing wins the round, and every mage on it scores %u. A kill adds 1. The team that wins the most rounds wins the match.", unsigned(RoundWinPoints));
			else if (damage)
				scoring = std::string(terms.DamageRules) + Format(" Last mage standing scores %u, second %u, third %u. A kill adds 1 for whoever dealt the victim the most damage in the last %.0f seconds.", unsigned(PlacementPoints[0]), unsigned(PlacementPoints[1]), unsigned(PlacementPoints[2]), double(DamageCreditSeconds));
			else
				scoring = std::string(terms.Rules) + " Last mage standing wins the round: 3 points, plus 1 for every rival you knock out.";
			scoring += rules.WinnerCard ? " After each round, everyone picks a card." : " After each round, everyone but the winner picks a card.";
			if (const std::string custom = CustomSummary(rules); !custom.empty())
				scoring += " This match: " + custom + ".";
			const TextStyle body = Theme::Body(14.5f, Theme::Text);
			const float panelWidth = std::min(size.x - 80.0f, 720.0f);
			const float lines = std::ceil(draw.MeasureWidth(scoring, body) / (panelWidth - 44.0f) + 0.35f);
			const UiRect panel = UiRect::Centered({ center, cardsTop + cardHeight * 0.4f }, { panelWidth, 52.0f + 20.0f * lines });
			ui.Panel(panel, 12.0f);
			UiRect text = panel.Shrink(22.0f, 18.0f);
			TextStyle labelStyle = Theme::Bold(12.0f, Theme::Muted);
			labelStyle.Tracking = 1.6f;
			draw.TextAligned(text.CutTop(16.0f), "HOW IT WORKS", labelStyle, { 0.5f, 0.5f });
			text.CutTop(8.0f);
			draw.TextWrapped(text, scoring, body, 0.5f, 1.05f);
		}
		draw.TextAligned({ 0.0f, headingY, size.x, 40.0f }, title, heading, { 0.5f, 0.5f });
		draw.TextAligned({ 0.0f, headingY + 36.0f, size.x, 22.0f }, hint, Theme::Floating(15.5f, Theme::Text, UiFont::Body), { 0.5f, 0.5f });

		if (picking || apexPick)
		{
			std::string line;
			for (auto news = m_CardNews.rbegin(); news != m_CardNews.rend() && line.size() < 120; ++news)
				line += (line.empty() ? "" : "   \xC2\xB7   ") + PlayerName(news->PlayerId) + " took " + GetCard(news->Card).Name;
			std::vector<std::string> waiting;
			for (const PlayerInfo& player : m_Client.GetPlayers())
				if (player.PlayerId == self ? loadout.OfferPending && !m_ChosenCard : player.Choosing)
					waiting.push_back(player.PlayerId == self ? std::string("you") : player.Name);
			if (!waiting.empty() && line.size() < 120)
			{
				std::string names;
				for (size_t i = 0; i < waiting.size() && i < 3; i++)
					names += (i == 0 ? "" : i + 1 == std::min<size_t>(waiting.size(), 3) ? " and " : ", ") + waiting[i];
				line += (line.empty() ? "Still choosing: " : "   \xC2\xB7   Still choosing: ") + names;
			}
			if (picking && !rules.WinnerCard && brawl.RoundTeam < MaxTeams && !WonBrawlRound(self))
				line += (line.empty() ? "" : "   \xC2\xB7   ") + std::string("The ") + GetTeamName(brawl.RoundTeam) + " team won the round and sits this pick out";
			else if (picking && !rules.WinnerCard && brawl.RoundWinner != NoPlayer && brawl.RoundWinner != self)
				line += (line.empty() ? "" : "   \xC2\xB7   ") + PlayerName(brawl.RoundWinner) + " won the round and gets no card";
			draw.TextAligned({ 0.0f, feedY, size.x, 20.0f }, line, Theme::Floating(14.5f, Theme::AccentLight, UiFont::Body), { 0.5f, 0.5f });
		}

		UiRect row = readyRow;
		UiRect cardsArea = row.CutLeft(250.0f);
		TextStyle ownedStyle = Theme::Bold(11.5f, Theme::Muted);
		ownedStyle.Tracking = 1.4f;
		draw.TextAligned(cardsArea.CutTop(14.0f), "YOUR CARDS", ownedStyle, { 0.0f, 0.5f });
		DrawCardRow(cardsArea, loadout.Cards, 28.0f, "None yet");
		UiRect button = row.CutLeft(260.0f).Shrink(10.0f, 0.0f);
		if (ui.Button(Ui::MakeId("brawlready"), button, loadout.Ready ? "Waiting for the others" : "Ready  (Enter)", loadout.Ready ? ButtonKind::Normal : ButtonKind::Primary, true, loadout.Ready ? UiIcon::Hourglass : UiIcon::CheckMark))
			m_Client.SetBrawlReady(!loadout.Ready);
		const std::string waiting = !picking && brawl.Players < 2 ? std::string("Waiting for a rival. Ready starts a round on your own") : Format("%u of %u ready, %s in %s s", unsigned(brawl.Ready), unsigned(std::max<uint8_t>(brawl.Players, 1)), picking ? "next round" : "first round", Seconds(brawl.Timer).c_str());
		draw.TextWrapped(row.Shrink(6.0f, 4.0f), waiting, Theme::Body(13.5f, Theme::Muted), 1.0f);

		if (rules.RandomSpells)
		{
			ui.Panel(strip, 12.0f);
			UiRect text = strip.Shrink(20.0f, 14.0f);
			draw.Icon(UiIcon::UnstableOrb, text.CutLeft(52.0f).Shrink(0.0f, 6.0f), { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			text.CutLeft(14.0f);
			TextStyle label = Theme::Bold(12.0f, Theme::Gold);
			label.Tracking = 1.6f;
			draw.TextAligned(text.CutTop(18.0f), "RANDOM SPELLS", label, { 0.0f, 0.5f });
			draw.TextWrapped(text, "Everyone gets three new spells when each round starts, bots included. Your spellbook comes back after the match.", Theme::Body(14.5f, Theme::Text));
			return;
		}
		DrawSpellStrip(strip);
		DrawDraggedSpell();
	}

	void SessionState::DrawSpellPicker(UiRect& left, const BrawlSpells& spells)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		ui.Separator(left.CutTop(20.0f), "YOUR SPELLS");
		left.CutTop(8.0f);
		UiRect slots = left.CutTop(88.0f);
		constexpr float SlotWidth = 141.0f;
		constexpr float SlotGap = 12.0f;
		for (uint32_t slot = 0; slot <= BrawlSlots; slot++)
		{
			const UiRect rect = slots.CutLeft(SlotWidth);
			slots.CutLeft(SlotGap);
			const bool basic = slot == 0;
			const AbilityId spell = basic ? BrawlBasic : spells[slot - 1];
			const UiId id = Ui::MakeId("brawlslot", slot);
			const UiInteraction interaction = ui.Interact(id, rect);
			const UiPayload* drag = ui.GetDrag();
			const bool target = !basic && drag && (drag->Type == SpellPayload || drag->Type == KeyPayload) && rect.Contains(ui.GetMouse());
			const bool selected = !basic && (m_PickingSlot == slot - 1 || target);
			const float hover = ui.Animate(id, interaction.Hovered || selected || (!basic && m_SelectedSpell.has_value()));
			if (!basic)
			{
				ui.OfferDrag(id, KeyPayload, int(slot - 1));
				if (!AcceptSpellDrop(rect, slot - 1, interaction.Clicked) && interaction.Clicked)
					m_PickingSlot = m_PickingSlot == slot - 1 ? std::nullopt : std::optional<uint32_t>(slot - 1);
			}
			const UiColor color = WithAlpha(SkillColor(spell));
			ui.Card(rect, basic ? 0.0f : hover, selected ? 1.0f : 0.0f, color);
			draw.Icon(IconFor(spell), { rect.X + 12.0f, rect.Y + 14.0f, 40.0f, 40.0f }, { .Color = Lighten(color, 0.5f), .ColorBottom = color, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextFit({ rect.X + 60.0f, rect.Y + 12.0f, rect.W - 68.0f, 22.0f }, GetAbility(spell).Name, Theme::Bold(15.0f, selected ? Theme::GoldLight : Theme::Text));
			draw.TextFit({ rect.X + 60.0f, rect.Y + 34.0f, rect.W - 68.0f, 20.0f }, basic ? std::string("shared") : selected ? std::string("pick one") : std::string(ActionWord(spell)), Theme::Body(13.0f, selected ? Theme::Gold : Theme::Faint));
			const char* key = basic ? "LMB" : SpellKeys[slot - 1];
			const TextStyle keyStyle = Theme::Bold(11.5f, Theme::AccentLight);
			const UiRect badge{ rect.X + 10.0f, rect.Bottom() - 24.0f, draw.MeasureWidth(key, keyStyle) + 12.0f, 16.0f };
			draw.Rect(badge, { .Fill = Rgba(0.03f, 0.025f, 0.06f, 0.94f), .Border = WithAlpha(Theme::Edge, 0.9f), .BorderWidth = 1.0f, .Chamfer = 3.0f });
			draw.TextAligned(badge, key, keyStyle, { 0.5f, 0.5f });
			if (interaction.Hovered && !drag)
				DescribeSpell(ui.Tooltip(), spell, ShownBrawlMode());
		}
		left.CutTop(16.0f);

		ui.Separator(left.CutTop(20.0f), "THE SPELLBOOK");
		left.CutTop(8.0f);
		constexpr size_t Columns = 4;
		constexpr float CardGap = 12.0f;
		constexpr float CardHeight = 70.0f;
		const float cardWidth = (left.W - CardGap * float(Columns - 1)) / float(Columns);
		for (size_t i = 0; i < std::size(BrawlSpellPool); i++)
		{
			const AbilityId spell = BrawlSpellPool[i];
			const UiRect card{ left.X + float(i % Columns) * (cardWidth + CardGap), left.Y + float(i / Columns) * (CardHeight + 8.0f), cardWidth, CardHeight };
			const UiId id = Ui::MakeId("brawlspell", i);
			const UiInteraction interaction = ui.Interact(id, card);
			ui.OfferDrag(id, SpellPayload, int(spell));
			const auto equipped = std::find(spells.begin(), spells.end(), spell);
			const bool owned = equipped != spells.end();
			const bool chosen = m_SelectedSpell == spell;
			const float hover = ui.Animate(id, interaction.Hovered || chosen);
			if (interaction.Clicked)
			{
				if (m_PickingSlot)
				{
					m_Client.PreferSpell(*m_PickingSlot, spell);
					m_PickingSlot.reset();
					m_SelectedSpell.reset();
				}
				else
					m_SelectedSpell = chosen ? std::nullopt : std::optional<AbilityId>(spell);
			}
			const UiColor color = WithAlpha(SkillColor(spell));
			ui.Card(card, hover, owned ? 0.6f : chosen ? 1.0f : 0.0f, chosen ? Theme::Astral : color);
			draw.Icon(IconFor(spell), { card.X + 9.0f, card.Y + 9.0f, 30.0f, 30.0f }, { .Color = Lighten(color, 0.5f), .ColorBottom = color, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextFit({ card.X + 46.0f, card.Y + 7.0f, card.W - 52.0f, 20.0f }, GetAbility(spell).Name, Theme::Bold(14.5f, owned ? Lighten(color, 0.4f) : Theme::Text));
			draw.TextFit({ card.X + 46.0f, card.Y + 26.0f, card.W - 52.0f, 16.0f }, ActionWord(spell), Theme::Body(12.0f, Theme::Faint));
			float summary = card.W - 18.0f;
			if (owned)
			{
				const char* key = SpellKeys[equipped - spells.begin()];
				const TextStyle keyStyle = Theme::Bold(11.5f, Theme::GoldLight);
				const UiRect badge{ card.Right() - draw.MeasureWidth(key, keyStyle) - 20.0f, card.Bottom() - 23.0f, draw.MeasureWidth(key, keyStyle) + 12.0f, 16.0f };
				draw.Rect(badge, { .Fill = Rgba(0.08f, 0.06f, 0.02f, 0.94f), .Border = WithAlpha(Theme::Gold, 0.9f), .BorderWidth = 1.0f, .Chamfer = 3.0f });
				draw.TextAligned(badge, key, keyStyle, { 0.5f, 0.5f });
				summary -= badge.W + 8.0f;
			}
			draw.TextFit({ card.X + 9.0f, card.Bottom() - 24.0f, summary, 18.0f }, SpellSummary(spell, ShownBrawlMode()), Theme::Body(12.5f, Theme::Muted));
			if (interaction.Hovered && !ui.GetDrag())
				DescribeSpell(ui.Tooltip(), spell, ShownBrawlMode());
		}
	}

	void SessionState::DrawSpellbook()
	{
		Ui& ui = m_App.GetUi();
		constexpr float Width = 1000.0f;
		constexpr float Height = 600.0f;
		const UiRect home{ (ui.GetSize().x - Width) * 0.5f, std::max((ui.GetSize().y - Height) * 0.5f - 24.0f, 12.0f), Width, Height };
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("spellbook"), "Spellbook", home, &open);
		if (!open)
		{
			m_Station.reset();
			return;
		}
		UiDrawList& draw = ui.Draw();
		draw.TextWrapped(content.CutTop(24.0f), "Everyone throws Ember Bolt with LMB. Choose three more spells for RMB, Q and R: click a slot, then a spell.", Theme::Body(14.5f, Theme::Muted));
		content.CutTop(8.0f);
		UiRect left = content.CutLeft(600.0f);
		content.CutLeft(28.0f);
		UiRect right = content;
		DrawSpellPicker(left, m_Client.GetPreferredSpells());

		ui.Separator(right.CutTop(20.0f), "PRACTICE");
		right.CutTop(8.0f);
		right.CutTop(draw.TextWrapped(right, "Your spells work everywhere in the Sun's Cradle, with the damage, mana and shoves of a damage spellbrawl. Try them on the dummies in the pit next to this book: wear one down and it topples, or shove it into the lava and it sinks. Either way a fresh one climbs out a moment later.", Theme::Body(14.5f, Theme::Text)) + 14.0f);
		ui.Separator(right.CutTop(20.0f), "KEPT FOR EVERY MATCH");
		right.CutTop(8.0f);
		right.CutTop(draw.TextWrapped(right, "These spells are saved with your character. Every Spellbrawl starts with them already picked, and you can still swap them in the draft.", Theme::Body(14.5f, Theme::Text)) + 14.0f);
		draw.TextWrapped(right, "Some dummies are rarer than others. Keep an eye on the pit.", Theme::Body(13.5f, Theme::Faint));
		DrawDraggedSpell();
	}

	void SessionState::DrawBotRules(UiRect& area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		TidyBrawlSeats();
		DefaultBrawlBots();
		uint32_t humans = 0;
		for (const PlayerInfo& player : m_Client.GetPlayers())
			humans += player.Bot ? 0u : 1u;
		const int limit = std::max(int(MaxPlayers) - std::max(int(humans), 1), 0);
		const bool teams = m_BrawlRules.Teams >= 2;
		int bots = std::min(int(CountRuleBots(m_BrawlRules)), limit);
		const int before = bots;
		StepRow(ui, area, Ui::MakeId("botcount"), bots, { .Label = "Bots", .Value = bots == 0 ? std::string("None") : std::to_string(bots), .Low = 0, .High = limit, .Tip = teams ? "Each new bot joins the team with the fewest mages. The Teams tab moves them around." : "Bots fill the empty seats and play like rivals." });
		if (teams && bots != before)
			AddBrawlBot(bots - before);
		else if (!teams)
			m_BrawlRules.Bots = uint8_t(std::clamp(bots, 0, limit));
		UiRect row = area.CutTop(40.0f);
		draw.TextAligned(row.CutLeft(170.0f), "Bot skill", Theme::Bold(16.0f, bots > 0 ? Theme::Text : Theme::Faint), { 0.0f, 0.5f });
		int level = std::min<int>(m_BrawlRules.BotLevel, int(BotLevels) - 1);
		if (ui.Choice(Ui::MakeId("botlevel"), row.CutLeft(std::min(300.0f, row.W)), level, BotLevelNames))
			m_BrawlRules.BotLevel = uint8_t(level);
		area.CutTop(6.0f);
	}

	void SessionState::DrawTeamChoice(UiRect& area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		UiRect row = area.CutTop(40.0f);
		draw.TextAligned(row.CutLeft(170.0f), "Teams", Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
		int choice = m_BrawlRules.Teams >= 2 ? int(m_BrawlRules.Teams) - 1 : 0;
		if (ui.Choice(Ui::MakeId("brawlteams"), row.CutLeft(std::min(300.0f, row.W)), choice, TeamChoices))
			SetBrawlTeams(choice == 0 ? 0 : uint8_t(choice + 1));
		const char* blurb = m_BrawlRules.Teams >= 2 ? "Last team standing wins. Sort mages on the Teams tab." : "Every mage for themselves.";
		draw.TextFit(area.CutTop(22.0f), blurb, Theme::Body(13.5f, Theme::Muted));
		area.CutTop(6.0f);
	}

	std::array<uint32_t, MaxTeams> SessionState::CountTeamMembers() const
	{
		std::array<uint32_t, MaxTeams> members{};
		for (const PlayerInfo& player : m_Client.GetPlayers())
			if (!player.Bot && player.PlayerId < MaxPlayers && m_BrawlRules.Seats[player.PlayerId] < m_BrawlRules.Teams)
				members[m_BrawlRules.Seats[player.PlayerId]]++;
		for (uint8_t team = 0; team < m_BrawlRules.Teams; team++)
			members[team] += m_BrawlRules.TeamBots[team];
		return members;
	}

	void SessionState::AddBrawlBot(int delta)
	{
		MatchRules& rules = m_BrawlRules;
		if (rules.Teams < 2)
			return;
		for (; delta > 0; delta--)
		{
			const std::array<uint32_t, MaxTeams> members = CountTeamMembers();
			uint8_t smallest = 0;
			for (uint8_t team = 1; team < rules.Teams; team++)
				if (members[team] < members[smallest])
					smallest = team;
			rules.TeamBots[smallest]++;
		}
		for (; delta < 0; delta++)
		{
			const std::array<uint32_t, MaxTeams> members = CountTeamMembers();
			uint8_t largest = NoTeam;
			for (uint8_t team = 0; team < rules.Teams; team++)
				if (rules.TeamBots[team] > 0 && (largest == NoTeam || members[team] >= members[largest]))
					largest = team;
			if (largest == NoTeam)
				break;
			rules.TeamBots[largest]--;
		}
	}

	void SessionState::SetBrawlTeams(uint8_t teams)
	{
		MatchRules& rules = m_BrawlRules;
		if (teams == rules.Teams)
			return;
		const uint32_t bots = CountRuleBots(rules);
		rules.Teams = teams;
		rules.TeamBots.fill(0);
		for (uint8_t& seat : rules.Seats)
			if (seat != SpectatorSeat && seat >= teams)
				seat = NoTeam;
		if (teams < 2)
		{
			rules.Bots = uint8_t(std::min<uint32_t>(bots, MaxBots));
			return;
		}
		TidyBrawlSeats();
		AddBrawlBot(int(bots));
	}

	void SessionState::TidyBrawlSeats()
	{
		MatchRules& rules = m_BrawlRules;
		std::array<bool, MaxPlayers> here{};
		uint32_t humans = 0;
		for (const PlayerInfo& player : m_Client.GetPlayers())
			if (!player.Bot && player.PlayerId < MaxPlayers)
			{
				here[player.PlayerId] = true;
				humans++;
			}
		for (uint32_t id = 0; id < MaxPlayers; id++)
			if (!here[id])
				rules.Seats[id] = NoTeam;
		const uint32_t room = MaxPlayers - std::min(std::max(humans, 1u), MaxPlayers);
		if (rules.Teams < 2)
		{
			for (uint8_t& seat : rules.Seats)
				if (seat != SpectatorSeat)
					seat = NoTeam;
			rules.Bots = uint8_t(std::min<uint32_t>(rules.Bots, room));
			return;
		}
		for (uint32_t id = 0; id < MaxPlayers; id++)
		{
			if (!here[id] || rules.Seats[id] == SpectatorSeat || rules.Seats[id] < rules.Teams)
				continue;
			const std::array<uint32_t, MaxTeams> members = CountTeamMembers();
			uint8_t smallest = 0;
			for (uint8_t team = 1; team < rules.Teams; team++)
				if (members[team] < members[smallest])
					smallest = team;
			rules.Seats[id] = smallest;
		}
		while (CountTeamBots(rules) > room)
			AddBrawlBot(-1);
	}

	void SessionState::DefaultBrawlBots()
	{
		if (m_BotsChosen)
			return;
		const uint32_t bots = CountRuleBots(m_BrawlRules);
		if (bots != m_AutoBots)
		{
			m_BotsChosen = true;
			return;
		}
		uint32_t humans = 0;
		for (const PlayerInfo& player : m_Client.GetPlayers())
			humans += player.Bot ? 0u : 1u;
		const uint32_t want = humans <= 1 ? m_SoloBots : 0u;
		if (want == bots)
			return;
		if (m_BrawlRules.Teams >= 2)
			AddBrawlBot(int(want) - int(bots));
		else
			m_BrawlRules.Bots = uint8_t(want);
		m_AutoBots = CountRuleBots(m_BrawlRules);
	}

	void SessionState::DrawArenaChoice(UiRect& area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		UiRect row = area.CutTop(40.0f);
		draw.TextAligned(row.CutLeft(170.0f), "Arena", Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
		ui.Choice(Ui::MakeId("brawlarena"), row.CutLeft(std::min(300.0f, row.W)), m_BrawlArena, ArenaNames);
		const MapKind arena = ChosenArena();
		const char* blurb = arena == MapKind::Caldera ? "An iron mold plate chained above the forge of the titans." : arena == MapKind::FrozenLake ? "Slippery ice, bouncing pillars and growing air holes." : arena == MapKind::SkyRuins ? "A domed observatory above the clouds. A burning field closes in around the hole in its floor." : arena == MapKind::StormPeak ? "A summit plateau in a lightning storm, ringed by a long fall." : "A different arena every match.";
		draw.TextFit(area.CutTop(22.0f), blurb, Theme::Body(13.5f, Theme::Muted));
		area.CutTop(6.0f);
	}

	void SessionState::DrawModeChoice(UiRect& area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		UiRect row = area.CutTop(40.0f);
		draw.TextAligned(row.CutLeft(170.0f), "Mode", Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
		int mode = int(m_BrawlRules.Mode);
		if (ui.Choice(Ui::MakeId("brawlmode"), row.CutLeft(std::min(300.0f, row.W)), mode, ModeNames))
			m_BrawlRules.Mode = BrawlMode(mode);
		const char* blurb = m_BrawlRules.Mode == BrawlMode::Damage ? "Spells hurt and cost mana. Placing and kills score." : "Spells only shove. Win rounds and knock rivals out.";
		draw.TextFit(area.CutTop(22.0f), blurb, Theme::Body(13.5f, Theme::Muted));
		area.CutTop(6.0f);
	}

	bool SessionState::WonBrawlRound(uint32_t playerId) const
	{
		const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
		if (brawl.RoundTeam < MaxTeams)
			return playerId != NoPlayer && m_Client.GetTeam(playerId) == brawl.RoundTeam;
		return brawl.RoundWinner != NoPlayer && brawl.RoundWinner == playerId;
	}

	BrawlMode SessionState::ShownBrawlMode() const
	{
		const SessionInfo& session = m_Client.GetSession();
		return session.Mode == GameMode::Brawl ? session.Rules.Mode : session.Mode == GameMode::Shardfall ? BrawlMode::Damage : m_BrawlRules.Mode;
	}

	void SessionState::DescribeBrawlSpell(UiTooltip& tooltip, AbilityId spell) const
	{
		if (m_Client.GetSession().Mode == GameMode::Shardfall)
		{
			SkillTraits skill = MakeRunSkill(spell);
			for (const SkillTraits& owned : m_Client.GetTraits().Skills)
				if (owned.Ability == spell)
					skill = owned;
			DescribeSpell(tooltip, spell, BrawlMode::Damage, &skill);
			const RunLoadout& run = m_Client.GetLocalPlayer().Run;
			const int32_t slot = FindRunSlot(run, spell);
			if (slot < 0)
				return;
			const RuneMask runes = SlotRunes(run, uint32_t(slot));
			tooltip.Separator();
			tooltip.Pair("Rune sockets", Format("%u of %u filled", unsigned(std::popcount(runes)), unsigned(OpenSockets(run, uint32_t(slot)))));
			for (const RuneDef& rune : Runes)
				if (HasRune(runes, rune.Id))
					tooltip.Text(std::string(rune.Name) + ": " + rune.Text, Lighten(RuneTone(rune.Id), 0.2f), 13.5f);
			return;
		}
		DescribeSpell(tooltip, spell, m_Client.GetSession().Mode == GameMode::Hub ? BrawlMode::Damage : ShownBrawlMode());
	}

	MapKind SessionState::ChosenArena() const
	{
		return m_BrawlArena == 1 ? MapKind::Caldera : m_BrawlArena == 2 ? MapKind::FrozenLake : m_BrawlArena == 3 ? MapKind::SkyRuins : m_BrawlArena == 4 ? MapKind::StormPeak : RandomBrawlMap;
	}

	std::string SessionState::EnterLabel() const
	{
		const MapKind arena = ChosenArena();
		return arena == RandomBrawlMap ? std::string("Enter a random arena") : std::string("Enter ") + TermsFor(arena).Title;
	}

	void SessionState::DrawCalderaGate()
	{
		Ui& ui = m_App.GetUi();
		constexpr float Width = 860.0f;
		constexpr float Height = 724.0f;
		const UiRect home{ (ui.GetSize().x - Width) * 0.5f, std::max((ui.GetSize().y - Height) * 0.5f - 66.0f, 12.0f), Width, Height };
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("calderagate"), "Caldera Gate", home, &open);
		if (!open)
		{
			m_Station.reset();
			return;
		}
		TidyBrawlSeats();
		DefaultBrawlBots();
		ui.Tabs(Ui::MakeId("gatetabs"), content.CutTop(36.0f), m_GateTab, GateTabs);
		content.CutTop(12.0f);
		const UiRect footer = content.CutBottom(96.0f);
		if (m_GateTab == 0)
			DrawGateMatch(content);
		else if (m_GateTab == 1)
			DrawGateCustom(content);
		else
			DrawGateTeams(content);
		DrawGateFooter(footer);
	}

	void SessionState::DrawShardRift()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		constexpr float Width = 820.0f;
		constexpr float Height = 650.0f;
		const UiRect home{ (ui.GetSize().x - Width) * 0.5f, std::max((ui.GetSize().y - Height) * 0.5f - 66.0f, 12.0f), Width, Height };
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("shardrift"), "Shard Rift", home, &open);
		if (!open)
		{
			m_Station.reset();
			return;
		}
		content.CutTop(draw.TextWrapped(content, "Shardfall is a co-op crystal hunt for up to four mages. Each of the three acts is a branching map of rooms: you pick every door, and each door shows what waits behind it. Fight rooms hold sleeping packs of crystal fighters and crawlers, champions fight in phases, and shops, shrines and rest rooms let you shape your build with shards, sigils and duo cards. Spells shove enemies, so knock them into walls and into each other. Each act ends at a guardian. Health never comes back on its own: you heal only at rest altars, at the merchant and when a guardian falls. Spells are free, and mana lets you overcast a spell that is still recharging. Kills spill shards for the merchant, and every cleared room gives the one reward shown on its door.", Theme::Body(14.5f, Theme::Text)) + 12.0f);
		UiRect strip = content.CutTop(28.0f);
		const size_t crawlers = size_t(std::count_if(std::begin(RunOnlyKinds), std::end(RunOnlyKinds), [](EntityKind kind) { return IsCrawlerKind(kind); }));
		const float slot = strip.W / float(crawlers);
		for (size_t i = 0; i < crawlers; i++)
		{
			const UiRect cell = strip.CutLeft(slot);
			const glm::vec3 tone = CrawlerTone(RunOnlyKinds[i], m_Time);
			draw.Diamond({ cell.X + 10.0f, cell.Center().y }, 16.0f, WithAlpha(tone), Lighten(WithAlpha(tone), 0.5f), 1.0f);
			draw.TextAligned({ cell.X + 24.0f, cell.Y, cell.W - 24.0f, cell.H }, RunOnlyNames[i + 1], Theme::Bold(15.0f, Lighten(WithAlpha(tone), 0.35f)), { 0.0f, 0.5f });
		}
		content.CutTop(14.0f);
		const UiRect footer = content.CutBottom(52.0f);
		ui.Separator(content.CutTop(20.0f), "TEST SETTINGS");
		content.CutTop(4.0f);
		draw.TextFit(content.CutTop(20.0f), "For trying things out. Leave them at the defaults for a normal run.", Theme::Body(13.5f, Theme::Muted));
		content.CutTop(8.0f);

		int minute = m_RunRules.StartMinute;
		StepRow(ui, content, Ui::MakeId("runminute"), minute, { .Label = "Start at minute", .Value = Format("%d:00", minute), .Low = 0, .High = MaxStartMinute, .Note = "Skips that many encounters, and enemies are tougher", .Tip = "Jumps the act clock forward and skips one encounter per minute, so enemy health and the prismatic timer act as if that much time had passed." });
		int level = m_RunRules.StartLevel;
		StepRow(ui, content, Ui::MakeId("runlevel"), level, { .Label = "Head start", .Value = level == 2 ? std::string("1 card") : Format("%d cards", level - 1), .Low = 1, .High = MaxStartLevel, .Note = "Extra card picks banked when the run begins" });
		int spawns = m_RunRules.Spawns;
		StepRow(ui, content, Ui::MakeId("runspawns"), spawns, { .Label = "Enemy spawns", .Value = spawns == 0 ? std::string("Off") : Percent(spawns), .Low = 0, .High = MaxSpawnRate, .Step = 25, .Note = spawns == 0 ? "Peaceful: nothing spawns" : "Scales the crowd size and how fast packs arrive" });
		int xp = m_RunRules.ShardRate;
		StepRow(ui, content, Ui::MakeId("runxp"), xp, { .Label = "Shard gain", .Value = Percent(xp), .Low = MinShardRate, .High = MaxShardRate, .Step = 25, .Note = "How many shards kills spill" });
		m_RunRules.StartMinute = uint8_t(minute);
		m_RunRules.StartLevel = uint8_t(level);
		m_RunRules.Spawns = uint16_t(spawns);
		m_RunRules.ShardRate = uint16_t(xp);

		UiRect row = content.CutTop(40.0f);
		draw.TextAligned(row.CutLeft(170.0f), "Only spawn", Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
		int only = m_RunRules.Only;
		if (ui.Choice(Ui::MakeId("runonly"), row.CutLeft(std::min(240.0f, row.W)), only, RunOnlyNames))
			m_RunRules.Only = uint8_t(only);
		draw.TextFit(row.Shrink(12.0f, 0.0f), m_RunRules.Only == 0 ? "The normal encounters" : RunOnlyKinds[m_RunRules.Only - 1] == EntityKind::PrismaticCrawler ? "A prismatic crawler is always out there" : IsFighterKind(RunOnlyKinds[m_RunRules.Only - 1]) ? "A few of that fighter at a time, no encounters" : "Packs of a single kind, caps still apply", Theme::Body(13.5f, Theme::Faint));
		UiRect affixRow = content.CutTop(40.0f);
		draw.TextAligned(affixRow.CutLeft(170.0f), "Elite affix", Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
		int affix = m_RunRules.Affix;
		if (ui.Choice(Ui::MakeId("runaffix"), affixRow.CutLeft(std::min(240.0f, affixRow.W)), affix, RunAffixNames))
			m_RunRules.Affix = uint8_t(affix);
		draw.TextFit(affixRow.Shrink(12.0f, 0.0f), m_RunRules.Affix == 0 ? "Normal elites" : "Only-spawned fighters arrive as elites with it", Theme::Body(13.5f, Theme::Faint));
		content.CutTop(10.0f);
		ToggleRow(ui, content, Ui::MakeId("runimmortal"), "Immortal players", m_RunRules.Immortal, true, "Hits still land and show, but nobody drops below 1 health");
		ToggleRow(ui, content, Ui::MakeId("runallrunes"), "Every rune", m_RunRules.AllRunes, true, "Everyone starts with one of each rune to socket");

		const RunRules defaults;
		const uint32_t players = std::max<uint32_t>(uint32_t(m_Client.GetPlayers().size()), 1u);
		const bool fits = players <= ShardfallMaxPlayers;
		UiRect bar = footer;
		const UiRect start = bar.CutRight(230.0f);
		bar.CutRight(12.0f);
		const UiRect reset = bar.CutRight(150.0f);
		if (ui.Button(Ui::MakeId("runreset"), reset, "Defaults", ButtonKind::Normal, m_RunRules != defaults))
			m_RunRules = defaults;
		const std::string who = fits ? Format("%u of %u seats taken. Everyone here comes along.", unsigned(players), unsigned(ShardfallMaxPlayers)) : Format("%u mages are here, a run takes at most %u.", unsigned(players), unsigned(ShardfallMaxPlayers));
		draw.TextFit({ bar.X, bar.Y, bar.W - 12.0f, 24.0f }, who, Theme::Body(14.0f, fits ? Theme::Muted : Theme::Danger));
		if (m_RunRules != defaults)
			draw.TextFit({ bar.X, bar.Y + 24.0f, bar.W - 12.0f, 22.0f }, "Test settings are on", Theme::Bold(14.0f, Theme::Warning));
		if (ui.Button(Ui::MakeId("runstart"), start, "Open the rift", ButtonKind::Primary, fits, UiIcon::MagicPortal))
		{
			m_Client.StartShardfall(m_RunRules);
			m_Station.reset();
		}
	}

	void SessionState::DrawGateMatch(UiRect area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const char* pitch = m_BrawlRules.Mode == BrawlMode::Damage ? "Spells hurt and cost mana, and they still nudge. Wear your rivals down on a crumbling island over lava or on the ice of the Frozen Lake." : "Spells don't hurt in a spellbrawl, they shove. Knock your rivals off a crumbling island into the lava, or through the ice of the Frozen Lake.";
		area.CutTop(draw.TextWrapped(area, pitch, Theme::Body(14.5f, Theme::Text)) + 12.0f);
		constexpr float Gap = 24.0f;
		UiRect left = area.CutLeft((area.W - Gap) * 0.5f);
		area.CutLeft(Gap);
		UiRect right = area;

		ui.Separator(left.CutTop(20.0f), "MATCH");
		left.CutTop(8.0f);
		int rounds = m_BrawlRules.Rounds;
		StepRow(ui, left, Ui::MakeId("gaterounds"), rounds, { .Label = "Rounds", .Value = std::to_string(rounds), .Low = MinBrawlRounds, .High = MaxBrawlRounds, .Tip = m_BrawlRules.Teams >= 2 ? "A team match ends early once a team can't be caught." : "Everyone plays every round. Points decide the winner." });
		m_BrawlRules.Rounds = uint8_t(rounds);
		DrawModeChoice(left);
		DrawArenaChoice(left);
		DrawTeamChoice(left);

		ui.Separator(right.CutTop(20.0f), "BOTS");
		right.CutTop(8.0f);
		DrawBotRules(right);
		right.CutTop(4.0f);
		ui.Separator(right.CutTop(20.0f), "YOUR SPELLS");
		right.CutTop(8.0f);
		if (m_BrawlRules.RandomSpells)
			right.CutTop(draw.TextWrapped(right, "Random spells are on: everyone gets three new spells when each round starts.", Theme::Body(14.0f, Theme::Gold)) + 10.0f);
		else
		{
			UiRect icons = right.CutTop(40.0f);
			const BrawlSpells& spells = m_Client.GetPreferredSpells();
			for (uint32_t slot = 0; slot <= BrawlSlots; slot++)
			{
				const AbilityId spell = slot == 0 ? BrawlBasic : spells[slot - 1];
				const UiRect icon = icons.CutLeft(40.0f);
				icons.CutLeft(10.0f);
				const UiColor color = WithAlpha(SkillColor(spell));
				draw.Icon(IconFor(spell), icon, { .Color = Lighten(color, 0.5f), .ColorBottom = color, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
				if (ui.Interact(Ui::MakeId("gatespell", slot), icon).Hovered)
					DescribeSpell(ui.Tooltip(), spell, ShownBrawlMode());
			}
			draw.TextFit(icons.Shrink(4.0f, 0.0f), "Change them at the spellbook", Theme::Body(13.5f, Theme::Faint));
			right.CutTop(10.0f);
		}
		ui.Separator(right.CutTop(20.0f), "RULES");
		right.CutTop(8.0f);
		const std::string custom = CustomSummary(m_BrawlRules);
		const std::string text = custom.empty() ? std::string("Standard rules. Tune life, mana, speeds, knockback and more on the Custom tab.") : "This match: " + custom + ".";
		draw.TextWrapped(right, text, Theme::Body(14.0f, custom.empty() ? Theme::Muted : Theme::Gold));
	}

	void SessionState::DrawGateCustom(UiRect area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		MatchRules& rules = m_BrawlRules;
		const bool damage = rules.Mode == BrawlMode::Damage;
		area.CutTop(draw.TextWrapped(area, "Bend the rules for this match. Everyone plays by the same numbers, bots included. Hover a setting to see what it does.", Theme::Body(14.5f, Theme::Text)) + 10.0f);
		constexpr float Gap = 24.0f;
		UiRect left = area.CutLeft((area.W - Gap) * 0.5f);
		area.CutLeft(Gap);
		UiRect right = area;
		const auto step = [&](const char* name, uint16_t& field, int low, int high, int unit, bool enabled, const char* tip, bool percent, std::string note = {})
		{
			int value = field;
			if (note.empty() && value == 100)
				note = "Default";
			StepRow(ui, left, Ui::MakeId(name), value, { .Label = name, .Value = percent ? Percent(value) : std::to_string(value), .Low = low, .High = high, .Step = unit, .Enabled = enabled, .Note = enabled ? note : std::string(), .Tip = tip, .LabelWidth = 156.0f, .ValueWidth = 84.0f });
			field = uint16_t(value);
		};

		ui.Separator(left.CutTop(20.0f), "MAGES");
		left.CutTop(8.0f);
		step("Starting life", rules.Health, MinBrawlHealth, MaxBrawlHealth, BrawlPoolStep, true, "Health every mage starts each round with. Lava and water still burn at the same rate.", false);
		step("Starting mana", rules.Mana, MinBrawlMana, MaxBrawlMana, BrawlPoolStep, damage, "The size of every mage's mana pool. Damage mode only.", false);
		step("Mana regen", rules.ManaRegen, 0, MaxRegenPercent, TuningStep, damage, "How fast mana comes back. At 0% the pool never refills during a round. Damage mode only.", true, Format("%.0f/s", double(BrawlManaRegen) * rules.ManaRegen / 100.0));
		step("Movement speed", rules.MoveSpeed, MinSpeedPercent, MaxMovePercent, TuningStep, true, "How fast mages walk. Dodges and shoves stay the same.", true);
		left.CutTop(2.0f);
		ui.Separator(left.CutTop(20.0f), "SPELLS");
		left.CutTop(8.0f);
		step("Spell damage", rules.Damage, MinDamagePercent, MaxDamagePercent, DamagePercentStep, damage, "How hard spells hit. Lava and water stay the same. Damage mode only.", true);
		step("Cast speed", rules.CastSpeed, MinSpeedPercent, MaxSpeedPercent, TuningStep, true, "How fast spells are cast. At 200% every cast takes half as long.", true);
		step("Cooldown speed", rules.CooldownSpeed, MinSpeedPercent, MaxSpeedPercent, TuningStep, true, "How fast spells come off cooldown. At 200% every cooldown is half as long. The dodge keeps its own cooldown.", true);
		step("Knockback", rules.Knockback, MinKnockbackPercent, MaxKnockbackPercent, TuningStep, true, "How far shoves and pulls throw mages, hooks included. Turn it up for a pinball match.", true);

		ui.Separator(right.CutTop(20.0f), "CARDS");
		right.CutTop(8.0f);
		ToggleRow(ui, right, Ui::MakeId("gatewinnercard"), "The round winner picks a card too", rules.WinnerCard, true, rules.WinnerCard ? "Everyone picks a card after every round" : "The winner sits the pick out, as in the first spellbrawls");
		ToggleRow(ui, right, Ui::MakeId("gateapex"), "Apex cards", rules.Apex, damage, damage ? "Run-defining cards, one per mage, offered from round 4" : "Apex cards only exist in damage mode");
		ToggleRow(ui, right, Ui::MakeId("gateapexdraft"), "Every mage starts with an apex pick", rules.ApexDraft, damage && rules.Apex, "Pick one of three apex cards in the ready check");
		rules.ApexDraft = rules.ApexDraft && rules.Apex;
		right.CutTop(2.0f);
		ui.Separator(right.CutTop(20.0f), "CHAOS");
		right.CutTop(8.0f);
		ToggleRow(ui, right, Ui::MakeId("gaterandomspells"), "Random spells every round", rules.RandomSpells, true, rules.RandomSpells ? "Three new spells for everyone when each round starts" : "Everyone plays the spells they picked");
		right.CutTop(2.0f);
		ui.Separator(right.CutTop(20.0f), "ARENA");
		right.CutTop(8.0f);
		ToggleRow(ui, right, Ui::MakeId("gatecrumble"), "The arena crumbles", rules.Crumble, true, rules.Crumble ? "The floor breaks up from the edge inward" : "The floor holds all round. Only cards can break it");
		if (right.H < 44.0f)
			return;
		right.CutTop(6.0f);
		const MatchRules defaults;
		const bool changed = rules.Health != defaults.Health || rules.Mana != defaults.Mana || rules.ManaRegen != defaults.ManaRegen || rules.MoveSpeed != defaults.MoveSpeed || rules.Damage != defaults.Damage || rules.CastSpeed != defaults.CastSpeed || rules.CooldownSpeed != defaults.CooldownSpeed || rules.Knockback != defaults.Knockback || rules.Apex != defaults.Apex || rules.ApexDraft != defaults.ApexDraft || rules.RandomSpells != defaults.RandomSpells || rules.Crumble != defaults.Crumble || rules.WinnerCard != defaults.WinnerCard;
		if (ui.Button(Ui::MakeId("gatereset"), right.CutTop(38.0f).CutLeft(240.0f), "Back to the defaults", ButtonKind::Normal, changed, UiIcon::Cycle))
		{
			rules.Health = defaults.Health;
			rules.Mana = defaults.Mana;
			rules.ManaRegen = defaults.ManaRegen;
			rules.MoveSpeed = defaults.MoveSpeed;
			rules.Damage = defaults.Damage;
			rules.CastSpeed = defaults.CastSpeed;
			rules.CooldownSpeed = defaults.CooldownSpeed;
			rules.Knockback = defaults.Knockback;
			rules.Apex = defaults.Apex;
			rules.ApexDraft = defaults.ApexDraft;
			rules.RandomSpells = defaults.RandomSpells;
			rules.Crumble = defaults.Crumble;
			rules.WinnerCard = defaults.WinnerCard;
		}
	}

	void SessionState::DrawGateTeams(UiRect area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		MatchRules& rules = m_BrawlRules;
		const bool teams = rules.Teams >= 2;
		const uint32_t self = m_Client.GetPlayerId();
		std::vector<const PlayerInfo*> humans;
		for (const PlayerInfo& player : m_Client.GetPlayers())
			if (!player.Bot && player.PlayerId < MaxPlayers)
				humans.push_back(&player);
		const uint32_t room = MaxPlayers - std::min<uint32_t>(std::max<uint32_t>(uint32_t(humans.size()), 1u), MaxPlayers);
		const char* intro = teams ? "Drag a mage onto another team, or click it to move it along. Mages dropped on Spectators watch the match instead of playing." : "Free for all. Turn teams on in the Match tab to split the mages up. Mages dropped on Spectators watch instead of playing.";
		area.CutTop(draw.TextWrapped(area, intro, Theme::Body(14.5f, Theme::Text)) + 10.0f);

		const auto seatOf = [&](uint32_t playerId) { return playerId < MaxPlayers ? rules.Seats[playerId] : NoTeam; };
		const auto cycle = [&](uint32_t playerId)
		{
			uint8_t& seat = rules.Seats[playerId];
			if (!teams)
				seat = seat == SpectatorSeat ? NoTeam : SpectatorSeat;
			else
				seat = seat == SpectatorSeat ? uint8_t(0) : seat + 1 < rules.Teams ? uint8_t(seat + 1) : SpectatorSeat;
		};
		std::optional<uint32_t> clicked;
		const auto humanChip = [&](const UiRect& rect, const PlayerInfo& player, UiColor color)
		{
			const UiId id = Ui::MakeId("seatchip", player.PlayerId);
			const UiInteraction interaction = ui.Interact(id, rect);
			ui.OfferDrag(id, SeatPayload, int(player.PlayerId));
			const float hover = ui.Animate(id, interaction.Hovered);
			if (interaction.Clicked)
				clicked = player.PlayerId;
			const bool watching = seatOf(player.PlayerId) == SpectatorSeat;
			draw.Rect(rect, { .Fill = Lighten(Rgba(0.07f, 0.055f, 0.13f, 0.95f), 0.06f * hover), .FillBottom = Rgba(0.035f, 0.03f, 0.07f, 0.95f), .Border = WithAlpha(Lighten(color, 0.25f * hover), 0.55f + 0.4f * hover), .BorderWidth = 1.2f, .Chamfer = 6.0f });
			draw.Icon(watching ? UiIcon::Eye : UiIcon::Hood, { rect.X + 8.0f, rect.Center().y - 9.0f, 18.0f, 18.0f }, { .Color = Lighten(color, 0.45f), .ColorBottom = color, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			const char* tag = player.PlayerId == self ? "you" : player.Host ? "host" : "";
			const TextStyle tagStyle = Theme::Body(12.0f, Theme::Faint);
			const float tagWidth = *tag ? draw.MeasureWidth(tag, tagStyle) + 8.0f : 0.0f;
			draw.TextFit({ rect.X + 32.0f, rect.Y, rect.W - 40.0f - tagWidth, rect.H }, player.Name, Theme::Bold(14.5f, Lighten(color, 0.55f)));
			if (*tag)
				draw.TextAligned({ rect.Right() - tagWidth - 4.0f, rect.Y, tagWidth, rect.H }, tag, tagStyle, { 0.5f, 0.5f });
			if (interaction.Hovered && !ui.GetDrag())
				ui.Tooltip().Header(player.Name, color, std::nullopt, watching ? UiIcon::Eye : UiIcon::Hood).Text(teams ? "Click to move to the next team, then to the spectators." : watching ? "Watching. Click to let this mage play." : "Playing. Click to make this mage watch instead.").Muted("You can also drag the mage where you want it.");
		};
		const auto botChip = [&](const UiRect& rect, UiColor color, UiId id)
		{
			draw.Rect(rect, { .Fill = Rgba(0.05f, 0.045f, 0.09f, 0.9f), .FillBottom = Rgba(0.03f, 0.025f, 0.05f, 0.9f), .Border = WithAlpha(color, 0.35f), .BorderWidth = 1.0f, .Chamfer = 6.0f });
			draw.Icon(UiIcon::Cog, { rect.X + 9.0f, rect.Center().y - 8.0f, 16.0f, 16.0f }, { .Color = WithAlpha(Lighten(color, 0.3f), 0.8f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextFit({ rect.X + 32.0f, rect.Y, rect.W - 64.0f, rect.H }, std::string(BotLevelNames[std::min<size_t>(rules.BotLevel, BotLevels - 1)]) + " bot", Theme::Body(14.0f, Theme::Muted));
			const UiRect remove{ rect.Right() - 28.0f, rect.Center().y - 11.0f, 22.0f, 22.0f };
			const UiInteraction interaction = ui.Interact(id, remove);
			const float hover = ui.Animate(id, interaction.Hovered);
			draw.Icon(UiIcon::CrossMark, remove.Shrink(4.0f), { .Color = glm::mix(Theme::Faint, Theme::Danger, hover) });
			if (interaction.Hovered)
				ui.Tooltip().Text("Remove this bot");
			return interaction.Clicked;
		};

		constexpr float ChipHeight = 30.0f;
		constexpr float ChipGap = 5.0f;
		constexpr float SpectatorHeight = 82.0f;
		const float buttonsHeight = teams ? 46.0f : 0.0f;
		const UiRect columns = area.CutTop(std::max(area.H - SpectatorHeight - buttonsHeight - 10.0f, 120.0f));
		area.CutTop(10.0f);
		const uint32_t count = teams ? rules.Teams : 1u;
		constexpr float ColumnGap = 10.0f;
		const float columnWidth = teams ? (columns.W - ColumnGap * float(count - 1)) / float(count) : std::min(columns.W, 300.0f);
		std::optional<std::pair<uint32_t, uint8_t>> moved;
		for (uint32_t column = 0; column < count; column++)
		{
			const uint8_t team = teams ? uint8_t(column) : NoTeam;
			const UiRect box{ columns.X + float(column) * (columnWidth + ColumnGap), columns.Y, columnWidth, columns.H };
			const UiColor color = teams ? WithAlpha(TeamColor(team)) : Theme::Accent;
			const UiPayload* drag = ui.GetDrag();
			const bool target = drag && drag->Type == SeatPayload && box.Contains(ui.GetMouse());
			draw.Rect(box, { .Fill = WithAlpha(Darken(color, 0.82f), 0.55f), .FillBottom = Rgba(0.02f, 0.015f, 0.04f, 0.6f), .Border = WithAlpha(color, target ? 0.95f : 0.45f), .BorderWidth = target ? 1.8f : 1.0f, .Glow = WithAlpha(color, target ? 0.25f : 0.0f), .GlowSize = 12.0f, .Chamfer = 9.0f });
			UiRect inner = box.Shrink(8.0f, 8.0f);
			const UiRect header = inner.CutTop(26.0f);
			uint32_t mages = 0;
			for (const PlayerInfo* player : humans)
				mages += teams ? (seatOf(player->PlayerId) == team ? 1u : 0u) : (seatOf(player->PlayerId) != SpectatorSeat ? 1u : 0u);
			const uint32_t bots = teams ? rules.TeamBots[team] : rules.Bots;
			draw.TextAligned(header, teams ? GetTeamName(team) : "Playing", Theme::Bold(16.0f, Lighten(color, 0.4f)), { 0.0f, 0.5f });
			draw.TextAligned(header, std::to_string(mages + bots), Theme::Title(18.0f), { 1.0f, 0.5f });
			inner.CutTop(6.0f);
			for (const PlayerInfo* player : humans)
			{
				const uint8_t seat = seatOf(player->PlayerId);
				if ((teams && seat != team) || (!teams && seat == SpectatorSeat))
					continue;
				if (inner.H < ChipHeight)
					break;
				humanChip(inner.CutTop(ChipHeight), *player, teams ? color : WithAlpha(ColorOf(player->PlayerId)));
				inner.CutTop(ChipGap);
			}
			for (uint32_t bot = 0; bot < bots && inner.H >= ChipHeight + 36.0f; bot++)
			{
				if (botChip(inner.CutTop(ChipHeight), color, Ui::MakeId("seatbot", column * 32u + bot)))
				{
					if (teams)
						rules.TeamBots[team]--;
					else
						rules.Bots--;
				}
				inner.CutTop(ChipGap);
			}
			if (inner.H >= 30.0f && ui.Button(Ui::MakeId("seataddbot", column), inner.CutTop(30.0f), "Add a bot", ButtonKind::Normal, CountRuleBots(rules) < room, UiIcon::Cog))
			{
				if (teams)
					rules.TeamBots[team]++;
				else
					rules.Bots++;
			}
			if (const std::optional<int> dropped = ui.AcceptDrop(box, SeatPayload))
				moved = { uint32_t(*dropped), team };
		}

		const UiRect spectators = area.CutTop(SpectatorHeight);
		{
			const UiPayload* drag = ui.GetDrag();
			const bool target = drag && drag->Type == SeatPayload && spectators.Contains(ui.GetMouse());
			draw.Rect(spectators, { .Fill = Rgba(0.04f, 0.035f, 0.08f, 0.6f), .Border = WithAlpha(target ? Theme::Astral : Theme::Edge, target ? 0.95f : 0.5f), .BorderWidth = target ? 1.8f : 1.0f, .Glow = WithAlpha(Theme::Astral, target ? 0.2f : 0.0f), .GlowSize = 12.0f, .Chamfer = 9.0f });
			UiRect inner = spectators.Shrink(10.0f, 8.0f);
			UiRect header = inner.CutTop(22.0f);
			draw.Icon(UiIcon::Eye, header.CutLeft(18.0f).Shrink(0.0f, 2.0f), { .Color = Theme::Astral });
			header.CutLeft(8.0f);
			draw.TextAligned(header, "Spectators", Theme::Bold(15.0f, Theme::AccentLight), { 0.0f, 0.5f });
			inner.CutTop(6.0f);
			UiRect line = inner.CutTop(ChipHeight);
			bool any = false;
			for (const PlayerInfo* player : humans)
			{
				if (seatOf(player->PlayerId) != SpectatorSeat || line.W < 120.0f)
					continue;
				humanChip(line.CutLeft(std::min(180.0f, line.W)), *player, Theme::Astral);
				line.CutLeft(8.0f);
				any = true;
			}
			if (!any)
				draw.TextAligned(line, "Nobody is watching. Drop a mage here to let them spectate.", Theme::Body(13.5f, Theme::Faint), { 0.0f, 0.5f });
			if (const std::optional<int> dropped = ui.AcceptDrop(spectators, SeatPayload))
				moved = { uint32_t(*dropped), SpectatorSeat };
		}
		if (moved && moved->first < MaxPlayers)
			rules.Seats[moved->first] = moved->second;
		else if (clicked && *clicked < MaxPlayers)
			cycle(*clicked);

		if (teams)
		{
			area.CutTop(10.0f);
			UiRect row = area.CutTop(36.0f);
			if (ui.Button(Ui::MakeId("seatshuffle"), row.CutLeft(220.0f), "Shuffle the mages", ButtonKind::Normal, true, UiIcon::Cycle))
			{
				std::vector<uint32_t> playing;
				for (const PlayerInfo* player : humans)
					if (seatOf(player->PlayerId) != SpectatorSeat)
						playing.push_back(player->PlayerId);
				Random random(uint64_t(m_Time * 1000.0f) * 2654435761ull + 17u);
				for (size_t i = playing.size(); i > 1; i--)
					std::swap(playing[i - 1], playing[random.Below(uint32_t(i))]);
				const uint32_t bots = CountTeamBots(rules);
				rules.TeamBots.fill(0);
				const uint32_t first = random.Below(rules.Teams);
				for (size_t i = 0; i < playing.size(); i++)
					rules.Seats[playing[i]] = uint8_t((first + i) % rules.Teams);
				AddBrawlBot(int(bots));
			}
			row.CutLeft(10.0f);
			if (ui.Button(Ui::MakeId("seateven"), row.CutLeft(220.0f), "Even out the bots", ButtonKind::Normal, CountTeamBots(rules) > 0, UiIcon::LifeInTheBalance))
			{
				const uint32_t bots = CountTeamBots(rules);
				rules.TeamBots.fill(0);
				AddBrawlBot(int(bots));
			}
		}

		if (const UiPayload* drag = ui.GetDrag(); drag && drag->Type == SeatPayload)
			if (const PlayerInfo* player = m_Client.FindPlayer(uint32_t(drag->Value)))
			{
				ui.SetLayer(UiLayer::Overlay);
				UiDrawList& top = ui.Draw();
				const UiRect ghost = UiRect::Centered(ui.GetMouse() + glm::vec2(0.0f, 6.0f), { 170.0f, 30.0f });
				top.Shadow(ghost, 10.0f, 14.0f, Rgba(0.0f, 0.0f, 0.0f, 0.5f), { 0.0f, 4.0f });
				top.Rect(ghost, { .Fill = Rgba(0.08f, 0.065f, 0.15f, 0.96f), .Border = Theme::Astral, .BorderWidth = 1.6f, .Chamfer = 6.0f });
				top.Icon(UiIcon::Hood, { ghost.X + 8.0f, ghost.Center().y - 9.0f, 18.0f, 18.0f }, { .Color = Theme::AccentLight });
				top.TextFit({ ghost.X + 32.0f, ghost.Y, ghost.W - 40.0f, ghost.H }, player->Name, Theme::Bold(14.5f, Theme::Text));
				ui.SetLayer(UiLayer::Windows);
			}
	}

	void SessionState::DrawGateFooter(UiRect area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const MatchRules& rules = m_BrawlRules;
		uint32_t playing = 0;
		std::vector<std::string> watchers;
		for (const PlayerInfo& player : m_Client.GetPlayers())
		{
			if (player.Bot || player.PlayerId >= MaxPlayers)
				continue;
			if (rules.Seats[player.PlayerId] == SpectatorSeat)
				watchers.push_back(player.PlayerId == m_Client.GetPlayerId() ? std::string("you") : player.Name);
			else
				playing++;
		}
		if (m_Client.GetPlayers().empty())
			playing = 1;
		const uint32_t bots = CountRuleBots(rules);
		std::string party;
		if (playing + bots == 0)
			party = "Nobody is playing. Add bots, or let a mage play";
		else if (playing + bots == 1)
			party = bots == 1 ? "A single bot fights alone. Add another mage or bot" : "Nobody else is here yet. Add a few bots, or wait for friends to join";
		else if (rules.Teams >= 2)
		{
			const std::array<uint32_t, MaxTeams> members = CountTeamMembers();
			for (uint8_t team = 0; team < rules.Teams; team++)
				party += std::string(team == 0 ? "" : "  against  ") + GetTeamName(team) + " " + std::to_string(members[team]);
		}
		else
			party = Format("%u mages step through together", unsigned(playing + bots)) + (bots == 0 ? std::string() : bots == 1 ? std::string(", one of them a bot") : Format(", %u of them bots", unsigned(bots)));
		if (!watchers.empty())
		{
			std::string names;
			for (size_t i = 0; i < watchers.size(); i++)
				names += (i == 0 ? "" : i + 1 == watchers.size() ? " and " : ", ") + watchers[i];
			party += ". " + names + (watchers.size() == 1 && watchers[0] != "you" ? " watches" : " watch");
		}
		draw.TextFit(area.CutTop(22.0f), party, Theme::Body(14.0f, playing + bots >= 2 ? Theme::Success : Theme::Muted));
		const std::string custom = CustomSummary(rules);
		if (!custom.empty())
			draw.TextFit(area.CutTop(20.0f), "Custom rules: " + custom, Theme::Body(13.5f, Theme::Gold));
		area.CutTop(10.0f);
		if (ui.Button(Ui::MakeId("gateenter"), area.CutTop(48.0f), EnterLabel(), ButtonKind::Primary, playing + bots > 0, UiIcon::Whirlwind))
		{
			m_Client.StartBrawl(ChosenArena(), m_BrawlRules);
			m_Station.reset();
		}
	}

	void SessionState::SyncSpellbook()
	{
		const SessionInfo& session = m_Client.GetSession();
		const GameStatus& status = m_Client.GetStatus();
		const bool choosing = session.Mode == GameMode::Brawl && IsChoosing(status);
		if ((session.Mode != GameMode::Hub && !choosing) || m_SpellsSentEpoch == session.Epoch || !m_Client.FindEntity(m_Client.GetPlayerNetId()))
			return;
		m_SpellsSentEpoch = session.Epoch;
		m_Client.SendPreferredSpells();
	}

	void SessionState::UpdateWonders()
	{
		const Level* level = m_Client.GetLevel();
		if (!level || !level->Floating || m_Client.GetSession().Mode != GameMode::Hub)
		{
			m_StarlitKnown = false;
			m_SeenLeviathan = -1.0f;
			return;
		}
		GameAudio& audio = m_App.GetGameAudio();
		const uint64_t lit = m_Client.GetProfile().Starlit;
		if (m_StarlitKnown && lit != m_SeenStarlit)
		{
			for (const Constellation& star : level->Constellations)
				if (((lit & ~m_SeenStarlit) >> size_t(star.Spell)) & 1u)
				{
					const glm::vec3 center(star.Position.x, star.Height + ProjectileHeight, star.Position.y);
					m_Effects->Burst(Fx::ArcaneBurst, center, glm::vec3(0.0f, 1.0f, 0.0f), 2.2f, SkillColor(star.Spell));
					m_Effects->Burst(Fx::Blink, center, glm::vec3(0.0f, 1.0f, 0.0f), 1.6f, SkillColor(star.Spell));
				}
			audio.PlayUi(Cue::UiRare, 0.8f);
			audio.PlayUi(Cue::Bell, 0.35f, 1.5f, 0.15f);
		}
		m_SeenStarlit = lit;
		m_StarlitKnown = true;

		const float pass = LeviathanPass();
		if (pass >= 0.2f && m_SeenLeviathan < 0.2f)
		{
			audio.PlayUi(Cue::Horn, 0.3f, 0.42f);
			audio.PlayUi(Cue::Rumble, 0.25f, 0.55f, 0.4f);
		}
		if (pass >= 0.62f && m_SeenLeviathan < 0.62f)
			audio.PlayUi(Cue::Horn, 0.24f, 0.36f);
		m_SeenLeviathan = pass;
	}

	void SessionState::DrawWonders(WorldRenderer& renderer) const
	{
		const Level* level = m_Client.GetLevel();
		if (!level || !level->Floating || m_Client.GetSession().Mode != GameMode::Hub)
			return;
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		const Vec2 eye = self ? self->Position : Vec2(m_CameraTarget.x, m_CameraTarget.z);
		const Landmark* observatory = level->FindLandmark(LandmarkKind::Observatory);
		const float near = observatory ? 1.0f - Smooth(4.0f, 15.0f, Length(eye - observatory->Position)) : 0.0f;
		const uint64_t lit = m_Client.GetProfile().Starlit;
		for (size_t index = 0; index < level->Constellations.size(); index++)
		{
			const Constellation& star = level->Constellations[index];
			const Sigil* sigil = FindSigil(star.Spell);
			if (!sigil || LengthSquared(star.Position - eye) > 30.0f * 30.0f)
				continue;
			const bool on = ((lit >> size_t(star.Spell)) & 1u) != 0;
			const float phase = float(index) * 1.93f;
			const glm::vec3 spell = SkillColor(star.Spell);
			const glm::vec3 color = on ? spell / std::max({ spell.r, spell.g, spell.b, 1e-3f }) : StarlightPale;
			const float strength = on ? 1.0f : 0.3f + 0.7f * near;
			const glm::vec3 center(star.Position.x, star.Height + ProjectileHeight + 0.08f * std::sin(m_Time * 0.6f + phase), star.Position.y);
			const auto point = [&](uint32_t i) { return center + glm::vec3(sigil->Points[i].x, 0.0f, sigil->Points[i].y) * SigilScale; };
			for (uint32_t i = 0; i < sigil->Lines; i++)
				DrawStreak(renderer, point(sigil->Links[i][0]), point(sigil->Links[i][1]), on ? 0.035f : 0.02f, color * ((on ? 0.9f : 0.22f) * strength));
			for (uint32_t i = 0; i < sigil->Stars; i++)
			{
				const float twinkle = 0.7f + 0.3f * std::sin(m_Time * (1.3f + 0.37f * float(i)) + float(i) * 2.1f + phase);
				DrawStar(renderer, point(i), (on ? 0.2f : 0.13f) * (0.85f + 0.15f * twinkle), color * ((on ? 2.0f : 1.5f) * strength * twinkle), phase + float(i));
			}
			if (on)
				DrawGlow(renderer, center, 1.3f, color * (0.1f + 0.03f * std::sin(m_Time * 1.1f + phase)), 0.8f);
		}

		if (const Landmark* islet = level->FindLandmark(LandmarkKind::CatIslet); islet && LengthSquared(islet->Position - eye) < 30.0f * 30.0f)
		{
			const Vec2 seat = islet->Position + Vec2(-0.45f, -0.35f);
			constexpr float CatYaw = -2.6f;
			float gaze = CatYaw + 0.5f * std::sin(m_Time * 0.13f) + 0.25f * std::sin(m_Time * 0.41f);
			if (self && LengthSquared(self->Position - seat) < 7.0f * 7.0f && LengthSquared(self->Position - seat) > 0.01f)
				gaze = AngleOf(self->Position - seat);
			const float turn = std::clamp(std::remainder(CatYaw - gaze, 2.0f * Pi), -1.2f, 1.2f);
			const glm::vec3 base(seat.x, level->Map.GetHeight(seat), seat.y);
			const glm::mat4 place = ModelTransform(base, CatYaw, 1.4f);
			const glm::mat4 identity(1.0f);
			const glm::vec3 neck(0.0f, 0.25f, 0.06f);
			const glm::vec3 rump(0.0f, 0.045f, -0.11f);
			DrawCritter(renderer, Critter::CatBody, place, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, base.y);
			DrawCritter(renderer, Critter::CatHead, place * glm::translate(identity, neck) * glm::rotate(identity, turn, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::translate(identity, -neck), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, base.y);
			DrawCritter(renderer, Critter::CatTail, place * glm::translate(identity, rump) * glm::rotate(identity, 0.18f * std::sin(m_Time * 1.6f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::translate(identity, -rump), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, base.y);
		}

		const TileMap& map = level->Map;
		const ParticleSprite step{ .Size = 0.9f, .Color = glm::vec4(0.32f, 0.45f, 1.0f, 0.55f), .Shape = ParticleShape::Glow, .Align = ParticleAlign::Ground, .Softness = 1.0f };
		for (const EntityState& entity : m_Client.GetEntities())
			if (entity.Kind == EntityKind::Player && !entity.Has(EntityFlags::Falling) && !entity.Has(EntityFlags::Lifted) && map.GetTerrain(int32_t(entity.Position.x), int32_t(entity.Position.y)) == TerrainId::Starpath)
			{
				ParticleSprite glow = step;
				glow.Position = Ground(entity.Position, 0.03f);
				renderer.DrawSprite(glow);
			}
		const int32_t reach = 6;
		for (int32_t y = int32_t(eye.y) - reach; y <= int32_t(eye.y) + reach; y++)
			for (int32_t x = int32_t(eye.x) - reach; x <= int32_t(eye.x) + reach; x++)
			{
				if (map.GetTerrain(x, y) != TerrainId::Starpath)
					continue;
				const float seed = Twinkle(uint32_t(x), uint32_t(y));
				const float pulse = std::sin(m_Time * (0.7f + 0.5f * seed) + seed * 40.0f);
				if (pulse < 0.9f)
					continue;
				const float spark = (pulse - 0.9f) / 0.1f;
				const glm::vec3 at(float(x) + 0.25f + 0.5f * Twinkle(uint32_t(y), uint32_t(x)), map.GetHeight({ float(x) + 0.5f, float(y) + 0.5f }) + 0.05f, float(y) + 0.25f + 0.5f * seed);
				DrawStar(renderer, at, 0.09f * spark, StarlightPale * (2.2f * spark), seed * 6.0f);
			}
	}

	glm::vec3 SessionState::BalloonAnchor(const EntityState& entity) const
	{
		constexpr float Height = 0.95f;
		constexpr float Depth = 14.0f;
		constexpr float Rise = 4.0f;
		float along = Rise;
		if (const Level* level = m_Client.GetLevel())
		{
			float best = 1e9f;
			for (const DriftLane& lane : level->DriftLanes)
			{
				const float t = ClosestPointParameter(lane.From, lane.To, entity.Position);
				const float distance = LengthSquared(Lerp(lane.From, lane.To, t) - entity.Position);
				if (distance >= best)
					continue;
				best = distance;
				along = std::min(t, 1.0f - t) * Length(lane.To - lane.From);
			}
		}
		const float rise = std::clamp(along / Rise, 0.0f, 1.0f);
		const float bob = 0.12f * std::sin(m_Time * 1.7f + float(entity.NetId));
		return { entity.Position.x, Height + bob - Depth * (1.0f - rise * rise * (3.0f - 2.0f * rise)), entity.Position.y };
	}

	void SessionState::DrawCritter(WorldRenderer& renderer, Critter critter, const glm::mat4& transform, const glm::vec4& tint, float emission, float ground, const glm::vec4& flash) const
	{
		for (const uint32_t mesh : m_Critters[size_t(critter)])
			renderer.DrawModel(mesh, transform, NoPalette, tint, emission, ground, glm::vec4(0.0f), 0u, flash);
	}

	void SessionState::DrawBalloon(WorldRenderer& renderer, const EntityState& entity) const
	{
		const auto fx = m_Fx.find(entity.NetId);
		const float fall = fx != m_Fx.end() ? std::max(fx->second.FallAge, 0.0f) : 0.0f;
		const bool popped = entity.Has(EntityFlags::Falling);
		const float yaw = entity.Facing + (popped ? fall * 7.0f : 0.0f);
		const Vec2 facing = FromAngle(yaw);
		const glm::vec3 forward(facing.x, 0.0f, facing.y);
		const glm::vec3 side(-forward.z, 0.0f, forward.x);
		glm::vec3 base = BalloonAnchor(entity) - glm::vec3(0.0f, 0.25f, 0.0f);
		if (popped)
			base.y -= 0.5f * FallGravity * fall * fall;
		DrawCritter(renderer, Critter::Duck, ModelTransform(base, yaw, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, base.y);
		if (popped)
			return;
		const float sway = 0.08f * std::sin(m_Time * 1.3f + float(entity.NetId) * 0.7f);
		const glm::vec3 knot = base + glm::vec3(0.0f, 0.28f + BalloonLift - 0.5f, 0.0f) + side * sway;
		const glm::vec3 tie = base + glm::vec3(0.0f, 0.33f, 0.0f) - forward * 0.02f;
		const glm::vec3 string = knot - tie;
		renderer.Draw(MeshKind::Cylinder, (knot + tie) * 0.5f, { 0.006f, glm::length(string) * 0.5f, 0.006f }, glm::vec3(0.85f, 0.82f, 0.75f), 0.0f, 0.0f, base.y);
		DrawCritter(renderer, Critter::Balloon, ModelTransform(knot + glm::vec3(0.0f, 0.045f, 0.0f), yaw + sway, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, base.y);
	}

	void SessionState::DrawDummy(WorldRenderer& renderer, const EntityState& entity) const
	{
		const DummyKind kind = DummyKind(entity.Variant);
		if (kind == DummyKind::Balloon)
		{
			DrawBalloon(renderer, entity);
			return;
		}
		const auto fx = m_Fx.find(entity.NetId);
		const float since = fx != m_Fx.end() ? fx->second.SinceHit : 10.0f;
		const float wobble = std::sin(since * 19.0f) * std::exp(-since * 4.5f) * 0.14f;
		glm::vec3 base = Ground(entity.Position);
		const bool falling = fx != m_Fx.end() && fx->second.FallAge >= 0.0f;
		if (falling)
			base.y = fx->second.FallFrom - 0.5f * FallGravity * fx->second.FallAge * fx->second.FallAge;
		if (kind == DummyKind::Duck)
		{
			base.y = (falling ? base.y : std::max(base.y, LavaSurface + 0.02f)) + 0.035f * std::sin(m_Time * 3.1f + float(entity.NetId)) + std::abs(wobble) - 0.06f;
			DrawCritter(renderer, Critter::Duck, ModelTransform(base, entity.Facing, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, base.y);
			return;
		}
		const bool golden = kind == DummyKind::Golden;
		const bool burning = entity.Has(EntityFlags::Burning);
		if (burning)
			base.y -= 0.9f * (1.0f - float(entity.Health) / 255.0f);
		const glm::vec4 tint = golden ? glm::vec4(1.75f, 1.45f, 0.55f, 0.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
		const float shine = golden ? 0.22f + 0.08f * std::sin(m_Time * 4.0f) : 0.0f;
		const glm::mat4 place = ModelTransform(base, entity.Facing, 1.0f);
		const glm::mat4 identity(1.0f);
		const glm::vec3 joint(0.0f, 0.74f, 0.0f);
		DrawCritter(renderer, Critter::DummyPost, place, tint, shine * 0.5f, base.y, HitLight(entity));
		DrawCritter(renderer, Critter::DummyBody, place * glm::translate(identity, joint) * glm::rotate(identity, wobble * 1.15f, glm::vec3(0.0f, 0.0f, 1.0f)) * glm::translate(identity, -joint), tint, shine, base.y, HitLight(entity));
		if (golden)
			DrawGlow(renderer, base + glm::vec3(0.0f, 1.3f, 0.0f), 0.9f, glm::vec3(1.0f, 0.7f, 0.2f) * (0.25f + 0.1f * std::sin(m_Time * 4.0f)));
		DrawBrawlPlayer(renderer, entity, Ground(entity.Position));
	}

	namespace
	{
		constexpr const char* RunKeys[] = { "RMB", "Q", "R", "F" };
		static_assert(std::size(RunKeys) == SkillSlots);
		constexpr const char* RomanRanks[] = { "", "I", "II", "III", "IV", "V" };
		static_assert(std::size(RomanRanks) == size_t(MaxSpellRank) + 1);

		std::string RunClock(float seconds)
		{
			const unsigned total = unsigned(std::max(seconds, 0.0f));
			return Format("%u:%02u", total / 60u, total % 60u);
		}

		bool DealsDamage(AbilityId spell)
		{
			const RunSpellDef* def = FindRunSpell(spell);
			return def && def->Damage > 0;
		}

		std::string RankStep(AbilityId spell, uint8_t rank)
		{
			std::vector<std::string> parts;
			if (DealsDamage(spell))
				parts.push_back(Format("+%.0f%% damage", double(RankDamage * 100.0f)));
			if (rank % 2 == 0)
				parts.push_back(Format("%.0f%% faster cooldown", double((1.0f - RankCooldown) * 100.0f)));
			else if (const RankMilestone* milestone = MilestoneAt(spell, rank))
				parts.push_back(milestone->Text);
			std::string text;
			for (size_t i = 0; i < parts.size(); i++)
				text += (i == 0 ? "" : ", ") + parts[i];
			return text;
		}

		std::string RunCardName(const RunCard& card)
		{
			switch (card.Kind)
			{
			case RunCardKind::Spell:
				return GetAbility(card.Spell()).Name;
			case RunCardKind::Rank:
				return std::string(GetAbility(card.Spell()).Name) + " rank up";
			case RunCardKind::Card:
				return GetCard(card.Upgrade()).Name;
			case RunCardKind::Rune:
				return std::string(GetRune(card.Rune()).Name) + " rune";
			default:
				return "Skipped";
			}
		}

		UiIcon RunCardIcon(const RunCard& card)
		{
			if (card.Kind == RunCardKind::Rune)
				return RuneIcon(card.Rune());
			return card.Kind == RunCardKind::Card ? IconFor(card.Upgrade()) : card.Kind == RunCardKind::None ? UiIcon::Hourglass : IconFor(card.Spell());
		}

		const char* SpellFormName(AbilityId spell)
		{
			const uint8_t forms = SpellForms(spell);
			return (forms & FormBlast) ? "Exploding projectile" : (forms & FormBolt) ? "Projectile" : (forms & FormChain) ? "Chaining projectile" : (forms & FormHook) ? "Hook" : (forms & FormStrike) ? "Ground spell" : (forms & FormNova) ? "Nova" : "Utility spell";
		}

		void DrawRuneStone(UiDrawList& draw, RuneId rune, glm::vec2 center, float radius, float alpha = 1.0f, float lift = 0.0f)
		{
			const UiColor tone = RuneTone(rune);
			const bool rare = GetRune(rune).Rarity >= CardRarity::Rare;
			draw.Circle(center + glm::vec2(0.0f, 2.0f), radius + 2.0f, Rgba(0.0f, 0.0f, 0.0f, 0.45f * alpha));
			draw.Circle(center, radius, WithAlpha(Darken(tone, 0.8f), 0.97f * alpha), WithAlpha(rare ? Theme::GoldLight : Lighten(tone, 0.15f), (0.75f + 0.25f * lift) * alpha), rare ? 1.8f : 1.3f);
			draw.Circle(center, radius * 0.76f, WithAlpha(Darken(tone, 0.6f), 0.5f * alpha));
			draw.Icon(RuneIcon(rune), UiRect::Centered(center, { radius * 1.3f, radius * 1.3f }), { .Color = WithAlpha(Lighten(tone, 0.6f), alpha), .ColorBottom = WithAlpha(tone, alpha), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f * alpha), .GlowColor = WithAlpha(tone, (0.2f + 0.3f * lift) * alpha) });
		}

		void DescribeRune(UiTooltip& tooltip, RuneId rune, const RunLoadout* loadout = nullptr)
		{
			const RuneDef& def = GetRune(rune);
			const UiColor tone = RuneTone(rune);
			tooltip.Accent = tone;
			tooltip.Header(def.Name, Lighten(tone, 0.25f), std::nullopt, RuneIcon(rune));
			tooltip.Muted(std::string(def.Rarity >= CardRarity::Rare ? "Rare " : "Common ") + (def.Kind == RuneKind::Trigger ? "trigger rune" : "rune"));
			tooltip.Space();
			tooltip.Text(def.Text);
			tooltip.Separator();
			tooltip.Pair("Fits", def.Needs);
			if (!loadout)
				return;
			const RunePlace place = FindRune(*loadout, rune);
			if (!place.IsSet())
				return;
			tooltip.Pair("Socketed in", GetAbility(SlotSpell(*loadout, uint32_t(place.Slot))).Name);
			if (rune == RuneId::Link)
			{
				const AbilityId linked = LinkedSpell(*loadout, uint32_t(place.Slot));
				tooltip.Pair("Casts", linked != AbilityId::Count ? GetAbility(linked).Name : "nothing yet", Theme::Muted, linked != AbilityId::Count ? WithAlpha(SkillColor(linked)) : Theme::Faint);
			}
		}

		UiColor RunCardColor(const RunCard& card)
		{
			if (card.Kind == RunCardKind::Rune)
				return RuneTone(card.Rune());
			if (card.Kind == RunCardKind::Card)
				return CardTint(GetCard(card.Upgrade()));
			if (card.Kind == RunCardKind::None)
				return Theme::Faint;
			return WithAlpha(SkillColor(card.Spell()));
		}
	}

	void SessionState::DrawRunCardFace(const UiRect& rect, const RunCard& card, float hover, std::optional<uint32_t> key, float reveal, bool banishing)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const UiRect face = rect.Translate({ 0.0f, -8.0f * hover });
		if (card.Kind == RunCardKind::Card)
			DrawCardFace(rect, card.Upgrade(), hover, 1.0f, key, false, reveal, 0.0f, 0.0f);
		else if (card.Kind >= RunCardKind::Sigil)
		{
			const RunItemLook look = ItemLookOf(card);
			const float fade = Smooth(0.0f, 0.35f, reveal);
			const bool small = face.W < 200.0f;
			UiRect body = face.Shrink(18.0f, 16.0f);
			const UiRect tagRect = body.CutTop(18.0f);
			const float iconSize = std::clamp(face.H * 0.22f, 40.0f, 72.0f);
			body.CutTop(face.H * 0.04f);
			const UiRect iconRect = UiRect::Centered({ body.Center().x, body.Y + iconSize * 0.5f + 6.0f }, { iconSize, iconSize });
			draw.Card(face, { .Color = look.Color, .Rarity = look.Rarity, .Margin = look.Rarity > 0 ? 40.0f : 28.0f, .Hover = hover, .Reveal = reveal, .Mouse = ui.GetMouse(), .MedallionY = iconRect.Center().y, .MedallionRadius = iconSize * 0.68f, .Seed = float(card.Id) * 0.53f + float(card.Kind) });
			TextStyle tagStyle = Theme::Bold(small ? 10.5f : 12.0f, WithAlpha(Lighten(look.Color, 0.25f), fade));
			tagStyle.Tracking = 1.8f;
			draw.TextAligned(tagRect, look.Tag, tagStyle, { 0.5f, 0.5f });
			draw.Icon(look.Icon, iconRect, { .Color = WithAlpha(Lighten(look.Color, 0.55f), fade), .ColorBottom = WithAlpha(look.Color, fade), .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f * fade), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f * fade), .GlowColor = WithAlpha(look.Color, 0.3f * fade) });
			body.CutTop(iconSize + 22.0f);
			TextStyle nameStyle = Theme::Title(small ? 16.0f : 20.0f);
			nameStyle.Color = WithAlpha(Lighten(look.Color, 0.45f), fade);
			nameStyle.ShadowColor = WithAlpha(nameStyle.ShadowColor, fade);
			draw.TextFit(body.CutTop(small ? 22.0f : 28.0f), look.Name, nameStyle, { 0.5f, 0.5f });
			body.CutTop(6.0f);
			const UiRect footer = body.CutBottom(18.0f);
			draw.TextWrapped(body, look.Text, Theme::Body(small ? 12.0f : 14.0f, WithAlpha(Theme::Muted, fade)), 0.5f, 1.05f);
			TextStyle footStyle = Theme::Bold(small ? 10.0f : 11.5f, WithAlpha(look.Color, 0.9f * fade));
			footStyle.Tracking = 1.4f;
			draw.TextAligned(footer, look.Footer, footStyle, { 0.5f, 0.5f });
			if (key)
			{
				const UiRect badge{ face.Right() - 32.0f, face.Y + 10.0f, 22.0f, 20.0f };
				draw.Rect(badge, { .Fill = Rgba(0.04f, 0.035f, 0.07f, 0.95f * fade), .Border = WithAlpha(Theme::Edge, 0.9f * fade), .BorderWidth = 1.0f, .Chamfer = 4.0f });
				draw.TextAligned(badge, std::to_string(*key + 1), Theme::Bold(12.5f, WithAlpha(Theme::AccentLight, fade)), { 0.5f, 0.5f });
			}
		}
		else
		{
			const AbilityId spell = card.Spell();
			const AbilityDef& def = GetAbility(spell);
			const bool fresh = card.Kind == RunCardKind::Spell;
			const RunLoadout& loadout = m_Client.GetLocalPlayer().Run;
			const uint8_t rank = std::clamp<uint8_t>(RankOf(loadout, spell), 1, MaxSpellRank - 1);
			const UiColor tint = WithAlpha(SkillColor(spell));
			const bool milestone = !fresh && MilestoneAt(spell, uint8_t(rank + 1)) != nullptr;
			const UiColor frame = fresh || milestone ? Theme::Gold : tint;
			UiRect body = face.Shrink(18.0f, 16.0f);
			const UiRect tagRect = body.CutTop(18.0f);
			const float iconSize = std::clamp(face.H * 0.24f, 44.0f, 80.0f);
			body.CutTop(face.H * 0.04f);
			const UiRect iconRect = UiRect::Centered({ body.Center().x, body.Y + iconSize * 0.5f + 6.0f }, { iconSize, iconSize });
			draw.Card(face, { .Color = frame, .Rarity = fresh || milestone ? 1u : 0u, .Margin = fresh || milestone ? 40.0f : 28.0f, .Hover = hover, .Reveal = reveal, .Mouse = ui.GetMouse(), .MedallionY = iconRect.Center().y, .MedallionRadius = iconSize * 0.68f, .Seed = float(card.Id) * 0.37f + (fresh ? 0.5f : 0.0f) });
			const float fade = Smooth(0.0f, 0.35f, reveal);
			const bool small = face.W < 200.0f;
			TextStyle tagStyle = Theme::Bold(small ? 10.5f : 12.0f, WithAlpha(fresh ? Theme::GoldLight : Lighten(tint, 0.2f), fade));
			tagStyle.Tracking = 1.8f;
			draw.TextAligned(tagRect, fresh ? std::string("NEW SPELL") : small ? Format("RANK %s", RomanRanks[rank + 1]) : Format("RANK %s  \xC2\xB7  %s", RomanRanks[rank + 1], Upper(def.Name).c_str()), tagStyle, { 0.5f, 0.5f });
			draw.Icon(IconFor(spell), iconRect, { .Color = WithAlpha(Lighten(tint, 0.55f), fade), .ColorBottom = WithAlpha(tint, fade), .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f * fade), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f * fade), .GlowColor = WithAlpha(tint, 0.25f * fade) });
			if (!fresh)
				for (uint8_t pip = 1; pip <= MaxSpellRank; pip++)
				{
					const glm::vec2 at{ iconRect.Center().x + (float(pip) - 3.0f) * 13.0f, iconRect.Bottom() + 9.0f };
					const bool lit = pip <= rank;
					const bool next = pip == rank + 1;
					draw.Diamond(at, next ? 10.0f : 8.0f, lit ? WithAlpha(tint, fade) : next ? WithAlpha(Theme::GoldLight, (0.6f + 0.4f * std::sin(m_Time * 6.0f)) * fade) : Rgba(0.05f, 0.04f, 0.08f, 0.9f * fade), WithAlpha(Theme::Edge, fade), 1.0f);
				}
			body.CutTop(iconSize + (fresh ? 22.0f : 30.0f));
			TextStyle nameStyle = Theme::Title(small ? 17.0f : 21.0f);
			nameStyle.Color = WithAlpha(fresh ? Theme::GoldLight : Theme::Text, fade);
			nameStyle.ColorBottom = WithAlpha(fresh ? Theme::Gold : nameStyle.ColorBottom, fade);
			nameStyle.GlowColor = WithAlpha(fresh ? Theme::Gold : nameStyle.GlowColor, (fresh ? 0.35f : 1.0f) * fade);
			nameStyle.ShadowColor = WithAlpha(nameStyle.ShadowColor, fade);
			draw.TextAligned(body.CutTop(small ? 22.0f : 28.0f), def.Name, nameStyle, { 0.5f, 0.5f });
			body.CutTop(6.0f);
			const UiRect footer = body.CutBottom(18.0f);
			const RunSpellDef* pitch = FindRunSpell(spell);
			const std::string step = RankStep(spell, uint8_t(rank + 1));
			const std::string text = fresh ? std::string(pitch && *pitch->Pitch ? pitch->Pitch : def.Description) : small ? std::string(1, char(std::toupper(static_cast<unsigned char>(step[0])))) + step.substr(1) + "." : Format("Rank %s to %s: ", RomanRanks[rank], RomanRanks[rank + 1]) + step + ".";
			draw.TextWrapped(body, text, Theme::Body(small ? 12.0f : 14.5f, WithAlpha(Theme::Muted, fade)), 0.5f, 1.05f);
			TextStyle footStyle = Theme::Bold(small ? 10.0f : 11.5f, WithAlpha(fresh ? Theme::Gold : tint, 0.9f * fade));
			footStyle.Tracking = 1.4f;
			const auto free = std::find(loadout.Spells.begin(), loadout.Spells.end(), AbilityId::Count);
			const std::string slot = free != loadout.Spells.end() ? Format("  \xC2\xB7  ON %s", RunKeys[free - loadout.Spells.begin()]) : std::string();
			draw.TextAligned(footer, fresh ? Upper(ActionWord(spell)) + slot : std::string(milestone ? "MILESTONE" : "RANK UP"), footStyle, { 0.5f, 0.5f });
			if (key)
			{
				const UiRect badge{ face.Right() - 32.0f, face.Y + 10.0f, 22.0f, 20.0f };
				draw.Rect(badge, { .Fill = Rgba(0.04f, 0.035f, 0.07f, 0.95f * fade), .Border = WithAlpha(Theme::Edge, 0.9f * fade), .BorderWidth = 1.0f, .Chamfer = 4.0f });
				draw.TextAligned(badge, std::to_string(*key + 1), Theme::Bold(12.5f, WithAlpha(Theme::AccentLight, fade)), { 0.5f, 0.5f });
			}
		}
		if (banishing)
		{
			const float pulse = 0.6f + 0.4f * std::sin(m_Time * 7.0f);
			draw.Rect(face, { .Fill = Rgba(0.35f, 0.02f, 0.06f, 0.18f + 0.12f * hover), .Border = WithAlpha(Theme::Danger, pulse), .BorderWidth = 2.0f, .Glow = WithAlpha(Theme::Danger, 0.25f * pulse), .GlowSize = 12.0f, .Chamfer = 16.0f });
			draw.TextAligned({ face.X, face.Bottom() - 46.0f, face.W, 20.0f }, "BANISH", Theme::Bold(13.0f, WithAlpha(Theme::Danger, pulse)), { 0.5f, 0.5f });
		}
	}

	void SessionState::DrawRunSpells(UiRect area, const RunLoadout& loadout, float size)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		for (uint32_t slot = 0; slot < AbilitySlots && area.W >= size; slot++)
		{
			const AbilityId spell = slot == 0 ? BrawlBasic : loadout.Spells[slot - 1];
			const UiRect tile = UiRect::Centered({ area.X + size * 0.5f, area.Center().y }, { size, size });
			area.CutLeft(size + 8.0f);
			const bool empty = spell == AbilityId::Count;
			const UiColor color = empty ? Theme::Faint : WithAlpha(SkillColor(spell));
			draw.Rect(tile, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Border = WithAlpha(color, empty ? 0.4f : 0.85f), .BorderWidth = 1.2f, .Chamfer = 6.0f });
			if (empty)
			{
				draw.Icon(UiIcon::Emerald, tile.Shrink(size * 0.28f), { .Color = Rgba(0.62f, 0.56f, 0.9f, 0.2f) });
				continue;
			}
			draw.Icon(IconFor(spell), tile.Shrink(size * 0.17f), { .Color = Lighten(color, 0.5f), .ColorBottom = color, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			const uint8_t rank = loadout.Ranks[slot];
			for (uint8_t pip = 1; pip <= MaxSpellRank; pip++)
				draw.Diamond({ tile.X + 6.0f + float(pip - 1) * (tile.W - 12.0f) / 4.0f, tile.Bottom() + 5.0f }, 6.0f, pip <= rank ? color : Rgba(0.05f, 0.04f, 0.08f, 0.9f), WithAlpha(Theme::Edge, 0.8f), 0.8f);
			if (ui.Interact(Ui::MakeId("runspell", slot), tile).Hovered)
			{
				UiTooltip& tooltip = ui.Tooltip();
				DescribeSpell(tooltip, spell, BrawlMode::Damage);
				tooltip.Pair("Rank", Format("%s of %s", RomanRanks[rank], RomanRanks[MaxSpellRank]));
			}
		}
	}

	void SessionState::DrawRunOffer()
	{
		const GameStatus& status = m_Client.GetStatus();
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (m_Client.GetSession().Mode != GameMode::Shardfall || !local.Valid || IsRunOver(status.Run.Phase))
		{
			m_RunPending.reset();
			m_RunBanishing = false;
			return;
		}
		const RunLoadout& run = local.Run;
		if (!(run.Offer == m_SeenOffer) || run.Offers != m_SeenOffers)
		{
			if (run.HasOffer() && !(run.Offer == m_SeenOffer))
			{
				m_RunReveal = 0.0f;
				if (!m_RunPending)
					m_App.GetGameAudio().PlayUi(Cue::UiNotify, 0.5f);
			}
			m_SeenOffer = run.Offer;
			m_SeenOffers = run.Offers;
			m_RunPending.reset();
			m_RunBanishing = m_RunBanishing && run.Banishes > 0;
		}
		if (!run.HasOffer() || m_Paused)
			return;
		Ui& ui = m_App.GetUi();
		const glm::vec2 size = ui.GetSize();
		m_RunReveal += ui.GetDeltaTime();
		const bool modal = status.Run.Paused;
		const bool ready = !m_RunPending;
		const auto take = [&](uint32_t choice)
		{
			const RunCard& card = run.Offer[choice];
			if (m_RunBanishing)
			{
				m_Client.BanishRunCard(choice);
				m_App.GetGameAudio().PlayUi(Cue::UiShatter, 0.6f);
			}
			else
			{
				m_Client.PickRunCard(choice);
				if (card.Kind == RunCardKind::Rune)
				{
					m_GrimoireOpen = true;
					m_MerchantOpen = false;
				}
				const bool rare = card.Kind == RunCardKind::Spell || (card.Kind == RunCardKind::Card && GetCard(card.Upgrade()).Rarity != CardRarity::Common) || (card.Kind == RunCardKind::Rune && GetRune(card.Rune()).Rarity != CardRarity::Common);
				m_App.GetGameAudio().PlayUi(rare ? Cue::UiRare : Cue::UiReward);
			}
			m_RunPending = choice;
			m_RunBanishing = false;
		};
		if (ready && !m_App.UiWantsKeyboard())
			for (uint32_t i = 0; i < CardChoices; i++)
				if (Rdn::Input::IsKeyPressed(CardKeys[i]) && !run.Offer[i].IsEmpty())
					take(i);

		uint32_t count = 0;
		for (const RunCard& card : run.Offer)
			count += card.IsEmpty() ? 0u : 1u;
		const float cardWidth = modal ? 250.0f : 178.0f;
		const float cardHeight = modal ? std::clamp(size.y * 0.36f, 250.0f, 330.0f) : 238.0f;
		const float gap = modal ? 36.0f : 12.0f;
		const float rowWidth = float(count) * cardWidth + float(count - 1) * gap;
		float top = 0.0f;
		float left = 0.0f;
		if (modal)
		{
			ui.SetLayer(UiLayer::World);
			ui.Draw().Rect(ui.GetScreen(), { .Fill = Rgba(0.02f, 0.012f, 0.05f, 0.62f), .FillBottom = Rgba(0.02f, 0.012f, 0.05f, 0.72f) });
			ui.SetLayer(UiLayer::Windows);
			ui.Block(ui.GetScreen());
			top = std::max(size.y * 0.5f - cardHeight * 0.5f - 40.0f, 150.0f);
			left = size.x * 0.5f - rowWidth * 0.5f;
		}
		else
		{
			top = size.y - 186.0f - cardHeight - 34.0f;
			left = size.x - rowWidth - 26.0f;
		}
		UiDrawList& draw = ui.Draw();
		if (modal)
		{
			TextStyle heading = Theme::Title(30.0f);
			heading.Tracking = 3.0f;
			heading.Color = Theme::GoldLight;
			heading.ColorBottom = Theme::Gold;
			const std::string title = m_RunBanishing ? std::string("BANISH A CARD") : status.Run.Phase == RunPhase::Respite && run.Offers == 1 ? std::string("THE GUARDIAN'S LEGACY") : run.Focus == RunCardKind::Sigil ? std::string("CHOOSE A SIGIL") : run.Focus == RunCardKind::Rune ? std::string("CHOOSE A RUNE") : run.Focus == RunCardKind::Spell ? std::string("CHOOSE A NEW SPELL") : run.Focus == RunCardKind::Rank ? std::string("RANK UP A SPELL") : std::string("CHOOSE A CARD");
			draw.TextAligned({ 0.0f, top - 92.0f, size.x, 40.0f }, title, heading, { 0.5f, 0.5f });
			std::string hint = m_RunBanishing ? "Click a card to remove it from this run for good. A new one takes its place." : "Choose one. Press 1, 2 or 3. The run waits while you choose.";
			if (run.Offers > 1 && !m_RunBanishing)
				hint += Format(" %u more after this one.", unsigned(run.Offers - 1));
			draw.TextAligned({ 0.0f, top - 54.0f, size.x, 22.0f }, hint, Theme::Floating(15.5f, Theme::Text, UiFont::Body), { 0.5f, 0.5f });
		}
		else
		{
			TextStyle label = Theme::Bold(12.0f, Theme::GoldLight);
			label.Tracking = 1.6f;
			const std::string line = (m_RunBanishing ? std::string("BANISH: CLICK A CARD") : "LEVEL UP  \xC2\xB7  PRESS 1, 2 OR 3") + (run.Offers > 1 ? Format("  \xC2\xB7  %u WAITING", unsigned(run.Offers)) : std::string());
			draw.TextAligned({ left, top - 26.0f, rowWidth, 18.0f }, line, label, { 0.5f, 0.5f });
		}

		float x = left;
		for (uint32_t i = 0; i < CardChoices; i++)
		{
			const RunCard& card = run.Offer[i];
			if (card.IsEmpty())
				continue;
			const float reveal = std::clamp((m_RunReveal - 0.1f * float(i)) / 0.45f, 0.0f, 1.0f);
			const float rise = 1.0f - reveal;
			const UiRect rect{ x, top + 36.0f * rise * rise * rise, cardWidth, cardHeight };
			x += cardWidth + gap;
			const UiId id = Ui::MakeId("runcard", i);
			const UiInteraction interaction = ui.Interact(id, rect);
			const float hover = ui.Animate(id, ready && interaction.Hovered);
			if (ready && interaction.Clicked)
				take(i);
			DrawRunCardFace(rect, card, hover, ready ? std::optional<uint32_t>(i) : std::nullopt, reveal, m_RunBanishing);
			if (m_RunPending == i)
				draw.Rect(rect.Translate({ 0.0f, -8.0f * hover }), { .Border = WithAlpha(Theme::Success, 0.9f), .BorderWidth = 2.0f, .Glow = WithAlpha(Theme::Success, 0.3f), .GlowSize = 14.0f, .Chamfer = 16.0f });
			if (interaction.Hovered && !modal)
			{
				if (card.Kind == RunCardKind::Card)
					DescribeCard(ui.Tooltip(), card.Upgrade(), BrawlMode::Damage);
				else if (card.Kind == RunCardKind::Rune)
					DescribeRune(ui.Tooltip(), card.Rune());
				else if (card.Kind == RunCardKind::Spell || card.Kind == RunCardKind::Rank)
					DescribeSpell(ui.Tooltip(), card.Spell(), BrawlMode::Damage);
			}
		}

		const float buttonHeight = modal ? 40.0f : 28.0f;
		const float buttonWidth = modal ? 190.0f : (rowWidth - 2.0f * 8.0f) / 3.0f;
		const float buttonGap = modal ? 14.0f : 8.0f;
		UiRect buttons{ modal ? size.x * 0.5f - (3.0f * buttonWidth + 2.0f * buttonGap) * 0.5f : left, top + cardHeight + (modal ? 26.0f : 10.0f), 3.0f * buttonWidth + 2.0f * buttonGap, buttonHeight };
		if (ui.Button(Ui::MakeId("run.reroll"), buttons.CutLeft(buttonWidth), Format(modal ? "Reroll  (%u left)" : "Reroll %u", unsigned(run.Rerolls)), ButtonKind::Normal, ready && run.Rerolls > 0))
		{
			m_Client.RerollRunCards();
			m_RunPending = uint32_t(CardChoices);
			m_RunBanishing = false;
			m_App.GetGameAudio().PlayUi(Cue::UiConfirm, 0.7f);
		}
		buttons.CutLeft(buttonGap);
		if (ui.Button(Ui::MakeId("run.banish"), buttons.CutLeft(buttonWidth), m_RunBanishing ? std::string("Cancel") : Format(modal ? "Banish  (%u left)" : "Banish %u", unsigned(run.Banishes)), m_RunBanishing ? ButtonKind::Primary : ButtonKind::Normal, ready && (run.Banishes > 0 || m_RunBanishing)))
			m_RunBanishing = !m_RunBanishing;
		buttons.CutLeft(buttonGap);
		if (ui.Button(Ui::MakeId("run.skip"), buttons.CutLeft(buttonWidth), std::string("Skip"), ButtonKind::Normal, ready))
		{
			m_Client.SkipRunCards();
			m_RunPending = uint32_t(CardChoices);
			m_RunBanishing = false;
			m_App.GetGameAudio().PlayUi(Cue::UiConfirm, 0.6f);
		}
		if (!modal)
			return;
		const UiRect strip = UiRect::Centered({ size.x * 0.5f, top + cardHeight + 26.0f + buttonHeight + 58.0f }, { 640.0f, 62.0f });
		UiRect spells = strip;
		UiRect cards = spells.CutRight(300.0f);
		TextStyle label = Theme::Bold(11.5f, Theme::Muted);
		label.Tracking = 1.4f;
		draw.TextAligned(spells.CutTop(16.0f), "YOUR SPELLS", label, { 0.0f, 0.5f });
		DrawRunSpells(spells.CutTop(40.0f), run, 36.0f);
		draw.TextAligned(cards.CutTop(16.0f), "YOUR CARDS", label, { 0.0f, 0.5f });
		DrawCardRow(cards.CutTop(36.0f), run.Cards, 28.0f, "None yet");
	}

	void SessionState::DrawRunRecap()
	{
		const GameStatus& status = m_Client.GetStatus();
		if (m_Client.GetSession().Mode != GameMode::Shardfall || !IsRunOver(status.Run.Phase) || m_Paused)
			return;
		Ui& ui = m_App.GetUi();
		const glm::vec2 size = ui.GetSize();
		const float fade = std::clamp(status.Run.PhaseClock / 1.2f, 0.0f, 1.0f);
		ui.SetLayer(UiLayer::World);
		ui.Draw().Rect(ui.GetScreen(), { .Fill = Rgba(0.02f, 0.012f, 0.05f, 0.7f * fade), .FillBottom = Rgba(0.02f, 0.012f, 0.05f, 0.82f * fade) });
		ui.SetLayer(UiLayer::Windows);
		if (status.Run.PhaseClock < 1.0f)
			return;
		ui.Block(ui.GetScreen());
		UiDrawList& draw = ui.Draw();
		const bool victory = status.Run.Phase == RunPhase::Victory;
		TextStyle title = Theme::Title(54.0f);
		title.Tracking = 5.0f;
		title.Color = victory ? Theme::GoldLight : Hex(0xFF8A7A);
		title.ColorBottom = victory ? Theme::Gold : Hex(0x9E2620);
		const float top = std::max(size.y * 0.1f, 40.0f);
		draw.TextAligned({ 0.0f, top, size.x, 60.0f }, victory ? "VICTORY" : "THE RUN IS OVER", title, { 0.5f, 0.5f });
		DrawTitleOrnament(size.x * 0.5f, top + 70.0f, draw.MeasureWidth(victory ? "VICTORY" : "THE RUN IS OVER", title));
		const ServerRunRecap* recap = m_Client.GetRecap() ? &*m_Client.GetRecap() : nullptr;
		const float clock = recap ? recap->RunClock : status.Run.RunClock;
		const std::string line = Format("%s  \xC2\xB7  %u rooms  \xC2\xB7  %s", (victory ? std::string("All three acts cleared") : "Fell in act " + std::to_string(status.Run.Act)).c_str(), unsigned(status.Run.Rooms), RunClock(clock).c_str());
		draw.TextAligned({ 0.0f, top + 84.0f, size.x, 24.0f }, line, Theme::Floating(17.0f, Theme::Text, UiFont::Body), { 0.5f, 0.5f });

		const UiRect area{ size.x * 0.5f - std::min(size.x - 80.0f, 1160.0f) * 0.5f, top + 128.0f, std::min(size.x - 80.0f, 1160.0f), size.y - top - 128.0f - 110.0f };
		if (!recap)
			draw.TextAligned(area, "Gathering the tally...", Theme::Floating(16.0f, Theme::Muted, UiFont::Body), { 0.5f, 0.3f });
		else
		{
			const size_t players = std::max<size_t>(recap->Players.size(), 1);
			const float columnGap = 18.0f;
			const float columnWidth = std::min((area.W - columnGap * float(players - 1)) / float(players), 470.0f);
			const uint32_t tilesPerRow = std::max(uint32_t((columnWidth - 36.0f + 4.0f) / 30.0f), 1u);
			float columnHeight = 0.0f;
			for (const RunRecapPlayer& player : recap->Players)
			{
				const float damageRows = float(std::clamp<size_t>(player.Damage.size(), 1, 6));
				const float pickRows = float((player.Picks.size() + tilesPerRow - 1) / tilesPerRow);
				columnHeight = std::max(columnHeight, 28.0f + 20.0f + 10.0f + 16.0f + 40.0f + 10.0f + 16.0f + damageRows * 24.0f + 10.0f + 16.0f + pickRows * 30.0f + 34.0f);
			}
			columnHeight = std::min(columnHeight, area.H);
			const float rowLeft = area.Center().x - (float(players) * columnWidth + float(players - 1) * columnGap) * 0.5f;
			for (size_t p = 0; p < recap->Players.size(); p++)
			{
				const RunRecapPlayer& player = recap->Players[p];
				const UiRect column{ rowLeft + float(p) * (columnWidth + columnGap), area.Y, columnWidth, columnHeight };
				ui.Panel(column, 12.0f);
				UiRect content = column.Shrink(18.0f, 14.0f);
				TextStyle name = Theme::Title(22.0f);
				name.Color = Lighten(WithAlpha(ColorOf(player.PlayerId)), 0.35f);
				draw.TextAligned(content.CutTop(28.0f), player.Name, name, { 0.0f, 0.5f });
				draw.TextAligned(content.CutTop(20.0f), Format("%u kills  \xC2\xB7  %u downs  \xC2\xB7  %u revives", unsigned(player.Kills), unsigned(player.Downs), unsigned(player.Revived)), Theme::Body(14.0f, Theme::Muted), { 0.0f, 0.5f });
				content.CutTop(10.0f);
				TextStyle label = Theme::Bold(11.5f, Theme::Muted);
				label.Tracking = 1.4f;
				draw.TextAligned(content.CutTop(16.0f), "SPELLS", label, { 0.0f, 0.5f });
				DrawRunSpells(content.CutTop(40.0f), player.Loadout, 34.0f);
				content.CutTop(10.0f);
				draw.TextAligned(content.CutTop(16.0f), "DAMAGE BY SPELL", label, { 0.0f, 0.5f });
				uint64_t total = 0;
				for (const auto& [spell, amount] : player.Damage)
					total += amount;
				const uint32_t peak = player.Damage.empty() ? 1u : std::max(player.Damage.front().second, 1u);
				if (player.Damage.empty())
					draw.TextAligned(content.CutTop(24.0f), "No damage dealt", Theme::Body(13.5f, Theme::Faint), { 0.0f, 0.5f });
				for (size_t s = 0; s < player.Damage.size() && s < 6; s++)
				{
					const auto& [spell, amount] = player.Damage[s];
					UiRect row = content.CutTop(24.0f);
					const UiColor color = WithAlpha(SkillColor(spell));
					draw.Icon(IconFor(spell), row.CutLeft(20.0f).Shrink(0.0f, 2.0f), { .Color = Lighten(color, 0.5f), .ColorBottom = color });
					row.CutLeft(8.0f);
					const UiRect value = row.CutRight(92.0f);
					const UiRect bar{ row.X, row.Center().y - 4.0f, row.W * float(amount) / float(peak), 8.0f };
					draw.Rect(bar, { .Fill = Lighten(color, 0.2f), .FillBottom = Darken(color, 0.25f), .Glow = WithAlpha(color, 0.25f), .GlowSize = 5.0f, .Chamfer = 2.0f });
					draw.TextAligned(value, Format("%u  %.0f%%", amount, double(float(amount) * 100.0f / float(std::max<uint64_t>(total, 1)))), Theme::Body(13.0f, Theme::Text), { 1.0f, 0.5f });
				}
				content.CutTop(10.0f);
				draw.TextAligned(content.CutTop(16.0f), Format("CARDS IN PICK ORDER  \xC2\xB7  %zu", player.Picks.size()), label, { 0.0f, 0.5f });
				constexpr float Tile = 26.0f;
				const uint32_t perRow = std::max(uint32_t((content.W + 4.0f) / (Tile + 4.0f)), 1u);
				for (size_t i = 0; i < player.Picks.size(); i++)
				{
					const RunPick& pick = player.Picks[i];
					const UiRect tile{ content.X + float(i % perRow) * (Tile + 4.0f), content.Y + float(i / perRow) * (Tile + 4.0f), Tile, Tile };
					if (tile.Bottom() > content.Bottom())
						break;
					const UiColor color = RunCardColor(pick.Card);
					draw.Rect(tile, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Border = WithAlpha(pick.Card.Kind == RunCardKind::Spell ? Theme::Gold : color, 0.8f), .BorderWidth = 1.0f, .Chamfer = 4.0f });
					draw.Icon(RunCardIcon(pick.Card), tile.Shrink(4.0f), { .Color = Lighten(color, 0.45f), .ColorBottom = color });
					if (ui.Interact(Ui::MakeId("recappick", p * 256 + i), tile).Hovered)
						ui.Tooltip().Header(RunCardName(pick.Card), color, std::nullopt, RunCardIcon(pick.Card)).Muted(Format("Taken in act %u, room %u", unsigned(pick.Stage >> 8), unsigned(pick.Stage & 0xFFu)));
				}
			}
		}
		const UiRect button = UiRect::Centered({ size.x * 0.5f, size.y - 70.0f }, { 300.0f, 46.0f });
		if (ui.Button(Ui::MakeId("run.home"), button, "Return to the Sun's Cradle", ButtonKind::Primary))
			m_Client.ReturnToHub();
		draw.TextAligned({ 0.0f, button.Bottom() + 6.0f, size.x, 18.0f }, Format("Everyone returns in %.0f s", double(std::max(RunEndSeconds - status.Run.PhaseClock, 0.0f))), Theme::Body(13.0f, Theme::Muted), { 0.5f, 0.5f });
	}

	void SessionState::DrawRunMarker()
	{
		const Level* level = m_Client.GetLevel();
		const RunPhase phase = m_Client.GetStatus().Run.Phase;
		if (m_Client.GetSession().Mode != GameMode::Shardfall || !level)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const glm::vec2 size = ui.GetSize();
		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (entity.Kind != EntityKind::Loot || entity.Variant < uint8_t(LootKind::Magnet) || entity.Variant >= uint8_t(LootKind::Count))
				continue;
			const std::optional<glm::vec2> pixels = ProjectToScreen(Ground(entity.Position, 1.25f));
			if (!pixels)
				continue;
			const glm::vec3 tone = LootColors[std::min<size_t>(entity.Variant, std::size(LootColors) - 1)];
			draw.TextAligned(UiRect::Centered(ui.ToLogical(*pixels), { 200.0f, 18.0f }), LootDefs[entity.Variant].Name, Theme::Floating(13.5f, Lighten(WithAlpha(tone), 0.35f), UiFont::Bold), { 0.5f, 0.5f });
		}
		constexpr float Inset = 64.0f;
		const UiRect safe{ Inset, Inset + 70.0f, size.x - 2.0f * Inset, size.y - 2.0f * Inset - 200.0f };
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		const bool settling = UsesRooms(m_Client.GetSession().Run) && m_Client.GetStatus().Run.RoomClock < StragglerDelay;
		if (phase == RunPhase::Act && self && !settling)
		{
			std::vector<const EntityState*> fighters;
			for (const EntityState& entity : m_Client.GetEntities())
				if (IsFighterKind(entity.Kind) && !entity.Has(EntityFlags::Dead))
					fighters.push_back(&entity);
			if (!fighters.empty() && fighters.size() <= StragglerMarkers)
				for (const EntityState* fighter : fighters)
				{
					const std::optional<glm::vec2> seen = ProjectToScreen(Ground(fighter->Position, 1.5f));
					if (!seen || safe.Contains(ui.ToLogical(*seen)))
						continue;
					const glm::vec2 offset = ui.ToLogical(*seen) - safe.Center();
					const float scale = std::min(std::abs(offset.x) > 1e-3f ? (safe.W * 0.5f) / std::abs(offset.x) : 1e9f, std::abs(offset.y) > 1e-3f ? (safe.H * 0.5f) / std::abs(offset.y) : 1e9f);
					const glm::vec2 edge = safe.Center() + offset * std::min(scale, 1.0f);
					const glm::vec2 heading = glm::normalize(offset);
					const UiColor danger = Hex(0xE04A3C);
					draw.Diamond(edge, 16.0f, Rgba(0.06f, 0.02f, 0.02f, 0.9f), danger, 1.4f);
					draw.Diamond(edge + heading * 13.0f, 7.0f, danger, Lighten(danger, 0.3f), 1.0f);
					const EnemyDef* def = FindEnemyDef(fighter->Kind);
					draw.TextAligned(UiRect::Centered(edge - heading * 26.0f, { 240.0f, 18.0f }), Format("%s  %.0f m", def ? def->Name : "", double(Length(fighter->Position - self->Position))), Theme::Floating(13.0f, Lighten(danger, 0.35f), UiFont::Bold), { 0.5f, 0.5f });
				}
		}
		const auto edgeMarker = [&](glm::vec2 point, UiColor tone, const std::string& label, float pulse)
		{
			const glm::vec2 center = safe.Center();
			const glm::vec2 offset = point - center;
			const float scale = std::min(std::abs(offset.x) > 1e-3f ? (safe.W * 0.5f) / std::abs(offset.x) : 1e9f, std::abs(offset.y) > 1e-3f ? (safe.H * 0.5f) / std::abs(offset.y) : 1e9f);
			const glm::vec2 edge = center + offset * std::min(scale, 1.0f);
			const glm::vec2 heading = glm::normalize(offset);
			draw.Diamond(edge, 20.0f, Rgba(0.04f, 0.03f, 0.08f, 0.92f), WithAlpha(tone, pulse), 1.5f);
			draw.Diamond(edge + heading * 16.0f, 8.0f, WithAlpha(tone, pulse), WithAlpha(Lighten(tone, 0.4f), pulse), 1.0f);
			draw.TextAligned(UiRect::Centered(edge - heading * 30.0f, { 240.0f, 20.0f }), label, Theme::Floating(14.0f, Lighten(tone, 0.3f), UiFont::Bold), { 0.5f, 0.5f });
		};
		const RunStatus& run = m_Client.GetStatus().Run;
		if (level->Doors.size() == DoorSlots && phase == RunPhase::Act && run.Room != NoRoom)
		{
			const ActMap map = MakeActMap(m_Client.GetSession().Run.Seed, run.Act);
			unsigned living = 0;
			for (const EntityState& entity : m_Client.GetEntities())
				living += entity.Kind == EntityKind::Player && !entity.Has(EntityFlags::Dead) ? 1u : 0u;
			for (uint8_t slot = 0; slot < DoorSlots; slot++)
			{
				const uint8_t target = DoorTarget(map, run.Room, slot);
				if (!(level->DoorMask & (1u << slot)) || target == NoRoom)
					continue;
				const MapRoom& room = map.At(target);
				const UiColor tone = WithAlpha(RoomTone(room.Kind));
				std::string label = RoomKindNames[size_t(room.Kind)];
				if (room.Reward != RoomReward::None)
					label += std::string(": ") + RoomRewardNames[size_t(room.Reward)];
				if (run.Cleared && living > 1 && run.Votes[slot] > 0)
					label += Format("   %u/%u", unsigned(run.Votes[slot]), living);
				const float pulse = run.Cleared ? 0.7f + 0.3f * std::sin(m_Time * 4.0f + float(slot)) : 0.45f;
				const std::optional<glm::vec2> seen = ProjectToScreen(Ground(level->Doors[slot], 2.6f));
				if (!seen)
					continue;
				const glm::vec2 point = ui.ToLogical(*seen);
				if (safe.Contains(point) || !run.Cleared)
					draw.TextAligned(UiRect::Centered(point, { 280.0f, 20.0f }), label, Theme::Floating(run.Cleared ? 15.0f : 13.0f, run.Cleared ? Lighten(tone, 0.3f) : WithAlpha(tone, 0.55f), UiFont::Bold), { 0.5f, 0.5f });
				else
					edgeMarker(point, tone, label, pulse);
			}
			const RoomKind calm = CurrentRoomKind();
			if ((calm == RoomKind::Shop || calm == RoomKind::Shrine) && level->Altars.size() == 2)
				if (const std::optional<glm::vec2> seen = ProjectToScreen(Ground((level->Altars[0] + level->Altars[1]) * 0.5f, 2.6f)))
					draw.TextAligned(UiRect::Centered(ui.ToLogical(*seen), { 260.0f, 20.0f }), calm == RoomKind::Shop ? "Crystal Merchant  (E)" : "Old Shrine  (E)", Theme::Floating(15.0f, Lighten(WithAlpha(RoomTone(calm)), 0.3f), UiFont::Bold), { 0.5f, 0.5f });
			const uint8_t rested = m_Client.GetLocalPlayer().Run.Rested;
			for (size_t i = 0; i < level->Altars.size() && i < 2 && calm == RoomKind::Rest; i++)
			{
				const std::optional<glm::vec2> seen = ProjectToScreen(Ground(level->Altars[i], 1.6f));
				if (!seen)
					continue;
				const UiColor tone = WithAlpha(i == 0 ? HealAltarTone : TrainAltarTone);
				const std::string label = rested == 0 ? (i == 0 ? Format("Rest: heal %.0f%%", double(RestHeal * 100.0f)) : std::string("Train: rank up a spell")) : rested == i + 1 ? std::string(i == 0 ? "Rested" : "Trained") : std::string();
				if (!label.empty())
					draw.TextAligned(UiRect::Centered(ui.ToLogical(*seen), { 240.0f, 20.0f }), label, Theme::Floating(14.5f, rested == 0 ? Lighten(tone, 0.3f) : WithAlpha(tone, 0.6f), UiFont::Bold), { 0.5f, 0.5f });
			}
		}
		if (level->BossArenaRadius <= 0.0f || (phase != RunPhase::Gate && phase != RunPhase::Respite))
			return;
		const std::optional<glm::vec2> pixels = ProjectToScreen(Ground(level->BossArena, 2.2f));
		if (!pixels)
			return;
		const glm::vec2 point = ui.ToLogical(*pixels);
		const bool rift = phase == RunPhase::Respite;
		const UiColor tone = rift ? Hex(0x8FE3FF) : Hex(0xC9A6FF);
		const float pulse = 0.65f + 0.35f * std::sin(m_Time * 4.0f);
		const float distance = self ? Length(self->Position - level->BossArena) : 0.0f;
		const std::string label = Format("%s  %.0f m", rift ? "Rift" : "Guardian stone", double(distance));
		const TextStyle style = Theme::Floating(14.0f, Lighten(tone, 0.3f), UiFont::Bold);
		if (safe.Contains(point))
		{
			if (distance > GateRadius + 1.0f)
				draw.TextAligned(UiRect::Centered(point - glm::vec2(0.0f, 18.0f), { 240.0f, 20.0f }), label, style, { 0.5f, 0.5f });
			return;
		}
		const glm::vec2 center = safe.Center();
		const glm::vec2 offset = point - center;
		const float scale = std::min(std::abs(offset.x) > 1e-3f ? (safe.W * 0.5f) / std::abs(offset.x) : 1e9f, std::abs(offset.y) > 1e-3f ? (safe.H * 0.5f) / std::abs(offset.y) : 1e9f);
		const glm::vec2 edge = center + offset * std::min(scale, 1.0f);
		const glm::vec2 heading = glm::normalize(offset);
		draw.Diamond(edge, 22.0f, Rgba(0.04f, 0.03f, 0.08f, 0.92f), WithAlpha(tone, pulse), 1.6f);
		draw.Diamond(edge + heading * 17.0f, 9.0f, WithAlpha(tone, pulse), WithAlpha(Lighten(tone, 0.4f), pulse), 1.0f);
		draw.TextAligned(UiRect::Centered(edge - heading * 30.0f, { 240.0f, 20.0f }), label, style, { 0.5f, 0.5f });
	}

	namespace
	{
		constexpr float StreakWindow = 2.5f;
		constexpr float MultiWindow = 0.35f;
		constexpr uint32_t MultiKills = 6;
		constexpr float MultiShow = 1.4f;
		constexpr float PerfectShow = 1.1f;

		struct StreakTier
		{
			uint32_t Kills;
			const char* Name;
			uint32_t Color;
		};

		constexpr StreakTier StreakTiers[] = {
			{ 10, "Streak", 0x9FDCEA },
			{ 25, "Rampage", 0x8FE3FF },
			{ 50, "Shatterstorm", 0xC9A6FF },
			{ 100, "Crystal Cataclysm", 0xFFCF5A },
			{ 200, "Kaleidoscope", 0xFF7AC9 },
		};

		const StreakTier* TierOf(uint32_t kills)
		{
			const StreakTier* tier = nullptr;
			for (const StreakTier& candidate : StreakTiers)
				if (kills >= candidate.Kills)
					tier = &candidate;
			return tier;
		}
	}

	void SessionState::UpdateRunFeel(float dt)
	{
		if (m_Client.GetSession().Mode != GameMode::Shardfall)
		{
			m_Streak = 0;
			m_RecentKills.clear();
			m_SeenBursts.clear();
			return;
		}
		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (entity.Kind != EntityKind::Effect)
				continue;
			const AbilityId ability = AbilityOf(entity);
			if ((ability != AbilityId::Ascension && ability != AbilityId::ShardBomb && ability != AbilityId::FrostClock) || !m_SeenBursts.emplace(entity.NetId, m_Time).second)
				continue;
			const float radius = GetAbility(ability).Reach * entity.GetScale();
			const glm::vec3 ground = Ground(entity.Position, 0.05f);
			const bool own = entity.Owner == m_Client.GetPlayerId();
			if (ability == AbilityId::Ascension)
			{
				m_Effects->Shockwave(ground, radius, 0.6f, 0.7f, 0.3f);
				m_Effects->Burst(Fx::ArcaneBurst, Ground(entity.Position, 0.6f), glm::vec3(0.0f, 1.0f, 0.0f), 2.0f, glm::vec3(1.0f, 0.86f, 0.5f));
				if (own)
					AddTrauma(0.22f);
			}
			else if (ability == AbilityId::ShardBomb)
			{
				m_Effects->Shockwave(ground, std::min(radius, 9.0f), 0.85f, 1.0f, 0.4f);
				m_Effects->Burst(Fx::Explosion, Ground(entity.Position, 0.4f), glm::vec3(0.0f, 1.0f, 0.0f), 2.2f, glm::vec3(1.0f, 0.6f, 0.3f));
				m_Effects->Burst(Fx::CrystalBurst, Ground(entity.Position, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f), 2.5f, glm::vec3(1.0f, 0.7f, 0.4f));
				AddTrauma(0.5f);
			}
			else
			{
				m_Effects->Shockwave(ground, std::min(radius, 9.0f), 1.0f, 0.6f, 0.35f);
				m_Effects->Burst(Fx::FrostNova, Ground(entity.Position, 0.3f), glm::vec3(0.0f, 1.0f, 0.0f), 2.4f);
				AddTrauma(0.25f);
			}
		}
		if (m_SeenBursts.size() > 64)
			std::erase_if(m_SeenBursts, [&](const auto& seen) { return m_Time - seen.second > 5.0f; });

		const uint32_t self = m_Client.GetPlayerId();
		for (const HitEvent& hit : m_Client.GetNewHits())
		{
			if (hit.Ability == uint8_t(AbilityId::PerfectDodge) && hit.Target == m_Client.GetPlayerNetId())
			{
				m_PerfectAge = 0.0f;
				m_Effects->Burst(Fx::ArcaneBurst, Ground(hit.Position, 0.8f), glm::vec3(0.0f, 1.0f, 0.0f), 1.3f, glm::vec3(1.0f, 0.86f, 0.5f));
				m_Effects->Shockwave(Ground(hit.Position, 0.05f), 2.2f, 0.4f, 0.45f, 0.2f);
				m_App.GetGameAudio().PlayUi(Cue::UiRare, 0.5f, 1.35f);
				AddTrauma(0.12f);
			}
			if (!hit.Killed || hit.Attacker != self || hit.OnPlayer)
				continue;
			const StreakTier* before = TierOf(m_Streak);
			m_Streak++;
			m_StreakBest = std::max(m_StreakBest, m_Streak);
			m_StreakIdle = 0.0f;
			m_StreakPulse = 1.0f;
			m_RecentKills.push_back({ m_Time, hit.Position });
			if (const StreakTier* after = TierOf(m_Streak); after && after != before && after->Kills >= 25)
			{
				m_App.GetGameAudio().PlayUi(Cue::UiRare, 0.55f, 1.0f + 0.1f * float(after - StreakTiers));
				m_StreakFlash = 1.0f;
			}
		}
		m_StreakIdle += dt;
		m_StreakPulse = std::max(m_StreakPulse - dt * 4.0f, 0.0f);
		m_StreakFlash = std::max(m_StreakFlash - dt * 1.5f, 0.0f);
		if (m_StreakIdle > StreakWindow && m_Streak > 0)
		{
			m_StreakShown = m_Streak;
			m_StreakFade = 1.0f;
			m_Streak = 0;
		}
		m_StreakFade = std::max(m_StreakFade - dt * 0.8f, 0.0f);
		std::erase_if(m_RecentKills, [&](const auto& kill) { return m_Time - kill.first > MultiWindow; });
		if (m_RecentKills.size() >= MultiKills)
		{
			Vec2 center;
			for (const auto& kill : m_RecentKills)
				center += kill.second;
			center = center * (1.0f / float(m_RecentKills.size()));
			const float size = float(m_RecentKills.size());
			m_Effects->Burst(Fx::CrystalBurst, Ground(center, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f), 1.2f + 0.08f * size, glm::vec3(0.75f, 0.9f, 1.0f));
			m_Effects->Shockwave(Ground(center, 0.05f), 2.0f + 0.2f * size, 0.45f, 0.5f, 0.25f);
			AddTrauma(std::min(0.12f + 0.025f * size, 0.4f));
			m_App.GetGameAudio().PlayUi(Cue::Glass, std::min(0.3f + 0.03f * size, 0.7f), 1.3f);
			m_MultiCount = uint32_t(m_RecentKills.size());
			m_MultiAge = 0.0f;
			m_RecentKills.clear();
		}
		m_MultiAge += dt;
		m_PerfectAge += dt;
	}

	void SessionState::DrawPerfectDodge()
	{
		if (m_Client.GetSession().Mode != GameMode::Shardfall || m_PerfectAge >= PerfectShow)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const glm::vec2 size = ui.GetSize();
		const float fade = std::clamp((PerfectShow - m_PerfectAge) / 0.35f, 0.0f, 1.0f);
		const float pop = 1.0f + 0.25f * std::max(1.0f - m_PerfectAge / 0.15f, 0.0f);
		const float y = size.y * 0.34f - 18.0f * m_PerfectAge;
		TextStyle title = Theme::Title(30.0f * pop);
		title.Color = WithAlpha(Theme::GoldLight, fade);
		title.ColorBottom = WithAlpha(Theme::Gold, fade);
		title.GlowColor = WithAlpha(Theme::Gold, 0.4f * fade);
		title.ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f * fade);
		draw.TextAligned({ 0.0f, y, size.x, 36.0f }, "Perfect dodge", title, { 0.5f, 0.5f });
		draw.TextAligned({ 0.0f, y + 32.0f, size.x, 20.0f }, "Your next spell crits", Theme::Floating(15.0f, WithAlpha(Theme::Text, fade), UiFont::Body), { 0.5f, 0.5f });
	}

	void SessionState::DrawRunStreak()
	{
		if (m_Client.GetSession().Mode != GameMode::Shardfall)
			return;
		const bool live = m_Streak >= 5;
		const uint32_t kills = live ? m_Streak : m_StreakShown;
		const float alpha = live ? 1.0f : m_StreakFade;
		if (alpha <= 0.0f || kills < 5)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const glm::vec2 size = ui.GetSize();
		const StreakTier* tier = TierOf(kills);
		const UiColor tone = WithAlpha(tier ? Hex(tier->Color) : Theme::AccentLight, alpha);
		const float pop = 1.0f + 0.18f * m_StreakPulse + 0.25f * m_StreakFlash;
		const UiRect area{ size.x - 250.0f, 150.0f, 220.0f, 110.0f };
		TextStyle number = Theme::Title(46.0f * pop);
		number.Color = Lighten(tone, 0.35f);
		number.ColorBottom = tone;
		number.GlowColor = WithAlpha(tone, 0.45f * alpha);
		number.ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f * alpha);
		draw.TextAligned({ area.X, area.Y, area.W, 56.0f }, std::to_string(kills), number, { 1.0f, 0.5f });
		TextStyle label = Theme::Caps(13.0f, WithAlpha(Lighten(tone, 0.2f), alpha), 2.5f);
		draw.TextAligned({ area.X, area.Y + 54.0f, area.W, 20.0f }, tier ? tier->Name : "Kills", label, { 1.0f, 0.5f });
		if (live)
		{
			const float left = std::clamp(1.0f - m_StreakIdle / StreakWindow, 0.0f, 1.0f);
			const UiRect bar{ area.Right() - 140.0f, area.Y + 78.0f, 140.0f, 4.0f };
			draw.Rect(bar, { .Fill = Rgba(0.02f, 0.02f, 0.05f, 0.7f * alpha), .Chamfer = 1.5f });
			draw.Rect({ bar.Right() - bar.W * left, bar.Y, bar.W * left, bar.H }, { .Fill = tone, .Glow = WithAlpha(tone, 0.35f), .GlowSize = 5.0f, .Chamfer = 1.5f });
		}
		if (m_MultiAge < MultiShow && m_MultiCount >= MultiKills)
		{
			const float fade = std::clamp((MultiShow - m_MultiAge) / 0.4f, 0.0f, 1.0f);
			const float rise = 12.0f * m_MultiAge;
			TextStyle multi = Theme::Floating(18.0f + 0.4f * float(std::min(m_MultiCount, 20u)), WithAlpha(Theme::GoldLight, fade), UiFont::Bold);
			draw.TextAligned({ area.X - 40.0f, area.Y + 88.0f - rise, area.W + 40.0f, 24.0f }, Format("%u at once!", m_MultiCount), multi, { 1.0f, 0.5f });
		}
	}

	void SessionState::DrawActMap()
	{
		const SessionInfo& session = m_Client.GetSession();
		const RunStatus& run = m_Client.GetStatus().Run;
		if (session.Mode != GameMode::Shardfall || !UsesRooms(session.Run) || run.Room == NoRoom || IsRunOver(run.Phase) || m_Paused)
			return;
		const bool doors = run.Cleared && run.Phase == RunPhase::Act;
		if (!m_ActMapOpen && !doors)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const glm::vec2 size = ui.GetSize();
		const ActMap map = MakeActMap(session.Run.Seed, run.Act);
		constexpr float Width = 232.0f;
		constexpr float Step = 50.0f;
		const float height = Step * float(MapLayers - 1) + 96.0f;
		const UiRect panel{ size.x - Width - 24.0f, std::max(110.0f, size.y * 0.5f - height * 0.5f), Width, height };
		draw.Rect(panel.Translate({ 0.0f, 5.0f }).Expand(3.0f), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.5f), .Softness = 14.0f, .Chamfer = 14.0f });
		draw.Rect(panel, { .Fill = Hex(0x120E22, 0.88f), .FillBottom = Hex(0x07060E, 0.92f), .Border = WithAlpha(Theme::Astral, 0.45f), .BorderWidth = 1.2f, .Chamfer = 12.0f });
		const char* act = run.Act == 1 ? "I" : run.Act == 2 ? "II" : "III";
		TextStyle heading = Theme::Title(15.0f);
		heading.Tracking = 2.0f;
		draw.TextAligned({ panel.X, panel.Y + 8.0f, panel.W, 20.0f }, Format("ACT %s ROUTE", act), heading, { 0.5f, 0.5f });
		const float top = panel.Y + 46.0f;
		const float bottom = panel.Bottom() - 40.0f;
		const auto nodeAt = [&](uint8_t room)
		{
			const uint8_t layer = RoomLayer(room);
			const float width = float(std::max<uint8_t>(map.Width[layer], 1));
			return glm::vec2(panel.X + 24.0f + (panel.W - 48.0f) * (float(RoomColumn(room)) + 0.5f) / width, bottom - (bottom - top) * float(layer) / float(MapLayers - 1));
		};
		const auto visited = [&](uint8_t room) { return (run.Route & RoomBit(room)) != 0; };
		for (uint8_t room = 0; room < MapRooms; room++)
		{
			if (!map.At(room).Exists() || RoomLayer(room) >= GuardianLayer)
				continue;
			for (uint8_t slot = 0; slot < DoorSlots; slot++)
			{
				const uint8_t next = DoorTarget(map, room, slot);
				if (next == NoRoom)
					continue;
				const bool walked = visited(room) && visited(next);
				const bool ahead = room == run.Room && doors;
				const UiColor color = walked ? WithAlpha(Lighten(Theme::Astral, 0.3f), 0.9f) : ahead ? WithAlpha(RoomTone(map.At(next).Kind), 0.75f + 0.25f * std::sin(m_Time * 4.0f)) : Rgba(0.6f, 0.6f, 0.75f, 0.22f);
				draw.Line(nodeAt(room), nodeAt(next), walked || ahead ? 2.2f : 1.2f, color);
			}
		}
		for (uint8_t room = 0; room < MapRooms; room++)
		{
			const MapRoom& node = map.At(room);
			if (!node.Exists())
				continue;
			const glm::vec2 point = nodeAt(room);
			const UiColor tone = WithAlpha(RoomTone(node.Kind));
			const bool here = room == run.Room;
			const bool past = visited(room) && !here;
			const bool reachable = RoomLayer(room) > RoomLayer(run.Room);
			const float alpha = here || reachable ? 1.0f : past ? 0.7f : 0.3f;
			const float radius = node.Kind == RoomKind::Guardian ? 12.0f : 9.0f;
			if (here)
				draw.Circle(point, radius + 5.0f + 1.5f * std::sin(m_Time * 4.0f), UiColor(0.0f), WithAlpha(Lighten(tone, 0.4f), 0.9f), 1.6f);
			if (node.Kind == RoomKind::Guardian)
				draw.Diamond(point, radius * 1.4f, WithAlpha(Darken(tone, 0.55f), 0.95f * alpha), WithAlpha(tone, alpha), 1.6f);
			else
				draw.Circle(point, radius, past || here ? WithAlpha(Darken(tone, 0.2f), 0.9f * alpha) : WithAlpha(Darken(tone, 0.7f), 0.9f * alpha), WithAlpha(tone, alpha), 1.4f);
			const char* glyph = node.Kind == RoomKind::Fight ? (node.Reward == RoomReward::Spell ? "S" : node.Reward == RoomReward::Rank ? "R" : node.Reward == RoomReward::Cache ? "*" : node.Reward == RoomReward::Rune ? "o" : "C") : node.Kind == RoomKind::Elite ? "E" : node.Kind == RoomKind::Rest ? "+" : node.Kind == RoomKind::Shop ? "$" : node.Kind == RoomKind::Shrine ? "^" : "G";
			draw.TextAligned(UiRect::Centered(point, { 24.0f, 18.0f }), glyph, Theme::Floating(11.5f, past || here ? Theme::Text : WithAlpha(Lighten(tone, 0.4f), alpha), UiFont::Bold), { 0.5f, 0.5f });
		}
		draw.TextFit({ panel.X + 8.0f, panel.Bottom() - 30.0f, panel.W - 16.0f, 14.0f }, "o rune  S spell  R rank  C card  * cache", Theme::Body(11.0f, Theme::Faint), { 0.5f, 0.5f });
		draw.TextAligned({ panel.X, panel.Bottom() - 16.0f, panel.W, 14.0f }, m_ActMapOpen ? "E champion  + rest  $ shop  ^ shrine   M close" : "E champion  + rest  $ shop  ^ shrine   M keep", Theme::Body(11.0f, Theme::Faint), { 0.5f, 0.5f });
	}

	RoomKind SessionState::CurrentRoomKind() const
	{
		const SessionInfo& session = m_Client.GetSession();
		const RunStatus& run = m_Client.GetStatus().Run;
		if (session.Mode != GameMode::Shardfall || !UsesRooms(session.Run) || run.Room == NoRoom)
			return RoomKind::None;
		return MakeActMap(session.Run.Seed, run.Act).At(run.Room).Kind;
	}

	void SessionState::DrawMerchant()
	{
		const RunStatus& run = m_Client.GetStatus().Run;
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		const RoomKind kind = CurrentRoomKind();
		const bool calm = kind == RoomKind::Shop || kind == RoomKind::Shrine || kind == RoomKind::Rest;
		if (!calm || run.Phase != RunPhase::Act || !local.Valid || m_Paused)
		{
			if (!calm)
				m_MerchantRoom = UINT32_MAX;
			return;
		}
		const uint32_t key = uint32_t(run.Act) * 64u + run.Room;
		if (key != m_MerchantRoom)
		{
			m_MerchantRoom = key;
			m_MerchantOpen = kind != RoomKind::Rest;
		}
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const RunLoadout& loadout = local.Run;
		const char* noun = kind == RoomKind::Shop ? "the merchant" : kind == RoomKind::Shrine ? "the shrine" : "your sigils";
		if (!m_MerchantOpen)
		{
			draw.TextAligned({ 0.0f, ui.GetSize().y - 230.0f, ui.GetSize().x, 22.0f }, std::string("E  open ") + noun, Theme::Floating(15.0f, Theme::GoldLight, UiFont::Bold), { 0.5f, 0.5f });
			return;
		}
		constexpr float Width = 690.0f;
		const float height = kind == RoomKind::Shop ? 640.0f : kind == RoomKind::Shrine ? 520.0f : 260.0f;
		const UiRect home{ 24.0f, 110.0f, Width, height };
		bool open = true;
		const char* title = kind == RoomKind::Shop ? "Crystal Merchant" : kind == RoomKind::Shrine ? "Old Shrine" : "Sigils";
		UiRect content = ui.Window(Ui::MakeId("merchant"), title, home, &open);
		if (!open)
		{
			m_MerchantOpen = false;
			return;
		}
		const UiColor shardTone = Hex(0x8FE3FF);
		draw.TextAligned(content.CutTop(24.0f), Format("%u shards", unsigned(loadout.Shards)), Theme::Bold(17.0f, shardTone), { 0.0f, 0.5f });
		content.CutTop(8.0f);
		if (kind == RoomKind::Shop)
		{
			for (uint32_t slot = 0; slot < ShopSlots; slot++)
			{
				const RunCard& item = loadout.Stock[slot];
				if (item.IsEmpty())
					continue;
				UiRect row = content.CutTop(70.0f).Shrink(0.0f, 4.0f);
				const bool sold = (loadout.Sold & (1u << slot)) != 0;
				const uint16_t price = ItemPrice(item, loadout.Draughts);
				RunItemLook look;
				if (item.Kind == RunCardKind::Card)
				{
					const CardDef& card = GetCard(item.Upgrade());
					look = { card.Name, RunCardDescription(item.Upgrade()), "CARD", "", card.Rarity >= CardRarity::Legendary ? Theme::GoldLight : card.Rarity >= CardRarity::Rare ? Hex(0xB59CFF) : Hex(0xC8D2E8), IconFor(item.Upgrade()), 0 };
				}
				else
					look = ItemLookOf(item);
				const float alpha = sold ? 0.4f : 1.0f;
				draw.Rect(row, { .Fill = Rgba(0.04f, 0.035f, 0.08f, 0.85f * alpha), .Border = WithAlpha(look.Color, 0.55f * alpha), .BorderWidth = 1.0f, .Chamfer = 8.0f });
				UiRect inner = row.Shrink(10.0f, 6.0f);
				draw.Icon(look.Icon, inner.CutLeft(46.0f).Shrink(2.0f, 4.0f), { .Color = WithAlpha(Lighten(look.Color, 0.5f), alpha), .ColorBottom = WithAlpha(look.Color, alpha), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
				inner.CutLeft(8.0f);
				const UiRect button = UiRect::Centered(inner.CutRight(118.0f).Center(), { 108.0f, 32.0f });
				UiRect header = inner.CutTop(20.0f);
				TextStyle tag = Theme::Bold(10.5f, WithAlpha(look.Color, 0.9f * alpha));
				tag.Tracking = 1.4f;
				draw.TextAligned(header.CutRight(80.0f), look.Tag, tag, { 1.0f, 0.5f });
				draw.TextAligned(header, look.Name, Theme::Bold(15.5f, WithAlpha(Lighten(look.Color, 0.3f), alpha)), { 0.0f, 0.5f });
				draw.TextWrapped(inner, look.Text, Theme::Body(12.5f, WithAlpha(Theme::Muted, alpha)), 0.0f, 1.0f);
				if (sold)
					draw.TextAligned(button, "Sold", Theme::Bold(14.0f, Theme::Faint), { 0.5f, 0.5f });
				else if (ui.Button(Ui::MakeId("buy", slot), button, Format("Buy  %u", unsigned(price)), ButtonKind::Primary, loadout.Shards >= price))
					m_Client.BuyRunItem(slot);
			}
		}
		else if (kind == RoomKind::Shrine)
		{
			for (uint32_t choice = 1; choice < uint32_t(ShrineChoice::Count); choice++)
			{
				UiRect row = content.CutTop(110.0f);
				const bool chosen = loadout.Rested == choice;
				draw.Rect(row.Shrink(0.0f, 6.0f), { .Fill = Rgba(0.05f, 0.04f, 0.09f, 0.8f), .Border = WithAlpha(chosen ? Theme::Gold : Theme::Edge, 0.8f), .BorderWidth = 1.0f, .Chamfer = 10.0f });
				UiRect inner = row.Shrink(16.0f, 14.0f);
				UiRect side = inner.CutRight(150.0f);
				draw.TextAligned(inner.CutTop(26.0f), ShrineNames[choice], Theme::Title(19.0f), { 0.0f, 0.5f });
				draw.TextWrapped(inner, ShrineTexts[choice], Theme::Body(14.0f, Theme::Muted), 0.0f, 1.05f);
				const UiRect button = UiRect::Centered(side.Center(), { 130.0f, 34.0f });
				if (chosen)
					draw.TextAligned(button, "Chosen", Theme::Bold(15.0f, Theme::Gold), { 0.5f, 0.5f });
				else if (ui.Button(Ui::MakeId("shrine", choice), button, "Choose", ButtonKind::Primary, loadout.Rested == 0))
					m_Client.UseShrine(choice);
			}
		}
		content.CutTop(10.0f);
		if (kind == RoomKind::Shrine)
			return;
		ui.Separator(content.CutTop(20.0f), "YOUR SIGILS");
		draw.TextFit(content.CutTop(20.0f), "The sigil in the first slot works twice with Leftmost Rule. Arrows swap neighbours.", Theme::Body(13.0f, Theme::Faint));
		UiRect row = content.CutTop(110.0f);
		const float slotWidth = row.W / float(SigilSlots);
		for (uint32_t slot = 0; slot < SigilSlots; slot++)
		{
			UiRect cell = row.CutLeft(slotWidth).Shrink(4.0f, 4.0f);
			const SigilId sigil = loadout.Sigils[slot];
			draw.Rect(cell, { .Fill = Rgba(0.04f, 0.035f, 0.08f, 0.85f), .Border = WithAlpha(sigil == SigilId::None ? Theme::Edge : Hex(SigilColor), 0.7f), .BorderWidth = 1.0f, .Chamfer = 8.0f });
			UiRect inner = cell.Shrink(8.0f, 6.0f);
			draw.TextAligned(inner.CutTop(16.0f), Format("SLOT %u", slot + 1), Theme::Bold(10.5f, Theme::Faint), { 0.5f, 0.5f });
			if (sigil == SigilId::None)
			{
				draw.TextAligned(inner, "empty", Theme::Body(13.0f, Theme::Faint), { 0.5f, 0.5f });
				continue;
			}
			const SigilDef& def = GetSigil(sigil);
			const UiRect name = inner.CutTop(36.0f);
			draw.TextWrapped(name, def.Name, Theme::Bold(13.5f, Lighten(Hex(SigilColor), 0.3f)), 0.5f, 1.0f);
			if (ui.Interact(Ui::MakeId("sigiltip", slot), name).Hovered)
			{
				UiTooltip& tooltip = ui.Tooltip();
				tooltip.Header(def.Name, Hex(SigilColor));
				tooltip.Text(def.Text);
			}
			UiRect buttons = inner.CutBottom(26.0f);
			const float third = buttons.W / 3.0f;
			if (ui.Button(Ui::MakeId("sigilleft", slot), buttons.CutLeft(third).Shrink(2.0f, 0.0f), "<", ButtonKind::Normal, slot > 0))
				m_Client.SwapSigils(slot, slot - 1);
			const UiRect sell = buttons.CutLeft(third).Shrink(2.0f, 0.0f);
			if (kind == RoomKind::Shop && ui.Button(Ui::MakeId("sigilsell", slot), sell, Format("+%u", unsigned(float(def.Price) * SigilSellShare)), ButtonKind::Normal, true))
				m_Client.SellSigil(slot);
			if (ui.Button(Ui::MakeId("sigilright", slot), buttons.Shrink(2.0f, 0.0f), ">", ButtonKind::Normal, slot + 1 < SigilSlots))
				m_Client.SwapSigils(slot, slot + 1);
		}
		if (kind != RoomKind::Shop)
			return;
		std::vector<CardId> owned;
		for (const CardId id : RunCardPool)
			if (HasCard(loadout.Cards, id))
				owned.push_back(id);
		constexpr float RowHeight = 30.0f;
		const UiRect side{ home.Right() + 12.0f, home.Y, 380.0f, std::min(104.0f + RowHeight * float(std::max<size_t>(owned.size(), 1)), 640.0f) };
		UiRect cards = ui.Window(Ui::MakeId("merchantcards"), "Your Cards", side);
		draw.TextFit(cards.CutTop(20.0f), "Sell a card you no longer want for half its price.", Theme::Body(13.0f, Theme::Faint));
		cards.CutTop(6.0f);
		if (owned.empty())
			draw.TextAligned(cards.CutTop(RowHeight), "No cards yet", Theme::Body(14.0f, Theme::Faint), { 0.0f, 0.5f });
		for (const CardId id : owned)
		{
			if (cards.H < RowHeight)
				break;
			UiRect line = cards.CutTop(RowHeight).Shrink(0.0f, 2.0f);
			const CardDef& card = GetCard(id);
			const uint8_t stack = StackOf(loadout.Cards, loadout.Second, loadout.Third, id);
			const UiColor tint = card.Rarity >= CardRarity::Legendary ? Theme::GoldLight : card.Rarity >= CardRarity::Rare ? Hex(0xB59CFF) : Hex(0xC8D2E8);
			const UiRect button = line.CutRight(96.0f);
			draw.TextAligned(line, stack > 1 ? Format("%s  x%u", card.Name, unsigned(stack)) : std::string(card.Name), Theme::Bold(14.0f, tint), { 0.0f, 0.5f });
			if (ui.Interact(Ui::MakeId("cardtip", uint64_t(id)), line).Hovered)
				ui.Tooltip().Header(card.Name, tint, std::nullopt, IconFor(id)).Text(RunCardDescription(id));
			if (ui.Button(Ui::MakeId("cardsell", uint64_t(id)), button, Format("Sell +%u", unsigned(CardSellPrice(id))), ButtonKind::Normal, true))
				m_Client.SellCard(id);
		}
	}

	void SessionState::DrawRunItems()
	{
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (m_Client.GetSession().Mode != GameMode::Shardfall || !local.Valid || !UsesRooms(m_Client.GetSession().Run))
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const RunLoadout& loadout = local.Run;
		std::vector<std::pair<std::string, UiColor>> lines;
		lines.push_back({ Format("%u shards", unsigned(loadout.Shards)), Hex(0x8FE3FF) });
		for (uint32_t slot = 0; slot < SigilSlots; slot++)
			if (loadout.Sigils[slot] != SigilId::None)
				lines.push_back({ Format("%u  ", slot + 1) + GetSigil(loadout.Sigils[slot]).Name, Lighten(Hex(SigilColor), 0.2f) });
		for (const DuoDef& duo : Duos)
			if (HasDuo(loadout, duo.Id))
				lines.push_back({ duo.Name, Hex(DuoColor) });
		for (const CurseDef& curse : Curses)
			if (HasCurse(loadout, curse.Id))
				lines.push_back({ std::string("Cursed: ") + curse.Name, Hex(CurseColor) });
		float y = ui.GetSize().y - 64.0f - 20.0f * float(lines.size());
		for (const auto& [text, color] : lines)
		{
			draw.Text({ 16.0f, y }, text, Theme::Floating(13.5f, color, UiFont::Bold));
			y += 20.0f;
		}
	}

	void SessionState::DrawChampionBar()
	{
		const RunStatus& run = m_Client.GetStatus().Run;
		if (m_Client.GetSession().Mode != GameMode::Shardfall || run.Champion == 0 || run.Phase != RunPhase::Act)
			return;
		const EntityState* champion = m_Client.FindEntity(run.Champion);
		if (!champion || champion->Has(EntityFlags::Dead))
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const ChampionDef* def = FindChampion(champion->Kind);
		const bool enraged = champion->Has(EntityFlags::Enraged);
		const UiColor tone = enraged ? Hex(0xFF5A3C) : Hex(0xE8B04A);
		const float fill = std::clamp(float(champion->Health) / 255.0f, 0.0f, 1.0f);
		const UiRect bar = UiRect::Centered({ ui.GetSize().x * 0.5f, 172.0f }, { 560.0f, 12.0f });
		TextStyle name = Theme::Title(19.0f);
		name.Tracking = 1.5f;
		draw.TextAligned({ bar.X, bar.Y - 28.0f, bar.W, 24.0f }, Upper(def ? def->Name : "Champion") + (enraged ? "  -  ENRAGED" : ""), name, { 0.5f, 0.5f });
		draw.Rect(bar.Expand(3.0f), { .Fill = Rgba(0.02f, 0.0f, 0.02f, 0.85f), .Border = WithAlpha(tone, 0.6f), .BorderWidth = 1.2f, .Chamfer = 4.0f });
		if (fill > 0.0f)
			draw.Rect({ bar.X, bar.Y, bar.W * fill, bar.H }, { .Fill = Lighten(tone, 0.2f), .FillBottom = Darken(tone, 0.35f), .Glow = WithAlpha(tone, 0.35f), .GlowSize = 8.0f, .Chamfer = 3.0f });
		for (const float mark : ChampionPhases)
			draw.Diamond({ bar.X + bar.W * mark, bar.Center().y }, 12.0f, Rgba(0.04f, 0.02f, 0.05f, 0.95f), WithAlpha(fill > mark ? Theme::GoldLight : Theme::Faint, 0.9f), 1.2f);
	}

	void SessionState::DrawGrimoire()
	{
		const SessionInfo& session = m_Client.GetSession();
		const GameStatus& status = m_Client.GetStatus();
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		Ui& ui = m_App.GetUi();
		m_RuneNote = std::max(m_RuneNote - ui.GetDeltaTime(), 0.0f);
		if (session.Mode != GameMode::Shardfall || !local.Valid || IsRunOver(status.Run.Phase))
		{
			m_GrimoireOpen = false;
			m_SelectedRune.reset();
			return;
		}
		const RunLoadout& run = local.Run;
		const RuneMask loose = LooseRunes(run);
		const bool choosing = run.HasOffer() && status.Run.Paused;
		const bool locked = status.Run.Fighting;
		const glm::vec2 size = ui.GetSize();
		if (locked && !m_RuneFighting)
			m_GrimoireOpen = false;
		m_RuneFighting = locked;
		if (!m_GrimoireOpen || m_Paused || choosing)
		{
			if (loose != 0 && !m_Paused && !choosing && !locked)
			{
				const float pulse = 0.7f + 0.3f * std::sin(m_Time * 4.0f);
				ui.Draw().TextAligned({ 0.0f, size.y - 258.0f, size.x, 22.0f }, std::popcount(loose) > 1 ? "G  socket your new runes" : "G  socket your new rune", Theme::Floating(15.0f, WithAlpha(Lighten(Hex(RuneColor), 0.2f), pulse), UiFont::Bold), { 0.5f, 0.5f });
			}
			m_SelectedRune.reset();
			return;
		}
		constexpr float Width = 620.0f;
		constexpr float RowHeight = 64.0f;
		constexpr float Cell = 52.0f;
		const uint32_t looseCount = uint32_t(std::popcount(loose));
		const uint32_t perRow = uint32_t((Width - 44.0f) / Cell);
		const float pouchHeight = Cell * float(std::max((looseCount + perRow - 1u) / perRow, 1u)) + 8.0f;
		const UiRect home{ 24.0f, 110.0f, Width, 150.0f + RowHeight * float(AbilitySlots) + pouchHeight };
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("grimoire"), "Grimoire", home, &open);
		if (!open)
		{
			m_GrimoireOpen = false;
			m_SelectedRune.reset();
			return;
		}
		UiDrawList& draw = ui.Draw();
		const UiPayload* drag = ui.GetDrag();
		const bool dragging = drag && drag->Type == RunePayload;
		if (m_SelectedRune && !HasRune(run.Runes, *m_SelectedRune))
			m_SelectedRune.reset();
		const std::optional<RuneId> held = dragging ? std::optional<RuneId>(RuneId(drag->Value)) : m_SelectedRune;
		const std::string guide = locked ? std::string("Enemies are awake. Runes stay locked until the fight is over.") : m_RuneNote > 0.0f ? std::string("That rune does not fit there.") : held ? Format("%s fits: %s. Drop it on a glowing socket.", GetRune(*held).Name, GetRune(*held).Needs) : std::string("Drag a rune onto a socket, or click it and then a socket. Right-click takes it out.");
		draw.TextFit(content.CutTop(20.0f), guide, Theme::Body(13.5f, locked || m_RuneNote > 0.0f ? Theme::Danger : held ? Lighten(RuneTone(*held), 0.25f) : Theme::Muted));
		content.CutTop(8.0f);
		const auto refuse = [&]
		{
			m_App.GetGameAudio().PlayUi(Cue::UiError, 0.6f);
			m_RuneNote = locked ? 0.0f : 2.0f;
			m_SelectedRune.reset();
		};
		const auto place = [&](RuneId rune, uint32_t slot, uint32_t socket)
		{
			if (locked || !CanSocket(run, rune, slot, socket))
				return refuse();
			if (run.Sockets[slot][socket] != rune)
			{
				m_Client.SocketRune(rune, slot, socket);
				m_App.GetGameAudio().PlayUi(Cue::UiConfirm, 0.7f);
			}
			m_SelectedRune.reset();
		};
		const auto remove = [&](RuneId rune)
		{
			if (locked)
				return refuse();
			m_Client.UnsocketRune(rune);
			m_App.GetGameAudio().PlayUi(Cue::UiClick, 0.7f);
			m_SelectedRune.reset();
		};
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
		{
			const UiRect row = content.CutTop(RowHeight).Shrink(0.0f, 3.0f);
			const AbilityId spell = SlotSpell(run, slot);
			const bool empty = spell >= AbilityId::Count;
			const bool fits = held && !empty && RuneFits(*held, spell);
			const float dim = held && !fits ? 0.38f : 1.0f;
			const UiColor color = empty ? Theme::Faint : WithAlpha(SkillColor(spell));
			draw.Rect(row, { .Fill = Rgba(0.04f, 0.035f, 0.08f, 0.85f * dim), .Border = WithAlpha(fits ? Hex(RuneColor) : color, (fits ? 0.85f : 0.4f) * dim), .BorderWidth = fits ? 1.5f : 1.0f, .Chamfer = 8.0f });
			UiRect inner = row.Shrink(10.0f, 6.0f);
			const UiRect tile = UiRect::Centered(inner.CutLeft(46.0f).Center(), { 44.0f, 44.0f });
			inner.CutLeft(10.0f);
			draw.Rect(tile, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Border = WithAlpha(color, 0.85f * dim), .BorderWidth = 1.2f, .Chamfer = 6.0f });
			if (empty)
			{
				draw.Icon(UiIcon::Emerald, tile.Shrink(12.0f), { .Color = Rgba(0.62f, 0.56f, 0.9f, 0.2f) });
				draw.TextAligned(inner, Format("Empty spell slot on %s", RunKeys[slot - 1]), Theme::Body(14.0f, WithAlpha(Theme::Faint, dim)), { 0.0f, 0.5f });
				continue;
			}
			draw.Icon(IconFor(spell), tile.Shrink(7.0f), { .Color = WithAlpha(Lighten(color, 0.5f), dim), .ColorBottom = WithAlpha(color, dim), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			if (ui.Interact(Ui::MakeId("grimspell", slot), tile).Hovered && !held)
				DescribeBrawlSpell(ui.Tooltip(), spell);
			UiRect sockets = inner.CutRight(float(MaxSockets) * 48.0f);
			UiRect label = inner;
			draw.TextFit(label.CutTop(26.0f), GetAbility(spell).Name, Theme::Bold(16.0f, WithAlpha(Lighten(color, 0.3f), dim)), { 0.0f, 0.7f });
			draw.TextFit(label, Format("Rank %s   %s", RomanRanks[run.Ranks[slot]], SpellFormName(spell)), Theme::Body(12.5f, WithAlpha(Theme::Faint, dim)), { 0.0f, 0.2f });
			const uint8_t opened = OpenSockets(run, slot);
			for (uint32_t socket = 0; socket < MaxSockets; socket++)
			{
				const glm::vec2 center{ sockets.X + 24.0f + 48.0f * float(socket), sockets.Center().y };
				const UiRect hit = UiRect::Centered(center, { 44.0f, 44.0f });
				const UiId id = Ui::MakeId("runesocket", slot * MaxSockets + socket);
				const UiInteraction interaction = ui.Interact(id, hit);
				if (socket >= opened)
				{
					const char* rank = socket == 2 ? "II" : "IV";
					draw.Circle(center, 13.0f, Rgba(0.02f, 0.02f, 0.04f, 0.6f * dim), WithAlpha(Theme::Edge, 0.35f * dim), 1.0f);
					draw.TextAligned(hit, rank, Theme::Bold(10.5f, WithAlpha(Theme::Faint, 0.85f * dim)), { 0.5f, 0.5f });
					if (interaction.Hovered && !held)
						ui.Tooltip().Text(Format("This socket opens when %s reaches rank %s.", GetAbility(spell).Name, rank), Theme::Muted);
					continue;
				}
				const RuneId rune = run.Sockets[slot][socket];
				const bool filled = rune < RuneId::Count;
				const float hover = ui.Animate(id, interaction.Hovered);
				const bool target = fits && !locked;
				const float beckon = 0.55f + 0.45f * std::sin(m_Time * 6.0f);
				draw.Circle(center, 19.0f, Rgba(0.015f, 0.012f, 0.03f, 0.95f * dim), WithAlpha(target ? Lighten(Hex(RuneColor), 0.2f) : Theme::Edge, (target ? 0.45f + 0.55f * beckon : 0.6f + 0.4f * hover) * dim), target ? 1.8f : 1.2f);
				if (filled)
				{
					ui.OfferDrag(id, RunePayload, int(rune));
					const bool lifted = dragging && RuneId(drag->Value) == rune;
					if (!lifted)
						DrawRuneStone(draw, rune, center, 16.0f, (RuneFits(rune, spell) ? 1.0f : 0.4f) * dim, std::max(hover, m_SelectedRune == rune ? 1.0f : 0.0f));
					if (m_SelectedRune == rune)
						draw.Circle(center, 21.5f, UiColor(0.0f), WithAlpha(Theme::GoldLight, 0.9f), 1.6f);
				}
				if (const std::optional<int> dropped = ui.AcceptDrop(hit, RunePayload))
					place(RuneId(*dropped), slot, socket);
				else if (interaction.RightClicked && filled)
					remove(rune);
				else if (interaction.Clicked && m_SelectedRune && m_SelectedRune != rune)
					place(*m_SelectedRune, slot, socket);
				else if (interaction.Clicked && filled)
					m_SelectedRune = m_SelectedRune == rune ? std::nullopt : std::optional<RuneId>(rune);
				if (interaction.Hovered && filled && !dragging)
					DescribeRune(ui.Tooltip(), rune, &run);
			}
		}
		content.CutTop(10.0f);
		ui.Separator(content.CutTop(20.0f), "RUNE POUCH");
		content.CutTop(4.0f);
		const UiRect pouch = content.CutTop(pouchHeight);
		if (const std::optional<int> dropped = ui.AcceptDrop(pouch, RunePayload); dropped && FindRune(run, RuneId(*dropped)).IsSet())
			remove(RuneId(*dropped));
		if (looseCount == 0)
			draw.TextAligned(pouch, run.Runes == 0 ? "No runes yet. Rune rooms, champion chests and the merchant carry them." : "Every rune you own sits in a socket.", Theme::Body(14.0f, Theme::Faint), { 0.5f, 0.5f });
		uint32_t index = 0;
		for (uint32_t id = 0; id < uint32_t(RuneId::Count); id++)
		{
			const RuneId rune = RuneId(id);
			if (!HasRune(loose, rune))
				continue;
			const glm::vec2 center{ pouch.X + 26.0f + Cell * float(index % perRow), pouch.Y + 4.0f + Cell * (float(index / perRow) + 0.5f) };
			index++;
			const UiRect hit = UiRect::Centered(center, { 46.0f, 46.0f });
			const UiId stone = Ui::MakeId("pouchrune", id);
			const UiInteraction interaction = ui.Interact(stone, hit);
			ui.OfferDrag(stone, RunePayload, int(rune));
			const float hover = ui.Animate(stone, interaction.Hovered);
			const bool selected = m_SelectedRune == rune;
			const bool lifted = dragging && RuneId(drag->Value) == rune;
			if (interaction.Clicked)
				m_SelectedRune = selected ? std::nullopt : std::optional<RuneId>(rune);
			if (!lifted)
				DrawRuneStone(draw, rune, center - glm::vec2(0.0f, 2.0f * hover), 19.0f, RuneFitsBuild(run, rune) ? 1.0f : 0.45f, std::max(hover, selected ? 1.0f : 0.0f));
			if (selected)
				draw.Circle(center, 24.0f, UiColor(0.0f), WithAlpha(Theme::GoldLight, 0.9f), 1.6f);
			if (interaction.Hovered && !dragging)
				DescribeRune(ui.Tooltip(), rune, &run);
		}
		if (dragging)
		{
			const UiLayer layer = ui.GetLayer();
			ui.SetLayer(UiLayer::Drag);
			DrawRuneStone(ui.Draw(), RuneId(drag->Value), ui.GetMouse(), 21.0f, 1.0f, 1.0f);
			ui.SetLayer(layer);
		}
	}
}
