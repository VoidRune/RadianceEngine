#include "SteamPlatform.h"
#include <GameShared/Net/Protocol.h>
#include <GameShared/Net/Transport.h>
#include <RadianceEngine/Core/Log.h>
#pragma warning(push, 0)
#pragma warning(disable : 4996)
#include <steam/steam_api.h>
#pragma warning(pop)
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <string_view>

namespace Game
{
	namespace
	{
		constexpr const char* PresenceKeys[] = { "steam_display", "status", "map", "round", "rounds", "connect", "steam_player_group", "steam_player_group_size" };
		constexpr std::string_view ConnectPrefix = "+connect_lobby ";
		constexpr float FriendsRefreshSeconds = 1.0f;
		constexpr float StoreSeconds = 1.0f;
		constexpr uint32_t SkydiverStep = 5;

		const char* DescribeEnterFailure(uint32_t response)
		{
			switch (response)
			{
			case k_EChatRoomEnterResponseDoesntExist:
				return "That game has ended";
			case k_EChatRoomEnterResponseFull:
				return "That game is full";
			case k_EChatRoomEnterResponseLimited:
				return "Limited Steam accounts can't join games";
			case k_EChatRoomEnterResponseNotAllowed:
			case k_EChatRoomEnterResponseBanned:
			case k_EChatRoomEnterResponseMemberBlockedYou:
			case k_EChatRoomEnterResponseYouBlockedMember:
				return "You can't join that game";
			default:
				return "Steam couldn't join that game";
			}
		}

