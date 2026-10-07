#pragma once
#include "GameShared/Net/Protocol.h"
#include "GameShared/Net/Transport.h"
#include "GameShared/Simulation/World.h"
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Game
{
	struct ServerConfig
	{
		uint16_t Port = DefaultPort;
		bool Listen = true;
		uint32_t TickRate = 60;
		uint32_t SnapshotInterval = 1;
		uint32_t MapSeed = 0;
		GameMode StartMode = GameMode::Hub;
		MapKind Map = MapKind::Town;
		uint8_t Tier = 1;
		ModifierMask Modifiers = 0;
		MatchRules Rules;
		uint32_t MaxPlayers = Game::MaxPlayers;
		float IntermissionSeconds = 6.0f;
		uint32_t FirstWave = 1;
		float LootRate = 1.0f;
		bool LagCompensation = true;
		bool UnlimitedMaterials = false;
		float ReturnDelay = 6.0f;
	};

	struct ServerPlayerStats
	{
		std::string Name;
		uint32_t InputBufferTarget = 0;
		uint32_t QueuedInputs = 0;
		ConnectionStats Connection;
	};

	class GameServer
	{
	public:
		explicit GameServer(const ServerConfig& config);
		~GameServer();
		GameServer(const GameServer&) = delete;
		GameServer& operator=(const GameServer&) = delete;

		bool Start();
		void Stop();
		void Update(float dt);
		ConnectionId OpenLocalConnection();

		bool IsRunning() const { return m_World != nullptr; }
		const ServerConfig& GetConfig() const { return m_Config; }
		const World* GetWorld() const { return m_World.get(); }
		World* GetWorld() { return m_World.get(); }
		const SessionInfo& GetSession() const { return m_Session; }
		void Travel(GameMode mode, MapKind map, uint8_t tier, ModifierMask modifiers, MatchRules rules = {});
		uint32_t GetPlayerCount() const;
		uint32_t GetStarvedTicks() const { return m_StarvedTicks; }
		float GetTickPhase() const { return m_Accumulator; }
		std::vector<ServerPlayerStats> GetPlayerStats() const;

	private:
		struct Client
		{
			ConnectionId Connection = InvalidConnection;
			bool Joined = false;
			float JoinTimer = 0.0f;
			uint32_t PlayerId = 0;
			std::string Name;
			Entity Player;
			std::deque<PlayerInput> PendingInputs;
			uint32_t LastReceivedInput = 0;
			uint32_t LastProcessedInput = 0;
			bool Buffering = true;
			uint32_t BufferTarget = 2;
			uint32_t Surplus = 0;
			float SinceStarved = 0.0f;
			uint32_t Kills = 0;
			uint32_t Deaths = 0;
			bool Host = false;
			std::optional<Inventory> Saved;
			PlayerTraits SentTraits;
			uint32_t TraitsResend = 0;
			uint32_t TraitsRefresh = 0;
		};

		void HandleEvent(const NetEvent& event);
		void HandleMessage(const NetMessage& message);
		void OnHello(Client& client, const ClientHello& hello);
		void OnInput(Client& client, const ClientInput& input);
		void OnAction(Client& client, const ClientAction& action);
		void SendInventory(const Client& client);
		void SendStash(const Client& client);
		void SendNotice(const Client& client, const std::string& text, bool success);
		Entity SpawnClient(Client& client, const Inventory& inventory);
		void CreateWorld(const SessionInfo& session);
		void ApplyTravel();
		void CheckReturn(float dt);
		void FlushNotices();
		void RemoveClient(ConnectionId connection);
		void DropSilentClients(float dt);
		void Tick();
		void ProcessInputs(Client& client);
		void HandleWorldEvents();
		void SendSnapshots();
		LocalPlayerState DescribeLocalPlayer(const Client& client) const;
		void BroadcastPlayerList();
		Client* FindClient(ConnectionId connection);
		Client* FindClientByPlayer(uint32_t playerId);
		bool AllocatePlayerId(uint32_t& playerId) const;

		template<typename T>
		void Send(const Client& client, const T& message, SendMode mode)
		{
			m_Net.Send(client.Connection, Encode(message), mode);
		}

		ServerConfig m_Config;
		NetServer m_Net;
		ConnectionId m_LocalConnection = InvalidConnection;
		std::unique_ptr<World> m_World;
		SessionInfo m_Session;
		std::optional<SessionInfo> m_PendingTravel;
		float m_ReturnTimer = 0.0f;
		uint32_t m_TravelCount = 0;
		std::vector<Client> m_Clients;
		float m_Accumulator = 0.0f;
		float m_PlayerListTimer = 0.0f;
		uint32_t m_StarvedTicks = 0;
		std::vector<NetEvent> m_Events;
		std::vector<NetMessage> m_Messages;
	};
}
