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
			HSteamListenSocket Socket = k_HSteamListenSocket_Invalid;
			uint16_t Port = 0;
			Clock::time_point CloseTime;
		};

		constexpr std::chrono::milliseconds RetiredSocketLifetime{ 1500 };
		constexpr int32_t ConnectTimeoutMs = 7000;
		constexpr int32_t SendRateBytes = 512 * 1024;

		int32_t s_References = 0;
		NetworkSimulation s_Simulation;
		std::unordered_map<int64_t, NetEndpoint*> s_Endpoints;
		int64_t s_NextEndpointId = 1;
		std::vector<RetiredSocket> s_RetiredSockets;

		void CloseRetiredSockets(auto&& shouldClose)
		{
			std::erase_if(s_RetiredSockets, [&](const RetiredSocket& retired)
			{
				if (!shouldClose(retired))
					return false;
				SteamNetworkingSockets()->CloseListenSocket(retired.Socket);
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

		void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
		{
			if (const auto it = s_Endpoints.find(info->m_info.m_nUserData); it != s_Endpoints.end())
				it->second->OnStatusChanged(*info);
			else if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting)
				SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, "The server is shutting down", false);
		}

		void ApplySimulation()
		{
			ISteamNetworkingUtils* utils = SteamNetworkingUtils();
			utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, s_Simulation.LagMs);
			utils->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, s_Simulation.LossPercent);
		}

		int SendFlags(SendMode mode)
		{
			return mode == SendMode::Reliable ? k_nSteamNetworkingSend_ReliableNoNagle : k_nSteamNetworkingSend_UnreliableNoNagle;
		}

		void ReceiveAll(std::vector<NetMessage>& messages, auto&& receive)
		{
			SteamNetworkingMessage_t* batch[64];
			while (true)
			{
				const int count = receive(batch, int(std::size(batch)));
				for (int i = 0; i < count; i++)
				{
					const uint8_t* data = static_cast<const uint8_t*>(batch[i]->m_pData);
					messages.push_back({ batch[i]->m_conn, std::vector<uint8_t>(data, data + batch[i]->m_cbSize) });
					batch[i]->Release();
				}
				if (count < int(std::size(batch)))
					return;
			}
		}

		ConnectionStats StatsOf(ConnectionId connection)
		{
			SteamNetConnectionRealTimeStatus_t status;
			if (connection == InvalidConnection || SteamNetworkingSockets()->GetConnectionRealTimeStatus(connection, &status, 0, nullptr) != k_EResultOK)
				return {};
			return { status.m_nPing, status.m_flConnectionQualityLocal, status.m_flConnectionQualityRemote, status.m_flInBytesPerSec, status.m_flOutBytesPerSec };
		}

		bool IsTimeout(const SteamNetConnectionStatusChangedCallback_t& info)
		{
			return info.m_info.m_eEndReason == k_ESteamNetConnectionEnd_Misc_Timeout;
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
		SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Important, &OnDebugOutput);
		SteamNetworkingUtils()->SetGlobalConfigValuePtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, reinterpret_cast<void*>(&OnConnectionStatusChanged));
		SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_TimeoutInitial, ConnectTimeoutMs);
		SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendRateMin, SendRateBytes);
		SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendRateMax, SendRateBytes);
		ApplySimulation();
		return true;
	}

	void NetLibrary::Shutdown()
	{
		if (s_References == 0 || --s_References > 0)
			return;
		CloseRetiredSockets([](const RetiredSocket&) { return true; });
		GameNetworkingSockets_Kill();
	}

	void NetLibrary::RunCallbacks()
	{
		if (s_References == 0)
			return;
		SteamNetworkingSockets()->RunCallbacks();
		if (!s_RetiredSockets.empty())
			CloseRetiredSockets([now = Clock::now()](const RetiredSocket& retired) { return retired.CloseTime <= now; });
	}

	void NetLibrary::SetSimulation(const NetworkSimulation& simulation)
	{
		s_Simulation = simulation;
		if (s_References > 0)
			ApplySimulation();
	}

	const NetworkSimulation& NetLibrary::GetSimulation()
	{
		return s_Simulation;
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

	bool NetServer::EnsurePollGroup()
	{
		if (m_PollGroup == 0)
			m_PollGroup = SteamNetworkingSockets()->CreatePollGroup();
		return m_PollGroup != 0;
	}

	bool NetServer::Listen(uint16_t port)
	{
		if (!EnsurePollGroup())
			return false;
		CloseRetiredSockets([port](const RetiredSocket& retired) { return retired.Port == port; });
		SteamNetworkingIPAddr address;
		address.SetIPv4(0, port);
		SteamNetworkingConfigValue_t option;
		option.SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, GetEndpointId());
		m_ListenSocket = SteamNetworkingSockets()->CreateListenSocketIP(address, 1, &option);
		if (m_ListenSocket == k_HSteamListenSocket_Invalid)
		{
			m_ListenSocket = 0;
			RDN_LOG_ERROR("Can't listen on UDP port {}, it may be in use", port);
			return false;
		}
		m_Port = port;
		RDN_LOG("Listening on UDP port {}", port);
		return true;
	}

	LocalConnection NetServer::OpenLocalConnection()
	{
		if (!EnsurePollGroup())
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
		sockets->SetConnectionPollGroup(serverEnd, m_PollGroup);
		m_Connections.push_back(serverEnd);
		m_Events.push_back({ NetEventType::Connected, serverEnd, {} });
		return { serverEnd, clientEnd };
	}

	void NetServer::Shutdown()
	{
		ISteamNetworkingSockets* sockets = SteamNetworkingSockets();
		for (ConnectionId connection : m_Connections)
			sockets->CloseConnection(connection, 0, "Server shut down", true);
		m_Connections.clear();
		if (m_ListenSocket != 0)
			s_RetiredSockets.push_back({ m_ListenSocket, m_Port, Clock::now() + RetiredSocketLifetime });
		if (m_PollGroup != 0)
			sockets->DestroyPollGroup(m_PollGroup);
		m_ListenSocket = 0;
		m_PollGroup = 0;
		m_Events.clear();
	}

	void NetServer::Send(ConnectionId connection, std::span<const uint8_t> data, SendMode mode)
	{
		SteamNetworkingSockets()->SendMessageToConnection(connection, data.data(), uint32_t(data.size()), SendFlags(mode), nullptr);
	}

	void NetServer::Close(ConnectionId connection, const char* reason)
	{
		const auto it = std::find(m_Connections.begin(), m_Connections.end(), connection);
		if (it == m_Connections.end())
			return;
		m_Connections.erase(it);
		SteamNetworkingSockets()->CloseConnection(connection, 0, reason, true);
	}

	void NetServer::Poll(std::vector<NetEvent>& events, std::vector<NetMessage>& messages)
	{
		events.insert(events.end(), std::make_move_iterator(m_Events.begin()), std::make_move_iterator(m_Events.end()));
		m_Events.clear();
		if (m_PollGroup != 0)
			ReceiveAll(messages, [this](SteamNetworkingMessage_t** batch, int size) { return SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(m_PollGroup, batch, size); });
	}

	int32_t NetServer::GetPing(ConnectionId connection) const
	{
		return StatsOf(connection).Ping;
	}

	ConnectionStats NetServer::GetStats(ConnectionId connection) const
	{
		return StatsOf(connection);
	}

	void NetServer::OnStatusChanged(SteamNetConnectionStatusChangedCallback_t& info)
	{
		ISteamNetworkingSockets* sockets = SteamNetworkingSockets();
		const bool known = std::find(m_Connections.begin(), m_Connections.end(), info.m_hConn) != m_Connections.end();
		switch (info.m_info.m_eState)
		{
		case k_ESteamNetworkingConnectionState_Connecting:
			if (known || info.m_info.m_hListenSocket == k_HSteamListenSocket_Invalid)
				break;
			if (m_ListenSocket == 0 || info.m_info.m_hListenSocket != m_ListenSocket)
			{
				sockets->CloseConnection(info.m_hConn, 0, "The server is shutting down", false);
				break;
			}
			if (sockets->AcceptConnection(info.m_hConn) != k_EResultOK || !sockets->SetConnectionPollGroup(info.m_hConn, m_PollGroup))
			{
				sockets->CloseConnection(info.m_hConn, 0, "Can't accept", false);
				break;
			}
			m_Connections.push_back(info.m_hConn);
			break;
		case k_ESteamNetworkingConnectionState_Connected:
			if (known)
				m_Events.push_back({ NetEventType::Connected, info.m_hConn, {} });
			break;
		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			if (known)
			{
				std::erase(m_Connections, info.m_hConn);
				m_Events.push_back({ NetEventType::Disconnected, info.m_hConn, info.m_info.m_szEndDebug, IsTimeout(info) });
			}
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
		m_Connection = SteamNetworkingSockets()->ConnectByIPAddress(remote, 1, &option);
		if (m_Connection == k_HSteamNetConnection_Invalid)
		{
			m_Connection = InvalidConnection;
			RDN_LOG_ERROR("Can't connect to {}", address);
			return false;
		}
		char description[SteamNetworkingIPAddr::k_cchMaxString];
		remote.ToString(description, sizeof(description), true);
		RDN_LOG("Connecting to {}", description);
		return true;
	}

	void NetClient::ConnectLocal(ConnectionId connection)
	{
		Disconnect("Reconnecting");
		m_Connection = connection;
		m_Connected = true;
		SteamNetworkingSockets()->SetConnectionUserData(connection, GetEndpointId());
		m_Events.push_back({ NetEventType::Connected, connection, {} });
	}

	void NetClient::Disconnect(const char* reason)
	{
		m_Events.clear();
		m_Messages.clear();
		if (m_Connection == InvalidConnection)
			return;
		SteamNetworkingSockets()->CloseConnection(m_Connection, 0, reason, true);
		m_Connection = InvalidConnection;
		m_Connected = false;
	}

	void NetClient::Send(std::span<const uint8_t> data, SendMode mode)
	{
		if (m_Connection != InvalidConnection)
			SteamNetworkingSockets()->SendMessageToConnection(m_Connection, data.data(), uint32_t(data.size()), SendFlags(mode), nullptr);
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
		if (m_Connection != InvalidConnection)
			ReceiveAll(messages, [this](SteamNetworkingMessage_t** batch, int size) { return SteamNetworkingSockets()->ReceiveMessagesOnConnection(m_Connection, batch, size); });
	}

	int32_t NetClient::GetPing() const
	{
		return GetStats().Ping;
	}

	ConnectionStats NetClient::GetStats() const
	{
		return m_Connected ? StatsOf(m_Connection) : ConnectionStats{};
	}

	void NetClient::OnStatusChanged(SteamNetConnectionStatusChangedCallback_t& info)
	{
		if (info.m_hConn != m_Connection)
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
			SteamNetworkingSockets()->CloseConnection(m_Connection, 0, nullptr, false);
			m_Connection = InvalidConnection;
			m_Connected = false;
			break;
		default:
			break;
		}
	}
}
