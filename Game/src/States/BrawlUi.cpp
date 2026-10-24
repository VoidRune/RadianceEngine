#include "SessionState.h"
#include "App/GameApp.h"
#include "Audio/GameAudio.h"
#include "Rendering/Models.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include "UI/Palette.h"
#include "UI/Ui.h"
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
		constexpr glm::vec3 WardGold{ 1.0f, 0.86f, 0.48f };
		constexpr glm::vec3 LavaGlow{ 1.0f, 0.42f, 0.12f };
		constexpr glm::vec3 FrostGlow{ 0.3f, 0.62f, 1.0f };
		constexpr glm::vec3 MarkGlow{ 0.66f, 0.4f, 1.0f };
		constexpr glm::vec3 BarrierGlow{ 0.72f, 0.88f, 1.0f };
		constexpr const char* SpellKeys[] = { "RMB", "Q", "R" };
		static_assert(std::size(SpellKeys) == SkillSlots);
		constexpr Rdn::KeyCode CardKeys[] = { Rdn::KeyCode::Key1, Rdn::KeyCode::Key2, Rdn::KeyCode::Key3 };
		static_assert(std::size(CardKeys) == CardChoices);
		constexpr float CardNewsSeconds = 9.0f;
		constexpr float ApexNewsSeconds = 4.5f;
		constexpr glm::vec3 LeylineGlow{ 0.42f, 1.0f, 0.78f };
		constexpr float CrumbleRange = 18.0f;
		constexpr glm::vec3 StarlightPale{ 0.55f, 0.66f, 1.0f };
		constexpr float SigilScale = 1.05f;
		constexpr const char* BotLevelNames[] = { "Easy", "Normal", "Hard" };
		constexpr const char* ArenaNames[] = { "Random", "Caldera", "Frozen Lake" };
		constexpr const char* ModeNames[] = { "Damage", "Knockback" };
		static_assert(std::size(BotLevelNames) == BotLevels);
		static_assert(std::size(ModeNames) == size_t(BrawlMode::Count));

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
			{ AbilityId::Gust, 7, { { 0.0f, 0.0f }, { 0.3f, 0.2f }, { 0.25f, 0.6f }, { -0.2f, 0.75f }, { -0.65f, 0.35f }, { -0.6f, -0.35f }, { 0.1f, -0.75f } }, 6, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 } } },
			{ AbilityId::Comet, 6, { { 0.6f, 0.6f }, { 0.15f, 0.2f }, { -0.35f, -0.25f }, { -0.85f, -0.7f }, { 0.3f, 0.7f }, { 0.7f, 0.3f } }, 5, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 4, 1 }, { 5, 1 } } },
			{ AbilityId::Warp, 6, { { -0.85f, -0.55f }, { -0.25f, 0.3f }, { 0.1f, -0.3f }, { 0.8f, 0.55f }, { 0.4f, 0.62f }, { 0.75f, 0.15f } }, 5, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 3, 5 } } },
			{ AbilityId::Shockwave, 7, { { 0.0f, 0.0f }, { 0.8f, 0.0f }, { 0.4f, 0.69f }, { -0.4f, 0.69f }, { -0.8f, 0.0f }, { -0.4f, -0.69f }, { 0.4f, -0.69f } }, 6, { { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 }, { 6, 1 } } },
			{ AbilityId::GravityWell, 7, { { 0.85f, 0.05f }, { 0.4f, 0.65f }, { -0.35f, 0.55f }, { -0.55f, -0.1f }, { -0.1f, -0.45f }, { 0.25f, -0.1f }, { 0.0f, 0.1f } }, 6, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 } } },
			{ AbilityId::ArcLightning, 6, { { -0.15f, 0.9f }, { 0.3f, 0.25f }, { -0.2f, 0.05f }, { 0.25f, -0.5f }, { -0.1f, -0.9f }, { 0.65f, 0.55f } }, 5, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 1, 5 } } },
			{ AbilityId::Swap, 6, { { -0.8f, 0.3f }, { 0.8f, 0.3f }, { 0.5f, 0.6f }, { 0.8f, -0.3f }, { -0.8f, -0.3f }, { -0.5f, -0.6f } }, 4, { { 0, 1 }, { 1, 2 }, { 3, 4 }, { 4, 5 } } },
			{ AbilityId::MirrorWard, 6, { { 0.0f, 0.85f }, { 0.65f, 0.5f }, { 0.55f, -0.3f }, { 0.0f, -0.85f }, { -0.55f, -0.3f }, { -0.65f, 0.5f } }, 6, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 0 } } },
			{ AbilityId::Frostbite, 7, { { 0.0f, 0.0f }, { 0.0f, 0.85f }, { 0.74f, 0.42f }, { 0.74f, -0.42f }, { 0.0f, -0.85f }, { -0.74f, -0.42f }, { -0.74f, 0.42f } }, 6, { { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 }, { 0, 6 } } },
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
			return std::max(CollapseGrace + (plan.StartRadius - CollapseMinRadius) / CollapseRate, FinalCollapseStart);
		}

		std::string SpellDamage(AbilityId id)
		{
			const AbilityDef& def = GetAbility(id);
			switch (id)
			{
			case AbilityId::Firebomb:
				return Format("%d, then %d on the skip", int(def.BrawlDamage), int(GetAbility(AbilityId::FirebombSkip).BrawlDamage));
			case AbilityId::StormStrike:
				return Format("%d, then %d on the jump", int(def.BrawlDamage), int(GetAbility(AbilityId::StormJump).BrawlDamage));
			case AbilityId::SplittingBolt:
				return Format("%d, or %d per shard", int(def.BrawlDamage), int(GetAbility(AbilityId::BoltShard).BrawlDamage));
			default:
				return def.Chain > 0 ? Format("%d per rival", int(def.BrawlDamage)) : std::to_string(def.BrawlDamage);
			}
		}

		std::string SpellSummary(AbilityId id, BrawlMode mode)
		{
			const AbilityDef& def = GetAbility(id);
			if (mode == BrawlMode::Damage)
			{
				const std::string cost = def.BrawlMana > 0.0f ? Format("%.0f mana", double(def.BrawlMana)) : std::string("free");
				const std::string effect = def.BrawlDamage > 0 ? Format("%d dmg", int(def.BrawlDamage)) : def.Ward > 0.0f ? std::string("Reflects") : std::string("Teleports");
				return Format("%s, %s   %g s", effect.c_str(), cost.c_str(), double(def.BrawlCooldown > 0.0f ? def.BrawlCooldown : def.Cooldown));
			}
			switch (id)
			{
			case AbilityId::ChainHook:
				return Format("Hooks and pulls   %.0f s", double(def.Cooldown));
			case AbilityId::Firebomb:
				return Format("Shove %.0f twice   %.1f s", double(def.Push), double(def.Cooldown));
			case AbilityId::SplittingBolt:
				return Format("Shove %.0f, splits   %.0f s", double(def.Push), double(def.Cooldown));
			case AbilityId::StormStrike:
				return Format("Shove %.0f, jumps   %.1f s", double(def.Push), double(def.Cooldown));
			default:
				break;
			}
			if (def.Swap)
				return Format("Trades places   %.0f s", double(def.Cooldown));
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
			case AbilityId::Gust:
				return "Blast";
			case AbilityId::Comet:
				return "Zone";
			case AbilityId::Warp:
				return "Blink";
			case AbilityId::Shockwave:
				return "Burst";
			case AbilityId::GravityWell:
				return "Pull";
			case AbilityId::ArcLightning:
				return "Chain";
			case AbilityId::Swap:
				return "Swap";
			case AbilityId::MirrorWard:
				return "Block";
			case AbilityId::Frostbite:
				return "Slow";
			case AbilityId::ChainHook:
				return "Hook";
			case AbilityId::Firebomb:
				return "Lob";
			case AbilityId::SplittingBolt:
				return "Split";
			case AbilityId::StormStrike:
				return "Strike";
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

		void DescribeSpell(UiTooltip& tooltip, AbilityId id, BrawlMode mode)
		{
			const AbilityDef& def = GetAbility(id);
			const UiColor color = WithAlpha(SkillColor(id));
			tooltip.Accent = WithAlpha(color, 0.85f);
			tooltip.Header(def.Name, color, std::nullopt, IconFor(id));
			tooltip.Space(2.0f).Text(def.Description, Theme::Text, 15.0f).Space(4.0f);
			tooltip.Pair("Role", ActionWord(id));
			const bool damage = mode == BrawlMode::Damage;
			if (damage)
			{
				if (def.BrawlDamage > 0)
					tooltip.Pair("Damage", SpellDamage(id));
				tooltip.Pair("Mana", def.BrawlMana > 0.0f ? Format("%.0f", double(def.BrawlMana)) : std::string("free"));
			}
			const float shove = damage ? DamagePushScale : 1.0f;
			if (def.Push > 0.0f)
				tooltip.Pair("Shove", Format(damage ? "%.1f" : "%.0f", double(def.Push * shove)));
			else if (def.Push < 0.0f)
				tooltip.Pair("Pull", "through the center");
			else if (id == AbilityId::ChainHook)
				tooltip.Pair("Pull", "to you and past you");
			if (id == AbilityId::ChainHook)
				tooltip.Pair("Press again", "yank yourself to the hook");
			else if (id == AbilityId::SplittingBolt)
				tooltip.Pair("Press again", Format("split into %u shards", unsigned(BoltShards)));
			else if (id == AbilityId::Firebomb)
				tooltip.Pair("Skips", Format(damage ? "%.1f further, shove %.1f" : "%.1f further, shove %.0f", double(FirebombSkipDistance), double(GetAbility(AbilityId::FirebombSkip).Push * shove)));
			else if (id == AbilityId::StormStrike)
				tooltip.Pair("Jumps", Format("once, up to %.1f", double(GetAbility(AbilityId::StormJump).Range)));
			if (def.Kind == AbilityKind::Projectile)
				tooltip.Pair("Range", Format("%.1f", double(def.ProjectileRange)));
			else if (def.Kind == AbilityKind::Ground || def.Kind == AbilityKind::Dash)
				tooltip.Pair("Range", Format("%.1f", double(def.Range)));
			else if (def.Reach > 0.0f)
				tooltip.Pair("Reach", Format("%.1f", double(def.Reach)));
			if (def.Chain > 0)
				tooltip.Pair("Jumps", std::to_string(def.Chain));
			tooltip.Pair("Cast time", def.CastTime > 0.0f ? Format("%.2f s", double(def.CastTime)) : std::string("instant"));
			tooltip.Pair("Cooldown", Format("%.1f s", double(damage && def.BrawlCooldown > 0.0f ? def.BrawlCooldown : def.Cooldown)));
			tooltip.Space(2.0f).Muted(def.CastMove <= 0.01f ? "You stand still while casting" : def.CastMove >= 0.99f ? "Cast it on the move" : "You slow down while casting");
		}
	}

	void SessionState::UpdateArena(float dt)
	{
		m_RestoreAge += dt;
		m_CrumbleCooldown = std::max(m_CrumbleCooldown - dt, 0.0f);
		const Level* level = m_Client.GetLevel();
		if (!level || m_Client.GetSession().Mode != GameMode::Brawl)
			return;
		const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
		UpdateCardNews(dt);
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
				const glm::vec3 center(seed.x, LavaSurface + 0.1f, seed.y);
				if (level->Kind == MapKind::FrozenLake)
					m_Effects->Burst(Fx::FrostBurst, center + glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.6f);
				else
				{
					m_Effects->Burst(Fx::FireBurst, center, glm::vec3(0.0f, 1.0f, 0.0f), 0.55f);
					m_Effects->Burst(Fx::Debris, center + glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.5f);
				}
				if (m_CrumbleCooldown <= 0.0f)
				{
					m_App.GetGameAudio().PlayCrumble(Vec2(center.x, center.z), level->Kind == MapKind::FrozenLake);
					m_CrumbleCooldown = 0.35f;
				}
			}
	}

	void SessionState::DrawBrawlPlayer(WorldRenderer& renderer, const EntityState& entity, const glm::vec3& base) const
	{
		if (entity.Has(EntityFlags::Warded))
		{
			const float pulse = 0.75f + 0.25f * std::sin(m_Time * 9.0f + float(entity.NetId));
			DrawGlow(renderer, base + glm::vec3(0.0f, 0.9f, 0.0f), 1.15f, WardGold * (0.3f * pulse), 0.6f);
			for (uint32_t i = 0; i < 3; i++)
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.25f + 0.6f * float(i), 0.0f), 0.78f - 0.14f * std::abs(float(i) - 1.0f), WardGold * (0.8f * pulse), 0.03f);
		}
		if (entity.Has(EntityFlags::Burning))
		{
			const float flicker = 0.7f + 0.3f * std::sin(m_Time * 17.0f + float(entity.NetId) * 1.3f);
			const glm::vec3 glow = m_Client.GetSession().Map == MapKind::FrozenLake ? FrostGlow : LavaGlow;
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
		if (entity.Has(EntityFlags::Leyline))
		{
			const float pulse = 0.75f + 0.25f * std::sin(m_Time * 3.0f + float(entity.NetId));
			DrawGroundGlow(renderer, base, 1.1f, LeylineGlow, 0.7f * pulse);
			DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.06f, 0.0f), 0.95f, LeylineGlow * (0.9f * pulse), 0.035f);
			DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.06f, 0.0f), 0.62f, LeylineGlow * (0.5f * pulse), 0.025f);
		}
		else if (entity.NetId == m_Client.GetPlayerNetId() && HasCard(m_Client.GetLocalPlayer().Brawl.Cards, CardId::Leyline))
		{
			const float charge = std::clamp(m_Client.GetPredictedState().Still / LeylineSeconds, 0.0f, 1.0f);
			if (charge > 0.05f)
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.06f, 0.0f), 0.3f + 0.65f * charge, LeylineGlow * (0.45f * charge), 0.025f);
		}
		if (entity.Has(EntityFlags::Barrier))
		{
			const float shimmer = 0.8f + 0.2f * std::sin(m_Time * 5.0f + float(entity.NetId));
			DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.75f, 0.0f), 0.7f, BarrierGlow * (0.3f * shimmer), 0.03f);
			DrawFloatingRing(renderer, base + glm::vec3(0.0f, 1.35f, 0.0f), 0.5f, BarrierGlow * (0.2f * shimmer), 0.025f);
			DrawGlow(renderer, base + glm::vec3(0.0f, 0.85f, 0.0f), 1.0f, BarrierGlow * (0.1f * shimmer), 0.6f);
		}
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
		else
			draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.4f, 0.08f, 0.0f, 0.07f), .Glow = Rgba(1.0f, 0.3f, 0.05f, 0.55f * throb), .GlowSize = 160.0f });
		draw.TextAligned({ 0.0f, ui.GetSize().y * 0.2f, ui.GetSize().x, 24.0f }, TermsFor(map).Danger, Theme::Bold(18.0f, map == MapKind::FrozenLake ? Hex(0xA8E4FF) : Hex(0xFFC08A)), { 0.5f, 0.5f });
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
		const uint32_t self = m_Client.GetPlayerId();
		switch (brawl.Phase)
		{
		case BrawlPhase::Fight:
			m_BannerTitle = Format("Round %u", unsigned(brawl.Round));
			if (m_Client.GetSession().Rules.Mode == BrawlMode::Damage)
				m_BannerText = brawl.Round == brawl.Rounds ? "The last round. Outlast them all" : "Outlast them all";
			else
				m_BannerText = Format(brawl.Round == brawl.Rounds ? "The last round. Shove them all into the %s" : "Shove them into the %s", TermsFor(status.Map).Hazard);
			m_BannerColor = Hex(0xFF8A4A);
			m_BannerTime = 2.6f;
			audio.PlayEvent(AudioEvent::FightStart);
			break;
		case BrawlPhase::Over:
			m_BannerTitle = brawl.RoundWinner == self ? "Round Won" : brawl.RoundWinner == NoPlayer ? "No Survivors" : PlayerName(brawl.RoundWinner);
			m_BannerText = brawl.RoundWinner == self ? Format("+%u points", unsigned(WinPoints(m_Client.GetSession().Rules.Mode))) : brawl.RoundWinner == NoPlayer ? Format("The %s took everyone", TermsFor(status.Map).Place) : Format("takes round %u", unsigned(brawl.Round));
			m_BannerColor = brawl.RoundWinner == self ? Hex(0xFFD36A) : Hex(0xC8C0B4);
			m_BannerTime = 3.5f;
			audio.PlayEvent(brawl.RoundWinner == self ? AudioEvent::RoundWon : AudioEvent::RoundLost);
			break;
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
		if (status.Match == MatchPhase::Ended)
			return;

		std::string line;
		UiColor color = status.Map == MapKind::FrozenLake ? Hex(0x9BE7FF) : Hex(0xFFB27A);
		if (IsChoosing(status))
		{
			line = Format("%u of %u ready", unsigned(brawl.Ready), unsigned(std::max<uint8_t>(brawl.Players, 1)));
			if (!m_LoadoutOpen)
				line += "   Tab opens your spellbook";
			color = Theme::AccentLight;
		}
		else if (brawl.Phase == BrawlPhase::Fight)
		{
			const float final = FinalCollapse(brawl.Plan);
			const BrawlTerms& terms = TermsFor(status.Map);
			if (brawl.Clock < CollapseGrace)
			{
				const std::string left = Seconds(CollapseGrace - brawl.Clock);
				line = std::string("The ") + terms.Floor + " holds for " + left + (left == "1" ? " more second" : " more seconds");
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
			const UiRect pill = UiRect::Centered({ center, 97.0f }, { lineWidth, 28.0f });
			const bool frozen = status.Map == MapKind::FrozenLake;
			draw.Rect(pill, { .Fill = frozen ? Rgba(0.03f, 0.08f, 0.16f, 0.78f) : Rgba(0.16f, 0.05f, 0.03f, 0.78f), .Border = WithAlpha(color, 0.7f), .BorderWidth = 1.0f, .Chamfer = 8.0f });
			draw.Icon(IsChoosing(status) ? UiIcon::ScrollUnfurled : frozen ? UiIcon::Snowflake : UiIcon::Fireball, { pill.X + 10.0f, pill.Center().y - 9.0f, 18.0f, 18.0f }, { .Color = color });
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
					if (IsApex(CardId(bit)))
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
		const UiRect panel = UiRect::Centered({ ui.GetSize().x * 0.5f, 66.0f + 10.0f * (1.0f - fade) }, { 580.0f, 104.0f });
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

	void SessionState::DrawCardFace(const UiRect& rect, CardId id, float hover, float fade, std::optional<uint32_t> key, bool taken, float reveal, float burst)
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
		UiRect body = face.Shrink(18.0f, 16.0f);
		const UiRect tagRect = body.CutTop(18.0f);
		const float iconSize = std::clamp(face.H * 0.24f, 52.0f, 80.0f);
		body.CutTop(face.H * 0.04f);
		const UiRect iconRect = UiRect::Centered({ body.Center().x, body.Y + iconSize * 0.5f + 6.0f }, { iconSize, iconSize });
		const float margin = burst > 0.0f ? 120.0f : legendary ? 58.0f : apex ? 52.0f : card.Rarity == CardRarity::Rare ? 36.0f : 28.0f;
		draw.Card(face, { .Color = rarity, .Rarity = uint32_t(card.Rarity), .Margin = margin, .Hover = lift, .Fade = fade, .Taken = burst, .Reveal = reveal, .Mouse = ui.GetMouse(), .MedallionY = iconRect.Center().y, .MedallionRadius = iconSize * 0.68f, .Seed = float(id) * 0.6180339f });
		fade *= Smooth(0.0f, 0.35f, reveal);
		TextStyle tagStyle = Theme::Bold(12.0f, WithAlpha(apex || legendary ? Lighten(rarity, 0.25f) : rarity, fade));
		tagStyle.Tracking = apex ? 4.0f : 1.8f;
		const std::string tag = card.Kind == CardKind::Upgrade ? "UPGRADE  \xC2\xB7  " + Upper(GetAbility(card.Spell).Name) : apex ? std::string("APEX") : std::string("WILD CARD");
		draw.TextAligned(tagRect, tag, tagStyle, { 0.5f, 0.5f });
		draw.Icon(IconFor(id), iconRect, { .Color = WithAlpha(Lighten(tint, 0.55f), fade), .ColorBottom = WithAlpha(tint, fade), .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f * fade), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f * fade), .GlowColor = WithAlpha(tint, (apex || legendary ? 0.35f : 0.18f) * fade) });
		body.CutTop(iconSize + 22.0f);
		TextStyle nameStyle = Theme::Title(21.0f);
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
		draw.TextAligned(body.CutTop(28.0f), card.Name, nameStyle, { 0.5f, 0.5f });
		body.CutTop(8.0f);
		const UiRect footer = body.CutBottom(18.0f);
		draw.TextWrapped(body, CardText(card, ShownBrawlMode()), Theme::Body(14.5f, WithAlpha(Theme::Muted, fade)), 0.5f, 1.05f);
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
		else if (const std::optional<int> moved = ui.AcceptDrop(rect, KeyPayload); moved && uint32_t(*moved) < SkillSlots)
			spell = spells[size_t(*moved)];
		else if (clicked && m_SelectedSpell)
			spell = m_SelectedSpell;
		if (!spell || slot >= SkillSlots)
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
		return (session.Mode == GameMode::Brawl && IsChoosing(m_Client.GetStatus())) || (session.Mode == GameMode::Hub && m_Station == StationKind::Spellbook);
	}

	void SessionState::DrawDraggedSpell()
	{
		Ui& ui = m_App.GetUi();
		const UiPayload* drag = ui.GetDrag();
		if (!drag || (drag->Type != SpellPayload && drag->Type != KeyPayload))
			return;
		const AbilityId spell = drag->Type == SpellPayload ? AbilityId(drag->Value) : m_Client.GetPreferredSpells()[size_t(std::clamp(drag->Value, 0, int(SkillSlots) - 1))];
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
		if (!IsChoosing(status) || !local.Valid || m_Paused)
			return;
		Ui& ui = m_App.GetUi();
		const BrawlStatus& brawl = status.Brawl;
		const BrawlLoadout& loadout = local.Brawl;
		const bool picking = brawl.Phase == BrawlPhase::Pick;
		const uint32_t self = m_Client.GetPlayerId();
		const bool champion = picking && brawl.RoundWinner == self;
		const bool offered = picking && loadout.OfferPending && !m_ChosenCard;
		if (offered && !m_App.UiWantsKeyboard())
			for (uint32_t i = 0; i < CardChoices; i++)
				if (Rdn::Input::IsKeyPressed(CardKeys[i]) && loadout.Offer[i] != CardId::Count)
					TakeCard(i, loadout.Offer[i]);
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
		std::string hint = "These are the spells from your Emberhold Spellbook. Drag a different one onto a key below if you want, then ready up.";
		if (champion)
		{
			title = "CHAMPION";
			hint = Format("You won round %u. No card for the champion, the others are choosing theirs.", unsigned(brawl.Round));
		}
		else if (picking && loadout.OfferPending && !m_ChosenCard)
		{
			title = "CHOOSE A CARD";
			hint = "Upgrades change how one of your spells works. Wild cards bend the rules. Press 1, 2 or 3.";
		}
		else if (picking)
		{
			title = m_ChosenCard ? "CARD TAKEN" : "BETWEEN ROUNDS";
			hint = m_ChosenCard ? "It lasts for the rest of the match. Swap spells below if you like, then ready up." : "No cards left to offer. Swap spells below if you like, then ready up.";
		}
		if (picking && !champion && loadout.Offer[0] != CardId::Count && (loadout.OfferPending || m_ChosenCard))
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
				const float fade = ui.Animate(Ui::MakeId("cardfade", i), !m_ChosenCard || taken, 6.0f) * 0.7f + 0.3f;
				if (offered && interaction.Clicked)
					TakeCard(i, card);
				DrawCardFace(rect, card, hover, fade, offered ? std::optional<uint32_t>(i) : std::nullopt, taken, reveal, taken ? std::max(1.0f - m_TakenAge / 0.9f, 0.0f) : 0.0f);
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
			const UiRect rules = UiRect::Centered({ center, cardsTop + cardHeight * 0.4f }, { std::min(size.x - 80.0f, 720.0f), 118.0f });
			ui.Panel(rules, 12.0f);
			UiRect text = rules.Shrink(22.0f, 18.0f);
			TextStyle labelStyle = Theme::Bold(12.0f, Theme::Muted);
			labelStyle.Tracking = 1.6f;
			draw.TextAligned(text.CutTop(16.0f), "HOW IT WORKS", labelStyle, { 0.5f, 0.5f });
			text.CutTop(8.0f);
			const BrawlTerms& terms = TermsFor(m_Client.GetSession().Map);
			const std::string scoring = m_Client.GetSession().Rules.Mode == BrawlMode::Damage ? std::string(terms.DamageRules) + Format(" Last mage standing scores %u, second %u, third %u. A kill adds 1 for whoever dealt the victim the most damage in the last %.0f seconds.", unsigned(PlacementPoints[0]), unsigned(PlacementPoints[1]), unsigned(PlacementPoints[2]), double(DamageCreditSeconds)) : std::string(terms.Rules) + " Last mage standing wins the round: 3 points, plus 1 for every rival you knock out.";
			draw.TextWrapped(text, scoring + " After each round, everyone but the winner picks a card.", Theme::Body(14.5f, Theme::Text), 0.5f, 1.05f);
		}
		draw.TextAligned({ 0.0f, headingY, size.x, 40.0f }, title, heading, { 0.5f, 0.5f });
		draw.TextAligned({ 0.0f, headingY + 36.0f, size.x, 22.0f }, hint, Theme::Floating(15.5f, Theme::Text, UiFont::Body), { 0.5f, 0.5f });

		if (picking)
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
			if (brawl.RoundWinner != NoPlayer && brawl.RoundWinner != self)
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
		if (ui.Button(Ui::MakeId("brawlready"), button, loadout.Ready ? "Waiting for the others" : "Ready", loadout.Ready ? ButtonKind::Normal : ButtonKind::Primary, true, loadout.Ready ? UiIcon::Hourglass : UiIcon::CheckMark))
			m_Client.SetBrawlReady(!loadout.Ready);
		const std::string waiting = !picking && brawl.Players < 2 ? std::string("Waiting for a rival. Ready starts a round on your own") : Format("%u of %u ready, %s in %s s", unsigned(brawl.Ready), unsigned(std::max<uint8_t>(brawl.Players, 1)), picking ? "next round" : "first round", Seconds(brawl.Timer).c_str());
		draw.TextWrapped(row.Shrink(6.0f, 4.0f), waiting, Theme::Body(13.5f, Theme::Muted), 1.0f);

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
		for (uint32_t slot = 0; slot <= SkillSlots; slot++)
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
		right.CutTop(draw.TextWrapped(right, "Your spells work everywhere in Emberhold, with the damage, mana and shoves of a damage spellbrawl. Try them on the dummies in the pit next to this book: wear one down and it topples, or shove it into the lava and it sinks. Either way a fresh one climbs out a moment later.", Theme::Body(14.5f, Theme::Text)) + 14.0f);
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
		const int limit = std::max(int(MaxPlayers) - std::max(int(m_Client.GetPlayers().size()), 1), 0);
		int bots = std::min(int(m_BrawlRules.Bots), limit);
		UiRect row = area.CutTop(40.0f);
		draw.TextAligned(row.CutLeft(150.0f), "Bots", Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
		if (ui.Button(Ui::MakeId("botcount"), row.CutLeft(40.0f), "-", ButtonKind::Normal, bots > 0))
			bots--;
		draw.TextAligned(row.CutLeft(90.0f), bots == 0 ? std::string("None") : std::to_string(bots), Theme::Title(22.0f), { 0.5f, 0.5f });
		if (ui.Button(Ui::MakeId("botcount") + 1, row.CutLeft(40.0f), "+", ButtonKind::Normal, bots < limit))
			bots++;
		m_BrawlRules.Bots = uint8_t(std::clamp(bots, 0, limit));
		area.CutTop(6.0f);
		row = area.CutTop(40.0f);
		draw.TextAligned(row.CutLeft(150.0f), "Bot skill", Theme::Bold(16.0f, m_BrawlRules.Bots > 0 ? Theme::Text : Theme::Faint), { 0.0f, 0.5f });
		int level = std::min<int>(m_BrawlRules.BotLevel, int(BotLevels) - 1);
		if (ui.Choice(Ui::MakeId("botlevel"), row.CutLeft(250.0f), level, BotLevelNames))
			m_BrawlRules.BotLevel = uint8_t(level);
		area.CutTop(6.0f);
	}

	void SessionState::DrawArenaChoice(UiRect& area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		UiRect row = area.CutTop(40.0f);
		draw.TextAligned(row.CutLeft(150.0f), "Arena", Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
		ui.Choice(Ui::MakeId("brawlarena"), row.CutLeft(std::min(330.0f, row.W)), m_BrawlArena, ArenaNames);
		const MapKind arena = ChosenArena();
		const char* blurb = arena == MapKind::Caldera ? "A crumbling island of basalt on a lake of lava." : arena == MapKind::FrozenLake ? "Slippery ice, bouncing ice pillars and air holes in a long frozen lake." : "A different arena every match.";
		draw.TextAligned(area.CutTop(22.0f), blurb, Theme::Body(13.5f, Theme::Muted), { 0.0f, 0.5f });
		area.CutTop(6.0f);
	}

	void SessionState::DrawModeChoice(UiRect& area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		UiRect row = area.CutTop(40.0f);
		draw.TextAligned(row.CutLeft(150.0f), "Mode", Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
		int mode = int(m_BrawlRules.Mode);
		if (ui.Choice(Ui::MakeId("brawlmode"), row.CutLeft(std::min(330.0f, row.W)), mode, ModeNames))
			m_BrawlRules.Mode = BrawlMode(mode);
		const char* blurb = m_BrawlRules.Mode == BrawlMode::Damage ? "Spells hurt and cost mana. Placing and kills score." : "Spells only shove. Win rounds and knock rivals out.";
		draw.TextAligned(area.CutTop(22.0f), blurb, Theme::Body(13.5f, Theme::Muted), { 0.0f, 0.5f });
		area.CutTop(6.0f);
	}

	BrawlMode SessionState::ShownBrawlMode() const
	{
		const SessionInfo& session = m_Client.GetSession();
		return session.Mode == GameMode::Brawl ? session.Rules.Mode : m_BrawlRules.Mode;
	}

	void SessionState::DescribeBrawlSpell(UiTooltip& tooltip, AbilityId spell) const
	{
		DescribeSpell(tooltip, spell, m_Client.GetSession().Mode == GameMode::Hub ? BrawlMode::Damage : ShownBrawlMode());
	}

	MapKind SessionState::ChosenArena() const
	{
		return m_BrawlArena == 1 ? MapKind::Caldera : m_BrawlArena == 2 ? MapKind::FrozenLake : RandomBrawlMap;
	}

	std::string SessionState::EnterLabel() const
	{
		const MapKind arena = ChosenArena();
		return arena == RandomBrawlMap ? std::string("Enter a random arena") : std::string("Enter ") + TermsFor(arena).Title;
	}

	void SessionState::DrawCalderaGate()
	{
		Ui& ui = m_App.GetUi();
		constexpr float Width = 560.0f;
		constexpr float Height = 636.0f;
		const UiRect home{ (ui.GetSize().x - Width) * 0.5f, std::max((ui.GetSize().y - Height) * 0.5f - 24.0f, 12.0f), Width, Height };
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("calderagate"), "Caldera Gate", home, &open);
		if (!open)
		{
			m_Station.reset();
			return;
		}
		UiDrawList& draw = ui.Draw();
		const uint32_t players = std::max<uint32_t>(uint32_t(m_Client.GetPlayers().size()), 1u);
		const uint32_t bots = m_BrawlRules.Bots;
		const char* pitch = m_BrawlRules.Mode == BrawlMode::Damage ? "Spells hurt and cost mana, and they still nudge. Wear your rivals down on a crumbling island over lava or on the ice of the Frozen Lake. The last mage standing takes the round." : "Spells don't hurt in a spellbrawl, they shove. Knock your rivals off a crumbling island into the lava, or through the ice of the Frozen Lake. The last mage standing takes the round.";
		content.CutTop(draw.TextWrapped(content, pitch, Theme::Body(14.5f, Theme::Text)) + 8.0f);
		std::string party = players == 1 && bots == 0 ? std::string("Nobody else is here yet. Add a few bots, or wait at the gate for friends to join.") : Format("%u mages will step through together.", players + bots);
		if (bots > 0)
			party += Format(bots == 1 ? " One of them is a bot." : " %u of them are bots.", bots);
		content.CutTop(draw.TextWrapped(content, party, Theme::Body(14.0f, players + bots == 1 ? Theme::Muted : Theme::Success)) + 12.0f);

		ui.Separator(content.CutTop(20.0f), "MATCH");
		content.CutTop(8.0f);
		int rounds = m_BrawlRules.Rounds;
		UiRect row = content.CutTop(40.0f);
		draw.TextAligned(row.CutLeft(150.0f), "Rounds", Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
		if (ui.Button(Ui::MakeId("gaterounds"), row.CutLeft(40.0f), "-", ButtonKind::Normal, rounds > MinBrawlRounds))
			rounds--;
		draw.TextAligned(row.CutLeft(90.0f), std::to_string(rounds), Theme::Title(22.0f), { 0.5f, 0.5f });
		if (ui.Button(Ui::MakeId("gaterounds") + 1, row.CutLeft(40.0f), "+", ButtonKind::Normal, rounds < MaxBrawlRounds))
			rounds++;
		m_BrawlRules.Rounds = uint8_t(std::clamp(rounds, int(MinBrawlRounds), int(MaxBrawlRounds)));
		content.CutTop(6.0f);
		DrawModeChoice(content);
		DrawArenaChoice(content);
		DrawBotRules(content);
		content.CutTop(4.0f);

		ui.Separator(content.CutTop(20.0f), "YOUR SPELLS");
		content.CutTop(8.0f);
		UiRect icons = content.CutTop(44.0f);
		const BrawlSpells& spells = m_Client.GetPreferredSpells();
		for (uint32_t slot = 0; slot <= SkillSlots; slot++)
		{
			const AbilityId spell = slot == 0 ? BrawlBasic : spells[slot - 1];
			const UiRect icon = icons.CutLeft(44.0f);
			icons.CutLeft(10.0f);
			const UiColor color = WithAlpha(SkillColor(spell));
			draw.Icon(IconFor(spell), icon, { .Color = Lighten(color, 0.5f), .ColorBottom = color, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			if (ui.Interact(Ui::MakeId("gatespell", slot), icon).Hovered)
				DescribeSpell(ui.Tooltip(), spell, ShownBrawlMode());
		}
		draw.TextAligned(icons, "Change them at the spellbook by the pit", Theme::Body(13.5f, Theme::Faint), { 0.0f, 0.5f });
		content.CutTop(10.0f);

		const MatchReward reward = GetBrawlReward(true, std::max(players + bots, 2u), 15);
		draw.TextAligned(content.CutTop(22.0f), Format("Winning with 15 points pays about %u gold", unsigned(reward.Gold)), Theme::Body(13.5f, Theme::Success), { 0.0f, 0.5f });
		content.CutTop(8.0f);
		if (ui.Button(Ui::MakeId("gateenter"), content.CutTop(48.0f), EnterLabel(), ButtonKind::Primary, true, UiIcon::Whirlwind))
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

	void SessionState::DrawCritter(WorldRenderer& renderer, Critter critter, const glm::mat4& transform, const glm::vec4& tint, float emission, float ground) const
	{
		for (const uint32_t mesh : m_Critters[size_t(critter)])
			renderer.DrawModel(mesh, transform, NoPalette, tint, emission, ground);
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
		DrawCritter(renderer, Critter::DummyPost, place, tint, shine * 0.5f, base.y);
		DrawCritter(renderer, Critter::DummyBody, place * glm::translate(identity, joint) * glm::rotate(identity, wobble * 1.15f, glm::vec3(0.0f, 0.0f, 1.0f)) * glm::translate(identity, -joint), tint, shine, base.y);
		if (golden)
			DrawGlow(renderer, base + glm::vec3(0.0f, 1.3f, 0.0f), 0.9f, glm::vec3(1.0f, 0.7f, 0.2f) * (0.25f + 0.1f * std::sin(m_Time * 4.0f)));
		DrawBrawlPlayer(renderer, entity, Ground(entity.Position));
	}
}
