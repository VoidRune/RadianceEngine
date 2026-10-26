#pragma once
#include <GameShared/Content/Achievements.h>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Game
{
	enum class FriendActivity : uint8_t
	{
		Joinable,
		Playing,
		Online,
		Away,
	};

	struct SteamFriend
	{
		uint64_t Id = 0;
		std::string Name;
		FriendActivity Activity = FriendActivity::Online;
		uint64_t Lobby = 0;
	};

	struct LobbyJoin
	{
		uint64_t Host = 0;
		std::string HostName;
		uint32_t Ticket = 0;
	};

	struct Presence
	{
		std::string Token = "#Status_Menu";
		std::string Status = "In the main menu";
		std::string Map;
		uint32_t Round = 0;
		uint32_t Rounds = 0;
		uint32_t Players = 0;
	};

	class SteamPlatform
	{
	public:
		explicit SteamPlatform(bool enabled);
		~SteamPlatform();
		SteamPlatform(const SteamPlatform&) = delete;
		SteamPlatform& operator=(const SteamPlatform&) = delete;

		void Update(float dt);
		bool IsReady() const { return m_Ready; }
		const std::string& GetPersonaName() const { return m_Persona; }
		bool IsOverlayEnabled() const;

		uint32_t OpenLobby(bool friendsCanJoin);
		void CloseLobby(uint32_t ticket);
		bool HasLobby(uint32_t ticket) const { return ticket != 0 && ticket == m_LobbyTicket && m_Lobby != 0; }
		uint64_t GetLobbyId() const { return m_Lobby; }
		bool IsLobbyOpenToFriends() const { return m_Lobby != 0 && m_LobbyFriends; }
		void JoinLobby(uint64_t lobby);
		bool IsJoining() const { return m_JoiningLobby != 0; }
		std::optional<LobbyJoin> TakeJoin() { return std::exchange(m_Join, std::nullopt); }
		std::string TakeError() { return std::exchange(m_Error, {}); }

		const std::vector<SteamFriend>& GetFriends();
		bool InviteFriend(uint64_t friendId);
		bool ShowInviteOverlay();

		void SetPresence(const Presence& presence);

		void EnableAchievements(bool enabled) { m_AchievementsEnabled = enabled; }
		void Unlock(AchievementMask achievements);
		void SetProgress(Achievement id, uint32_t current);
		AchievementMask GetUnlocked() const { return m_Unlocked; }

	private:
		struct Listener;

		void LeaveLobby();
		void OnLobbyCreated(uint64_t lobby, bool succeeded);
		void OnLobbyEntered(uint64_t lobby, uint32_t response);
		void OnJoinRequested(const char* connect);
		void OnStatsReceived();
		void RefreshFriends();
		void ApplyPresence();
		void PushAchievements();

		bool m_Ready = false;
		std::unique_ptr<Listener> m_Listener;
		std::string m_Persona;
		uint64_t m_User = 0;
		uint32_t m_AppId = 0;
		uint64_t m_Lobby = 0;
		bool m_LobbyFriends = false;
		uint32_t m_LobbyTicket = 0;
		uint32_t m_CreatingTicket = 0;
		uint32_t m_NextTicket = 0;
		uint64_t m_JoiningLobby = 0;
		std::optional<LobbyJoin> m_Join;
		std::string m_Error;
		std::vector<SteamFriend> m_Friends;
		float m_FriendsAge = 1e9f;
		Presence m_Presence;
		std::array<std::string, 8> m_SentPresence;
		bool m_AchievementsEnabled = true;
		AchievementMask m_Unlocked = 0;
		AchievementMask m_Wanted = 0;
		AchievementMask m_Missing = 0;
		std::array<uint32_t, size_t(Achievement::Count)> m_Progress{};
		bool m_StoreDue = false;
		float m_StoreDelay = 0.0f;
	};
}