		bool NameBefore(const std::string& a, const std::string& b)
		{
			return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) { return std::tolower(uint8_t(x)) < std::tolower(uint8_t(y)); });
		}
	}

	struct SteamPlatform::Listener
	{
		explicit Listener(SteamPlatform* owner)
			: Owner(owner)
		{
		}

		void OnCreated(LobbyCreated_t* created, bool failure)
		{
			Owner->OnLobbyCreated(failure ? 0 : created->m_ulSteamIDLobby, !failure && created->m_eResult == k_EResultOK);
		}

		SteamPlatform* Owner = nullptr;
		CCallResult<Listener, LobbyCreated_t> Created;

		STEAM_CALLBACK(Listener, OnEntered, LobbyEnter_t)
		{
			Owner->OnLobbyEntered(pParam->m_ulSteamIDLobby, pParam->m_EChatRoomEnterResponse);
		}

		STEAM_CALLBACK(Listener, OnLobbyRequest, GameLobbyJoinRequested_t)
		{
			Owner->JoinLobby(pParam->m_steamIDLobby.ConvertToUint64());
		}

		STEAM_CALLBACK(Listener, OnPresenceRequest, GameRichPresenceJoinRequested_t)
		{
			Owner->OnJoinRequested(pParam->m_rgchConnect);
		}

		STEAM_CALLBACK(Listener, OnStats, UserStatsReceived_t)
		{
			if (pParam->m_eResult == k_EResultOK && pParam->m_steamIDUser.ConvertToUint64() == Owner->m_User)
				Owner->OnStatsReceived();
		}
	};

	SteamPlatform::SteamPlatform(bool enabled)
	{
		if (!enabled)
		{
			RDN_LOG("Steam is off for this run");
			return;
		}
		SteamErrMsg error{};
		if (SteamAPI_InitEx(&error) != k_ESteamAPIInitResult_OK)
		{
			RDN_LOG_WARNING("Steam is unavailable ({}), so friends join with your address", static_cast<const char*>(error));
			return;
		}
		m_Ready = true;
		m_Listener = std::make_unique<Listener>(this);
		m_Persona = SteamFriends()->GetPersonaName();
		m_User = SteamUser()->GetSteamID().ConvertToUint64();
		m_AppId = SteamUtils()->GetAppID();
		NetLibrary::AttachRelay(SteamNetworkingSockets_SteamAPI(), SteamNetworkingUtils_SteamAPI());
		OnStatsReceived();
		ApplyPresence();
		RDN_LOG("Steam is ready: signed in as {} (app {})", m_Persona, m_AppId);
	}

	SteamPlatform::~SteamPlatform()
	{
		if (!m_Ready)
			return;
		LeaveLobby();
		if (m_StoreDue)
			SteamUserStats()->StoreStats();
		SteamFriends()->ClearRichPresence();
		NetLibrary::DetachRelay();
		m_Listener.reset();
		SteamAPI_Shutdown();
	}

	void SteamPlatform::Update(float dt)
	{
		if (!m_Ready)
			return;
		SteamAPI_RunCallbacks();
		m_FriendsAge += dt;
		m_StoreDelay = std::max(m_StoreDelay - dt, 0.0f);
		if (m_StoreDue && m_StoreDelay <= 0.0f)
		{
			m_StoreDue = false;
			m_StoreDelay = StoreSeconds;
			SteamUserStats()->StoreStats();
		}
	}

	bool SteamPlatform::IsOverlayEnabled() const
	{
		return m_Ready && SteamUtils()->IsOverlayEnabled();
	}

	uint32_t SteamPlatform::OpenLobby(bool friendsCanJoin)
	{
		if (!m_Ready)
			return 0;
		LeaveLobby();
		m_LobbyFriends = friendsCanJoin;
		m_CreatingTicket = ++m_NextTicket;
		if (!m_Listener->Created.IsActive())
			m_Listener->Created.Set(SteamMatchmaking()->CreateLobby(friendsCanJoin ? k_ELobbyTypeFriendsOnly : k_ELobbyTypePrivate, int(MaxPlayers)), m_Listener.get(), &Listener::OnCreated);
		return m_CreatingTicket;
	}

	void SteamPlatform::CloseLobby(uint32_t ticket)
	{
		if (!m_Ready || ticket == 0)
			return;
		if (ticket == m_CreatingTicket)
			m_CreatingTicket = 0;
		if (ticket != m_LobbyTicket)
			return;
		LeaveLobby();
		ApplyPresence();
	}

	void SteamPlatform::LeaveLobby()
	{
		if (m_Lobby != 0)
		{
			SteamMatchmaking()->LeaveLobby(CSteamID(m_Lobby));
			RDN_LOG("Left Steam lobby {}", m_Lobby);
		}
		m_Lobby = 0;
		m_LobbyTicket = 0;
		m_CreatingTicket = 0;
	}

	void SteamPlatform::OnLobbyCreated(uint64_t lobby, bool succeeded)
	{
		const uint32_t ticket = std::exchange(m_CreatingTicket, 0);
		if (!succeeded)
		{
			if (ticket != 0)
				RDN_LOG_WARNING("Steam couldn't open a lobby, so friends can only join with your address");
			return;
		}
		ISteamMatchmaking* matchmaking = SteamMatchmaking();
		const CSteamID id(lobby);
		if (ticket == 0)
		{
			matchmaking->LeaveLobby(id);
			return;
		}
		m_Lobby = lobby;
		m_LobbyTicket = ticket;
		matchmaking->SetLobbyType(id, m_LobbyFriends ? k_ELobbyTypeFriendsOnly : k_ELobbyTypePrivate);
		matchmaking->SetLobbyData(id, "version", std::to_string(ProtocolVersion).c_str());
		matchmaking->SetLobbyData(id, "host", std::to_string(m_User).c_str());
		matchmaking->SetLobbyData(id, "access", m_LobbyFriends ? "friends" : "invite");
		RDN_LOG("Opened Steam lobby {} ({})", lobby, m_LobbyFriends ? "friends can join" : "invite only");
		ApplyPresence();
	}

	void SteamPlatform::JoinLobby(uint64_t lobby)
	{
		if (!m_Ready || lobby == 0 || lobby == m_Lobby || lobby == m_JoiningLobby)
			return;
		m_JoiningLobby = lobby;
		SteamMatchmaking()->JoinLobby(CSteamID(lobby));
		RDN_LOG("Joining Steam lobby {}", lobby);
	}

	void SteamPlatform::OnLobbyEntered(uint64_t lobby, uint32_t response)
	{
		if (lobby == 0 || lobby != m_JoiningLobby)
			return;
		m_JoiningLobby = 0;
		if (response != k_EChatRoomEnterResponseSuccess)
		{
			m_Error = DescribeEnterFailure(response);
			return;
		}
		ISteamMatchmaking* matchmaking = SteamMatchmaking();
		const CSteamID id(lobby);
		const uint64_t host = std::strtoull(matchmaking->GetLobbyData(id, "host"), nullptr, 10);
		const char* problem = nullptr;
		if (std::string_view(matchmaking->GetLobbyData(id, "version")) != std::to_string(ProtocolVersion))
			problem = "Your friend is playing a different version of the game";
		else if (host == m_User)
			problem = "That's your own game";
		else if (host == 0 || matchmaking->GetLobbyOwner(id).ConvertToUint64() != host)
			problem = "That game has ended";
		if (problem)
		{
			matchmaking->LeaveLobby(id);
			m_Error = problem;
			return;
		}
		LeaveLobby();
		m_Lobby = lobby;
		m_LobbyTicket = ++m_NextTicket;
		m_LobbyFriends = std::string_view(matchmaking->GetLobbyData(id, "access")) == "friends";
		m_Join = LobbyJoin{ host, SteamFriends()->GetFriendPersonaName(CSteamID(host)), m_LobbyTicket };
		RDN_LOG("Entered Steam lobby {}, hosted by {}", lobby, m_Join->HostName);
		ApplyPresence();
	}

	void SteamPlatform::OnJoinRequested(const char* connect)
	{
		const std::string_view text(connect);
		const size_t at = text.find(ConnectPrefix);
		if (at != std::string_view::npos)
			JoinLobby(std::strtoull(connect + at + ConnectPrefix.size(), nullptr, 10));
	}

	const std::vector<SteamFriend>& SteamPlatform::GetFriends()
	{
		if (m_Ready && m_FriendsAge >= FriendsRefreshSeconds)
		{
			m_FriendsAge = 0.0f;
			RefreshFriends();
		}
		return m_Friends;
	}

	void SteamPlatform::RefreshFriends()
	{
		m_Friends.clear();
		ISteamFriends* friends = SteamFriends();
		const int count = friends->GetFriendCount(k_EFriendFlagImmediate);
		for (int i = 0; i < count; i++)
		{
			const CSteamID id = friends->GetFriendByIndex(i, k_EFriendFlagImmediate);
			const EPersonaState state = friends->GetFriendPersonaState(id);
			if (state == k_EPersonaStateOffline || state == k_EPersonaStateInvisible)
				continue;
			SteamFriend entry{ id.ConvertToUint64(), friends->GetFriendPersonaName(id) };
			FriendGameInfo_t game{};
			if (friends->GetFriendGamePlayed(id, &game) && game.m_gameID.AppID() == m_AppId)
			{
				entry.Lobby = game.m_steamIDLobby.IsValid() ? game.m_steamIDLobby.ConvertToUint64() : 0;
				entry.Activity = entry.Lobby != 0 ? FriendActivity::Joinable : FriendActivity::Playing;
			}
			else
				entry.Activity = state == k_EPersonaStateAway || state == k_EPersonaStateSnooze || state == k_EPersonaStateBusy ? FriendActivity::Away : FriendActivity::Online;
			m_Friends.push_back(std::move(entry));
		}
		std::sort(m_Friends.begin(), m_Friends.end(), [](const SteamFriend& a, const SteamFriend& b) { return a.Activity != b.Activity ? a.Activity < b.Activity : NameBefore(a.Name, b.Name); });
	}

	bool SteamPlatform::InviteFriend(uint64_t friendId)
	{
		if (!m_Ready || m_Lobby == 0)
			return false;
		const bool sent = SteamMatchmaking()->InviteUserToLobby(CSteamID(m_Lobby), CSteamID(friendId));
		RDN_LOG("{} a Steam invite to {}", sent ? "Sent" : "Couldn't send", friendId);
		return sent;
	}

	bool SteamPlatform::ShowInviteOverlay()
	{
		if (!m_Ready || m_Lobby == 0 || !SteamUtils()->IsOverlayEnabled())
			return false;
		SteamFriends()->ActivateGameOverlayInviteDialog(CSteamID(m_Lobby));
		return true;
	}

	void SteamPlatform::SetPresence(const Presence& presence)
	{
		if (!m_Ready)
			return;
		m_Presence = presence;
		ApplyPresence();
	}

	void SteamPlatform::ApplyPresence()
	{
		if (!m_Ready)
			return;
		const std::string lobby = m_Lobby != 0 ? std::to_string(m_Lobby) : std::string();
		const std::string values[] = {
			m_Presence.Token,
			m_Presence.Status,
			m_Presence.Map,
			m_Presence.Round > 0 ? std::to_string(m_Presence.Round) : std::string(),
			m_Presence.Rounds > 0 ? std::to_string(m_Presence.Rounds) : std::string(),
			m_Lobby != 0 && m_LobbyFriends ? std::string(ConnectPrefix) + lobby : std::string(),
			lobby,
			m_Lobby != 0 && m_Presence.Players > 0 ? std::to_string(m_Presence.Players) : std::string(),
		};
		static_assert(std::size(values) == std::size(PresenceKeys));
		for (size_t i = 0; i < std::size(PresenceKeys); i++)
			if (m_SentPresence[i] != values[i])
			{
				m_SentPresence[i] = values[i];
				SteamFriends()->SetRichPresence(PresenceKeys[i], values[i].c_str());
			}
	}

	void SteamPlatform::Unlock(AchievementMask achievements)
	{
		if (!m_Ready || !m_AchievementsEnabled)
			return;
		m_Wanted |= achievements & AllAchievements;
		PushAchievements();
	}

	void SteamPlatform::SetProgress(Achievement id, uint32_t current)
	{
		const AchievementDef& def = GetAchievement(id);
		uint32_t& known = m_Progress[size_t(id)];
		current = std::min(current, def.Goal);
		if (!m_Ready || !m_AchievementsEnabled || !def.Stat || current <= known)
			return;
		const uint32_t before = std::exchange(known, current);
		ISteamUserStats* stats = SteamUserStats();
		if (!stats->SetStat(def.Stat, int32_t(current)))
			return;
		m_StoreDue = true;
		const bool milestone = id != Achievement::Skydiver || current / SkydiverStep != before / SkydiverStep;
		if (before > 0 && milestone && current < def.Goal && (m_Unlocked & AchievementBit(id)) == 0)
			stats->IndicateAchievementProgress(def.Key, current, def.Goal);
	}

	void SteamPlatform::OnStatsReceived()
	{
		ISteamUserStats* stats = SteamUserStats();
		for (const AchievementDef& def : Achievements)
			if (bool achieved = false; stats->GetAchievement(def.Key, &achieved) && achieved)
				m_Unlocked |= AchievementBit(def.Id);
		PushAchievements();
	}

	void SteamPlatform::PushAchievements()
	{
		if (!m_AchievementsEnabled)
			return;
		ISteamUserStats* stats = SteamUserStats();
		for (const AchievementDef& def : Achievements)
		{
			const AchievementMask bit = AchievementBit(def.Id);
			if ((m_Wanted & bit) == 0 || (m_Unlocked & bit) != 0)
				continue;
			if (stats->SetAchievement(def.Key))
			{
				m_Unlocked |= bit;
				m_StoreDue = true;
				RDN_LOG("Unlocked the Steam achievement {}", def.Key);
			}
			else if ((m_Missing & bit) == 0)
			{
				m_Missing |= bit;
				RDN_LOG_WARNING("Steam app {} has no achievement {} yet", m_AppId, def.Key);
			}
		}
	}
}
