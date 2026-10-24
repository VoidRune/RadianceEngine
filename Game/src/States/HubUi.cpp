#include "SessionState.h"
#include "App/GameApp.h"
#include "Rendering/Models.h"
#include "UI/ItemUi.h"
#include "UI/Palette.h"
#include "UI/Ui.h"
#include <GameShared/Content/Crafting.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace Game
{
	namespace
	{
		constexpr int StashPayload = 10;
		constexpr int CarryPayload = 11;
		constexpr int SourcePayload = 12;
		constexpr uint32_t StashColumns = 8;
		constexpr uint32_t PackColumns = 4;
		constexpr float SlotSize = 54.0f;
		constexpr float SlotGap = 6.0f;
		constexpr const char* SourceTabs[] = { "Backpack", "Equipped", "Stash" };
		const UiColor CorruptColor = Hex(0xE45AA9);

		template<typename... Arguments>
		std::string Format(const char* format, Arguments... arguments)
		{
			char text[192];
			std::snprintf(text, sizeof(text), format, arguments...);
			return text;
		}

		UiRect StationRect(const Ui& ui, float width, float height)
		{
			return { (ui.GetSize().x - width) * 0.5f, std::max((ui.GetSize().y - height) * 0.5f - 24.0f, 12.0f), width, height };
		}

		UiRect GridSlot(const UiRect& grid, uint32_t index, uint32_t columns)
		{
			return { grid.X + float(index % columns) * (SlotSize + SlotGap), grid.Y + float(index / columns) * (SlotSize + SlotGap), SlotSize, SlotSize };
		}

		float GridHeight(uint32_t count, uint32_t columns)
		{
			return float((count + columns - 1) / columns) * (SlotSize + SlotGap) - SlotGap;
		}

		const char* CategoryName(StatCategory category)
		{
			return GetStatCategoryName(category);
		}

		void DrawDragged(Ui& ui, const Item* item)
		{
			if (!item || !item->IsValid())
				return;
			UiDrawList& layer = ui.Draw(UiLayer::Drag);
			const UiRect icon = UiRect::Centered(ui.GetMouse(), { 52.0f, 52.0f });
			layer.Shadow(icon, 20.0f, 14.0f, WithAlpha(ItemColor(*item), 0.45f), { 0.0f, 0.0f });
			DrawItemIcon(layer, *item, icon);
		}

		void ActionRow(UiDrawList& draw, const UiRect& row, bool hovered)
		{
			draw.Rect(row, { .Fill = Rgba(0.8f, 0.75f, 1.0f, hovered ? 0.05f : 0.03f), .Border = WithAlpha(Theme::Edge, hovered ? 0.75f : 0.45f), .BorderWidth = 1.0f, .Chamfer = 7.0f });
		}
	}

	const Item* SessionState::FindClientItem(ItemRef ref) const
	{
		const ServerInventory& inventory = m_Client.GetInventory();
		switch (ref.Location)
		{
		case ItemLocation::Pack:
			return ref.Index < BackpackSize ? &inventory.Pack[ref.Index] : nullptr;
		case ItemLocation::Gear:
			return ref.Index < GearSlotCount ? &inventory.Gear[ref.Index] : nullptr;
		case ItemLocation::Gem:
			return ref.Index < SkillSlots ? &inventory.Gems[ref.Index] : nullptr;
		case ItemLocation::Stash:
			return ref.Index < StashSize ? &m_Client.GetStash()[ref.Index] : nullptr;
		default:
			return nullptr;
		}
	}

	std::optional<ItemRef> SessionState::LocateItem(uint32_t id) const
	{
		if (id == 0)
			return std::nullopt;
		const ServerInventory& inventory = m_Client.GetInventory();
		const auto find = [&](const auto& items, ItemLocation location) -> std::optional<ItemRef>
		{
			for (size_t i = 0; i < items.size(); i++)
				if (items[i].Id == id)
					return ItemRef{ location, uint16_t(i) };
			return std::nullopt;
		};
		if (auto ref = find(inventory.Pack, ItemLocation::Pack))
			return ref;
		if (auto ref = find(inventory.Gear, ItemLocation::Gear))
			return ref;
		if (auto ref = find(inventory.Gems, ItemLocation::Gem))
			return ref;
		return find(m_Client.GetStash(), ItemLocation::Stash);
	}

	void SessionState::DrawMaterials(const UiRect& area, bool compact)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const MaterialStock& stock = m_Client.GetMaterials();
		const bool unlimited = m_Client.GetSession().UnlimitedMaterials;
		const float size = compact ? 17.0f : 20.0f;
		float x = area.X;
		for (size_t i = 0; i < stock.size(); i++)
		{
			const Material material = Material(i);
			const float width = DrawMaterial(draw, { x, area.Center().y }, material, unlimited ? "\xE2\x88\x9E" : FormatAmount(stock[i]), size, Theme::Text);
			if (ui.Interact(Ui::MakeId("material", i + (compact ? 16 : 0)), { x - 2.0f, area.Y, width + 4.0f, area.H }).Hovered)
				DescribeMaterial(ui.Tooltip(), material, stock[i], unlimited);
			x += width + (compact ? 12.0f : 18.0f);
		}
		if (unlimited && !compact)
			draw.TextAligned(area, "Unlimited materials (testing)", Theme::Body(13.5f, Theme::Warning), { 1.0f, 0.5f });
	}

	void SessionState::DrawStationWindow()
	{
		if (!m_Station)
			return;
		switch (*m_Station)
		{
		case StationKind::Stash:
			DrawStashWindow();
			break;
		case StationKind::Forge:
			DrawForgeWindow();
			break;
		case StationKind::Lapidary:
			DrawLapidaryWindow();
			break;
		case StationKind::MissionBoard:
			DrawMissionBoard();
			break;
		case StationKind::ProvingGrounds:
			DrawProvingGrounds();
			break;
		case StationKind::Spellbook:
			DrawSpellbook();
			break;
		case StationKind::CalderaGate:
			DrawCalderaGate();
			break;
		case StationKind::Chess:
			DrawChess();
			break;
		default:
			break;
		}
	}

	void SessionState::DrawProvingGrounds()
	{
		Ui& ui = m_App.GetUi();
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("proving"), "Proving Grounds", StationRect(ui, 1340.0f, 708.0f), &open);
		if (!open)
		{
			m_Station.reset();
			return;
		}
		UiDrawList& draw = ui.Draw();
		const uint32_t players = std::max<uint32_t>(uint32_t(m_Client.GetPlayers().size()), 1u);
		const std::string who = players == 1 ? "Nobody else is here yet, so the arena and the royale are only practice, but bots can fill a Spellbrawl." : Format("Fight the other %u player%s in this game.", unsigned(players - 1), players == 2 ? "" : "s");
		draw.TextWrapped(content.CutTop(40.0f), who + " In the arena and the royale damage between players is scaled down, so fights last a while. In Spellbrawl spells hurt and cost mana, or pick Knockback for pure shoving.", Theme::Body(14.5f, Theme::Muted));
		content.CutTop(8.0f);
		const float column = (content.W - 48.0f) / 3.0f;
		UiRect left = content.CutLeft(column);
		content.CutLeft(24.0f);
		UiRect right = content.CutLeft(column);
		content.CutLeft(24.0f);
		UiRect third = content;

		const auto heading = [&](UiRect& column, UiIcon icon, const char* title, const char* subtitle)
		{
			UiRect row = column.CutTop(48.0f);
			draw.Icon(icon, { row.X, row.Y + 4.0f, 40.0f, 40.0f }, { .Color = Theme::AccentLight, .ColorBottom = Theme::Accent, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f) });
			draw.Text({ row.X + 52.0f, row.Y + 2.0f }, title, Theme::Title(22.0f));
			draw.Text({ row.X + 52.0f, row.Y + 28.0f }, subtitle, Theme::Body(14.0f, Theme::Muted));
			column.CutTop(8.0f);
		};

		heading(left, UiIcon::CrossedSwords, "Arena", GetMapDef(MapKind::Crucible).Name);
		left.CutTop(draw.TextWrapped(left, GetMapDef(MapKind::Crucible).Description, Theme::Body(14.0f, Theme::Text)) + 10.0f);
		ui.Separator(left.CutTop(20.0f), "RULES");
		left.CutTop(8.0f);
		UiRect teams = left.CutTop(40.0f);
		const UiRect ffa = teams.CutLeft((teams.W - 10.0f) * 0.5f);
		teams.CutLeft(10.0f);
		if (ui.Button(Ui::MakeId("arenaffa"), ffa, "Free for all", m_ArenaRules.Teams ? ButtonKind::Normal : ButtonKind::Primary))
			m_ArenaRules.Teams = false;
		if (ui.Button(Ui::MakeId("arenateams"), teams, "Two teams", m_ArenaRules.Teams ? ButtonKind::Primary : ButtonKind::Normal))
			m_ArenaRules.Teams = true;
		left.CutTop(10.0f);
		const auto stepper = [&](const char* label, const std::string& value, UiId id, int& current, int low, int high, int step)
		{
			UiRect row = left.CutTop(40.0f);
			draw.TextAligned(row.CutLeft(150.0f), label, Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
			if (ui.Button(id, row.CutLeft(40.0f), "-", ButtonKind::Normal, current > low))
				current = std::max(current - step, low);
			draw.TextAligned(row.CutLeft(90.0f), value, Theme::Title(22.0f), { 0.5f, 0.5f });
			if (ui.Button(id + 1, row.CutLeft(40.0f), "+", ButtonKind::Normal, current < high))
				current = std::min(current + step, high);
			left.CutTop(6.0f);
		};
		int goal = m_ArenaRules.ScoreGoal;
		int minutes = m_ArenaRules.Minutes;
		stepper(m_ArenaRules.Teams ? "Team kills to win" : "Kills to win", std::to_string(goal), Ui::MakeId("arenagoal"), goal, MinScoreGoal, MaxScoreGoal, m_ArenaRules.Teams ? 5 : 1);
		stepper("Time limit", std::to_string(minutes) + " min", Ui::MakeId("arenatime"), minutes, MinMatchMinutes, MaxMatchMinutes, 1);
		m_ArenaRules.ScoreGoal = uint8_t(goal);
		m_ArenaRules.Minutes = uint8_t(minutes);
		draw.TextWrapped(left.CutTop(60.0f), "You fight with your own gear. Dead players come back after 3 seconds at the spawn farthest from their enemies. Shrines heal, restore mana and hand out power buffs that you lose when you die.", Theme::Body(13.5f, Theme::Faint));
		const MatchReward arenaReward = GetArenaReward(true, std::max(players, 2u), 10);
		draw.TextAligned(left.CutTop(22.0f), Format("Rewards gold and forge shards, about %u gold for a win with 10 kills", unsigned(arenaReward.Gold)), Theme::Body(13.5f, Theme::Success), { 0.0f, 0.5f });
		left.CutTop(6.0f);
		if (ui.Button(Ui::MakeId("startarena"), left.CutTop(46.0f), "Enter the Crucible", ButtonKind::Primary, true, UiIcon::CrossedSwords))
		{
			m_Client.StartArena(MapKind::Crucible, m_ArenaRules);
			m_Station.reset();
		}

		heading(right, UiIcon::Crown, "Battle Royale", "Last one standing wins");
		ui.Separator(right.CutTop(20.0f), "ISLAND");
		right.CutTop(8.0f);
		for (const MapKind map : { MapKind::Ashlands, MapKind::Town, MapKind::Caves })
		{
			const UiRect card = right.CutTop(54.0f);
			right.CutTop(6.0f);
			const UiId id = Ui::MakeId("royalemap", uint64_t(map));
			const UiInteraction interaction = ui.Interact(id, card);
			const bool selected = m_RoyaleMap == map;
			const float hover = ui.Animate(id, interaction.Hovered || selected);
			if (interaction.Clicked)
				m_RoyaleMap = map;
			ui.Card(card, hover, selected ? 1.0f : 0.0f);
			draw.Text({ card.X + 14.0f, card.Y + 7.0f }, GetMapDef(map).Name, Theme::Bold(17.0f, selected ? Theme::AccentLight : Theme::Text));
			const char* size = map == MapKind::Ashlands ? "Large island, best for 4 to 12 players" : map == MapKind::Town ? "Medium, good for 2 to 6 players" : "Twisting caverns, good for 2 to 6 players";
			draw.Text({ card.X + 14.0f, card.Y + 30.0f }, size, Theme::Body(13.5f, Theme::Muted));
		}
		right.CutTop(4.0f);
		draw.TextWrapped(right.CutTop(76.0f), "Everyone starts with a plain copy of their weapon and their first skill gem. Loot chests, guarded vaults and falling Ember Caches for gear and gems, and stay inside the shrinking ash storm. Gear found in the match stays in the match.", Theme::Body(13.5f, Theme::Faint));
		const MatchReward royaleReward = GetRoyaleReward(1, std::max(players, 2u), 3);
		draw.TextAligned(right.CutTop(22.0f), Format("A win with 3 kills pays %u gold, shards, dust and starsteel", unsigned(royaleReward.Gold)), Theme::Body(13.5f, Theme::Success), { 0.0f, 0.5f });
		right.CutTop(6.0f);
		if (ui.Button(Ui::MakeId("startroyale"), right.CutTop(46.0f), "Start the Battle Royale", ButtonKind::Primary, true, UiIcon::Crown))
		{
			m_Client.StartRoyale(m_RoyaleMap);
			m_Station.reset();
		}

		heading(third, UiIcon::Whirlwind, "Spellbrawl", "The Caldera or the Frozen Lake");
		const char* pitch = m_BrawlRules.Mode == BrawlMode::Damage ? "Spells hurt and cost mana. Wear your rivals down, and mind the lava and the ice. The last mage standing takes the round." : "Spells don't hurt here, they shove. Knock your rivals into the lava or through the ice. The last mage standing takes the round.";
		third.CutTop(draw.TextWrapped(third, pitch, Theme::Body(14.0f, Theme::Text)) + 10.0f);
		ui.Separator(third.CutTop(20.0f), "RULES");
		third.CutTop(8.0f);
		{
			int rounds = m_BrawlRules.Rounds;
			UiRect row = third.CutTop(40.0f);
			draw.TextAligned(row.CutLeft(150.0f), "Rounds", Theme::Bold(16.0f, Theme::Text), { 0.0f, 0.5f });
			if (ui.Button(Ui::MakeId("brawlrounds"), row.CutLeft(40.0f), "-", ButtonKind::Normal, rounds > MinBrawlRounds))
				rounds--;
			draw.TextAligned(row.CutLeft(90.0f), std::to_string(rounds), Theme::Title(22.0f), { 0.5f, 0.5f });
			if (ui.Button(Ui::MakeId("brawlrounds") + 1, row.CutLeft(40.0f), "+", ButtonKind::Normal, rounds < MaxBrawlRounds))
				rounds++;
			m_BrawlRules.Rounds = uint8_t(std::clamp(rounds, int(MinBrawlRounds), int(MaxBrawlRounds)));
			third.CutTop(6.0f);
			DrawModeChoice(third);
			DrawArenaChoice(third);
			DrawBotRules(third);
		}
		third.CutTop(draw.TextWrapped(third, "Everyone has 100 health and the same Ember Bolt, plus three spells from their spellbook. Between rounds everyone but the winner picks a card. Your gear only changes how you look.", Theme::Body(13.5f, Theme::Faint)) + 8.0f);
		const MatchReward brawlReward = GetBrawlReward(true, std::max(players + m_BrawlRules.Bots, 2u), 15);
		draw.TextAligned(third.CutTop(22.0f), Format("Winning with 15 points pays about %u gold", unsigned(brawlReward.Gold)), Theme::Body(13.5f, Theme::Success), { 0.0f, 0.5f });
		third.CutTop(6.0f);
		if (ui.Button(Ui::MakeId("startbrawl"), third.CutTop(46.0f), EnterLabel(), ButtonKind::Primary, true, UiIcon::Whirlwind))
		{
			m_Client.StartBrawl(ChosenArena(), m_BrawlRules);
			m_Station.reset();
		}
	}

	void SessionState::DrawStashWindow()
	{
		Ui& ui = m_App.GetUi();
		const float stashWidth = float(StashColumns) * (SlotSize + SlotGap) - SlotGap;
		const float packWidth = float(PackColumns) * (SlotSize + SlotGap) - SlotGap;
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("stash"), "Stash", StationRect(ui, stashWidth + packWidth + 30.0f + 36.0f, 560.0f), &open);
		if (!open)
		{
			m_Station.reset();
			return;
		}
		UiDrawList& draw = ui.Draw();
		const UiRect result = content.CutBottom(24.0f);
		const UiPayload* drag = ui.GetDrag();
		const ServerInventory& inventory = m_Client.GetInventory();
		const StashTab& stash = m_Client.GetStash();
		const Item& weapon = inventory.Gear[size_t(GearSlot::Weapon)];
		const uint8_t affinity = weapon.IsValid() ? GetItemBase(weapon.Base).Affinity : 0;
		DrawMaterials(content.CutTop(26.0f), false);
		content.CutTop(10.0f);
		UiRect left = content.CutLeft(stashWidth);
		content.CutLeft(30.0f);
		UiRect right = content;

		const uint32_t stored = uint32_t(std::count_if(stash.begin(), stash.end(), [](const Item& item) { return item.IsValid(); }));
		ui.Separator(left.CutTop(20.0f), Format("STASH  %u / %u", stored, unsigned(StashSize)));
		left.CutTop(10.0f);
		const UiRect stashGrid{ left.X, left.Y, stashWidth, GridHeight(uint32_t(StashSize), StashColumns) };
		for (uint32_t slot = 0; slot < StashSize; slot++)
		{
			const UiRect rect = GridSlot(stashGrid, slot, StashColumns);
			const UiId id = Ui::MakeId("stashslot", slot);
			const Item& item = stash[slot];
			const UiInteraction interaction = ui.Interact(id, rect);
			const bool dragged = drag && drag->Type == StashPayload && drag->Value == int(slot);
			DrawItemSlot(ui, rect, dragged ? Item{} : item, { .Hovered = interaction.Hovered, .Target = drag && drag->Type == CarryPayload && !item.IsValid() });
			if (interaction.Clicked && item.IsValid())
				m_Client.WithdrawItem(slot);
			if (item.IsValid())
				ui.OfferDrag(id, StashPayload, int(slot));
			if (const std::optional<int> from = ui.AcceptDrop(rect, CarryPayload))
				m_Client.DepositItem(uint32_t(*from), slot);
			if (interaction.Hovered && item.IsValid() && !drag)
			{
				UiTooltip& tooltip = ui.Tooltip();
				DescribeItem(tooltip, item, affinity);
				tooltip.Space(4.0f).Muted("Click to take it, or drag it to the backpack or the salvage bin", 13.5f);
			}
		}
		left.CutTop(stashGrid.H + 12.0f);
		draw.TextWrapped(left, "Everything here is saved with your character and stays in Emberhold. Unclaimed drops from a mission are sent here when you come home.", Theme::Body(13.5f, Theme::Faint));

		const uint32_t carried = uint32_t(std::count_if(inventory.Pack.begin(), inventory.Pack.end(), [](const Item& item) { return item.IsValid(); }));
		ui.Separator(right.CutTop(20.0f), Format("BACKPACK  %u / %u", carried, unsigned(BackpackSize)));
		right.CutTop(10.0f);
		const UiRect packGrid{ right.X, right.Y, packWidth, GridHeight(uint32_t(BackpackSize), PackColumns) };
		for (uint32_t slot = 0; slot < BackpackSize; slot++)
		{
			const UiRect rect = GridSlot(packGrid, slot, PackColumns);
			const UiId id = Ui::MakeId("carryslot", slot);
			const Item& item = inventory.Pack[slot];
			const UiInteraction interaction = ui.Interact(id, rect);
			const bool dragged = drag && drag->Type == CarryPayload && drag->Value == int(slot);
			DrawItemSlot(ui, rect, dragged ? Item{} : item, { .Hovered = interaction.Hovered, .Target = drag && drag->Type == StashPayload && !item.IsValid() });
			if (interaction.Clicked && item.IsValid())
				m_Client.DepositItem(slot);
			if (item.IsValid())
				ui.OfferDrag(id, CarryPayload, int(slot));
			if (const std::optional<int> from = ui.AcceptDrop(rect, StashPayload))
				m_Client.WithdrawItem(uint32_t(*from), slot);
			if (interaction.Hovered && item.IsValid() && !drag)
			{
				UiTooltip& tooltip = ui.Tooltip();
				DescribeItem(tooltip, item, affinity);
				tooltip.Space(4.0f).Muted("Click to store it, or drag it to the salvage bin", 13.5f);
			}
		}
		right.CutTop(packGrid.H + 18.0f);

		ui.Separator(right.CutTop(20.0f), "SALVAGE");
		right.CutTop(8.0f);
		const UiRect bin = right.CutTop(96.0f);
		const Item* held = nullptr;
		if (drag && drag->Type == StashPayload)
			held = &stash[size_t(drag->Value)];
		else if (drag && drag->Type == CarryPayload)
			held = &inventory.Pack[size_t(drag->Value)];
		const bool over = held && bin.Contains(ui.GetMouse());
		const float pulse = 0.5f + 0.5f * std::sin(ui.GetTime() * 4.0f);
		draw.Rect(bin, { .Fill = Rgba(0.28f, 0.06f, 0.12f, over ? 0.45f : held ? 0.25f : 0.12f), .Border = WithAlpha(over ? Theme::Warning : Theme::Edge, held ? 0.6f + 0.4f * pulse : 0.5f), .BorderWidth = over ? 1.8f : 1.2f, .Chamfer = 8.0f});
		draw.Icon(UiIcon::AnvilImpact, { bin.X + 12.0f, bin.Y + 12.0f, 34.0f, 34.0f }, { .Color = over ? Theme::Warning : Theme::Muted, .ColorBottom = over ? Darken(Theme::Warning, 0.3f) : Theme::Faint });
		if (held && held->IsValid())
		{
			draw.TextAligned({ bin.X + 56.0f, bin.Y + 8.0f, bin.W - 64.0f, 22.0f }, "Release to salvage for", Theme::Bold(14.5f, Theme::Text), { 0.0f, 0.5f });
			DrawCost(draw, { bin.X + 56.0f, bin.Y + 34.0f, bin.W - 64.0f, 20.0f }, GetSalvageValue(*held), MaterialStock{}, true);
			draw.TextAligned({ bin.X + 12.0f, bin.Bottom() - 26.0f, bin.W - 24.0f, 20.0f }, "The item is destroyed", Theme::Body(13.0f, Theme::Danger), { 0.0f, 0.5f });
		}
		else
			draw.TextWrapped({ bin.X + 56.0f, bin.Y + 12.0f, bin.W - 66.0f, bin.H - 16.0f }, "Drag an item here to break it down into gold, forge shards and dust", Theme::Body(13.5f, Theme::Muted));
		if (const std::optional<int> from = ui.AcceptDrop(bin, StashPayload))
			m_Client.SalvageItem({ ItemLocation::Stash, uint16_t(*from) });
		if (const std::optional<int> from = ui.AcceptDrop(bin, CarryPayload))
			m_Client.SalvageItem({ ItemLocation::Pack, uint16_t(*from) });
		DrawResultLine(result);
		DrawDragged(ui, held);
	}

	bool SessionState::DrawItemSource(const UiRect& area, bool gems, uint32_t selectedId, ItemRef& picked)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		UiRect content = area;
		UiRect tabs = content.CutTop(32.0f);
		for (int tab = 0; tab < int(std::size(SourceTabs)); tab++)
		{
			const UiRect button = tabs.CutLeft(116.0f);
			tabs.CutLeft(8.0f);
			if (ui.Button(Ui::MakeId("source", uint64_t(tab) + (gems ? 8 : 0)), button, SourceTabs[tab], m_SourceTab == tab ? ButtonKind::Primary : ButtonKind::Normal))
				m_SourceTab = tab;
		}
		content.CutTop(10.0f);
		std::vector<ItemRef> refs;
		const ServerInventory& inventory = m_Client.GetInventory();
		const auto add = [&](const auto& items, ItemLocation location)
		{
			for (size_t i = 0; i < items.size(); i++)
				if (items[i].IsValid() && items[i].IsGem() == gems)
					refs.push_back({ location, uint16_t(i) });
		};
		if (m_SourceTab == 0)
			add(inventory.Pack, ItemLocation::Pack);
		else if (m_SourceTab == 1)
		{
			if (gems)
				add(inventory.Gems, ItemLocation::Gem);
			else
				add(inventory.Gear, ItemLocation::Gear);
		}
		else
			add(m_Client.GetStash(), ItemLocation::Stash);
		if (refs.empty())
		{
			draw.TextAligned(content.CutTop(30.0f), gems ? "No skill gems here" : "No gear here", Theme::Body(14.5f, Theme::Faint), { 0.0f, 0.5f });
			return false;
		}
		const uint32_t columns = uint32_t(std::max((content.W + SlotGap) / (SlotSize + SlotGap), 1.0f));
		const UiRect grid{ content.X, content.Y, content.W, GridHeight(uint32_t(refs.size()), columns) };
		bool changed = false;
		const Item& weapon = inventory.Gear[size_t(GearSlot::Weapon)];
		const uint8_t affinity = weapon.IsValid() ? GetItemBase(weapon.Base).Affinity : 0;
		for (uint32_t i = 0; i < refs.size(); i++)
		{
			const UiRect rect = GridSlot(grid, i, columns);
			if (rect.Bottom() > area.Bottom() + 1.0f)
				break;
			const Item& item = *FindClientItem(refs[i]);
			const UiId id = Ui::MakeId(gems ? "gemsource" : "gearsource", item.Id);
			const UiInteraction interaction = ui.Interact(id, rect);
			DrawItemSlot(ui, rect, item, { .Hovered = interaction.Hovered, .Highlight = item.Id == selectedId });
			ui.OfferDrag(id, SourcePayload, int(item.Id));
			if (interaction.Clicked)
			{
				picked = refs[i];
				changed = true;
			}
			if (interaction.Hovered && !ui.GetDrag())
			{
				UiTooltip& tooltip = ui.Tooltip();
				DescribeItem(tooltip, item, affinity);
				tooltip.Space(4.0f).Muted(refs[i].Location == ItemLocation::Gear || refs[i].Location == ItemLocation::Gem ? "Equipped, changes apply right away. Click to work on it" : "Click to work on it", 13.5f);
			}
		}
		return changed;
	}

	void SessionState::DrawResultLine(const UiRect& area)
	{
		if (m_Notices.empty())
			return;
		const Notice& notice = m_Notices.back();
		const float alpha = std::clamp((3.5f - notice.Age) / 0.6f, 0.0f, 1.0f);
		m_App.GetUi().Draw().TextAligned(area, notice.Text, Theme::Bold(15.5f, WithAlpha(notice.Color, notice.Color.a * alpha)), { 0.5f, 0.5f });
	}

	void SessionState::DrawForgeWindow()
	{
		Ui& ui = m_App.GetUi();
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("forge"), "Forge", StationRect(ui, 880.0f, 760.0f), &open);
		if (!open)
		{
			m_Station.reset();
			return;
		}
		UiDrawList& draw = ui.Draw();
		const UiRect result = content.CutBottom(24.0f);
		const UiPayload* drag = ui.GetDrag();
		const MaterialStock& stock = m_Client.GetMaterials();
		const bool unlimited = m_Client.GetSession().UnlimitedMaterials;
		const std::optional<ItemRef> ref = LocateItem(m_ForgeItem);
		const Item* item = ref ? FindClientItem(*ref) : nullptr;
		if (item && item->IsGem())
			item = nullptr;
		if (!item)
			m_ForgeItem = 0;
		if (item && m_ForgeAffix >= item->AffixCount)
			m_ForgeAffix = 0;
		const Item& weapon = m_Client.GetInventory().Gear[size_t(GearSlot::Weapon)];
		const uint8_t affinity = weapon.IsValid() ? GetItemBase(weapon.Base).Affinity : 0;

		DrawMaterials(content.CutTop(26.0f), false);
		content.CutTop(10.0f);
		UiRect top = content.CutTop(436.0f);
		UiRect left = top.CutLeft(330.0f);
		top.CutLeft(24.0f);
		UiRect right = top;

		ui.Separator(left.CutTop(20.0f), "ON THE ANVIL");
		left.CutTop(10.0f);
		UiRect header = left.CutTop(84.0f);
		const UiRect anvil = header.CutLeft(84.0f);
		header.CutLeft(14.0f);
		const UiId anvilId = Ui::MakeId("anvil");
		const UiInteraction anvilHover = ui.Interact(anvilId, anvil);
		DrawItemSlot(ui, anvil, item ? *item : Item{}, { .Hovered = anvilHover.Hovered, .Target = drag && drag->Type == SourcePayload, .EmptyIcon = UiIcon::AnvilImpact });
		if (const std::optional<int> dropped = ui.AcceptDrop(anvil, SourcePayload))
		{
			m_ForgeItem = uint32_t(*dropped);
			m_ForgeAffix = 0;
		}
		if (item && anvilHover.Hovered && !drag)
			DescribeItem(ui.Tooltip(), *item, affinity);
		if (item)
		{
			draw.TextFit({ header.X, header.Y + 4.0f, header.W, 24.0f }, GetItemName(*item), Theme::Bold(17.0f, Lighten(ItemColor(*item), 0.2f)));
			draw.Text({ header.X, header.Y + 32.0f }, Format("Level %u %s %s", unsigned(item->Level), GetRarity(item->Quality).Name, GetItemBase(item->Base).Name), Theme::Body(14.0f, Theme::Muted));
			draw.Text({ header.X, header.Y + 54.0f }, Format("%u of %u affix slots, tiers up to %u", unsigned(item->AffixCount), unsigned(GetAffixSlots(item->Quality)), unsigned(MaxAffixTier(item->Level))), Theme::Body(14.0f, Theme::Faint));
		}
		else
			draw.TextWrapped({ header.X, header.Y + 8.0f, header.W, header.H }, "Pick an item below or drag one onto the anvil", Theme::Body(15.0f, Theme::Muted));
		left.CutTop(14.0f);
		if (item)
		{
			for (uint8_t i = 0; i < GetAffixSlots(item->Quality); i++)
			{
				const UiRect row = left.CutTop(34.0f);
				left.CutTop(6.0f);
				if (i >= item->AffixCount)
				{
					draw.Rect(row, { .Border = WithAlpha(Theme::Edge, 0.35f), .BorderWidth = 1.0f, .Chamfer = 6.0f });
					draw.TextAligned(row.Shrink(12.0f, 0.0f), "Open slot, imbue to fill it", Theme::Body(14.0f, Theme::Faint), { 0.0f, 0.5f });
					continue;
				}
				const Affix& affix = item->Affixes[i];
				const UiId id = Ui::MakeId("affix", i);
				const UiInteraction interaction = ui.Interact(id, row);
				const bool selected = m_ForgeAffix == i;
				if (interaction.Clicked)
					m_ForgeAffix = i;
				draw.Rect(row, { .Fill = Rgba(0.75f, 0.65f, 1.0f, selected ? 0.1f : interaction.Hovered ? 0.05f : 0.02f), .Border = selected ? Theme::Accent : WithAlpha(Theme::Edge, 0.5f), .BorderWidth = selected ? 1.6f : 1.0f, .Chamfer = 6.0f});
				const UiRect pill{ row.X + 8.0f, row.Center().y - 10.0f, 30.0f, 20.0f };
				const UiColor tier = TierColor(affix.Tier);
				draw.Rect(pill, { .Fill = WithAlpha(Darken(tier, 0.55f), 0.9f), .Border = tier, .BorderWidth = 1.0f, .Chamfer = 4.0f });
				draw.TextAligned(pill, Format("T%u", unsigned(affix.Tier)), Theme::Bold(12.5f, Lighten(tier, 0.3f)), { 0.5f, 0.5f });
				draw.TextFit({ pill.Right() + 10.0f, row.Y, row.Right() - pill.Right() - 18.0f, row.H }, DescribeAffix(affix), Theme::Body(14.5f, Hex(0x8FB8FF)));
				if (interaction.Hovered && !drag)
				{
					const AffixRange range = GetAffixRange(affix.Kind, affix.Tier);
					UiTooltip& tooltip = ui.Tooltip();
					tooltip.Header(DescribeAffix(affix), tier);
					tooltip.Muted(Format("Tier %u of %u, rolls %s to %s", unsigned(affix.Tier), unsigned(AffixTierCount), DescribeAffix({ affix.Kind, range.Min, affix.Tier }).c_str(), DescribeAffix({ affix.Kind, range.Max, affix.Tier }).c_str()));
					tooltip.Text("Click to pick it for honing or reforging", Theme::Text, 14.0f);
				}
			}
		}

		ui.Separator(right.CutTop(20.0f), "WORK THE ITEM");
		right.CutTop(10.0f);
		for (uint32_t index = 0; index < uint32_t(ForgeAction::Count); index++)
		{
			const ForgeAction action = ForgeAction(index);
			const UiRect row = right.CutTop(76.0f);
			right.CutTop(6.0f);
			const std::optional<std::string> reason = item ? CheckForge(*item, action, m_ForgeAffix, m_ImbueCategory) : std::optional<std::string>("Nothing on the anvil");
			const MaterialStock cost = item ? GetForgeCost(*item, action, m_ForgeAffix) : MaterialStock{};
			const bool affordable = unlimited || CanAfford(stock, cost);
			ActionRow(draw, row, row.Contains(ui.GetMouse()));
			const UiRect button{ row.Right() - 118.0f, row.Y + 21.0f, 106.0f, 34.0f };
			const UiRect text{ row.X + 12.0f, row.Y + 6.0f, button.X - row.X - 20.0f, row.H - 12.0f };
			draw.Text({ text.X, text.Y }, GetForgeActionName(action), Theme::Bold(17.0f, Theme::AccentLight));
			if (action == ForgeAction::Hone && item && !reason)
				draw.TextAligned({ text.X, text.Y, text.W, 20.0f }, Format("%.0f%% chance", double(GetHoneChance(*item, m_ForgeAffix)) * 100.0), Theme::Bold(14.0f, Theme::Success), { 1.0f, 0.5f });
			if (action == ForgeAction::Imbue)
			{
				float x = text.X + 72.0f;
				for (uint32_t c = 0; c < uint32_t(StatCategory::Count); c++)
				{
					const StatCategory category = StatCategory(c);
					const TextStyle style = Theme::Bold(12.5f, m_ImbueCategory == category ? Theme::Ink : Theme::Muted);
					const UiRect chip{ x, text.Y + 1.0f, draw.MeasureWidth(CategoryName(category), style) + 16.0f, 20.0f };
					const UiInteraction pick = ui.Interact(Ui::MakeId("category", c), chip);
					draw.Rect(chip, { .Fill = m_ImbueCategory == category ? Theme::Accent : Rgba(0.8f, 0.75f, 1.0f, pick.Hovered ? 0.08f : 0.03f), .Border = WithAlpha(Theme::Accent, 0.6f), .BorderWidth = 1.0f, .Chamfer = 5.0f });
					draw.TextAligned(chip, CategoryName(category), style, { 0.5f, 0.5f });
					if (pick.Clicked)
						m_ImbueCategory = category;
					x = chip.Right() + 6.0f;
				}
			}
			draw.TextFit({ text.X, text.Y + 24.0f, text.W, 20.0f }, reason && item ? *reason : DescribeForgeAction(action), Theme::Body(13.5f, reason && item ? Theme::Danger : Theme::Muted));
			if (item && !reason)
				DrawCost(draw, { text.X, text.Y + 46.0f, text.W, 18.0f }, cost, stock, unlimited);
			if (ui.Button(Ui::MakeId("forgeaction", index), button, GetForgeActionName(action), action == ForgeAction::Hone || action == ForgeAction::Ascend ? ButtonKind::Primary : ButtonKind::Normal, item && !reason && affordable) && ref)
				m_Client.ForgeItem(*ref, action, m_ForgeAffix, m_ImbueCategory);
		}

		ui.Separator(content.CutTop(20.0f), "YOUR GEAR");
		content.CutTop(10.0f);
		ItemRef picked;
		if (DrawItemSource(content, false, m_ForgeItem, picked))
			if (const Item* chosen = FindClientItem(picked))
			{
				m_ForgeItem = chosen->Id;
				m_ForgeAffix = 0;
			}
		DrawResultLine(result);
		if (drag && drag->Type == SourcePayload)
			if (const std::optional<ItemRef> dragged = LocateItem(uint32_t(drag->Value)))
				DrawDragged(ui, FindClientItem(*dragged));
	}

	void SessionState::DrawLapidaryWindow()
	{
		Ui& ui = m_App.GetUi();
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("lapidary"), "Lapidary", StationRect(ui, 820.0f, 640.0f), &open);
		if (!open)
		{
			m_Station.reset();
			return;
		}
		UiDrawList& draw = ui.Draw();
		const UiRect result = content.CutBottom(24.0f);
		const UiPayload* drag = ui.GetDrag();
		const MaterialStock& stock = m_Client.GetMaterials();
		const bool unlimited = m_Client.GetSession().UnlimitedMaterials;
		const std::optional<ItemRef> ref = LocateItem(m_LapidaryGem);
		const Item* gem = ref ? FindClientItem(*ref) : nullptr;
		if (gem && !gem->IsGem())
			gem = nullptr;
		if (!gem)
			m_LapidaryGem = 0;

		DrawMaterials(content.CutTop(26.0f), false);
		content.CutTop(10.0f);
		UiRect top = content.CutTop(330.0f);
		UiRect left = top.CutLeft(300.0f);
		top.CutLeft(24.0f);
		UiRect right = top;

		ui.Separator(left.CutTop(20.0f), "ON THE ALTAR");
		left.CutTop(10.0f);
		UiRect header = left.CutTop(84.0f);
		const UiRect altar = header.CutLeft(84.0f);
		header.CutLeft(14.0f);
		const UiInteraction altarHover = ui.Interact(Ui::MakeId("altar"), altar);
		DrawItemSlot(ui, altar, gem ? *gem : Item{}, { .Hovered = altarHover.Hovered, .Target = drag && drag->Type == SourcePayload, .EmptyIcon = UiIcon::Gems });
		if (const std::optional<int> dropped = ui.AcceptDrop(altar, SourcePayload))
			m_LapidaryGem = uint32_t(*dropped);
		if (gem && altarHover.Hovered && !drag)
			DescribeItem(ui.Tooltip(), *gem, 0xFF);
		if (gem)
		{
			draw.TextFit({ header.X, header.Y + 4.0f, header.W, 24.0f }, GetItemName(*gem), Theme::Bold(17.0f, Lighten(ItemColor(*gem), 0.2f)));
			draw.Text({ header.X, header.Y + 32.0f }, Format("Level %u of %u", unsigned(gem->Level), unsigned(gem->Corrupted ? CorruptedGemLevel : MaxGemLevel)), Theme::Body(14.0f, Theme::Muted));
			draw.Text({ header.X, header.Y + 54.0f }, Format("Quality %u%% of %u%%", unsigned(gem->GemQuality), unsigned(MaxGemQuality)), Theme::Body(14.0f, Theme::Faint));
			left.CutTop(14.0f);
			const AbilityDef& ability = GetAbility(gem->Skill);
			const float scale = (1.0f + 0.06f * float(gem->Level - 1)) * (1.0f + 0.01f * float(gem->GemQuality));
			if (ability.Damage > 0)
				draw.Text({ left.X, left.Y }, Format("%s deals %.0f%% of its base damage", ability.Name, double(scale) * 100.0), Theme::Body(14.5f, Theme::Text));
			left.CutTop(24.0f);
			if (gem->Mod != GemMod::None)
				draw.TextWrapped(left.CutTop(40.0f), Format("%s: %s", GetGemMod(gem->Mod).Name, GetGemMod(gem->Mod).Description), Theme::Body(14.5f, CorruptColor));
			if (gem->Corrupted)
				draw.TextWrapped(left.CutTop(40.0f), "Corrupted. The gem is sealed and can't be changed again.", Theme::Body(14.5f, CorruptColor));
		}
		else
			draw.TextWrapped({ header.X, header.Y + 8.0f, header.W, header.H }, "Pick a skill gem below or drag one onto the altar", Theme::Body(15.0f, Theme::Muted));

		ui.Separator(right.CutTop(20.0f), "SHAPE THE GEM");
		right.CutTop(10.0f);
		for (uint32_t index = 0; index < uint32_t(GemAction::Count); index++)
		{
			const GemAction action = GemAction(index);
			const UiRect row = right.CutTop(84.0f);
			right.CutTop(6.0f);
			const std::optional<std::string> reason = gem ? CheckGemCraft(*gem, action) : std::optional<std::string>("Nothing on the altar");
			const MaterialStock cost = gem ? GetGemCost(*gem, action) : MaterialStock{};
			const bool affordable = unlimited || CanAfford(stock, cost);
			ActionRow(draw, row, row.Contains(ui.GetMouse()));
			const UiRect button{ row.Right() - 118.0f, row.Y + 25.0f, 106.0f, 34.0f };
			const UiRect text{ row.X + 12.0f, row.Y + 6.0f, button.X - row.X - 20.0f, row.H - 12.0f };
			draw.Text({ text.X, text.Y }, GetGemActionName(action), Theme::Bold(17.0f, action == GemAction::Corrupt ? CorruptColor : Theme::AccentLight));
			draw.TextWrapped({ text.X, text.Y + 22.0f, text.W, 36.0f }, reason && gem ? *reason : DescribeGemAction(action), Theme::Body(13.5f, reason && gem ? Theme::Danger : Theme::Muted));
			if (gem && !reason)
				DrawCost(draw, { text.X, text.Y + 56.0f, text.W, 18.0f }, cost, stock, unlimited);
			if (ui.Button(Ui::MakeId("gemaction", index), button, GetGemActionName(action), action == GemAction::Corrupt ? ButtonKind::Danger : ButtonKind::Primary, gem && !reason && affordable) && ref)
				m_Client.CraftGem(*ref, action);
		}

		ui.Separator(content.CutTop(20.0f), "YOUR SKILL GEMS");
		content.CutTop(10.0f);
		ItemRef picked;
		if (DrawItemSource(content, true, m_LapidaryGem, picked))
			if (const Item* chosen = FindClientItem(picked))
				m_LapidaryGem = chosen->Id;
		DrawResultLine(result);
		if (drag && drag->Type == SourcePayload)
			if (const std::optional<ItemRef> dragged = LocateItem(uint32_t(drag->Value)))
				DrawDragged(ui, FindClientItem(*dragged));
	}

	void SessionState::DrawMissionBoard()
	{
		Ui& ui = m_App.GetUi();
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("board"), "Mission Board", StationRect(ui, 900.0f, 652.0f), &open);
		if (!open)
		{
			m_Station.reset();
			return;
		}
		UiDrawList& draw = ui.Draw();
		ui.Separator(content.CutTop(20.0f), "DESTINATION");
		content.CutTop(10.0f);
		UiRect cards = content.CutTop(118.0f);
		const float cardWidth = (cards.W - 14.0f) * 0.5f;
		for (const MapKind map : { MapKind::Town, MapKind::Caves })
		{
			const UiRect card = cards.CutLeft(cardWidth);
			cards.CutLeft(14.0f);
			const MapDef& def = GetMapDef(map);
			const EnemyDef* boss = FindEnemyDef(map == MapKind::Caves ? EntityKind::Wyrm : EntityKind::Archivist);
			const UiId id = Ui::MakeId("boardmap", uint64_t(map));
			const UiInteraction interaction = ui.Interact(id, card);
			const bool selected = m_BoardMap == map;
			const float hover = ui.Animate(id, interaction.Hovered || selected);
			if (interaction.Clicked)
				m_BoardMap = map;
			ui.Card(card, hover, selected ? 1.0f : 0.0f);
			draw.Icon(map == MapKind::Caves ? UiIcon::SkullCrossedBones : UiIcon::Door, { card.X + 14.0f, card.Y + 14.0f, 40.0f, 40.0f }, { .Color = selected ? Theme::AccentLight : Theme::Muted, .ColorBottom = selected ? Theme::Accent : Theme::Faint });
			draw.Text({ card.X + 66.0f, card.Y + 10.0f }, def.Name, Theme::Bold(19.0f, selected ? Theme::AccentLight : Theme::Text));
			if (boss)
				draw.Text({ card.X + 66.0f, card.Y + 34.0f }, Format("Boss: %s, %s", boss->Name, boss->Title), Theme::Bold(13.5f, Hex(0xFF8A7A)));
			draw.PushClip(card.Shrink(2.0f));
			draw.TextWrapped({ card.X + 66.0f, card.Y + 56.0f, card.W - 78.0f, card.H - 60.0f }, def.Description, Theme::Body(13.5f, Theme::Muted));
			draw.PopClip();
		}
		content.CutTop(16.0f);

		ui.Separator(content.CutTop(20.0f), "DIFFICULTY");
		content.CutTop(10.0f);
		UiRect tierRow = content.CutTop(40.0f);
		draw.TextAligned(tierRow.CutLeft(56.0f), "Tier", Theme::Bold(17.0f, Theme::Text), { 0.0f, 0.5f });
		if (ui.Button(Ui::MakeId("tierdown"), tierRow.CutLeft(40.0f), "-", ButtonKind::Normal, m_BoardTier > 1))
			m_BoardTier--;
		draw.TextAligned(tierRow.CutLeft(56.0f), std::to_string(m_BoardTier), Theme::Title(26.0f), { 0.5f, 0.5f });
		if (ui.Button(Ui::MakeId("tierup"), tierRow.CutLeft(40.0f), "+", ButtonKind::Normal, m_BoardTier < int(MaxMissionTier)))
			m_BoardTier++;
		tierRow.CutLeft(20.0f);
		const uint32_t monsterLevel = MonsterLevelForTier(uint32_t(m_BoardTier));
		draw.TextAligned(tierRow, Format("Monsters level %u, the boss level %u. Drops roll up to item level %u, affix tier %u.", unsigned(monsterLevel), unsigned(monsterLevel + 2), unsigned(std::min(monsterLevel + 2, MaxItemLevel)), unsigned(MaxAffixTier(std::min(monsterLevel + 2, MaxItemLevel)))), Theme::Body(14.5f, Theme::Muted), { 0.0f, 0.5f });
		content.CutTop(16.0f);

		ui.Separator(content.CutTop(20.0f), "MODIFIERS");
		content.CutTop(10.0f);
		UiRect grid = content.CutTop(3.0f * 58.0f + 2.0f * 8.0f);
		const float chipWidth = (grid.W - 16.0f) / 3.0f;
		for (uint32_t i = 0; i < uint32_t(MissionModifier::Count); i++)
		{
			const ModifierDef& modifier = MissionModifiers[i];
			const UiRect chip{ grid.X + float(i % 3) * (chipWidth + 8.0f), grid.Y + float(i / 3) * 66.0f, chipWidth, 58.0f };
			const UiId id = Ui::MakeId("modifier", i);
			const UiInteraction interaction = ui.Interact(id, chip);
			const bool active = HasModifier(m_BoardModifiers, modifier.Id);
			if (interaction.Clicked)
				m_BoardModifiers ^= ModifierMask(1u << i);
			const UiColor accent = modifier.Penalty ? Theme::Danger : Theme::Warning;
			draw.Rect(chip, { .Fill = active ? WithAlpha(Darken(accent, 0.7f), 0.55f) : Rgba(0.8f, 0.75f, 1.0f, interaction.Hovered ? 0.06f : 0.025f), .Border = active ? accent : WithAlpha(Theme::Edge, 0.5f), .BorderWidth = active ? 1.6f : 1.0f, .Chamfer = 7.0f, .Seed = float(i) * 0.19f });
			draw.Icon(modifier.Penalty ? UiIcon::SkullCrack : UiIcon::Biceps, { chip.X + 10.0f, chip.Y + 9.0f, 20.0f, 20.0f }, { .Color = active ? Lighten(accent, 0.3f) : Theme::Faint, .ColorBottom = active ? accent : Theme::Faint });
			draw.Text({ chip.X + 38.0f, chip.Y + 8.0f }, modifier.Name, Theme::Bold(15.5f, active ? Theme::Text : Theme::Muted));
			draw.TextAligned({ chip.X + 10.0f, chip.Y + 8.0f, chip.W - 20.0f, 20.0f }, Format("+%.0f%% / +%.0f%%", double(modifier.Quantity) * 100.0, double(modifier.Rarity) * 100.0), Theme::Body(12.5f, active ? Theme::Success : Theme::Faint), { 1.0f, 0.5f });
			draw.TextFit({ chip.X + 10.0f, chip.Y + 32.0f, chip.W - 20.0f, 20.0f }, modifier.Description, Theme::Body(13.0f, active ? Theme::Text : Theme::Faint));
			if (interaction.Hovered)
				ui.Tooltip().Header(modifier.Name, accent).Text(modifier.Description).Muted(Format("Rewards +%.0f%% item quantity and +%.0f%% rarity", double(modifier.Quantity) * 100.0, double(modifier.Rarity) * 100.0)).Muted(modifier.Penalty ? "Weakens your party" : "Strengthens the monsters");
		}
		content.CutTop(14.0f);

		const MissionRewards rewards = GetMissionRewards(uint32_t(m_BoardTier), m_BoardModifiers);
		UiRect footer = content.CutTop(48.0f);
		const UiRect horde = footer.CutRight(170.0f);
		footer.CutRight(12.0f);
		const UiRect start = footer.CutRight(230.0f);
		draw.TextAligned({ footer.X, footer.Y, footer.W, 24.0f }, Format("Item quantity +%.0f%%, rarity +%.0f%%", double(rewards.Quantity - 1.0f) * 100.0, double(rewards.Rarity) * 100.0), Theme::Bold(16.0f, Theme::Success), { 0.0f, 0.5f });
		const uint32_t players = std::max<uint32_t>(uint32_t(m_Client.GetPlayers().size()), 1u);
		draw.TextAligned({ footer.X, footer.Y + 24.0f, footer.W, 22.0f }, Format("%u revives for the party. Kill 70%% of the monsters to wake the boss.", unsigned(3 + std::min(players, 4u) - 1)), Theme::Body(14.0f, Theme::Muted), { 0.0f, 0.5f });
		if (ui.Button(Ui::MakeId("startmission"), start, "Open the way", ButtonKind::Primary, true, UiIcon::MagicPortal))
		{
			m_Client.StartMission(m_BoardMap, uint8_t(m_BoardTier), m_BoardModifiers);
			m_Station.reset();
		}
		if (ui.Button(Ui::MakeId("starthorde"), horde, "Endless horde", ButtonKind::Normal, true, UiIcon::CrossedSwords))
		{
			m_Client.StartHorde(m_BoardMap);
			m_Station.reset();
		}
		if (ui.Interact(Ui::MakeId("hordehint"), horde).Hovered)
			ui.Tooltip().Header("Endless horde", Theme::AccentLight, std::nullopt, UiIcon::CrossedSwords).Text("The old wave mode: monsters keep coming and you pick an upgrade after every wave.").Muted("The run ends when everyone falls, then you return to Emberhold.");
	}

	void SessionState::DrawMissionStatus()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const GameStatus& status = m_Client.GetStatus();
		const SessionInfo& session = m_Client.GetSession();
		const float center = ui.GetSize().x * 0.5f;
		const EnemyDef* boss = FindEnemyDef(session.Map == MapKind::Caves ? EntityKind::Wyrm : EntityKind::Archivist);
		std::string title = GetMapDef(session.Map).Name;
		std::string subtitle;
		UiColor subtitleColor = Theme::Text;
		if (session.Mode == GameMode::Hub)
			subtitle = "Walk up to a station and press E";
		else
		{
			title += Format("  -  Tier %u", unsigned(session.Tier));
			switch (status.Mission)
			{
			case MissionPhase::Explore:
				subtitle = Format("Slay %u more monsters to draw out %s", unsigned(status.KillGoal > status.Kills ? status.KillGoal - status.Kills : 0), boss ? boss->Name : "the boss");
				break;
			case MissionPhase::Boss:
				subtitle = boss ? std::string(boss->Name) + ", " + boss->Title : "The boss is awake";
				subtitleColor = Hex(0xFF8A7A);
				break;
			case MissionPhase::Victory:
				subtitle = "Victory! Open the chest, then step through the gateway";
				subtitleColor = Theme::Success;
				break;
			case MissionPhase::Failed:
				subtitle = "Mission failed, returning to Emberhold";
				subtitleColor = Theme::Danger;
				break;
			}
		}
		TextStyle titleStyle = Theme::Title(28.0f);
		titleStyle.Tracking = 2.0f;
		const float titleWidth = draw.MeasureWidth(title, titleStyle);
		draw.Shadow(UiRect::Centered({ center, 42.0f }, { titleWidth + 160.0f, 52.0f }), 26.0f, 30.0f, Rgba(0.0f, 0.0f, 0.0f, 0.55f), { 0.0f, 0.0f });
		draw.TextAligned({ 0.0f, 16.0f, ui.GetSize().x, 34.0f }, title, titleStyle, { 0.5f, 0.5f });
		DrawTitleOrnament(center, 32.0f, titleWidth);
		draw.TextAligned({ 0.0f, 54.0f, ui.GetSize().x, 22.0f }, subtitle, Theme::Floating(16.0f, subtitleColor, UiFont::Body), { 0.5f, 0.5f });
		if (session.Mode != GameMode::Mission)
			return;

		float y = 82.0f;
		if (status.Mission == MissionPhase::Explore && status.KillGoal > 0)
		{
			const UiRect bar = UiRect::Centered({ center, y + 6.0f }, { 300.0f, 8.0f });
			const float fraction = std::clamp(float(status.Kills) / float(status.KillGoal), 0.0f, 1.0f);
			draw.Rect(bar.Expand(1.5f), { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.7f), .Radius = 5.0f, .Border = WithAlpha(Theme::Edge, 0.8f), .BorderWidth = 1.0f });
			draw.Rect({ bar.X, bar.Y, bar.W * fraction, bar.H }, { .Fill = Theme::AccentLight, .FillBottom = Theme::Accent, .Radius = 4.0f });
			y += 18.0f;
		}
		if (status.Mission == MissionPhase::Boss)
		{
			DrawBossBar(y);
			y += 44.0f;
		}
		std::string details = Format("%u revive%s left", unsigned(status.Revives), status.Revives == 1 ? "" : "s");
		for (const ModifierDef& modifier : MissionModifiers)
			if (HasModifier(session.Modifiers, modifier.Id))
				details += std::string("   ") + modifier.Name;
		draw.TextAligned({ 0.0f, y, ui.GetSize().x, 20.0f }, details, Theme::Floating(13.5f, status.Revives == 0 ? Theme::Danger : Theme::Muted, UiFont::Body), { 0.5f, 0.5f });
	}

	void SessionState::DrawBossBar(float top)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const GameStatus& status = m_Client.GetStatus();
		const float fraction = float(status.BossHealth) / 255.0f;
		const UiRect bar = UiRect::Centered({ ui.GetSize().x * 0.5f, top + 14.0f }, { 560.0f, 18.0f });
		const float pulse = 0.5f + 0.5f * std::sin(ui.GetTime() * 3.0f);
		draw.Shadow(bar, 9.0f, 14.0f, Rgba(0.9f, 0.15f, 0.1f, 0.25f + 0.15f * pulse), { 0.0f, 0.0f });
		draw.Rect(bar.Expand(2.0f), { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.85f), .Radius = 6.0f, .Border = WithAlpha(Theme::Accent, 0.85f), .BorderWidth = 1.4f });
		draw.Rect({ bar.X, bar.Y, bar.W * fraction, bar.H }, { .Fill = Hex(0xFF5A48), .FillBottom = Hex(0x8E1612), .Radius = 5.0f });
		for (const float notch : { 0.66f, 0.33f })
			draw.Rect({ bar.X + bar.W * notch - 1.0f, bar.Y - 2.0f, 2.0f, bar.H + 4.0f }, { .Fill = WithAlpha(Theme::AccentLight, 0.8f) });
		draw.TextAligned(bar, Format("%.0f%%", double(fraction) * 100.0), Theme::Floating(13.0f, Hex(0xFFF2E6)), { 0.5f, 0.5f });
	}

	void SessionState::DrawTitleOrnament(float center, float y, float titleWidth)
	{
		UiDrawList& draw = m_App.GetUi().Draw();
		for (const float side : { -1.0f, 1.0f })
		{
			const float start = center + side * (titleWidth * 0.5f + 20.0f);
			constexpr float Length = 96.0f;
			draw.Rect({ side < 0.0f ? start - Length : start, y, Length, 1.0f }, { .Fill = WithAlpha(Theme::AccentLight, side < 0.0f ? 0.0f : 0.75f), .FillBottom = WithAlpha(Theme::AccentLight, side < 0.0f ? 0.75f : 0.0f), .Horizontal = true });
			draw.Rect(UiRect::Centered({ start, y + 0.5f }, glm::vec2(14.0f)), { .Fill = WithAlpha(Theme::Accent, 0.45f), .Radius = 7.0f, .Softness = 5.0f, .Additive = 1.0f });
			draw.Diamond({ start, y + 0.5f }, 9.0f, Theme::AccentLight, WithAlpha(Theme::Ink, 0.9f), 1.0f);
			draw.Diamond({ start + side * 14.0f, y + 0.5f }, 4.5f, WithAlpha(Theme::Accent, 0.85f));
		}
	}

	void SessionState::DrawBanner()
	{
		if (m_BannerTime <= 0.0f || m_BannerTitle.empty())
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const float alpha = std::clamp(m_BannerTime / 0.8f, 0.0f, 1.0f) * std::clamp((4.0f - m_BannerTime) / 0.4f, 0.0f, 1.0f);
		const float y = ui.GetSize().y * 0.26f;
		TextStyle title = Theme::Title(54.0f);
		title.Tracking = 5.0f;
		title.Color = WithAlpha(Lighten(m_BannerColor, 0.35f), alpha);
		title.ColorBottom = WithAlpha(m_BannerColor, alpha);
		title.ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f * alpha);
		title.ShadowBlur = 10.0f;
		draw.Shadow(UiRect::Centered({ ui.GetSize().x * 0.5f, y + 30.0f }, { draw.MeasureWidth(m_BannerTitle, title) + 200.0f, 90.0f }), 45.0f, 60.0f, Rgba(0.0f, 0.0f, 0.0f, 0.5f * alpha), { 0.0f, 0.0f });
		draw.TextAligned({ 0.0f, y, ui.GetSize().x, 60.0f }, m_BannerTitle, title, { 0.5f, 0.5f });
		draw.TextAligned({ 0.0f, y + 62.0f, ui.GetSize().x, 26.0f }, m_BannerText, Theme::Floating(19.0f, WithAlpha(Theme::Text, alpha), UiFont::Body), { 0.5f, 0.5f });
	}

	void SessionState::DrawInteractionLabels()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const Level* level = m_Client.GetLevel();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (!level || !self || m_Overview)
			return;
		const auto label = [&](UiId id, Vec2 position, float lift, UiIcon icon, const std::string& title, const char* action, bool near) -> bool
		{
			const std::optional<glm::vec2> pixels = ProjectToScreen(Ground(position, lift));
			if (!pixels)
				return false;
			const glm::vec2 point = ui.ToLogical(*pixels);
			const TextStyle titleStyle = Theme::Bold(15.0f, Theme::AccentLight);
			const TextStyle actionStyle = Theme::Body(13.0f, Theme::Text);
			const float width = std::max(draw.MeasureWidth(title, titleStyle), action ? draw.MeasureWidth(action, actionStyle) : 0.0f) + 54.0f;
			const float height = action ? 46.0f : 30.0f;
			const UiRect box{ point.x - width * 0.5f, point.y - height, width, height };
			const UiInteraction interaction = ui.Interact(id, box);
			const bool lit = near || interaction.Hovered;
			draw.Rect(box.Translate({ 0.0f, 3.0f }), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.5f), .Softness = 8.0f, .Chamfer = 8.0f });
			draw.Rect(box, { .Fill = Rgba(0.06f, 0.05f, 0.11f, 0.92f), .FillBottom = Rgba(0.03f, 0.025f, 0.06f, 0.92f), .Border = WithAlpha(Theme::Accent, lit ? 1.0f : 0.6f), .BorderWidth = lit ? 1.8f : 1.2f, .Glow = WithAlpha(Theme::Accent, lit ? 0.12f : 0.0f), .GlowSize = 10.0f, .Chamfer = 8.0f});
			draw.Icon(icon, { box.X + 9.0f, box.Y + 6.0f, 18.0f, 18.0f }, { .Color = Theme::AccentLight, .ColorBottom = Theme::Accent });
			draw.Text({ box.X + 34.0f, box.Y + 5.0f }, title, titleStyle);
			if (action)
				draw.Text({ box.X + 34.0f, box.Y + 25.0f }, action, actionStyle);
			return interaction.Clicked;
		};
		for (size_t i = 0; i < level->Stations.size(); i++)
		{
			const Station& station = level->Stations[i];
			if (LengthSquared(station.Position - self->Position) > 81.0f)
				continue;
			const bool near = int32_t(i) == m_NearStation;
			const StationDef& def = GetStation(station.Kind);
			const std::string action = std::string("Press E to ") + def.Action;
			if (label(Ui::MakeId("stationlabel", i), station.Position, 2.6f, IconFor(station.Kind), def.Name, near ? action.c_str() : nullptr, near) && near)
				OpenStation(station.Kind);
		}
		if (const Landmark* observatory = level->FindLandmark(LandmarkKind::Observatory); observatory && m_AimPoint && m_Client.GetSession().Mode == GameMode::Hub && LengthSquared(self->Position - observatory->Position) <= observatory->Radius * observatory->Radius)
			for (const Constellation& star : level->Constellations)
			{
				if (LengthSquared(Vec2(m_AimPoint->x, m_AimPoint->z) - star.Position) > star.Radius * star.Radius)
					continue;
				const bool lit = ((m_Client.GetProfile().Starlit >> size_t(star.Spell)) & 1u) != 0;
				const std::string name = std::string(GetAbility(star.Spell).Name) + " constellation";
				const std::string hint = lit ? std::string("It burns for you") : std::string("Cast ") + GetAbility(star.Spell).Name + " at it";
				label(Ui::MakeId("starlabel", size_t(star.Spell)), star.Position, star.Height + 1.6f - GroundHeight(star.Position), IconFor(star.Spell), name, hint.c_str(), lit);
			}
		if (const Landmark* board = level->FindLandmark(LandmarkKind::FallBoard); board && m_Client.GetSession().Mode == GameMode::Hub && LengthSquared(board->Position - self->Position) <= board->Radius * board->Radius)
		{
			const std::string tally = Format("Falls today: %u", unsigned(m_Client.GetStatus().HubFalls));
			label(Ui::MakeId("fallboard"), board->Position, 2.2f, UiIcon::SkullCrossedBones, "Days since someone fell off: 0", tally.c_str(), false);
		}
		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (LengthSquared(entity.Position - self->Position) > 100.0f)
				continue;
			if (entity.Kind == EntityKind::Chest)
			{
				const bool near = entity.NetId == m_NearChest;
				const ChestKind kind = ChestKind(std::min<uint32_t>(entity.Tag, uint32_t(ChestKind::Count) - 1));
				if (entity.Variant && kind != ChestKind::Boss)
					continue;
				const std::string name = entity.Variant ? "Chest (opened)" : GetChestDef(kind).Name;
				if (label(Ui::MakeId("chestlabel", entity.NetId), entity.Position, 1.6f, UiIcon::Chest, name, near ? "Press E to open it" : nullptr, near) && near)
					m_Client.OpenChest();
			}
			else if (entity.Kind == EntityKind::Gateway)
			{
				const bool near = entity.NetId == m_NearGateway;
				if (label(Ui::MakeId("gatelabel", entity.NetId), entity.Position, 3.0f, UiIcon::MagicPortal, "Gateway to Emberhold", near ? "Press E to go home" : nullptr, near) && near)
					m_Client.UseGateway();
			}
		}
	}
}
