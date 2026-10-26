#include "Transport.h"
#include <RadianceEngine/Core/Log.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <algorithm>
#include <chrono>
#include <unordered_map>

namespace Game
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		struct RetiredSocket
		{
			NetRoute Route = NetRoute::Direct;
			HSteamListenSocket Socket = k_HSteamListenSocket_Invalid;
			uint16_t Port = 0;
			Clock::time_point CloseTime;
		};

		struct Link
		{
			NetRoute Route = NetRoute::Direct;
			HSteamNetConnection Handle = k_HSteamNetConnection_Invalid;
		};

		struct Relay
		{
			ISteamNetworkingSockets* Sockets = nullptr;
			ISteamNetworkingUtils* Utils = nullptr;
		};

		constexpr std::chrono::milliseconds RetiredSocketLifetime{ 1500 };
		constexpr int32_t ConnectTimeoutMs = 7000;
		constexpr int32_t RelayConnectTimeoutMs = 20000;
		constexpr int32_t SendRateBytes = 512 * 1024;
		constexpr int RelayPort = 0;

		int32_t s_References = 0;
		NetworkSimulation s_Simulation;
		Relay s_Relay;
		std::unordered_map<int64_t, NetEndpoint*> s_Endpoints;
		int64_t s_NextEndpointId = 1;
		std::vector<RetiredSocket> s_RetiredSockets;
		std::unordered_map<ConnectionId, Link> s_Links;
		std::unordered_map<uint64_t, ConnectionId> s_LinkIds;
		ConnectionId s_NextConnection = 1;

		ISteamNetworkingSockets* SocketsFor(NetRoute route)
		{
			return route == NetRoute::Relay ? s_Relay.Sockets : SteamNetworkingSockets();
		}

		uint64_t LinkKey(NetRoute route, HSteamNetConnection handle)
		{
			return (uint64_t(route) << 32) | handle;
		}

		ConnectionId Bind(NetRoute route, HSteamNetConnection handle)
		{
			const ConnectionId connection = s_NextConnection++;
			s_Links[connection] = { route, handle };
			s_LinkIds[LinkKey(route, handle)] = connection;
			return connection;
		}

		ConnectionId Lookup(NetRoute route, HSteamNetConnection handle)
		{
			const auto it = s_LinkIds.find(LinkKey(route, handle));
			return it != s_LinkIds.end() ? it->second : InvalidConnection;
		}

		const Link* Resolve(ConnectionId connection)
		{
			const auto it = s_Links.find(connection);
			return it != s_Links.end() && SocketsFor(it->second.Route) ? &it->second : nullptr;
		}

		void Release(ConnectionId connection, const char* reason, bool linger)
		{
			const auto it = s_Links.find(connection);
			if (it == s_Links.end())
				return;
			const Link link = it->second;
			s_Links.erase(it);
			s_LinkIds.erase(LinkKey(link.Route, link.Handle));
			if (ISteamNetworkingSockets* sockets = SocketsFor(link.Route))
				sockets->CloseConnection(link.Handle, 0, reason, linger);
		}

		void ForgetRoute(NetRoute route)
		{
			std::erase_if(s_Links, [&](const auto& entry)
			{
				if (entry.second.Route != route)
					return false;
				s_LinkIds.erase(LinkKey(route, entry.second.Handle));
				return true;
			});
		}

		void CloseRetiredSockets(auto&& shouldClose)
		{
			std::erase_if(s_RetiredSockets, [&](const RetiredSocket& retired)
			{
				if (!shouldClose(retired))
					return false;
				if (ISteamNetworkingSockets* sockets = SocketsFor(retired.Route))
					sockets->CloseListenSocket(retired.Socket);
				return true;
			});
		}

		void OnDebugOutput(ESteamNetworkingSocketsDebugOutputType type, const char* message)
		{
			if (type <= k_ESteamNetworkingSocketsDebugOutputType_Error)
				RDN_LOG_ERROR("Network: {}", message);
			else if (type == k_ESteamNetworkingSocketsDebugOutputType_Warning)
				RDN_LOG_WARNING("Network: {}", message);
			else
				RDN_LOG("Network: {}", message);
		}

		void Dispatch(NetRoute route, SteamNetConnectionStatusChangedCallback_t& info)
		{
			if (const auto it = s_Endpoints.find(info.m_info.m_nUserData); it != s_Endpoints.end())
				it->second->OnStatusChanged(route, info);
			else if (info.m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting)
				SocketsFor(route)->CloseConnection(info.m_hConn, 0, "The server is shutting down", false);
		}

		void OnDirectStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
		{
			Dispatch(NetRoute::Direct, *info);
		}

		void OnRelayStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
		{
			if (s_Relay.Sockets)
				Dispatch(NetRoute::Relay, *info);
		}

		void Configure(ISteamNetworkingUtils* utils, FnSteamNetConnectionStatusChanged callback, int32_t timeout)
		{
			utils->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Important, &OnDebugOutput);
			utils->SetGlobalConfigValuePtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, reinterpret_cast<void*>(callback));
			utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_TimeoutInitial, timeout);
			utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendRateMin, SendRateBytes);
			utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendRateMax, SendRateBytes);
		}

		void ApplySimulation(ISteamNetworkingUtils* utils)
		{
			utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, s_Simulation.LagMs);
			utils->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, s_Simulation.LossPercent);
		}

		int SendFlags(SendMode mode)
		{
			return mode == SendMode::Reliable ? k_nSteamNetworkingSend_ReliableNoNagle : k_nSteamNetworkingSend_UnreliableNoNagle;
		}

		void ReceiveAll(std::vector<NetMessage>& messages, auto&& receive, auto&& identify)
		{
			SteamNetworkingMessage_t* batch[64];
			while (true)
			{
				const int count = receive(batch, int(std::size(batch)));
				for (int i = 0; i < count; i++)
				{
					const uint8_t* data = static_cast<const uint8_t*>(batch[i]->m_pData);
					if (const ConnectionId connection = identify(batch[i]->m_conn); connection != InvalidConnection)
						messages.push_back({ connection, std::vector<uint8_t>(data, data + batch[i]->m_cbSize) });
					batch[i]->Release();
				}
				if (count < int(std::size(batch)))
					return;
			}
		}

		ConnectionStats StatsOf(ConnectionId connection)
		{
			const Link* link = Resolve(connection);
			SteamNetConnectionRealTimeStatus_t status;
			if (!link || SocketsFor(link->Route)->GetConnectionRealTimeStatus(link->Handle, &status, 0, nullptr) != k_EResultOK)
				return {};
			return { status.m_nPing, status.m_flConnectionQualityLocal, status.m_flConnectionQualityRemote, status.m_flInBytesPerSec, status.m_flOutBytesPerSec, link->Route == NetRoute::Relay };
		}

		bool IsTimeout(const SteamNetConnectionStatusChangedCallback_t& info)
		{
			return info.m_info.m_eEndReason == k_ESteamNetConnectionEnd_Misc_Timeout || info.m_info.m_eEndReason == k_ESteamNetConnectionEnd_Misc_P2P_Rendezvous;
		}
	}

	bool NetLibrary::Initialize()
	{
		if (s_References++ > 0)
			return true;
		SteamNetworkingErrMsg error;
		if (!GameNetworkingSockets_Init(nullptr, error))
		{
			RDN_LOG_ERROR("GameNetworkingSockets_Init failed: {}", error);
			s_References = 0;
			return false;
		}
		Configure(SteamNetworkingUtils(), &OnDirectStatusChanged, ConnectTimeoutMs);
		ApplySimulation(SteamNetworkingUtils());
		return true;
	}

	void NetLibrary::Shutdown()
	{
		if (s_References == 0 || --s_References > 0)
			return;
		CloseRetiredSockets([](const RetiredSocket& retired) { return retired.Route == NetRoute::Direct; });
		ForgetRoute(NetRoute::Direct);
		GameNetworkingSockets_Kill();
	}

	void NetLibrary::RunCallbacks()
	{
		if (s_References == 0)
			return;
		SteamNetworkingSockets()->RunCallbacks();
		if (s_Relay.Sockets)
			s_Relay.Sockets->RunCallbacks();
		if (!s_RetiredSockets.empty())
			CloseRetiredSockets([now = Clock::now()](const RetiredSocket& retired) { return retired.CloseTime <= now; });
	}

	void NetLibrary::SetSimulation(const NetworkSimulation& simulation)
	{
		s_Simulation = simulation;
		if (s_References > 0)
			ApplySimulation(SteamNetworkingUtils());
		if (s_Relay.Utils)
			ApplySimulation(s_Relay.Utils);
	}

	const NetworkSimulation& NetLibrary::GetSimulation()
	{
		return s_Simulation;
	}

	void NetLibrary::AttachRelay(ISteamNetworkingSockets* sockets, ISteamNetworkingUtils* utils)
	{
		if (!sockets || !utils)
			return;
		DetachRelay();
		s_Relay = { sockets, utils };
		Configure(utils, &OnRelayStatusChanged, RelayConnectTimeoutMs);
		ApplySimulation(utils);
		utils->InitRelayNetworkAccess();
	}

	void NetLibrary::DetachRelay()
	{
		if (!s_Relay.Sockets)
			return;
		CloseRetiredSockets([](const RetiredSocket& retired) { return retired.Route == NetRoute::Relay; });
		for (const auto& [connection, link] : s_Links)
			if (link.Route == NetRoute::Relay)
				s_Relay.Sockets->CloseConnection(link.Handle, 0, "Steam shut down", false);
		ForgetRoute(NetRoute::Relay);
		s_Relay.Utils->SetGlobalConfigValuePtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, nullptr);
		s_Relay = {};
	}

	bool NetLibrary::HasRelay()
	{
		return s_Relay.Sockets != nullptr;
	}

	NetEndpoint::NetEndpoint()
		: m_EndpointId(s_NextEndpointId++)
	{
		s_Endpoints.emplace(m_EndpointId, this);
	}

	NetEndpoint::~NetEndpoint()
	{
		s_Endpoints.erase(m_EndpointId);
	}

	NetServer::NetServer()
	{
		NetLibrary::Initialize();
	}

	NetServer::~NetServer()
	{
		Shutdown();
		NetLibrary::Shutdown();
	}

	bool NetServer::EnsurePollGroup(NetRoute route)
	{
		uint32_t& group = m_PollGroups[size_t(route)];
		if (group == 0)
			if (ISteamNetworkingSockets* sockets = SocketsFor(route))
				group = sockets->CreatePollGroup();
		return group != 0;
	}

	bool NetServer::Listen(uint16_t port)
	{
		if (!EnsurePollGroup(NetRoute::Direct))
			return false;
		CloseRetiredSockets([port](const RetiredSocket& retired) { return retired.Route == NetRoute::Direct && retired.Port == port; });
		SteamNetworkingIPAddr address;
		address.SetIPv4(0, port);
		SteamNetworkingConfigValue_t option;
		option.SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, GetEndpointId());
		const HSteamListenSocket socket = SteamNetworkingSockets()->CreateListenSocketIP(address, 1, &option);
		if (socket == k_HSteamListenSocket_Invalid)
		{
			RDN_LOG_ERROR("Can't listen on UDP port {}, it may be in use", port);
			return false;
		}
		m_ListenSockets[size_t(NetRoute::Direct)] = socket;
		m_Port = port;
		RDN_LOG("Listening on UDP port {}", port);
		return true;
	}

	bool NetServer::ListenRelay()
	{
		if (!NetLibrary::HasRelay() || !EnsurePollGroup(NetRoute::Relay))
			return false;
		SteamNetworkingConfigValue_t option;
		option.SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, GetEndpointId());
		const HSteamListenSocket socket = s_Relay.Sockets->CreateListenSocketP2P(RelayPort, 1, &option);
		if (socket == k_HSteamListenSocket_Invalid)
		{
			RDN_LOG_ERROR("Can't accept players through Steam");
			return false;
		}
		m_ListenSockets[size_t(NetRoute::Relay)] = socket;
		RDN_LOG("Accepting players through the Steam relay network");
		return true;
	}

	LocalConnection NetServer::OpenLocalConnection()
	{
		if (!EnsurePollGroup(NetRoute::Direct))
			return {};
		ISteamNetworkingSockets* sockets = SteamNetworkingSockets();
		HSteamNetConnection serverEnd = k_HSteamNetConnection_Invalid;
		HSteamNetConnection clientEnd = k_HSteamNetConnection_Invalid;
		if (!sockets->CreateSocketPair(&serverEnd, &clientEnd, s_Simulation.IsActive(), nullptr, nullptr))
		{
			RDN_LOG_ERROR("Can't create the local connection");
			return {};
		}
		sockets->SetConnectionUserData(serverEnd, GetEndpointId());
		sockets->SetConnectionPollGroup(serverEnd, m_PollGroups[size_t(NetRoute::Direct)]);
		const LocalConnection local{ Bind(NetRoute::Direct, serverEnd), Bind(NetRoute::Direct, clientEnd) };
		m_Connections.push_back(local.Server);
		m_Events.push_back({ NetEventType::Connected, local.Server, {} });
		return local;
	}

	void NetServer::Shutdown()
	{
		for (ConnectionId connection : m_Connections)
			Release(connection, "Server shut down", true);
		m_Connections.clear();
		for (size_t route = 0; route < size_t(NetRoute::Count); route++)
		{
			ISteamNetworkingSockets* sockets = SocketsFor(NetRoute(route));
			if (m_ListenSockets[route] != 0 && sockets)
				s_RetiredSockets.push_back({ NetRoute(route), m_ListenSockets[route], route == size_t(NetRoute::Direct) ? m_Port : uint16_t(0), Clock::now() + RetiredSocketLifetime });
			if (m_PollGroups[route] != 0 && sockets)
				sockets->DestroyPollGroup(m_PollGroups[route]);
		}
		m_ListenSockets.fill(0);
		m_PollGroups.fill(0);
		m_Events.clear();
	}

	void NetServer::Send(ConnectionId connection, std::span<const uint8_t> data, SendMode mode)
	{
		if (const Link* link = Resolve(connection))
			SocketsFor(link->Route)->SendMessageToConnection(link->Handle, data.data(), uint32_t(data.size()), SendFlags(mode), nullptr);
	}

	void NetServer::Close(ConnectionId connection, const char* reason)
	{
		const auto it = std::find(m_Connections.begin(), m_Connections.end(), connection);
		if (it == m_Connections.end())
			return;
		m_Connections.erase(it);
		Release(connection, reason, true);
	}

	void NetServer::Poll(std::vector<NetEvent>& events, std::vector<NetMessage>& messages)
	{
		events.insert(events.end(), std::make_move_iterator(m_Events.begin()), std::make_move_iterator(m_Events.end()));
		m_Events.clear();
		for (size_t route = 0; route < size_t(NetRoute::Count); route++)
		{
			ISteamNetworkingSockets* sockets = SocketsFor(NetRoute(route));
			if (m_PollGroups[route] == 0 || !sockets)
				continue;
			ReceiveAll(messages,
				[&](SteamNetworkingMessage_t** batch, int size) { return sockets->ReceiveMessagesOnPollGroup(m_PollGroups[route], batch, size); },
				[&](HSteamNetConnection handle) { return Lookup(NetRoute(route), handle); });
		}
	}

	int32_t NetServer::GetPing(ConnectionId connection) const
	{
		return StatsOf(connection).Ping;
	}

	ConnectionStats NetServer::GetStats(ConnectionId connection) const
	{
		return StatsOf(connection);
	}

	void NetServer::OnStatusChanged(NetRoute route, SteamNetConnectionStatusChangedCallback_t& info)
	{
		ISteamNetworkingSockets* sockets = SocketsFor(route);
		const ConnectionId connection = Lookup(route, info.m_hConn);
		const bool known = connection != InvalidConnection && std::find(m_Connections.begin(), m_Connections.end(), connection) != m_Connections.end();
		switch (info.m_info.m_eState)
		{
		case k_ESteamNetworkingConnectionState_Connecting:
			if (known || info.m_info.m_hListenSocket == k_HSteamListenSocket_Invalid)
				break;
			if (m_ListenSockets[size_t(route)] == 0 || info.m_info.m_hListenSocket != m_ListenSockets[size_t(route)])
			{
				sockets->CloseConnection(info.m_hConn, 0, "The server is shutting down", false);
				break;
			}
			if (sockets->AcceptConnection(info.m_hConn) != k_EResultOK || !sockets->SetConnectionPollGroup(info.m_hConn, m_PollGroups[size_t(route)]))
			{
				sockets->CloseConnection(info.m_hConn, 0, "Can't accept", false);
				break;
			}
			m_Connections.push_back(Bind(route, info.m_hConn));
			break;
		case k_ESteamNetworkingConnectionState_Connected:
			if (known)
				m_Events.push_back({ NetEventType::Connected, connection, {} });
			break;
		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			if (known)
			{
				std::erase(m_Connections, connection);
				m_Events.push_back({ NetEventType::Disconnected, connection, info.m_info.m_szEndDebug, IsTimeout(info) });
				Release(connection, nullptr, false);
			}
			else
				sockets->CloseConnection(info.m_hConn, 0, nullptr, false);
			break;
		default:
			break;
		}
	}

	NetClient::NetClient()
	{
		NetLibrary::Initialize();
	}

	NetClient::~NetClient()
	{
		Disconnect("Client closed");
		NetLibrary::Shutdown();
	}

	bool NetClient::Connect(const std::string& address, uint16_t defaultPort)
	{
		Disconnect("Reconnecting");
		SteamNetworkingIPAddr remote;
		remote.Clear();
		const std::string text = address == "localhost" ? "127.0.0.1" : address.starts_with("localhost:") ? "127.0.0.1" + address.substr(9) : address;
		if (!remote.ParseString(text.c_str()))
		{
			RDN_LOG_ERROR("\"{}\" is not a valid IP address", address);
			return false;
		}
		if (remote.m_port == 0)
			remote.m_port = defaultPort;

		SteamNetworkingConfigValue_t option;
		option.SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, GetEndpointId());
		const HSteamNetConnection handle = SteamNetworkingSockets()->ConnectByIPAddress(remote, 1, &option);
		if (handle == k_HSteamNetConnection_Invalid)
		{
			RDN_LOG_ERROR("Can't connect to {}", address);
			return false;
		}
		m_Connection = Bind(NetRoute::Direct, handle);
		char description[SteamNetworkingIPAddr::k_cchMaxString];
		remote.ToString(description, sizeof(description), true);
		RDN_LOG("Connecting to {}", description);
		return true;
	}

	bool NetClient::ConnectRelay(uint64_t steamId)
	{
		Disconnect("Reconnecting");
		if (!NetLibrary::HasRelay())
		{
			RDN_LOG_ERROR("Can't reach a host through Steam while Steam is unavailable");
			return false;
		}
		SteamNetworkingIdentity identity;
		identity.Clear();
		identity.SetSteamID64(steamId);
		SteamNetworkingConfigValue_t option;
		option.SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, GetEndpointId());
		const HSteamNetConnection handle = s_Relay.Sockets->ConnectP2P(identity, RelayPort, 1, &option);
		if (handle == k_HSteamNetConnection_Invalid)
		{
			RDN_LOG_ERROR("Can't connect to {} through Steam", steamId);
			return false;
		}
		m_Connection = Bind(NetRoute::Relay, handle);
		RDN_LOG("Connecting to {} through the Steam relay network", steamId);
		return true;
	}

	void NetClient::ConnectLocal(ConnectionId connection)
	{
		Disconnect("Reconnecting");
		const Link* link = Resolve(connection);
		if (!link)
			return;
		m_Connection = connection;
		m_Connected = true;
		SocketsFor(link->Route)->SetConnectionUserData(link->Handle, GetEndpointId());
		m_Events.push_back({ NetEventType::Connected, connection, {} });
	}

	void NetClient::Disconnect(const char* reason)
	{
		m_Events.clear();
		m_Messages.clear();
		if (m_Connection == InvalidConnection)
			return;
		Release(m_Connection, reason, true);
		m_Connection = InvalidConnection;
		m_Connected = false;
	}

	void NetClient::Send(std::span<const uint8_t> data, SendMode mode)
	{
		if (const Link* link = Resolve(m_Connection))
			SocketsFor(link->Route)->SendMessageToConnection(link->Handle, data.data(), uint32_t(data.size()), SendFlags(mode), nullptr);
	}

	void NetClient::Poll(std::vector<NetEvent>& events, std::vector<NetMessage>& messages)
	{
		events.insert(events.end(), std::make_move_iterator(m_Events.begin()), std::make_move_iterator(m_Events.end()));
		m_Events.clear();
		messages.insert(messages.end(), std::make_move_iterator(m_Messages.begin()), std::make_move_iterator(m_Messages.end()));
		m_Messages.clear();
		Receive(messages);
	}

	void NetClient::Receive(std::vector<NetMessage>& messages) const
	{
		const Link* link = Resolve(m_Connection);
		if (!link)
			return;
		ISteamNetworkingSockets* sockets = SocketsFor(link->Route);
		ReceiveAll(messages,
			[&](SteamNetworkingMessage_t** batch, int size) { return sockets->ReceiveMessagesOnConnection(link->Handle, batch, size); },
			[this](HSteamNetConnection) { return m_Connection; });
	}

	int32_t NetClient::GetPing() const
	{
		return GetStats().Ping;
	}

	ConnectionStats NetClient::GetStats() const
	{
		return m_Connected ? StatsOf(m_Connection) : ConnectionStats{};
	}

	void NetClient::OnStatusChanged(NetRoute route, SteamNetConnectionStatusChangedCallback_t& info)
	{
		if (m_Connection == InvalidConnection || Lookup(route, info.m_hConn) != m_Connection)
			return;
		switch (info.m_info.m_eState)
		{
		case k_ESteamNetworkingConnectionState_Connected:
			m_Connected = true;
			m_Events.push_back({ NetEventType::Connected, m_Connection, {} });
			break;
		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			Receive(m_Messages);
			m_Events.push_back({ NetEventType::Disconnected, m_Connection, info.m_info.m_szEndDebug[0] ? info.m_info.m_szEndDebug : "Connection lost", IsTimeout(info) });
			Release(m_Connection, nullptr, false);
			m_Connection = InvalidConnection;
			m_Connected = false;
			break;
		default:
			break;
		}
	}
}
