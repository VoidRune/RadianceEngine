#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct SteamNetConnectionStatusChangedCallback_t;
class ISteamNetworkingSockets;
class ISteamNetworkingUtils;

namespace Game
{
	using ConnectionId = uint32_t;
	constexpr ConnectionId InvalidConnection = 0;

	enum class NetRoute : uint8_t
	{
		Direct,
		Relay,
		Count,
	};

	enum class SendMode
	{
		Reliable,
		Unreliable,
	};

	enum class NetEventType
	{
		Connected,
		Disconnected,
	};

	struct NetEvent
	{
		NetEventType Type = NetEventType::Connected;
		ConnectionId Connection = InvalidConnection;
		std::string Reason;
		bool TimedOut = false;
	};

	struct ConnectionStats
	{
		int32_t Ping = -1;
		float Quality = 0.0f;
		float RemoteQuality = 0.0f;
		float InBytesPerSecond = 0.0f;
		float OutBytesPerSecond = 0.0f;
		bool Relayed = false;
	};

	struct NetMessage
	{
		ConnectionId Connection = InvalidConnection;
		std::vector<uint8_t> Data;
	};

	struct LocalConnection
	{
		ConnectionId Server = InvalidConnection;
		ConnectionId Client = InvalidConnection;
	};

	struct NetworkSimulation
	{
		int32_t LagMs = 0;
		float LossPercent = 0.0f;

		bool IsActive() const { return LagMs > 0 || LossPercent > 0.0f; }
	};

	class NetLibrary
	{
	public:
		static bool Initialize();
		static void Shutdown();
		static void RunCallbacks();
		static void SetSimulation(const NetworkSimulation& simulation);
		static const NetworkSimulation& GetSimulation();
		static void AttachRelay(ISteamNetworkingSockets* sockets, ISteamNetworkingUtils* utils);
		static void DetachRelay();
		static bool HasRelay();
	};

	class NetEndpoint
	{
	public:
		NetEndpoint();
		virtual ~NetEndpoint();
		NetEndpoint(const NetEndpoint&) = delete;
		NetEndpoint& operator=(const NetEndpoint&) = delete;

		int64_t GetEndpointId() const { return m_EndpointId; }
		virtual void OnStatusChanged(NetRoute route, SteamNetConnectionStatusChangedCallback_t& info) = 0;

	private:
		int64_t m_EndpointId = 0;
	};

	class NetServer final : public NetEndpoint
	{
	public:
		NetServer();
		~NetServer() override;

		bool Listen(uint16_t port);
		bool ListenRelay();
		LocalConnection OpenLocalConnection();
		void Shutdown();

		void Send(ConnectionId connection, std::span<const uint8_t> data, SendMode mode);
		void Close(ConnectionId connection, const char* reason);
		void Poll(std::vector<NetEvent>& events, std::vector<NetMessage>& messages);

		bool IsListening() const { return m_ListenSockets[size_t(NetRoute::Direct)] != 0; }
		bool IsRelayListening() const { return m_ListenSockets[size_t(NetRoute::Relay)] != 0; }
		uint16_t GetPort() const { return m_Port; }
		int32_t GetPing(ConnectionId connection) const;
		ConnectionStats GetStats(ConnectionId connection) const;

		void OnStatusChanged(NetRoute route, SteamNetConnectionStatusChangedCallback_t& info) override;

	private:
		bool EnsurePollGroup(NetRoute route);

		std::array<uint32_t, size_t(NetRoute::Count)> m_ListenSockets{};
		std::array<uint32_t, size_t(NetRoute::Count)> m_PollGroups{};
		uint16_t m_Port = 0;
		std::vector<ConnectionId> m_Connections;
		std::vector<NetEvent> m_Events;
	};

	class NetClient final : public NetEndpoint
	{
	public:
		NetClient();
		~NetClient() override;

		bool Connect(const std::string& address, uint16_t defaultPort);
		bool ConnectRelay(uint64_t steamId);
		void ConnectLocal(ConnectionId connection);
		void Disconnect(const char* reason);

		void Send(std::span<const uint8_t> data, SendMode mode);
		void Poll(std::vector<NetEvent>& events, std::vector<NetMessage>& messages);

		bool IsConnected() const { return m_Connected; }
		int32_t GetPing() const;
		ConnectionStats GetStats() const;

		void OnStatusChanged(NetRoute route, SteamNetConnectionStatusChangedCallback_t& info) override;

	private:
		void Receive(std::vector<NetMessage>& messages) const;

		ConnectionId m_Connection = InvalidConnection;
		bool m_Connected = false;
		std::vector<NetEvent> m_Events;
		std::vector<NetMessage> m_Messages;
	};
}
