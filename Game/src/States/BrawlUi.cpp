#include "SessionState.h"
#include "App/GameApp.h"
#include "Audio/GameAudio.h"
#include "Rendering/Models.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include "UI/Palette.h"
#include "UI/Ui.h"
#include <RadianceEngine/Window/Input.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace Game
{
	namespace
	{
		constexpr glm::vec3 WardGold{ 1.0f, 0.86f, 0.48f };
		constexpr glm::vec3 LavaGlow{ 1.0f, 0.42f, 0.12f };
		constexpr const char* SpellKeys[] = { "RMB", "Q", "R" };
		static_assert(std::size(SpellKeys) == SkillSlots);
		constexpr Rdn::KeyCode BoonKeys[] = { Rdn::KeyCode::Key1, Rdn::KeyCode::Key2, Rdn::KeyCode::Key3 };
		static_assert(std::size(BoonKeys) == BoonChoices);
		constexpr float CrumbleRange = 18.0f;

		template<typename... Arguments>
		std::string Format(const char* format, Arguments... arguments)
		{
			char text[192];
			std::snprintf(text, sizeof(text), format, arguments...);
			return text;
		}

		std::string Seconds(float seconds)
		{
			return std::to_string(int(std::ceil(std::max(seconds, 0.0f))));
		}

		bool IsChoosing(const GameStatus& status)
		{
			return status.Mode == GameMode::Brawl && status.Match != MatchPhase::Ended && (status.Brawl.Phase == BrawlPhase::Draft || status.Brawl.Phase == BrawlPhase::Shop);
		}

		float FinalCollapse(const CollapsePlan& plan)
		{
			return std::max(CollapseGrace + (plan.StartRadius - CollapseMinRadius) / CollapseRate, FinalCollapseStart);
		}

		std::string SpellSummary(AbilityId id)
		{
			const AbilityDef& def = GetAbility(id);
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

		void DescribeSpell(UiTooltip& tooltip, AbilityId id)
		{
			const AbilityDef& def = GetAbility(id);
			const UiColor color = WithAlpha(SkillColor(id));
			tooltip.Accent = WithAlpha(color, 0.85f);
			tooltip.Header(def.Name, color, std::nullopt, IconFor(id));
			tooltip.Space(2.0f).Text(def.Description, Theme::Text, 15.0f).Space(4.0f);
			if (def.Push > 0.0f)
				tooltip.Pair("Shove", Format("%.0f", double(def.Push)));
			else if (def.Push < 0.0f)
				tooltip.Pair("Pull", "to the center");
			if (def.Kind == AbilityKind::Projectile)
				tooltip.Pair("Range", Format("%.1f", double(def.ProjectileRange)));
			else if (def.Kind == AbilityKind::Ground || def.Kind == AbilityKind::Dash)
				tooltip.Pair("Range", Format("%.1f", double(def.Range)));
			else if (def.Reach > 0.0f)
				tooltip.Pair("Reach", Format("%.1f", double(def.Reach)));
			if (def.Chain > 0)
				tooltip.Pair("Jumps", std::to_string(def.Chain));
			tooltip.Pair("Cast time", def.CastTime > 0.0f ? Format("%.2f s", double(def.CastTime)) : std::string("instant"));
			tooltip.Pair("Cooldown", Format("%.1f s", double(def.Cooldown)));
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
						if (map.GetTerrain(x, y) == TerrainId::Basalt && TileCollapseTime(m_SeenPlan, x, y) <= m_SeenClock && TileCollapseTime(brawl.Plan, x, y) > brawl.Clock)
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
				if (m_Fallen[index] || map.GetTerrain(x, y) != TerrainId::Basalt)
					continue;
				const float collapse = TileCollapseTime(brawl.Plan, x, y);
				if (collapse > brawl.Clock)
					continue;
				m_Fallen[index] = 1;
				if (brawl.Clock - collapse > 0.5f)
					continue;
				const glm::vec3 center(float(x) + 0.5f, LavaSurface + 0.1f, float(y) + 0.5f);
				m_Effects->Burst(Fx::FireBurst, center, glm::vec3(0.0f, 1.0f, 0.0f), 0.55f);
				m_Effects->Burst(Fx::Debris, center + glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.5f);
				if (m_CrumbleCooldown <= 0.0f)
				{
					m_App.GetGameAudio().PlayCrumble(Vec2(center.x, center.z));
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
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.25f + 0.6f * float(i), 0.0f), 0.78f - 0.14f * std::abs(float(i) - 1.0f), WardGold * (1.5f * pulse), 0.03f);
		}
		if (entity.Has(EntityFlags::Burning))
		{
			const float flicker = 0.7f + 0.3f * std::sin(m_Time * 17.0f + float(entity.NetId) * 1.3f);
			DrawGroundGlow(renderer, base, 0.9f, LavaGlow, 0.9f * flicker);
			DrawGlow(renderer, base + glm::vec3(0.0f, 0.35f, 0.0f), 0.7f, LavaGlow * (0.6f * flicker));
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
		draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.4f, 0.08f, 0.0f, 0.07f), .Glow = Rgba(1.0f, 0.3f, 0.05f, 0.55f * throb), .GlowSize = 160.0f });
		draw.TextAligned({ 0.0f, ui.GetSize().y * 0.2f, ui.GetSize().x, 24.0f }, "You are burning! Get back onto the stone", Theme::Bold(18.0f, Hex(0xFFC08A)), { 0.5f, 0.5f });
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
			m_BannerText = brawl.Round == brawl.Rounds ? "The last round. Shove them all into the lava" : "Shove them into the lava";
			m_BannerColor = Hex(0xFF8A4A);
			m_BannerTime = 2.6f;
			audio.PlayEvent(AudioEvent::FightStart);
			break;
		case BrawlPhase::Over:
			m_BannerTitle = brawl.RoundWinner == self ? "Round Won" : brawl.RoundWinner == NoPlayer ? "No Survivors" : PlayerName(brawl.RoundWinner);
			m_BannerText = brawl.RoundWinner == self ? Format("+%u points", unsigned(RoundWinPoints)) : brawl.RoundWinner == NoPlayer ? "The caldera took everyone" : Format("takes round %u", unsigned(brawl.Round));
			m_BannerColor = brawl.RoundWinner == self ? Hex(0xFFD36A) : Hex(0xC8C0B4);
			m_BannerTime = 3.5f;
			audio.PlayEvent(brawl.RoundWinner == self ? AudioEvent::RoundWon : AudioEvent::RoundLost);
			break;
		case BrawlPhase::Shop:
			m_LoadoutOpen = true;
			m_ChosenBoon.reset();
			m_PickingSlot.reset();
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
				subtitle = "Pick three spells, the first round starts in " + Seconds(brawl.Timer);
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
			case BrawlPhase::Shop:
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
		UiColor color = Hex(0xFFB27A);
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
			if (brawl.Clock < CollapseGrace)
				line = "The stone holds for " + Seconds(CollapseGrace - brawl.Clock) + " more seconds";
			else if (brawl.Clock < final)
				line = "The caldera is crumbling";
			else
			{
				line = "Sudden death, the last stone is sinking";
				color = Hex(0xFF7A5A);
			}
		}
		if (!line.empty())
		{
			const TextStyle style = Theme::Floating(15.0f, color, UiFont::Bold);
			const float lineWidth = draw.MeasureWidth(line, style) + 54.0f;
			const UiRect pill = UiRect::Centered({ center, 97.0f }, { lineWidth, 28.0f });
			draw.Rect(pill, { .Fill = Rgba(0.16f, 0.05f, 0.03f, 0.78f), .Border = WithAlpha(color, 0.7f), .BorderWidth = 1.0f, .Chamfer = 8.0f });
			draw.Icon(IsChoosing(status) ? UiIcon::ScrollUnfurled : UiIcon::Fireball, { pill.X + 10.0f, pill.Center().y - 9.0f, 18.0f, 18.0f }, { .Color = color });
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

	void SessionState::DrawBrawlLoadout()
	{
		const GameStatus& status = m_Client.GetStatus();
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (!IsChoosing(status) || !local.Valid || m_Paused)
			return;
		Ui& ui = m_App.GetUi();
		const BrawlStatus& brawl = status.Brawl;
		const BrawlLoadout& loadout = local.Brawl;
		const bool shop = brawl.Phase == BrawlPhase::Shop;
		const uint32_t self = m_Client.GetPlayerId();
		if (shop && loadout.OfferPending && !m_App.UiWantsKeyboard() && !m_ChosenBoon)
			for (uint32_t i = 0; i < BoonChoices; i++)
				if (Rdn::Input::IsKeyPressed(BoonKeys[i]) && loadout.Offer[i] != BoonId::Count)
				{
					m_Client.ChooseBoon(i);
					m_ChosenBoon = i;
				}
		if (!m_LoadoutOpen)
			return;

		constexpr float Width = 1000.0f;
		constexpr float Height = 610.0f;
		const UiRect home{ (ui.GetSize().x - Width) * 0.5f, std::max((ui.GetSize().y - Height) * 0.5f + 30.0f, 120.0f), Width, Height };
		UiRect content = ui.Window(Ui::MakeId("brawlbook"), shop ? "Between Rounds" : "Spellbook", home, &m_LoadoutOpen);
		UiDrawList& draw = ui.Draw();
		const char* intro = shop ? "Swap spells if you want, and take a boon. Click a slot, then a spell to put in it." : "Everyone throws Ember Bolt with LMB. Choose three more spells for RMB, Q and R: click a slot, then a spell.";
		draw.TextWrapped(content.CutTop(24.0f), intro, Theme::Body(14.5f, Theme::Muted));
		content.CutTop(8.0f);
		UiRect left = content.CutLeft(600.0f);
		content.CutLeft(28.0f);
		UiRect right = content;

		ui.Separator(left.CutTop(20.0f), "YOUR SPELLS");
		left.CutTop(8.0f);
		const uint32_t picking = m_PickingSlot.value_or(0) % SkillSlots;
		UiRect slots = left.CutTop(88.0f);
		constexpr float SlotWidth = 141.0f;
		constexpr float SlotGap = 12.0f;
		for (uint32_t slot = 0; slot <= SkillSlots; slot++)
		{
			const UiRect rect = slots.CutLeft(SlotWidth);
			slots.CutLeft(SlotGap);
			const bool basic = slot == 0;
			const AbilityId spell = basic ? BrawlBasic : loadout.Spells[slot - 1];
			const UiId id = Ui::MakeId("brawlslot", slot);
			const UiInteraction interaction = ui.Interact(id, rect);
			const bool selected = !basic && picking == slot - 1;
			const float hover = ui.Animate(id, interaction.Hovered || selected);
			if (interaction.Clicked && !basic)
				m_PickingSlot = slot - 1;
			const UiColor color = WithAlpha(SkillColor(spell));
			ui.Card(rect, basic ? 0.0f : hover, selected ? 1.0f : 0.0f, color);
			draw.Icon(IconFor(spell), { rect.X + 12.0f, rect.Y + 14.0f, 40.0f, 40.0f }, { .Color = Lighten(color, 0.5f), .ColorBottom = color, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextFit({ rect.X + 60.0f, rect.Y + 12.0f, rect.W - 68.0f, 22.0f }, GetAbility(spell).Name, Theme::Bold(15.0f, selected ? Theme::GoldLight : Theme::Text));
			draw.TextFit({ rect.X + 60.0f, rect.Y + 34.0f, rect.W - 68.0f, 20.0f }, basic ? std::string("shared") : selected ? std::string("pick one") : std::string("change"), Theme::Body(13.0f, selected ? Theme::Gold : Theme::Faint));
			const char* key = basic ? "LMB" : SpellKeys[slot - 1];
			const TextStyle keyStyle = Theme::Bold(11.5f, Theme::AccentLight);
			const UiRect badge{ rect.X + 10.0f, rect.Bottom() - 24.0f, draw.MeasureWidth(key, keyStyle) + 12.0f, 16.0f };
			draw.Rect(badge, { .Fill = Rgba(0.03f, 0.025f, 0.06f, 0.94f), .Border = WithAlpha(Theme::Edge, 0.9f), .BorderWidth = 1.0f, .Chamfer = 3.0f });
			draw.TextAligned(badge, key, keyStyle, { 0.5f, 0.5f });
			if (interaction.Hovered)
				DescribeSpell(ui.Tooltip(), spell);
		}
		left.CutTop(16.0f);

		ui.Separator(left.CutTop(20.0f), "THE SPELLBOOK");
		left.CutTop(8.0f);
		constexpr float CardWidth = 192.0f;
		constexpr float CardHeight = 84.0f;
		constexpr float CardGap = 12.0f;
		for (size_t i = 0; i < std::size(BrawlSpellPool); i++)
		{
			const AbilityId spell = BrawlSpellPool[i];
			const UiRect card{ left.X + float(i % 3) * (CardWidth + CardGap), left.Y + float(i / 3) * (CardHeight + 10.0f), CardWidth, CardHeight };
			const UiId id = Ui::MakeId("brawlspell", i);
			const UiInteraction interaction = ui.Interact(id, card);
			const auto equipped = std::find(loadout.Spells.begin(), loadout.Spells.end(), spell);
			const bool owned = equipped != loadout.Spells.end();
			const float hover = ui.Animate(id, interaction.Hovered);
			if (interaction.Clicked)
			{
				m_Client.PickBrawlSpell(picking, spell);
				m_PickingSlot = (picking + 1) % SkillSlots;
			}
			const UiColor color = WithAlpha(SkillColor(spell));
			ui.Card(card, hover, owned ? 0.6f : 0.0f, color);
			draw.Icon(IconFor(spell), { card.X + 10.0f, card.Y + 12.0f, 34.0f, 34.0f }, { .Color = Lighten(color, 0.5f), .ColorBottom = color, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextFit({ card.X + 52.0f, card.Y + 10.0f, card.W - 60.0f, 22.0f }, GetAbility(spell).Name, Theme::Bold(15.5f, owned ? Lighten(color, 0.4f) : Theme::Text));
			draw.TextFit({ card.X + 52.0f, card.Y + 32.0f, card.W - 60.0f, 20.0f }, SpellSummary(spell), Theme::Body(13.0f, Theme::Muted));
			if (owned)
			{
				const char* key = SpellKeys[equipped - loadout.Spells.begin()];
				const TextStyle keyStyle = Theme::Bold(11.5f, Theme::GoldLight);
				const UiRect badge{ card.Right() - draw.MeasureWidth(key, keyStyle) - 22.0f, card.Bottom() - 24.0f, draw.MeasureWidth(key, keyStyle) + 12.0f, 16.0f };
				draw.Rect(badge, { .Fill = Rgba(0.08f, 0.06f, 0.02f, 0.94f), .Border = WithAlpha(Theme::Gold, 0.9f), .BorderWidth = 1.0f, .Chamfer = 3.0f });
				draw.TextAligned(badge, key, keyStyle, { 0.5f, 0.5f });
			}
			if (interaction.Hovered)
				DescribeSpell(ui.Tooltip(), spell);
		}

		const UiRect footer = right.CutBottom(96.0f);
		if (shop)
		{
			ui.Separator(right.CutTop(20.0f), "CHOOSE A BOON");
			right.CutTop(8.0f);
			if (loadout.OfferPending && !m_ChosenBoon)
			{
				for (uint32_t i = 0; i < BoonChoices; i++)
				{
					if (loadout.Offer[i] == BoonId::Count)
						continue;
					const BoonDef& boon = GetBoon(loadout.Offer[i]);
					const UiRect card = right.CutTop(70.0f);
					right.CutTop(8.0f);
					const UiId id = Ui::MakeId("brawlboon", i);
					const UiInteraction interaction = ui.Interact(id, card);
					const float hover = ui.Animate(id, interaction.Hovered);
					if (interaction.Clicked)
					{
						m_Client.ChooseBoon(i);
						m_ChosenBoon = i;
					}
					ui.Card(card, hover, 0.0f, Theme::Gold);
					draw.Icon(IconFor(boon.Id), { card.X + 12.0f, card.Y + 15.0f, 40.0f, 40.0f }, { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .Outline = 0.8f, .OutlineColor = Rgba(0.0f, 0.0f, 0.02f, 0.7f) });
					draw.Text({ card.X + 64.0f, card.Y + 10.0f }, boon.Name, Theme::Bold(16.5f, Theme::GoldLight));
					draw.TextFit({ card.X + 64.0f, card.Y + 34.0f, card.W - 104.0f, 22.0f }, boon.Description, Theme::Body(14.0f, Theme::Text));
					const UiRect badge{ card.Right() - 32.0f, card.Y + 10.0f, 22.0f, 20.0f };
					draw.Rect(badge, { .Fill = Rgba(0.04f, 0.035f, 0.05f, 0.95f), .Radius = 4.0f, .Border = WithAlpha(Theme::Accent, 0.8f), .BorderWidth = 1.0f });
					draw.TextAligned(badge, std::to_string(i + 1), Theme::Bold(13.0f, Theme::AccentLight), { 0.5f, 0.5f });
					const uint8_t stacks = loadout.Boons[size_t(boon.Id)];
					draw.TextAligned({ card.X, card.Bottom() - 22.0f, card.W - 12.0f, 18.0f }, Format("%u of %u", unsigned(stacks), unsigned(boon.MaxPicks)), Theme::Body(12.5f, Theme::Faint), { 1.0f, 0.5f });
				}
			}
			else
			{
				const char* message = m_ChosenBoon ? "Boon taken. It lasts for the rest of the match." : brawl.RoundWinner == self ? "Champions get no boon. Everyone else just got a little stronger." : "No boons left to offer.";
				right.CutTop(draw.TextWrapped(right.CutTop(60.0f), message, Theme::Body(15.0f, m_ChosenBoon ? Theme::Success : Theme::Muted)) + 10.0f);
			}
		}
		else
		{
			ui.Separator(right.CutTop(20.0f), "HOW IT WORKS");
			right.CutTop(8.0f);
			const char* rules = "Spells don't hurt, they shove. Lava burns, and the stone crumbles from the edge inward. Get knocked in and you can climb back out, but every second costs health, and the lower your health the further you fly. Last mage standing wins the round: 3 points, plus 1 for every rival you knock out.";
			right.CutTop(draw.TextWrapped(right, rules, Theme::Body(14.5f, Theme::Text)) + 14.0f);
		}

		ui.Separator(right.CutTop(20.0f), "YOUR BOONS");
		right.CutTop(8.0f);
		UiRect owned = right.CutTop(36.0f);
		bool any = false;
		for (const BoonDef& boon : Boons)
		{
			const uint8_t stacks = loadout.Boons[size_t(boon.Id)];
			if (stacks == 0)
				continue;
			any = true;
			const UiRect tile = owned.CutLeft(36.0f);
			owned.CutLeft(6.0f);
			draw.Rect(tile, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Radius = 6.0f, .Border = WithAlpha(Theme::Gold, 0.8f), .BorderWidth = 1.2f });
			draw.Icon(IconFor(boon.Id), tile.Shrink(6.0f), { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextAligned(tile.Shrink(2.0f, 0.0f).Translate({ 3.0f, 3.0f }), std::to_string(stacks), Theme::Floating(12.0f, Theme::Text), { 1.0f, 1.0f });
			if (ui.Interact(Ui::MakeId("ownedboon", uint64_t(boon.Id)), tile).Hovered)
				ui.Tooltip().Header(boon.Name, Theme::Gold, std::nullopt, IconFor(boon.Id)).Text(boon.Description).Muted(Format("Taken %u of %u times", unsigned(stacks), unsigned(boon.MaxPicks)));
		}
		if (!any)
			draw.TextAligned(owned, shop ? "None yet" : "You pick one after every round", Theme::Body(14.0f, Theme::Faint), { 0.0f, 0.5f });

		UiRect bottom = footer;
		const std::string waiting = Format("%u of %u ready, %s in %s s", unsigned(brawl.Ready), unsigned(std::max<uint8_t>(brawl.Players, 1)), shop ? "next round" : "first round", Seconds(brawl.Timer).c_str());
		draw.TextAligned(bottom.CutTop(24.0f), waiting, Theme::Body(14.0f, Theme::Muted), { 0.0f, 0.5f });
		bottom.CutTop(6.0f);
		if (ui.Button(Ui::MakeId("brawlready"), bottom.CutTop(48.0f), loadout.Ready ? "Waiting for the others" : "Ready", loadout.Ready ? ButtonKind::Normal : ButtonKind::Primary, true, loadout.Ready ? UiIcon::Hourglass : UiIcon::CheckMark))
			m_Client.SetBrawlReady(!loadout.Ready);
	}
}
