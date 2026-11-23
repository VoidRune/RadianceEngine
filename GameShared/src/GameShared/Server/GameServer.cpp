#include "GameServer.h"
#include "GameShared/Content/Fighters.h"
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

		uint8_t HealthFraction(const Health& health)
		{
			if (health.Current <= 0)
				return 0;
			return uint8_t(std::clamp((health.Current * 255 + health.Max - 1) / std::max(health.Max, 1), 1, 255));
		}

		BrawlSpells SanitizeSpells(const BrawlSpells& spells)
		{
			BrawlSpells chosen = DefaultBrawlSpells;
			for (uint32_t slot = 0; slot < BrawlSlots; slot++)
				AssignSpell(chosen, slot, spells[slot]);
			return chosen;
		}

		const char* DescribeMode(GameMode mode)
		{
			switch (mode)
			{
			case GameMode::Brawl:
				return "spellbrawl";
			case GameMode::Shardfall:
				return "shardfall";
			default:
				return "hub";
			}
		}
	}

	GameServer::GameServer(const ServerConfig& config)
		: m_Config(config)
	{
		m_Config.TickRate = std::clamp(m_Config.TickRate, 1u, 240u);
		m_Config.SnapshotInterval = std::max(m_Config.SnapshotInterval, 1u);
		m_Config.MaxPlayers = std::clamp(m_Config.MaxPlayers, 1u, GetPlayerCap(m_Config.StartMode));
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
		if (m_Config.Listen && m_Config.Relay && !m_Net.ListenRelay())
			RDN_LOG_WARNING("Friends can only join with the address while the Steam relay is unavailable");
		MapKind map = m_Config.Map;
		m_BrawlChoice = IsBrawlMap(map) ? map : RandomBrawlMap;
		if (m_Config.StartMode == GameMode::Brawl && !IsBrawlMap(map))
			map = map == RandomBrawlMap ? BrawlMaps[m_BotRandom.Below(uint32_t(std::size(BrawlMaps)))] : MapKind::Caldera;
		else if (m_Config.StartMode == GameMode::Shardfall && !IsShardfallMap(map))
			map = MapKind::ProvingHollow;
		else if (m_Config.StartMode == GameMode::Hub)
			map = MapKind::Hub;
		CreateWorld({
			.Mode = m_Config.StartMode,
			.Map = map,
			.MapSeed = m_Config.MapSeed,
			.Rules = m_Config.Rules,
			.Run = m_Config.Run,
		});
		RDN_LOG("Server started: {} (seed {}), {} props, {} portals, {} Hz, {}", GetMapDef(m_Session.Map).Name, m_Session.MapSeed, m_World->GetLevel().Props.size(), m_World->GetLevel().Portals.size(), m_Config.TickRate, m_Config.Listen ? "accepting players" : "local only");
		return true;
	}

	void GameServer::CreateWorld(const SessionInfo& session)
	{
		m_Session = session;
		m_Session.Rules = SanitizeRules(session.Rules);
		m_Session.Run = session.Mode == GameMode::Shardfall ? SanitizeRunRules(session.Run) : RunRules{};
		if (m_Session.Mode == GameMode::Shardfall)
			PrepareRunSession(m_Session.Map, m_Session.MapSeed, m_Session.Run);
		m_Session.Epoch = ++m_TravelCount;
		m_World = std::make_unique<World>(WorldSettings{
			.Seed = m_Session.MapSeed,
			.Map = m_Session.Map,
			.Mode = m_Session.Mode,
			.Rules = m_Session.Rules,
			.Run = m_Session.Run,
			.Players = std::max(GetPlayerCount(), 1u),
			.TickRate = m_Config.TickRate,
			.LagCompensation = m_Config.LagCompensation,
		});
		m_ReturnTimer = 0.0f;
		m_RecapSent = false;
		m_RunRevision = 0;
	}

	void GameServer::Travel(GameMode mode, MapKind map, MatchRules rules, RunRules run)
	{
		if (!m_World)
			return;
		const bool hub = mode == GameMode::Hub;
		const bool valid = hub || (mode == GameMode::Brawl ? IsBrawlMap(map) : mode == GameMode::Shardfall && IsShardfallMap(map));
		if (!valid)
			return;
		if (GetPlayerCount() > GetPlayerCap(mode))
		{
			RDN_LOG_WARNING("{} needs at most {} players, {} are here", DescribeMode(mode), GetPlayerCap(mode), GetPlayerCount());
			return;
		}
		const uint32_t seed = hub ? m_Config.MapSeed : (uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()) ^ (m_TravelCount * 0x9E3779B9u)) | 1u;
		m_PendingTravel = SessionInfo{ .Mode = mode, .Map = hub ? MapKind::Hub : map, .MapSeed = seed, .Rules = rules, .Run = run };
	}

	void GameServer::ApplyTravel()
	{
		if (!m_PendingTravel)
			return;
		const SessionInfo next = *m_PendingTravel;
		m_PendingTravel.reset();
		const GameMode previous = m_Session.Mode;
		for (Client& client : m_Clients)
			if (client.Joined)
				client.Spells = m_World->GetChosenSpells(client.PlayerId);

		m_Bots.clear();
		m_Squad.Clear();
		CreateWorld(next);
		RDN_LOG("Travelled to {} ({})", GetMapDef(m_Session.Map).Name, DescribeMode(m_Session.Mode));
		const bool resetScores = IsPvpMode(previous) || IsPvpMode(m_Session.Mode);
		for (Client& client : m_Clients)
		{
			if (!client.Joined)
				continue;
			if (resetScores)
			{
				client.Kills = 0;
				client.Deaths = 0;
			}
			client.Player = m_World->SpawnPlayer(client.PlayerId, client.Spells);
			client.PendingInputs.clear();
			client.Buffering = true;
			client.Rematch = false;
			ServerTravel travel;
			travel.PlayerNetId = m_World->GetEntities().TryGet<Replicated>(client.Player)->NetId;
			travel.Session = m_Session;
			travel.ServerTick = m_World->GetTick();
			Send(client, travel, SendMode::Reliable);
			SendChess(client);
		}
		FillBots();
		m_World->TakeEvents();
		m_World->TakeNotices();
		BroadcastPlayerList();
	}

	void GameServer::ApplyArenaChange()
	{
		m_Session.Map = m_World->GetSettings().Map;
		m_Session.MapSeed = m_World->GetSettings().Seed;
		m_Session.Epoch = ++m_TravelCount;
		for (Client& client : m_Clients)
		{
			if (!client.Joined)
				continue;
			client.PendingInputs.clear();
			client.Buffering = true;
			ServerTravel travel;
			if (const Replicated* replicated = m_World->GetEntities().TryGet<Replicated>(client.Player))
				travel.PlayerNetId = replicated->NetId;
			travel.Session = m_Session;
			travel.ServerTick = m_World->GetTick();
			Send(client, travel, SendMode::Reliable);
		}
		RDN_LOG("The run moved on to {}", GetMapDef(m_Session.Map).Name);
	}

	void GameServer::CheckReturn(float dt)
	{
		const GameStatus status = m_World->GetStatus();
		const bool runOver = status.Mode == GameMode::Shardfall && IsRunOver(status.Run.Phase);
		if (runOver && !m_RecapSent)
			SendRecap();
		if (!runOver && (!IsPvpMode(status.Mode) || status.Match != MatchPhase::Ended))
		{
			m_ReturnTimer = 0.0f;
			return;
		}
		m_ReturnTimer += dt;
		const bool everyone = std::none_of(m_Clients.begin(), m_Clients.end(), [](const Client& client) { return client.Joined && !client.Rematch; });
		if (!runOver && status.Mode == GameMode::Brawl && everyone && GetPlayerCount() > 0 && !m_PendingTravel)
			StartBrawl(m_BrawlChoice, m_Session.Rules);
		if (m_ReturnTimer >= (runOver ? RunEndSeconds : ReturnSeconds()) && !m_PendingTravel)
			Travel(GameMode::Hub, MapKind::Hub);
	}

	float GameServer::ReturnSeconds() const
	{
		const bool voted = std::any_of(m_Clients.begin(), m_Clients.end(), [](const Client& client) { return client.Joined && client.Rematch; });
		return MatchEndSeconds + (voted ? RematchWaitSeconds : 0.0f);
	}

	void GameServer::StartBrawl(MapKind choice, const MatchRules& rules)
	{
		m_BrawlChoice = choice;
		const MapKind map = choice == RandomBrawlMap ? BrawlMaps[m_BotRandom.Below(uint32_t(std::size(BrawlMaps)))] : choice;
		Travel(GameMode::Brawl, map, rules);
		RDN_LOG("Spellbrawl on {}", GetMapDef(map).Name);
	}

	void GameServer::OnRematch(Client& client, bool rematch)
	{
		if (m_Session.Mode != GameMode::Brawl || m_World->GetMatch().GetPhase() != MatchPhase::Ended || m_PendingTravel || client.Rematch == rematch)
			return;
		client.Rematch = rematch;
		RDN_LOG("{} {} a rematch", client.Name, rematch ? "wants" : "no longer wants");
		if (rematch && client.Host)
			StartBrawl(m_BrawlChoice, m_Session.Rules);
		BroadcastPlayerList();
	}

	void GameServer::SendRecap()
	{
		m_RecapSent = true;
		const RunDirector& run = m_World->GetRun();
		const RunStatus& status = run.GetStatus();
		ServerRunRecap recap{ .Victory = status.Phase == RunPhase::Victory, .Act = status.Act, .RunClock = status.RunClock, .Rooms = status.Rooms };
		for (const Client& client : m_Clients)
		{
			if (!client.Joined || client.PlayerId >= MaxPlayers)
				continue;
			const RunSeat& seat = run.GetSeat(client.PlayerId);
			RunRecapPlayer player{ .PlayerId = client.PlayerId, .Name = client.Name, .Kills = seat.Kills, .Downs = seat.Downs, .Revived = seat.Revived, .Loadout = seat.Loadout, .Picks = seat.Picks };
			for (size_t spell = 0; spell < seat.Damage.size(); spell++)
				if (seat.Damage[spell] > 0)
					player.Damage.push_back({ AbilityId(spell), seat.Damage[spell] });
			std::sort(player.Damage.begin(), player.Damage.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
			recap.Players.push_back(std::move(player));
		}
		for (const Client& client : m_Clients)
			if (client.Joined)
				Send(client, recap, SendMode::Reliable);
		RDN_LOG("Shardfall run {} in act {} after {} rooms and {:.0f} s", recap.Victory ? "won" : "lost", status.Act, status.Rooms, double(status.RunClock));
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
				stats.push_back({ client.Name, client.BufferTarget, uint32_t(client.PendingInputs.size()), m_Net.GetStats(client.Connection), client.SnapshotBytes, client.Snapshots });
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
		if (const uint32_t revision = m_World->GetRun().GetRevision(); m_World->IsShardfall() && revision != m_RunRevision)
		{
			m_RunRevision = revision;
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
		client.Spells = SanitizeSpells(hello.Spells);
		client.Host = client.Connection == m_LocalConnection;
		if (client.Host)
			client.BufferTarget = MinBufferTarget;
		client.Player = m_World->SpawnPlayer(client.PlayerId, client.Spells);

		ServerWelcome welcome;
		welcome.PlayerId = playerId;
		welcome.PlayerNetId = m_World->GetEntities().TryGet<Replicated>(client.Player)->NetId;
		welcome.Session = m_Session;
		welcome.TickRate = m_Config.TickRate;
		welcome.SnapshotInterval = m_Config.SnapshotInterval;
		welcome.ServerTick = m_World->GetTick();
		welcome.PropHash = GetPropLibraryHash();
		Send(client, welcome, SendMode::Reliable);
		SendChess(client);
		RDN_LOG("{} joined as player {}", client.Name, playerId + 1);
		FillBots();
		BroadcastPlayerList();
	}

	void GameServer::OnAction(Client& client, const ClientAction& action)
	{
		bool changed = false;
		switch (action.Action)
		{
		case ActionType::ReturnToHub:
			if (!m_World->IsHub() && (!m_World->IsPvp() || client.Host || m_World->GetMatch().GetPhase() == MatchPhase::Ended))
				Travel(GameMode::Hub, MapKind::Hub);
			break;
		case ActionType::StartMatch:
			if (m_World->IsHub())
			{
				const GameMode mode = GameMode(std::min<uint32_t>(action.Value, uint32_t(GameMode::Count)));
				const MapKind map = MapKind(std::min<uint32_t>(action.Target, uint32_t(MapKind::Count)));
				if (mode == GameMode::Brawl && (map == RandomBrawlMap || IsBrawlMap(map)))
				{
					StartBrawl(map, action.Rules);
					RDN_LOG("{} started a {} round {}spellbrawl", client.Name, action.Rules.Rounds, action.Rules.Teams >= 2 ? std::to_string(action.Rules.Teams) + " team " : std::string());
				}
			}
			break;
		case ActionType::Rematch:
			OnRematch(client, action.Value != 0);
			break;
		case ActionType::StartRun:
			if (m_World->IsHub())
			{
				const RunRules run = UnpackRunRules(action);
				Travel(GameMode::Shardfall, MapKind::ProvingHollow, {}, run);
				RDN_LOG("{} opened a Shardfall run{}", client.Name, run == RunRules{} ? "" : " with test settings");
			}
			break;
		case ActionType::Spectate:
			m_World->Spectate(client.Player, int16_t(action.Value));
			break;
		case ActionType::JoinMatch:
			if (m_World->JoinMatch(client.Player))
			{
				RDN_LOG("{} stopped spectating and joins the match", client.Name);
				BroadcastPlayerList();
			}
			break;
		case ActionType::BrawlSpell:
			m_World->PickBrawlSpell(client.Player, action.Value, AbilityId(std::min<uint32_t>(action.Target, uint32_t(AbilityId::Count))));
			break;
		case ActionType::BrawlCard:
			m_World->ChooseCard(client.Player, action.Value);
			break;
		case ActionType::RunPick:
			m_World->RunPick(client.Player, action.Value);
			break;
		case ActionType::RunReroll:
			m_World->RunReroll(client.Player);
			break;
		case ActionType::RunBanish:
			m_World->RunBanish(client.Player, action.Value);
			break;
		case ActionType::RunSkip:
			m_World->RunSkip(client.Player);
			break;
		case ActionType::RunPause:
			m_World->RunPause(client.Player, action.Value != 0);
			break;
		case ActionType::RunBuy:
			m_World->RunBuy(client.Player, action.Value);
			break;
		case ActionType::RunSell:
			m_World->RunSell(client.Player, action.Value);
			break;
		case ActionType::RunSellCard:
			m_World->RunSellCard(client.Player, action.Value);
			break;
		case ActionType::RunSwapSigils:
			m_World->RunSwapSigils(client.Player, action.Value, action.Target);
			break;
		case ActionType::RunShrine:
			m_World->RunShrine(client.Player, action.Value);
			break;
		case ActionType::RunSocket:
			m_World->RunSocket(client.Player, action.Value, action.Target >> 8, action.Target & 0xFFu);
			break;
		case ActionType::RunUnsocket:
			m_World->RunUnsocket(client.Player, action.Value);
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
		default:
			break;
		}
		if (changed)
		{
			HandleWorldEvents();
			FlushNotices();
		}
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
		for (const World::Award& award : m_World->TakeAwards())
			for (Client& client : m_Clients)
			{
				const AchievementMask bit = AchievementBit(award.Id);
				if (!client.Joined || client.PlayerId != award.PlayerId || (award.Id != Achievement::Scholar && (client.Awarded & bit) != 0))
					continue;
				client.Awarded |= bit;
				Send(client, ServerAchievement{ .Id = award.Id, .Detail = award.Detail }, SendMode::Reliable);
				if (award.Id != Achievement::Scholar)
					RDN_LOG("{} earned {}", client.Name, GetAchievement(award.Id).Name);
			}
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
		client.Encoder.Acknowledge(message.Acked);
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
		if (m_World->GetSettings().Map != m_Session.Map || m_World->GetSettings().Seed != m_Session.MapSeed)
			ApplyArenaChange();
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
			case WorldEventType::RoundStarted:
				RDN_LOG("Round {} of {} begins with {} players", event.Detail, m_World->GetMatch().GetBrawl().Rounds, m_World->GetMatch().GetEntrants());
				break;
			case WorldEventType::RoundEnded:
				if (event.PlayerId != NoPlayer)
					RDN_LOG("{} wins round {}", NameOf(event.PlayerId), event.Detail);
				else if (const uint8_t team = m_World->GetMatch().GetBrawl().RoundTeam; team < MaxTeams)
					RDN_LOG("The {} team wins round {}", GetTeamName(team), event.Detail);
				else
					RDN_LOG("Round {} ends without a winner", event.Detail);
				statsChanged = true;
				break;
			case WorldEventType::LootPicked:
				statsChanged = statsChanged || event.Detail != uint8_t(LootKind::Health);
				break;
			case WorldEventType::ChessChanged:
				for (const Client& watcher : m_Clients)
					if (watcher.Joined)
						SendChess(watcher);
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
		if (IsPvpMode(snapshot.Status.Mode) && snapshot.Status.Match == MatchPhase::Ended)
			snapshot.Status.MatchTime = std::max(ReturnSeconds() - m_ReturnTimer, 0.0f);
		snapshot.Hits = m_World->TakeHits();
		if (m_World->IsShardfall())
		{
			MergeHits(snapshot.Hits);
			snapshot.Orbs = m_World->GetRun().GetOrbs().GetOrbs();
		}
		EntityRegistry& entities = m_World->GetEntities();
		struct Visibility
		{
			uint32_t Player = NoPlayer;
		};
		std::vector<EntityState> states;
		std::vector<Visibility> visibility;
		entities.Each<Replicated>([&](Entity entity, const Replicated& replicated)
		{
			const Transform* transform = entities.TryGet<Transform>(entity);
			if (!transform)
				return;
			EntityState state{ .NetId = replicated.NetId, .Kind = replicated.Kind, .Position = transform->Position, .Facing = transform->Facing, .Owner = replicated.Owner, .Tag = replicated.Tag, .Variant = replicated.Variant, .Scale = replicated.Scale, .Charge = replicated.Charge };
			if (const Health* health = entities.TryGet<Health>(entity))
				state.Health = HealthFraction(*health);
			if (const PlayerController* controller = entities.TryGet<PlayerController>(entity))
			{
				if (!controller->State.Alive)
				{
					state.Flags |= uint16_t(EntityFlags::Dead);
					if (m_World->IsShardfall())
						state.Charge = PackCharge(m_World->GetRun().GetRevive(controller->PlayerId));
				}
				if (controller->State.Invulnerable > 0.0f || controller->State.DashDistance > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Dashing);
				if (controller->State.Chill > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Chilled);
				if (controller->State.Stun > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Stunned);
				if (controller->State.Static > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Static);
				if (IsWarded(controller->State))
				{
					state.Flags |= uint16_t(EntityFlags::Warded);
					state.Ward = PackGuard(controller->State.WardLeft, WardDurability(controller->Traits));
				}
				if (controller->State.WardShatter > 0.0f)
					state.Flags |= uint32_t(EntityFlags::WardShattered);
				if (m_World->GetMatch().IsBurning(controller->PlayerId) && m_Session.Mode == GameMode::Brawl)
					state.Flags |= uint16_t(EntityFlags::Burning);
				if (m_World->GetMatch().IsIgnited(controller->PlayerId))
					state.Flags |= uint16_t(EntityFlags::Ignited);
				if (m_World->GetMatch().GetMarkedBy(controller->PlayerId) != NoPlayer)
					state.Flags |= uint16_t(EntityFlags::Marked);
				if (m_World->GetMatch().GetBloodShield(controller->PlayerId) > 0)
					state.Flags |= uint32_t(EntityFlags::BloodShield);
				if (m_World->HasCrystalWard(controller->PlayerId))
					state.Flags |= uint16_t(EntityFlags::Barrier);
				if (controller->Traits.WeaveStacks > 0 && controller->State.Weave >= controller->Traits.WeaveStacks)
					state.Flags |= uint32_t(EntityFlags::Archmage);
				if (LengthSquared(controller->State.Push) > FlungSpeed * FlungSpeed)
					state.Flags |= uint16_t(EntityFlags::Flung);
				if (controller->State.IsFalling())
					state.Flags |= uint16_t(controller->State.Fall < FallSeconds ? EntityFlags::Falling : EntityFlags::Lifted);
				state.Variant = uint8_t(AbilityId::Count);
				if (controller->State.CastSlot >= 0)
				{
					const SkillTraits& skill = controller->Traits.Skills[size_t(controller->State.CastSlot)];
					state.Flags |= uint16_t(EntityFlags::Attacking);
					state.Variant = uint8_t(skill.Ability);
					if (IsCharged(skill))
						state.Charge = PackCharge(ChargePower(controller->State, skill));
					if (IsGuard(skill))
						state.Charge = PackGuard(GuardLeft(controller->State, controller->Traits), skill.Durability);
					if (AimsFromPoint(skill.Ability))
					{
						state.CastPoint = controller->State.CastPoint;
						state.CastAim = AngleOf(controller->State.CastAim);
					}
				}
				if (IsGuardBroken(controller->State, controller->Traits))
					state.Flags |= uint32_t(EntityFlags::Shattered);
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
				if (enemy->Burn > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Ignited);
				if (enemy->Mark > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Marked);
				if (enemy->Shock > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Static);
				if (enemy->Stun > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Stunned);
				if (enemy->Sprout > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Pending);
				if (enemy->Lunge > 0.0f)
					state.Flags |= uint16_t(EntityFlags::Dashing);
				if (enemy->Shielded)
					state.Flags |= uint16_t(EntityFlags::Barrier);
				if (const EnemyDef* def = FindEnemyDef(enemy->Type); def && def->Behavior == EnemyBehavior::Treasure && enemy->Age >= BurrowSeconds - BurrowWarning)
					state.Flags |= uint16_t(EntityFlags::Falling);
				if (enemy->Mirror > 0.0f)
					state.Flags |= uint32_t(EntityFlags::Mirror);
				if (enemy->Empowered > 0.0f)
					state.Flags |= uint32_t(EntityFlags::Empowered);
				if (enemy->Ward > 0.0f)
					state.Flags |= uint32_t(EntityFlags::Warded);
				if (enemy->Champion)
					state.Flags |= uint32_t(EntityFlags::Champion);
				if (enemy->Champion && enemy->Phase >= std::size(ChampionPhases))
					state.Flags |= uint32_t(EntityFlags::Enraged);
				if (LengthSquared(enemy->Push) > EnemyFlungSpeed * EnemyFlungSpeed)
					state.Flags |= uint32_t(EntityFlags::Flung);
				if (const FighterKit* kit = FindFighterKit(enemy->Type))
				{
					state.Variant = uint8_t(AbilityId::Count);
					if (kit->Shell > 0.0f && enemy->Cracked <= 0.0f)
						state.Flags |= uint32_t(EntityFlags::Shell);
					if (kit->Shell > 0.0f && enemy->Cracked > ShellRegrow - 0.4f)
						state.Flags |= uint32_t(EntityFlags::Shattered);
					if (enemy->DashLeft > 0.0f)
						state.Flags |= uint16_t(EntityFlags::Dashing);
					if (enemy->Stagger > 0.0f)
						state.Flags |= uint16_t(EntityFlags::Stunned) | uint32_t(EntityFlags::Staggered);
					if (enemy->GuardBroken > kit->GuardCooldown - 0.4f)
						state.Flags |= uint32_t(EntityFlags::Shattered);
					if (enemy->GuardUp)
					{
						state.Flags |= uint32_t(EntityFlags::Guard);
						state.Charge = PackGuard(enemy->GuardLeft, kit->Guard);
					}
					if (enemy->Move >= 0)
					{
						const FighterMove& move = kit->Moves[size_t(enemy->Move)];
						state.Flags |= uint16_t(EntityFlags::Attacking);
						state.Variant = uint8_t(move.Ability);
						state.Charge = PackCharge(enemy->Windup > 0.0f ? 1.0f - enemy->Windup / std::max(move.Windup, 0.01f) : 1.0f);
						state.CastPoint = enemy->AimPoint;
					}
				}
			}
			if (const PendingStrike* pending = entities.TryGet<PendingStrike>(entity))
			{
				state.Flags |= uint16_t(EntityFlags::Pending);
				state.Charge = PackLead(pending->Lead);
			}
			states.push_back(state);
			visibility.push_back({ replicated.Visibility });
		});
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
				snapshot.Entities.push_back(states[i]);
			}
			snapshot.LastProcessedInput = client.LastProcessedInput;
			snapshot.LocalPlayer = DescribeLocalPlayer(client);
			const std::vector<uint8_t> bytes = client.Encoder.Encode(snapshot, focus);
			client.SnapshotBytes += bytes.size();
			client.Snapshots++;
			m_Net.Send(client.Connection, bytes, SendMode::Unreliable);
		}
	}

	LocalPlayerState GameServer::DescribeLocalPlayer(const Client& client) const
	{
		LocalPlayerState local;
		const EntityRegistry& entities = m_World->GetEntities();
		const Transform* transform = entities.TryGet<Transform>(client.Player);
		const PlayerController* controller = entities.TryGet<PlayerController>(client.Player);
		const Health* health = entities.TryGet<Health>(client.Player);
		if (!transform || !controller || !health)
			return local;
		local.Valid = true;
		local.Motion = *transform;
		local.State = controller->State;
		local.Traits = controller->Traits;
		local.Health = health->Current;
		local.MaxHealth = health->Max;
		local.RespawnTime = controller->State.Alive ? 0.0f : controller->RespawnTimer;
		local.Watching = controller->Watching;
		local.KilledBy = controller->KilledBy;
		if (m_Session.Mode == GameMode::Brawl)
		{
			local.Brawl = m_World->GetMatch().GetLoadout(client.PlayerId);
			local.Shield = m_World->GetMatch().GetWardGauge(client.PlayerId);
			local.BloodShield = m_World->GetMatch().GetBloodShield(client.PlayerId);
		}
		if (m_Session.Mode == GameMode::Hub)
			local.Starlit = m_World->GetStarlit(client.PlayerId);
		if (m_Session.Mode == GameMode::Shardfall)
		{
			local.Run = m_World->GetRun().GetLoadout(client.PlayerId);
			local.Revive = PackCharge(m_World->GetRun().GetRevive(client.PlayerId));
		}
		return local;
	}

	void GameServer::BroadcastPlayerList()
	{
		m_PlayerListTimer = 0.0f;
		ServerPlayerList list;
		const bool pvp = m_World->IsPvp();
		const auto describe = [&](uint32_t playerId, const std::string& name, const Cosmetics& style)
		{
			const PlayerLook look{ .Style = style };
			const MatchScore& score = m_World->GetMatch().GetScore(playerId);
			return PlayerInfo{
				.PlayerId = playerId,
				.Name = name,
				.Kills = score.Kills,
				.Deaths = score.Deaths,
				.Assists = score.Assists,
				.Damage = score.Damage,
				.Look = look,
				.Team = m_World->GetTeam(playerId),
				.Placement = pvp ? score.Placement : uint8_t(0),
				.Score = m_World->GetMatch().GetPoints(playerId),
				.Rounds = m_World->GetMatch().GetRoundWins(playerId),
				.Cards = m_World->GetCards(playerId),
				.Choosing = m_Session.Mode == GameMode::Brawl ? m_World->GetMatch().GetLoadout(playerId).OfferPending : m_Session.Mode == GameMode::Shardfall && m_World->GetRun().GetLoadout(playerId).HasOffer(),
				.Spectator = m_World->GetMatch().IsSpectator(playerId),
			};
		};
		for (const Client& client : m_Clients)
		{
			if (!client.Joined)
				continue;
			PlayerInfo info = describe(client.PlayerId, client.Name, client.Style);
			if (!pvp)
			{
				info.Kills = client.Kills;
				info.Deaths = client.Deaths;
			}
			info.Ping = m_Net.GetPing(client.Connection);
			info.Host = client.Host;
			info.Honors = client.Honors;
			info.Rematch = client.Rematch;
			list.Players.push_back(info);
		}
		for (const Bot& bot : m_Bots)
		{
			PlayerInfo info = describe(bot.PlayerId, bot.Name, bot.Style);
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
		const uint32_t seats = std::min(m_Config.MaxPlayers, GetPlayerCap(m_Session.Mode));
		for (playerId = 0; playerId < seats; playerId++)
			if (std::none_of(m_Clients.begin(), m_Clients.end(), [&](const Client& client) { return client.Joined && client.PlayerId == playerId; }) && std::none_of(m_Bots.begin(), m_Bots.end(), [&](const Bot& bot) { return bot.PlayerId == playerId; }))
				return true;
		return false;
	}

	void GameServer::FillBots()
	{
		if (!m_World || m_Session.Mode != GameMode::Brawl || GetPlayerCount() == 0)
			return;
		const MatchRules& rules = m_Session.Rules;
		if (rules.Teams < 2)
		{
			while (m_Bots.size() < rules.Bots && SpawnBot(NoTeam))
				;
			return;
		}
		for (uint8_t team = 0; team < rules.Teams; team++)
		{
			uint32_t have = uint32_t(std::count_if(m_Bots.begin(), m_Bots.end(), [&](const Bot& bot) { return m_World->GetTeam(bot.PlayerId) == team; }));
			while (have < rules.TeamBots[team] && SpawnBot(team))
				have++;
		}
	}

	bool GameServer::SpawnBot(uint8_t team)
	{
		uint32_t playerId = 0;
		if (!AllocatePlayerId(playerId))
			return false;
		const BotSkill skill = BotSkill(std::min<uint32_t>(m_Session.Rules.BotLevel, uint32_t(BotSkill::Count) - 1));
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
		bot.Style.Cast = Focus(m_BotRandom.Below(uint32_t(Focus::Count)));
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
		m_World->SeatPlayer(playerId, team);
		m_World->SpawnPlayer(playerId);
		m_Squad.Add(playerId, BotProfile{ .Skill = skill, .Style = style, .Seed = m_BotRandom.Next() });
		m_Bots.push_back(bot);
		if (team != NoTeam)
			RDN_LOG("{} joined the {} team as a {} {} bot", bot.Name, GetTeamName(team), BotSkillNames[size_t(skill)], BotStyleNames[size_t(style)]);
		else
			RDN_LOG("{} joined as a {} {} bot", bot.Name, BotSkillNames[size_t(skill)], BotStyleNames[size_t(style)]);
		return true;
	}

	void GameServer::DropBot()
	{
		if (m_Bots.empty())
			return;
		auto leaving = std::prev(m_Bots.end());
		if (m_World && m_Session.Rules.Teams >= 2)
		{
			std::array<uint32_t, MaxTeams> members{};
			for (uint32_t id = 0; id < MaxPlayers; id++)
				if (const uint8_t team = m_World->GetTeam(id); team < MaxTeams)
					members[team]++;
			for (auto it = m_Bots.begin(); it != m_Bots.end(); ++it)
			{
				const uint8_t team = m_World->GetTeam(it->PlayerId);
				const uint8_t current = m_World->GetTeam(leaving->PlayerId);
				if (team < MaxTeams && (current >= MaxTeams || members[team] >= members[current]))
					leaving = it;
			}
		}
		const Bot bot = *leaving;
		m_Bots.erase(leaving);
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
