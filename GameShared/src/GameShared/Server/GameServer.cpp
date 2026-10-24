#include "GameServer.h"
#include "GameShared/Content/Hub.h"
#include "GameShared/Content/PropLibrary.h"
#include <RadianceEngine/Core/Log.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr size_t MaxQueuedInputs = 24;
		constexpr uint32_t MinBufferTarget = 1;
		constexpr uint32_t MaxBufferTarget = 6;
		constexpr float BufferRelaxSeconds = 4.0f;
		constexpr float BufferGrowCooldown = 1.0f;
		constexpr uint32_t SurplusTicks = 30;
		constexpr uint32_t TraitsResendTicks = 30;
		constexpr uint32_t TraitsRefreshTicks = 120;
		constexpr float JoinTimeout = 10.0f;
		constexpr float PlayerListInterval = 2.0f;
		constexpr float FlungSpeed = 6.0f;

		PlayerInput Sanitize(PlayerInput input)
		{
			if (!std::isfinite(input.Move.x) || !std::isfinite(input.Move.y))
				input.Move = {};
			input.Move = ClampLength(input.Move, 1.0f);
			if (!std::isfinite(input.Aim))
				input.Aim = 0.0f;
			input.AimDistance = std::isfinite(input.AimDistance) ? QuantizeAimDistance(input.AimDistance) : 0.0f;
			input.Buttons &= KnownInputButtons;
			return input;
		}

		ItemBase LookOf(const Item& item)
		{
			return item.IsValid() ? item.Base : ItemBase::Count;
		}

		uint8_t HealthFraction(const Health& health)
		{
			if (health.Current <= 0)
				return 0;
			return uint8_t(std::clamp((health.Current * 255 + health.Max - 1) / std::max(health.Max, 1), 1, 255));
		}

		Inventory ToInventory(const GearSet& gear)
		{
			Inventory inventory;
			inventory.Gear = gear.Gear;
			inventory.Gems = gear.Gems;
			inventory.Pack = gear.Pack;
			inventory.Stash = gear.Stash;
			inventory.Materials = gear.Materials;
			return inventory;
		}

		Inventory MakeRoyaleLoadout(const Inventory& real)
		{
			Inventory loadout;
			loadout.Stash = real.Stash;
			loadout.Materials = real.Materials;
			const Item& weapon = real.Gear[size_t(GearSlot::Weapon)];
			loadout.Gear[size_t(GearSlot::Weapon)] = MakeItem(weapon.IsValid() && IsWeapon(weapon.Base) ? weapon.Base : ItemBase::Wand, 1);
			for (const Item& gem : real.Gems)
				if (gem.IsGem())
				{
					loadout.Gems[0] = MakeGem(gem.Skill, 1);
					break;
				}
			return loadout;
		}

		std::string ApplyReward(Inventory& inventory, const MatchReward& reward)
		{
			const std::pair<Material, uint32_t> grants[] = { { Material::Gold, reward.Gold }, { Material::Shard, reward.Shards }, { Material::ArcaneDust, reward.Dust }, { Material::Starsteel, reward.Starsteel } };
			std::string text;
			for (const auto& [material, amount] : grants)
			{
				if (amount == 0)
					continue;
				uint32_t& stock = inventory.Materials[size_t(material)];
				stock = std::min(stock + amount, MaxMaterialStack);
				text += std::string(text.empty() ? "Match reward: " : ", ") + "+" + std::to_string(amount) + " " + GetMaterial(material).Name;
			}
			return text;
		}

		const char* DescribeMode(GameMode mode)
		{
			switch (mode)
			{
			case GameMode::Hub:
				return "hub";
			case GameMode::Horde:
				return "horde";
			case GameMode::Arena:
				return "arena";
			case GameMode::Royale:
				return "battle royale";
			case GameMode::Brawl:
				return "spellbrawl";
			default:
				return "mission";
			}
		}
	}

	GameServer::GameServer(const ServerConfig& config)
		: m_Config(config)
	{
		m_Config.TickRate = std::clamp(m_Config.TickRate, 1u, 240u);
		m_Config.SnapshotInterval = std::max(m_Config.SnapshotInterval, 1u);
		m_Config.MaxPlayers = std::clamp(m_Config.MaxPlayers, 1u, Game::MaxPlayers);
	}

	GameServer::~GameServer()
	{
		Stop();
	}

	bool GameServer::Start()
	{
		if (m_World)
			return true;
		if (m_Config.MapSeed == 0)
			m_Config.MapSeed = uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()) | 1u;
		m_BotRandom.Seed(uint64_t(m_Config.MapSeed) * 0x9E3779B97F4A7C15ULL + 0xB07u);
		if (m_Config.Listen && !m_Net.Listen(m_Config.Port))
			return false;
		MapKind map = m_Config.Map;
		if (m_Config.StartMode == GameMode::Hub)
			map = MapKind::Hub;
		else if (m_Config.StartMode == GameMode::Arena && !IsArenaMap(map))
			map = MapKind::Crucible;
		else if (m_Config.StartMode == GameMode::Royale && !IsRoyaleMap(map))
			map = MapKind::Ashlands;
		else if (m_Config.StartMode == GameMode::Brawl && !IsBrawlMap(map))
			map = map == RandomBrawlMap ? BrawlMaps[m_BotRandom.Below(uint32_t(std::size(BrawlMaps)))] : MapKind::Caldera;
		CreateWorld({
			.Mode = m_Config.StartMode,
			.Map = map,
			.MapSeed = m_Config.MapSeed,
			.Tier = m_Config.Tier,
			.Modifiers = m_Config.Modifiers,
			.Rules = m_Config.Rules,
		});
		RDN_LOG("Server started: {} (seed {}), {} props, {} portals, {} Hz, {}{}", GetMapDef(m_Session.Map).Name, m_Session.MapSeed, m_World->GetLevel().Props.size(), m_World->GetLevel().Portals.size(), m_Config.TickRate, m_Config.Listen ? "accepting players" : "local only", m_Config.UnlimitedMaterials ? ", unlimited crafting materials" : "");
		return true;
	}

	void GameServer::CreateWorld(const SessionInfo& session)
	{
		m_Session = session;
		m_Session.Tier = uint8_t(std::clamp<uint32_t>(session.Tier, 1u, MaxMissionTier));
		m_Session.Modifiers = session.Mode == GameMode::Mission ? ModifierMask(session.Modifiers & AllModifiers) : ModifierMask(0);
		m_Session.Rules = SanitizeRules(session.Rules);
		m_Session.Epoch = ++m_TravelCount;
		m_Session.UnlimitedMaterials = m_Config.UnlimitedMaterials;
		m_World = std::make_unique<World>(WorldSettings{
			.Seed = m_Session.MapSeed,
			.Map = m_Session.Map,
			.Mode = m_Session.Mode,
			.Tier = m_Session.Tier,
			.Modifiers = m_Session.Modifiers,
			.Rules = m_Session.Rules,
			.Players = std::max(GetPlayerCount(), 1u),
			.TickRate = m_Config.TickRate,
			.IntermissionSeconds = m_Config.IntermissionSeconds,
			.FirstWave = m_Config.FirstWave,
			.LootRate = m_Config.LootRate,
			.LagCompensation = m_Config.LagCompensation,
			.UnlimitedMaterials = m_Config.UnlimitedMaterials,
			.RestartOnDefeat = m_Config.StartMode == GameMode::Horde,
		});
		m_ReturnTimer = 0.0f;
	}

	void GameServer::Travel(GameMode mode, MapKind map, uint8_t tier, ModifierMask modifiers, MatchRules rules)
	{
		if (!m_World)
			return;
		const bool hub = mode == GameMode::Hub;
		const bool valid = hub || (mode == GameMode::Arena ? IsArenaMap(map) : mode == GameMode::Royale ? IsRoyaleMap(map) : mode == GameMode::Brawl ? IsBrawlMap(map) : IsMissionMap(map));
		if (!valid)
			return;
		const uint32_t seed = hub ? m_Config.MapSeed : (uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()) ^ (m_TravelCount * 0x9E3779B9u)) | 1u;
		m_PendingTravel = SessionInfo{ .Mode = mode, .Map = hub ? MapKind::Hub : map, .MapSeed = seed, .Tier = tier, .Modifiers = modifiers, .Rules = rules };
	}

	Entity GameServer::SpawnClient(Client& client, const Inventory& inventory)
	{
		if (m_Session.Mode == GameMode::Royale)
		{
			client.Saved = inventory;
			return m_World->SpawnPlayer(client.PlayerId, MakeRoyaleLoadout(inventory));
		}
		client.Saved.reset();
		return m_World->SpawnPlayer(client.PlayerId, inventory);
	}

	void GameServer::ApplyTravel()
	{
		if (!m_PendingTravel)
			return;
		const SessionInfo next = *m_PendingTravel;
		m_PendingTravel.reset();
		const GameMode previous = m_Session.Mode;

		struct Carry
		{
			uint32_t PlayerId = 0;
			Inventory Items;
			uint32_t Rescued = 0;
			MatchReward Reward;
		};
		std::vector<Carry> carried;
		EntityRegistry& entities = m_World->GetEntities();
		for (Client& client : m_Clients)
		{
			if (!client.Joined)
				continue;
			const Inventory* inventory = entities.TryGet<Inventory>(client.Player);
			if (!client.Saved && !inventory)
				continue;
			Carry carry{ client.PlayerId, client.Saved ? *client.Saved : *inventory };
			carry.Items.Upgrades = {};
			carry.Items.Offer = {};
			carry.Items.OfferPending = false;
			if (m_World->IsPvp())
				carry.Reward = m_World->GetMatch().GetReward(*m_World, client.PlayerId);
			client.Saved.reset();
			carried.push_back(carry);
		}
		entities.Each<Loot>([&](Entity, Loot& loot)
		{
			if (loot.Kind != LootKind::Item || !loot.Drop.IsValid() || loot.ForPlayer == NoPlayer)
				return;
			const auto owner = std::find_if(carried.begin(), carried.end(), [&](const Carry& carry) { return carry.PlayerId == loot.ForPlayer; });
			if (owner == carried.end())
				return;
			const auto free = [](auto& items) { return std::find_if(items.begin(), items.end(), [](const Item& item) { return !item.IsValid(); }); };
			if (const auto slot = free(owner->Items.Stash); slot != owner->Items.Stash.end())
				*slot = loot.Drop;
			else if (const auto pack = free(owner->Items.Pack); pack != owner->Items.Pack.end())
				*pack = loot.Drop;
			else
				return;
			owner->Rescued++;
		});

		m_Bots.clear();
		m_Squad.Clear();
		CreateWorld(next);
		RDN_LOG("Travelled to {} ({}{})", GetMapDef(m_Session.Map).Name, m_Session.Mode == GameMode::Mission ? "mission tier " + std::to_string(m_Session.Tier) : std::string(DescribeMode(m_Session.Mode)), m_Session.Modifiers ? ", " + std::to_string(CountModifiers(m_Session.Modifiers)) + " modifiers" : "");
		const bool resetScores = IsPvpMode(previous) || IsPvpMode(m_Session.Mode);
		for (Client& client : m_Clients)
		{
			if (!client.Joined)
				continue;
			const auto carry = std::find_if(carried.begin(), carried.end(), [&](const Carry& entry) { return entry.PlayerId == client.PlayerId; });
			Inventory inventory = carry != carried.end() ? carry->Items : ToInventory(MakeStarterGear(StarterKit::Arcanist));
			const std::string reward = carry != carried.end() ? ApplyReward(inventory, carry->Reward) : std::string();
			if (resetScores)
			{
				client.Kills = 0;
				client.Deaths = 0;
			}
			client.Player = SpawnClient(client, inventory);
			client.PendingInputs.clear();
			client.Buffering = true;
			ServerTravel travel;
			travel.PlayerNetId = m_World->GetEntities().TryGet<Replicated>(client.Player)->NetId;
			travel.Session = m_Session;
			travel.ServerTick = m_World->GetTick();
			Send(client, travel, SendMode::Reliable);
			SendInventory(client);
			SendStash(client);
			SendChess(client);
			if (carry != carried.end() && carry->Rescued > 0)
				SendNotice(client, std::to_string(carry->Rescued) + (carry->Rescued == 1 ? " item you left behind was" : " items you left behind were") + " sent to your stash", true);
			SendNotice(client, reward, true);
		}
		FillBots();
		m_World->TakeEvents();
		m_World->TakeNotices();
		BroadcastPlayerList();
	}

	void GameServer::CheckReturn(float dt)
	{
		const GameStatus status = m_World->GetStatus();
		const bool failed = (status.Mode == GameMode::Mission && status.Mission == MissionPhase::Failed) || (status.Mode == GameMode::Horde && !m_World->GetSettings().RestartOnDefeat && status.Phase == GamePhase::Defeat);
		const bool finished = IsPvpMode(status.Mode) && status.Match == MatchPhase::Ended;
		if (!failed && !finished)
		{
			m_ReturnTimer = 0.0f;
			return;
		}
		m_ReturnTimer += dt;
		if (m_ReturnTimer >= (finished ? MatchEndSeconds : m_Config.ReturnDelay) && !m_PendingTravel)
			Travel(GameMode::Hub, MapKind::Hub, 1, 0);
	}

	void GameServer::Stop()
	{
		if (!m_World)
			return;
		m_Net.Shutdown();
		m_Clients.clear();
		m_Bots.clear();
		m_Squad.Clear();
		m_World.reset();
		RDN_LOG("Server stopped");
	}

	ConnectionId GameServer::OpenLocalConnection()
	{
		if (!m_World)
			return InvalidConnection;
		const LocalConnection local = m_Net.OpenLocalConnection();
		m_LocalConnection = local.Server;
		return local.Client;
	}

	uint32_t GameServer::GetPlayerCount() const
	{
		return uint32_t(std::count_if(m_Clients.begin(), m_Clients.end(), [](const Client& client) { return client.Joined; }));
	}

	std::vector<ServerPlayerStats> GameServer::GetPlayerStats() const
	{
		std::vector<ServerPlayerStats> stats;
		for (const Client& client : m_Clients)
			if (client.Joined)
				stats.push_back({ client.Name, client.BufferTarget, uint32_t(client.PendingInputs.size()), m_Net.GetStats(client.Connection) });
		return stats;
	}

	void GameServer::Update(float dt)
	{
		if (!m_World)
			return;
		m_Events.clear();
		m_Messages.clear();
		m_Net.Poll(m_Events, m_Messages);
		for (const NetEvent& event : m_Events)
			HandleEvent(event);
		for (const NetMessage& message : m_Messages)
			HandleMessage(message);
		DropSilentClients(dt);
		ApplyTravel();

		const float step = m_World->GetTickSeconds();
		m_Accumulator = std::min(m_Accumulator + dt, step * 8.0f);
		while (m_Accumulator >= step && !m_PendingTravel)
		{
			m_Accumulator -= step;
			Tick();
			CheckReturn(step);
		}

		m_PlayerListTimer += dt;
		if (const uint32_t revision = m_World->GetMatch().GetCardRevision(); revision != m_CardRevision)
		{
			m_CardRevision = revision;
			m_PlayerListTimer = PlayerListInterval;
		}
		if (m_PlayerListTimer >= PlayerListInterval)
			BroadcastPlayerList();
	}

	void GameServer::HandleEvent(const NetEvent& event)
	{
		if (event.Type == NetEventType::Connected)
		{
			if (!FindClient(event.Connection))
				m_Clients.push_back({ .Connection = event.Connection });
			return;
		}
		if (const Client* client = FindClient(event.Connection); client && client->Joined)
			RDN_LOG("{} left ({})", client->Name, event.Reason);
		RemoveClient(event.Connection);
	}

	void GameServer::HandleMessage(const NetMessage& message)
	{
		Client* client = FindClient(message.Connection);
		const std::optional<MessageType> type = PeekType(message.Data);
		if (!client || !type)
			return;
		switch (*type)
		{
		case MessageType::ClientHello:
			if (ClientHello hello; !client->Joined && Decode(message.Data, hello))
				OnHello(*client, hello);
			break;
		case MessageType::ClientInput:
			if (ClientInput input; client->Joined && Decode(message.Data, input))
				OnInput(*client, input);
			break;
		case MessageType::ClientAction:
			if (ClientAction action; client->Joined && Decode(message.Data, action))
				OnAction(*client, action);
			break;
		default:
			break;
		}
	}

	void GameServer::OnHello(Client& client, const ClientHello& hello)
	{
		const auto reject = [&](const std::string& reason)
		{
			Send(client, ServerReject{ .Reason = reason }, SendMode::Reliable);
			m_Net.Close(client.Connection, reason.c_str());
			RemoveClient(client.Connection);
		};
		if (hello.Version != ProtocolVersion)
			return reject("The server runs a different version of the game");
		uint32_t playerId = 0;
		if (!AllocatePlayerId(playerId) && !m_Bots.empty())
			DropBot();
		if (!AllocatePlayerId(playerId))
			return reject("The server is full");

		client.Joined = true;
		client.PlayerId = playerId;
		client.Name = hello.Name.empty() ? "Player " + std::to_string(playerId + 1) : hello.Name.substr(0, 24);
		client.Honors = hello.Honors & AllHonors;
		client.Style = SanitizeCosmetics(hello.Style);
		const StarterKit kit = hello.Kit < uint8_t(StarterKit::Count) ? StarterKit(hello.Kit) : StarterKit::Arcanist;
		client.Host = client.Connection == m_LocalConnection;
		if (client.Host)
			client.BufferTarget = MinBufferTarget;
		client.Player = SpawnClient(client, ToInventory(hello.HasGear ? SanitizeGear(hello.Gear) : MakeStarterGear(kit)));

		ServerWelcome welcome;
		welcome.PlayerId = playerId;
		welcome.PlayerNetId = m_World->GetEntities().TryGet<Replicated>(client.Player)->NetId;
		welcome.Session = m_Session;
		welcome.TickRate = m_Config.TickRate;
		welcome.SnapshotInterval = m_Config.SnapshotInterval;
		welcome.ServerTick = m_World->GetTick();
		welcome.PropHash = GetPropLibraryHash();
		Send(client, welcome, SendMode::Reliable);
		SendInventory(client);
		SendStash(client);
		SendChess(client);
		const Inventory* inventory = m_World->GetEntities().TryGet<Inventory>(client.Player);
		RDN_LOG("{} joined as player {} with a {}", client.Name, playerId + 1, inventory ? GetItemName(inventory->Gear[size_t(GearSlot::Weapon)]) : "weapon");
		FillBots();
		BroadcastPlayerList();
	}

	void GameServer::OnAction(Client& client, const ClientAction& action)
	{
		bool changed = false;
		switch (action.Action)
		{
		case ActionType::Equip:
			changed = m_World->EquipItem(client.Player, action.Value);
			break;
		case ActionType::EquipTo:
			changed = m_World->EquipItemTo(client.Player, action.Value, GearSlot(std::min<uint32_t>(action.Target, uint32_t(GearSlot::Count))));
			break;
		case ActionType::Unequip:
			changed = m_World->UnequipItem(client.Player, GearSlot(std::min<uint32_t>(action.Value, uint32_t(GearSlot::Count))));
			break;
		case ActionType::Drop:
			changed = m_World->DropItem(client.Player, action.Value);
			break;
		case ActionType::SlotGem:
			changed = m_World->SlotGem(client.Player, action.Value, action.Target);
			break;
		case ActionType::UnslotGem:
			changed = m_World->UnslotGem(client.Player, action.Value);
			break;
		case ActionType::ChooseUpgrade:
			changed = m_World->ChooseUpgrade(client.Player, action.Value);
			break;
		case ActionType::StashDeposit:
			changed = m_World->StashItem(client.Player, action.Value, action.Target);
			break;
		case ActionType::StashWithdraw:
			changed = m_World->TakeFromStash(client.Player, action.Value, action.Target);
			break;
		case ActionType::Salvage:
			changed = m_World->SalvageItem(client.Player, DecodeItemRef(action.Value));
			break;
		case ActionType::Forge:
			m_World->ForgeItem(client.Player, DecodeItemRef(action.Target), ForgeAction(std::min<uint32_t>(action.Value, uint32_t(ForgeAction::Count))), action.Extra & 0xFFu, StatCategory(std::min<uint32_t>((action.Extra >> 8) & 0xFFu, uint32_t(StatCategory::Count))));
			changed = true;
			break;
		case ActionType::GemCraft:
			m_World->CraftGem(client.Player, DecodeItemRef(action.Target), GemAction(std::min<uint32_t>(action.Value, uint32_t(GemAction::Count))));
			changed = true;
			break;
		case ActionType::StartMission:
			if (m_World->IsHub() && IsMissionMap(MapKind(std::min<uint32_t>(action.Value, uint32_t(MapKind::Count)))))
			{
				const GameMode mode = GameMode((action.Target >> 8) & 0xFFu) == GameMode::Horde ? GameMode::Horde : GameMode::Mission;
				Travel(mode, MapKind(action.Value), uint8_t(action.Target & 0xFFu), ModifierMask(action.Extra));
				RDN_LOG("{} opened a {} to {}", client.Name, mode == GameMode::Horde ? "horde" : "tier " + std::to_string(action.Target & 0xFFu) + " mission", GetMapDef(MapKind(action.Value)).Name);
			}
			break;
		case ActionType::ReturnToHub:
			if (!m_World->IsHub() && (!m_World->IsPvp() || client.Host || m_World->GetMatch().GetPhase() == MatchPhase::Ended))
				Travel(GameMode::Hub, MapKind::Hub, 1, 0);
			break;
		case ActionType::StartMatch:
			if (m_World->IsHub())
			{
				const GameMode mode = GameMode(std::min<uint32_t>(action.Value, uint32_t(GameMode::Count)));
				MapKind map = MapKind(std::min<uint32_t>(action.Target, uint32_t(MapKind::Count)));
				if (mode == GameMode::Brawl && map == RandomBrawlMap)
					map = BrawlMaps[m_BotRandom.Below(uint32_t(std::size(BrawlMaps)))];
				if ((mode == GameMode::Arena && IsArenaMap(map)) || (mode == GameMode::Royale && IsRoyaleMap(map)) || (mode == GameMode::Brawl && IsBrawlMap(map)))
				{
					const MatchRules rules = DecodeRules(action.Extra);
					Travel(mode, map, 1, 0, rules);
					RDN_LOG("{} started {} on {}", client.Name, mode == GameMode::Royale ? "a battle royale" : mode == GameMode::Brawl ? "a " + std::to_string(rules.Rounds) + " round spellbrawl" : rules.Teams ? "a team arena match" : "a free for all arena match", GetMapDef(map).Name);
				}
			}
			break;
		case ActionType::Spectate:
			m_World->Spectate(client.Player, int16_t(action.Value));
			break;
		case ActionType::BrawlSpell:
			m_World->PickBrawlSpell(client.Player, action.Value, AbilityId(std::min<uint32_t>(action.Target, uint32_t(AbilityId::Count))));
			break;
		case ActionType::BrawlCard:
			m_World->ChooseCard(client.Player, action.Value);
			break;
		case ActionType::Wish:
			m_World->MakeWish(client.Player);
			break;
		case ActionType::Chess:
			changed = m_World->PlayChess(client.Player, Chess::Command(std::min<uint32_t>(action.Value, uint32_t(Chess::Command::Count))), action.Target, action.Extra);
			break;
		case ActionType::SetHonors:
			if (client.Honors != (action.Value & AllHonors))
			{
				client.Honors = uint8_t(action.Value & AllHonors);
				BroadcastPlayerList();
			}
			break;
		case ActionType::SetCosmetics:
			if (const Cosmetics style = UnpackCosmetics(action.Value); style != client.Style)
			{
				client.Style = style;
				BroadcastPlayerList();
			}
			break;
		case ActionType::BrawlReady:
			m_World->SetBrawlReady(client.Player, action.Value != 0);
			break;
		case ActionType::OpenChest:
			changed = m_World->OpenChest(client.Player);
			break;
		case ActionType::UseGateway:
			if (m_World->IsNearGateway(client.Player))
				Travel(GameMode::Hub, MapKind::Hub, 1, 0);
			break;
		default:
			break;
		}
		if (changed)
		{
			HandleWorldEvents();
			FlushNotices();
		}
	}

	void GameServer::SendInventory(const Client& client)
	{
		const Inventory* inventory = m_World->GetEntities().TryGet<Inventory>(client.Player);
		if (!inventory)
			return;
		ServerInventory message;
		message.Gear = inventory->Gear;
		message.Gems = inventory->Gems;
		message.Pack = inventory->Pack;
		message.Upgrades = inventory->Upgrades;
		Send(client, message, SendMode::Reliable);
	}

	void GameServer::SendStash(const Client& client)
	{
		if (const Inventory* inventory = m_World->GetEntities().TryGet<Inventory>(client.Player))
			Send(client, ServerStash{ .Stash = inventory->Stash }, SendMode::Reliable);
	}

	void GameServer::SendChess(const Client& client)
	{
		if (m_Session.Mode != GameMode::Hub)
			return;
		const Chess::Match& match = m_World->GetChess();
		Send(client, ServerChess{ .Revision = match.Revision, .Seats = match.Seats, .Result = match.Result, .Winner = match.Winner, .DrawOffer = match.DrawOffer, .Moves = match.Moves }, SendMode::Reliable);
	}

	void GameServer::SendNotice(const Client& client, const std::string& text, bool success)
	{
		if (!text.empty())
			Send(client, ServerNotice{ .Text = text, .Success = success }, SendMode::Reliable);
	}

	void GameServer::FlushNotices()
	{
		for (const World::Notice& notice : m_World->TakeNotices())
			for (const Client& client : m_Clients)
				if (client.Joined && (notice.PlayerId == NoPlayer || notice.PlayerId == client.PlayerId))
					SendNotice(client, notice.Text, notice.Success);
	}

	void GameServer::OnInput(Client& client, const ClientInput& message)
	{
		for (const PlayerInput& input : message.Inputs)
		{
			if (input.Sequence <= client.LastReceivedInput)
				continue;
			client.LastReceivedInput = input.Sequence;
			client.PendingInputs.push_back(Sanitize(input));
		}
		while (client.PendingInputs.size() > MaxQueuedInputs)
			client.PendingInputs.pop_front();
	}

	void GameServer::RemoveClient(ConnectionId connection)
	{
		const auto it = std::find_if(m_Clients.begin(), m_Clients.end(), [&](const Client& client) { return client.Connection == connection; });
		if (it == m_Clients.end())
			return;
		const bool joined = it->Joined;
		if (joined)
			m_World->RemovePlayer(it->Player);
		m_Clients.erase(it);
		if (joined && GetPlayerCount() == 0)
			while (!m_Bots.empty())
				DropBot();
		else if (joined)
			FillBots();
		if (joined)
			BroadcastPlayerList();
	}

	void GameServer::DropSilentClients(float dt)
	{
		std::vector<ConnectionId> silent;
		for (Client& client : m_Clients)
			if (!client.Joined && (client.JoinTimer += dt) > JoinTimeout)
				silent.push_back(client.Connection);
		for (ConnectionId connection : silent)
		{
			m_Net.Close(connection, "Never introduced itself");
			RemoveClient(connection);
		}
	}

	void GameServer::Tick()
	{
		for (Client& client : m_Clients)
			if (client.Joined)
				ProcessInputs(client);
		m_Squad.Update(*m_World);
		m_World->Step();
		HandleWorldEvents();
		FlushNotices();
		if (m_World->GetTick() % m_Config.SnapshotInterval == 0)
			SendSnapshots();
	}

	void GameServer::ProcessInputs(Client& client)
	{
		const float step = m_World->GetTickSeconds();
		client.SinceStarved += step;
		if (client.PendingInputs.empty())
		{
			if (!client.Buffering)
			{
				m_StarvedTicks++;
				if (client.SinceStarved > BufferGrowCooldown && client.BufferTarget < MaxBufferTarget)
					client.BufferTarget++;
				client.SinceStarved = 0.0f;
			}
			client.Buffering = true;
			return;
		}
		if (client.SinceStarved > BufferRelaxSeconds && client.BufferTarget > MinBufferTarget)
		{
			client.BufferTarget--;
			client.SinceStarved = 0.0f;
		}
		if (client.Buffering && client.PendingInputs.size() < client.BufferTarget)
			return;
		client.Buffering = false;
		client.Surplus = client.PendingInputs.size() > client.BufferTarget ? client.Surplus + 1 : 0;
		const bool drain = client.PendingInputs.size() > client.BufferTarget + 2 || client.Surplus >= SurplusTicks;
		if (drain)
			client.Surplus = 0;
		const size_t steps = drain ? 2 : 1;
		for (size_t i = 0; i < steps; i++)
		{
			const PlayerInput input = client.PendingInputs.front();
			client.PendingInputs.pop_front();
			m_World->ApplyPlayerInput(client.Player, input);
			client.LastProcessedInput = input.Sequence;
		}
	}

	void GameServer::HandleWorldEvents()
	{
		bool statsChanged = false;
		for (const WorldEvent& event : m_World->TakeEvents())
		{
			Client* client = FindClientByPlayer(event.PlayerId);
			switch (event.Type)
			{
			case WorldEventType::EnemyKilled:
				if (client && !m_World->IsPvp())
				{
					client->Kills++;
					statsChanged = true;
				}
				break;
			case WorldEventType::PlayerDied:
				if (client)
				{
					client->Deaths++;
					statsChanged = true;
					if (!m_World->IsPvp())
						RDN_LOG("{} died", client->Name);
				}
				break;
			case WorldEventType::MatchStarted:
				RDN_LOG("The {} match has begun with {} players", DescribeMode(m_Session.Mode), m_World->GetMatch().GetEntrants());
				statsChanged = true;
				break;
			case WorldEventType::MatchEnded:
				if (event.Detail != NoTeam)
					RDN_LOG("The match is over, the {} team wins", GetTeamName(event.Detail));
				else if (event.PlayerId != NoPlayer)
					RDN_LOG("The match is over, {} wins", NameOf(event.PlayerId));
				else
					RDN_LOG("The match is over without a winner");
				statsChanged = true;
				break;
			case WorldEventType::StormClosing:
				RDN_LOG("The ash storm closes in (stage {})", event.Detail + 1);
				break;
			case WorldEventType::RoundStarted:
				RDN_LOG("Round {} of {} begins with {} players", event.Detail, m_World->GetMatch().GetBrawl().Rounds, m_World->GetMatch().GetEntrants());
				break;
			case WorldEventType::RoundEnded:
				if (event.PlayerId != NoPlayer)
					RDN_LOG("{} wins round {}", NameOf(event.PlayerId), event.Detail);
				else
					RDN_LOG("Round {} ends without a winner", event.Detail);
				statsChanged = true;
				break;
			case WorldEventType::CacheDropped:
				break;
			case WorldEventType::LootPicked:
				statsChanged = statsChanged || event.Detail != uint8_t(LootKind::Health);
				break;
			case WorldEventType::InventoryChanged:
				if (client)
					SendInventory(*client);
				statsChanged = true;
				break;
			case WorldEventType::StashChanged:
				if (client)
					SendStash(*client);
				break;
			case WorldEventType::ChessChanged:
				for (const Client& watcher : m_Clients)
					if (watcher.Joined)
						SendChess(watcher);
				break;
			case WorldEventType::MaterialsChanged:
			case WorldEventType::ChestOpened:
				break;
			case WorldEventType::BossAwakened:
				if (const EnemyDef* def = FindEnemyDef(EntityKind(event.Detail)))
					RDN_LOG("{} awakens", def->Name);
				break;
			case WorldEventType::BossDefeated:
				RDN_LOG("The boss is dead, mission complete");
				break;
			case WorldEventType::MissionFailed:
				RDN_LOG("Mission failed, no revives left");
				break;
			case WorldEventType::UpgradesOffered:
				if (client)
					RDN_LOG("{} gets to choose an upgrade", client->Name);
				break;
			case WorldEventType::WaveStarted:
				RDN_LOG("Wave {} started", m_World->GetStatus().Wave);
				break;
			case WorldEventType::WaveCleared:
				RDN_LOG("Wave {} cleared", m_World->GetStatus().Wave);
				break;
			case WorldEventType::Defeat:
				RDN_LOG("Everyone died on wave {}", m_World->GetStatus().Wave);
				break;
			case WorldEventType::Teleported:
				if (const Portal* portal = m_World->GetLevel().FindPortal(event.Detail); client && portal)
					RDN_LOG("{} went to the {}", client->Name, portal->Label);
				break;
			}
		}
		std::vector<KillRecord> kills = m_World->TakeKills();
		if (!kills.empty())
		{
			for (const KillRecord& kill : kills)
			{
				if (kill.Killer != NoPlayer)
					RDN_LOG("{} slew {}", NameOf(kill.Killer), NameOf(kill.Victim));
				else
					RDN_LOG("{} fell", NameOf(kill.Victim));
			}
			const ServerKillFeed feed{ .Kills = std::move(kills) };
			for (const Client& client : m_Clients)
				if (client.Joined)
					Send(client, feed, SendMode::Reliable);
			statsChanged = true;
		}
		if (statsChanged)
			BroadcastPlayerList();
	}

	void GameServer::SendSnapshots()
	{
		ServerSnapshot snapshot;
		snapshot.Epoch = m_Session.Epoch;
		snapshot.Tick = m_World->GetTick();
		snapshot.Status = m_World->GetStatus();
		snapshot.Hits = m_World->TakeHits();
		EntityRegistry& entities = m_World->GetEntities();
		struct Visibility
		{
			uint32_t Player = NoPlayer;
			uint16_t Opened = 0;
			bool Chest = false;
			bool Landmark = false;
		};
		std::vector<EntityState> states;
		std::vector<Visibility> visibility;
		entities.Each<Replicated>([&](Entity entity, const Replicated& replicated)
		{
			const Transform* transform = entities.TryGet<Transform>(entity);
			if (!transform)
				return;
			EntityState state{ .NetId = replicated.NetId, .Kind = replicated.Kind, .Position = transform->Position, .Facing = transform->Facing, .Owner = replicated.Owner, .Tag = replicated.Tag, .Variant = replicated.Variant, .Scale = replicated.Scale };
			if (const Health* health = entities.TryGet<Health>(entity))
				state.Health = HealthFraction(*health);
			if (const PlayerController* controller = entities.TryGet<PlayerController>(entity))
			{
				if (!controller->State.Alive)
					state.Flags |= uint16_t(EntityFlags::Dead);
				if (controller->State.Invulnerable > 0.0f || controller->State.DashDistance > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Dashing);
				if (controller->State.Chill > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Chilled);
				if (controller->State.Stun > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Stunned);
				if (controller->State.Static > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Static);
				if (controller->State.Ward > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Warded);
				if (m_World->GetMatch().IsBurning(controller->PlayerId) && m_Session.Mode == GameMode::Brawl)
					state.Flags |= uint16_t(EntityFlags::Burning);
				if (m_World->GetMatch().IsIgnited(controller->PlayerId))
					state.Flags |= uint16_t(EntityFlags::Ignited);
				if (m_World->GetMatch().GetMarkedBy(controller->PlayerId) != NoPlayer)
					state.Flags |= uint16_t(EntityFlags::Marked);
				if (m_World->GetMatch().GetShield(controller->PlayerId) > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Barrier);
				if (controller->State.Still >= LeylineSeconds && m_World->GetMatch().HasPlayerCard(controller->PlayerId, CardId::Leyline))
					state.Flags |= uint32_t(EntityFlags::Leyline);
				if (LengthSquared(controller->State.Push) > FlungSpeed * FlungSpeed)
					state.Flags |= uint16_t(EntityFlags::Flung);
				if (controller->State.IsFalling())
					state.Flags |= uint16_t(controller->State.Fall < FallSeconds ? EntityFlags::Falling : EntityFlags::Lifted);
				state.Variant = uint8_t(AbilityId::Count);
				if (controller->State.CastSlot >= 0)
				{
					state.Flags |= uint16_t(EntityFlags::Attacking);
					state.Variant = uint8_t(controller->Traits.Skills[size_t(controller->State.CastSlot)].Ability);
				}
			}
			if (const Dummy* dummy = entities.TryGet<Dummy>(entity))
			{
				if (dummy->Burning)
					state.Flags |= uint16_t(EntityFlags::Burning);
				if (LengthSquared(dummy->Push) > FlungSpeed * FlungSpeed && dummy->Fall <= 0.0f)
					state.Flags |= uint16_t(EntityFlags::Flung);
				if (dummy->Fall > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Falling);
			}
			if (const Enemy* enemy = entities.TryGet<Enemy>(entity))
			{
				if (enemy->Windup > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Attacking);
				if (enemy->Elite)
					state.Flags |= uint16_t(EntityFlags::Elite);
				if (enemy->Chill > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Chilled);
				if (enemy->Stun > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Stunned);
			}
			if (entities.TryGet<PendingStrike>(entity))
				state.Flags |= uint16_t(EntityFlags::Pending);
			const Chest* chest = entities.TryGet<Chest>(entity);
			states.push_back(state);
			visibility.push_back({ replicated.Visibility, chest ? chest->Opened : uint16_t(0), chest != nullptr, chest && chest->Kind == ChestKind::Cache && chest->Opened == 0 });
		});
		const bool cull = m_Session.Mode == GameMode::Royale;
		for (Client& client : m_Clients)
		{
			if (!client.Joined)
				continue;
			snapshot.Entities.clear();
			uint32_t self = 0;
			uint32_t watched = 0;
			Vec2 focus;
			if (const Replicated* replicated = entities.TryGet<Replicated>(client.Player))
				self = replicated->NetId;
			if (const Transform* transform = entities.TryGet<Transform>(client.Player))
				focus = transform->Position;
			if (const PlayerController* controller = entities.TryGet<PlayerController>(client.Player); controller && !controller->State.Alive && controller->Watching != NoPlayer)
			{
				const Entity target = m_World->FindPlayer(controller->Watching);
				if (const Transform* transform = entities.TryGet<Transform>(target))
					focus = transform->Position;
				if (const Replicated* replicated = entities.TryGet<Replicated>(target))
					watched = replicated->NetId;
			}
			for (size_t i = 0; i < states.size(); i++)
			{
				if (visibility[i].Player != NoPlayer && visibility[i].Player != client.PlayerId)
					continue;
				if (cull && !visibility[i].Landmark && states[i].NetId != self && states[i].NetId != watched && LengthSquared(states[i].Position - focus) > RoyaleRelevance * RoyaleRelevance)
					continue;
				snapshot.Entities.push_back(states[i]);
				if (visibility[i].Chest)
					snapshot.Entities.back().Variant = uint8_t((visibility[i].Opened >> (client.PlayerId % 16)) & 1u);
			}
			snapshot.LastProcessedInput = client.LastProcessedInput;
			snapshot.LocalPlayer = DescribeLocalPlayer(client);
			if (LocalPlayerState& local = snapshot.LocalPlayer; local.Valid)
			{
				if (local.Traits != client.SentTraits)
				{
					client.SentTraits = local.Traits;
					client.TraitsResend = TraitsResendTicks;
				}
				if (client.TraitsRefresh == 0)
				{
					client.TraitsRefresh = TraitsRefreshTicks;
					client.TraitsResend = std::max(client.TraitsResend, 1u);
				}
				client.TraitsRefresh--;
				local.HasTraits = client.TraitsResend > 0;
				client.TraitsResend -= local.HasTraits ? 1 : 0;
			}
			Send(client, snapshot, SendMode::Unreliable);
		}
	}

	LocalPlayerState GameServer::DescribeLocalPlayer(const Client& client) const
	{
		LocalPlayerState local;
		const EntityRegistry& entities = m_World->GetEntities();
		const Transform* transform = entities.TryGet<Transform>(client.Player);
		const PlayerController* controller = entities.TryGet<PlayerController>(client.Player);
		const Health* health = entities.TryGet<Health>(client.Player);
		const Inventory* inventory = entities.TryGet<Inventory>(client.Player);
		if (!transform || !controller || !health || !inventory)
			return local;
		local.Valid = true;
		local.Motion = *transform;
		local.State = controller->State;
		local.Buffs = controller->Buffs;
		local.Traits = controller->Traits;
		local.Offer = inventory->Offer;
		local.OfferPending = inventory->OfferPending;
		local.Health = health->Current;
		local.MaxHealth = health->Max;
		local.RespawnTime = controller->State.Alive ? 0.0f : controller->RespawnTimer;
		local.Materials = inventory->Materials;
		local.Watching = controller->Watching;
		local.KilledBy = controller->KilledBy;
		if (m_Session.Mode == GameMode::Brawl)
		{
			local.Brawl = m_World->GetMatch().GetLoadout(client.PlayerId);
			local.Shield = uint8_t(std::clamp(std::ceil(m_World->GetMatch().GetShield(client.PlayerId)), 0.0f, 255.0f));
		}
		if (m_Session.Mode == GameMode::Hub)
			local.Starlit = m_World->GetStarlit(client.PlayerId);
		return local;
	}

	void GameServer::BroadcastPlayerList()
	{
		m_PlayerListTimer = 0.0f;
		ServerPlayerList list;
		const EntityRegistry& entities = m_World->GetEntities();
		const bool pvp = m_World->IsPvp();
		const auto describe = [&](uint32_t playerId, const std::string& name, Entity player, const Cosmetics& style)
		{
			const PlayerController* controller = entities.TryGet<PlayerController>(player);
			const Inventory* inventory = entities.TryGet<Inventory>(player);
			PlayerLook look;
			look.Style = style;
			if (inventory)
			{
				const Item& weapon = inventory->Gear[size_t(GearSlot::Weapon)];
				look.Weapon = weapon.IsValid() ? weapon.Base : ItemBase::Wand;
				look.WeaponQuality = weapon.Quality;
				look.Head = LookOf(inventory->Gear[size_t(GearSlot::Head)]);
				look.Body = LookOf(inventory->Gear[size_t(GearSlot::Body)]);
				look.Feet = LookOf(inventory->Gear[size_t(GearSlot::Feet)]);
			}
			const MatchScore& score = m_World->GetMatch().GetScore(playerId);
			return PlayerInfo{
				.PlayerId = playerId,
				.Name = name,
				.Kills = score.Kills,
				.Deaths = score.Deaths,
				.Assists = score.Assists,
				.Damage = score.Damage,
				.Buffs = controller ? controller->Buffs : BuffStacks{},
				.Look = look,
				.Team = m_World->GetTeam(playerId),
				.Placement = pvp ? score.Placement : uint8_t(0),
				.Score = m_World->GetMatch().GetPoints(playerId),
				.Rounds = m_World->GetMatch().GetRoundWins(playerId),
				.Cards = m_World->GetCards(playerId),
				.Choosing = m_Session.Mode == GameMode::Brawl && m_World->GetMatch().GetLoadout(playerId).OfferPending,
			};
		};
		for (const Client& client : m_Clients)
		{
			if (!client.Joined)
				continue;
			PlayerInfo info = describe(client.PlayerId, client.Name, client.Player, client.Style);
			if (!pvp)
			{
				info.Kills = client.Kills;
				info.Deaths = client.Deaths;
			}
			info.Ping = m_Net.GetPing(client.Connection);
			info.Host = client.Host;
			info.Honors = client.Honors;
			list.Players.push_back(info);
		}
		for (const Bot& bot : m_Bots)
		{
			PlayerInfo info = describe(bot.PlayerId, bot.Name, m_World->FindPlayer(bot.PlayerId), bot.Style);
			info.Ping = 0;
			info.Bot = true;
			list.Players.push_back(info);
		}
		std::sort(list.Players.begin(), list.Players.end(), [](const PlayerInfo& a, const PlayerInfo& b) { return a.PlayerId < b.PlayerId; });
		for (const Client& client : m_Clients)
			if (client.Joined)
				Send(client, list, SendMode::Reliable);
	}

	GameServer::Client* GameServer::FindClient(ConnectionId connection)
	{
		const auto it = std::find_if(m_Clients.begin(), m_Clients.end(), [&](const Client& client) { return client.Connection == connection; });
		return it != m_Clients.end() ? &*it : nullptr;
	}

	GameServer::Client* GameServer::FindClientByPlayer(uint32_t playerId)
	{
		const auto it = std::find_if(m_Clients.begin(), m_Clients.end(), [&](const Client& client) { return client.Joined && client.PlayerId == playerId; });
		return it != m_Clients.end() ? &*it : nullptr;
	}

	bool GameServer::AllocatePlayerId(uint32_t& playerId) const
	{
		for (playerId = 0; playerId < m_Config.MaxPlayers; playerId++)
			if (std::none_of(m_Clients.begin(), m_Clients.end(), [&](const Client& client) { return client.Joined && client.PlayerId == playerId; }) && std::none_of(m_Bots.begin(), m_Bots.end(), [&](const Bot& bot) { return bot.PlayerId == playerId; }))
				return true;
		return false;
	}

	void GameServer::FillBots()
	{
		if (!m_World || m_Session.Mode != GameMode::Brawl || GetPlayerCount() == 0)
			return;
		const BotSkill skill = BotSkill(std::min<uint32_t>(m_Session.Rules.BotLevel, uint32_t(BotSkill::Count) - 1));
		while (m_Bots.size() < m_Session.Rules.Bots)
		{
			uint32_t playerId = 0;
			if (!AllocatePlayerId(playerId))
				break;
			Bot bot{ .PlayerId = playerId };
			for (uint32_t attempt = 0; attempt < 32 && bot.Name.empty(); attempt++)
			{
				const std::string name = PickBotName(m_BotNames++);
				const auto taken = [&](const std::string& other) { return other == name; };
				if (std::none_of(m_Clients.begin(), m_Clients.end(), [&](const Client& client) { return client.Joined && taken(client.Name); }) && std::none_of(m_Bots.begin(), m_Bots.end(), [&](const Bot& other) { return taken(other.Name); }))
					bot.Name = name;
			}
			if (bot.Name.empty())
				bot.Name = "Bot " + std::to_string(playerId + 1);
			bot.Style.Head = Headwear(m_BotRandom.Below(uint32_t(Headwear::Count)));
			const uint32_t first = m_BotRandom.Below(uint32_t(BotStyle::Count));
			size_t fewest = m_Bots.size() + 1;
			for (uint32_t i = 0; i < uint32_t(BotStyle::Count); i++)
			{
				const BotStyle style = BotStyle((first + i) % uint32_t(BotStyle::Count));
				const size_t count = size_t(std::count_if(m_Bots.begin(), m_Bots.end(), [&](const Bot& other) { return other.Personality == style; }));
				if (count < fewest)
				{
					fewest = count;
					bot.Personality = style;
				}
			}
			const BotStyle style = bot.Personality;
			m_World->SpawnPlayer(playerId, ToInventory(MakeStarterGear(StarterKit::Arcanist)));
			m_Squad.Add(playerId, BotProfile{ .Skill = skill, .Style = style, .Seed = m_BotRandom.Next() });
			m_Bots.push_back(bot);
			RDN_LOG("{} joined as a {} {} bot", bot.Name, BotSkillNames[size_t(skill)], BotStyleNames[size_t(style)]);
		}
	}

	void GameServer::DropBot()
	{
		if (m_Bots.empty())
			return;
		const Bot bot = m_Bots.back();
		m_Bots.pop_back();
		m_Squad.Remove(bot.PlayerId);
		if (m_World)
			if (const Entity player = m_World->FindPlayer(bot.PlayerId); player.IsValid())
				m_World->RemovePlayer(player);
		RDN_LOG("{} left", bot.Name);
	}

	std::string GameServer::NameOf(uint32_t playerId) const
	{
		for (const Client& client : m_Clients)
			if (client.Joined && client.PlayerId == playerId)
				return client.Name;
		for (const Bot& bot : m_Bots)
			if (bot.PlayerId == playerId)
				return bot.Name;
		return "Someone";
	}
}
