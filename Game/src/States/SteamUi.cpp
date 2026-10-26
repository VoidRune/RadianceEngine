#include "SessionState.h"
#include "App/GameApp.h"
#include "Audio/GameAudio.h"
#include "UI/Ui.h"
#include <RadianceEngine/Window/Input.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <iterator>

namespace Game
{
	namespace
	{
		constexpr float ToastSeconds = 5.5f;
		constexpr size_t MaxToasts = 3;
		constexpr float ToastWidth = 380.0f;
		constexpr float ToastHeight = 78.0f;
		constexpr float InviteSeconds = 8.0f;
		constexpr float FriendRow = 48.0f;

		constexpr UiIcon AchievementIcons[] = {
			UiIcon::BurningEmbers, UiIcon::PointyHat, UiIcon::UnstableOrb, UiIcon::EnergyShield, UiIcon::SpearHook, UiIcon::Cycle, UiIcon::Meditation,
			UiIcon::BleedingHeart, UiIcon::Laurels, UiIcon::Upgrade, UiIcon::ScrollUnfurled, UiIcon::Dodge, UiIcon::StarSwirl, UiIcon::Eye,
		};
		static_assert(std::size(AchievementIcons) == size_t(Achievement::Count));

		const char* DescribeActivity(FriendActivity activity)
		{
			switch (activity)
			{
			case FriendActivity::Joinable:
				return "Playing, in a party";
			case FriendActivity::Playing:
				return "Playing";
			case FriendActivity::Away:
				return "Away";
			default:
				return "Online";
			}
		}

		bool IsHonor(Achievement id)
		{
			return std::find(std::begin(HonorAchievements), std::end(HonorAchievements), id) != std::end(HonorAchievements);
		}
	}

	void SessionState::UpdateSteam(float dt)
	{
		SteamPlatform& steam = m_App.GetSteam();
		for (const Achievement id : m_Client.TakeAchievements())
		{
			steam.Unlock(AchievementBit(id));
			m_Toasts.push_back({ id });
			m_App.GetGameAudio().PlayUi(Cue::UiRare, 0.85f);
		}
		for (size_t i = 0; i < m_Toasts.size() && i < MaxToasts; i++)
			m_Toasts[i].Age += dt;
		std::erase_if(m_Toasts, [](const Toast& toast) { return toast.Age >= ToastSeconds; });
		for (auto& [id, remaining] : m_Invited)
			remaining -= dt;
		std::erase_if(m_Invited, [](const auto& invite) { return invite.second <= 0.0f; });
		const CharacterProfile& profile = m_Client.GetProfile();
		steam.SetProgress(Achievement::Skydiver, profile.Falls);
		steam.SetProgress(Achievement::Scholar, ProgressOf(Achievement::Scholar, profile.Falls, profile.Scholar));
		if (const std::string error = steam.TakeError(); !error.empty())
			m_Notices.push_back({ error, Theme::Danger });
		steam.SetPresence(DescribePresence());
	}

	Presence SessionState::DescribePresence() const
	{
		Presence presence;
		for (const PlayerInfo& player : m_Client.GetPlayers())
			presence.Players += player.Bot ? 0u : 1u;
		if (m_Client.GetState() != ClientState::InGame)
		{
			presence.Token = "#Status_Joining";
			presence.Status = "Joining a game";
			return presence;
		}
		const SessionInfo& session = m_Client.GetSession();
		const GameStatus& status = m_Client.GetStatus();
		const std::string map = GetMapDef(session.Map).Name;
		switch (session.Mode)
		{
		case GameMode::Hub:
			presence.Token = "#Status_Hub";
			presence.Status = "In Emberhold";
			break;
		case GameMode::Brawl:
			presence.Map = map;
			if (m_Client.IsSpectating())
			{
				presence.Token = "#Status_Watching";
				presence.Status = "Watching a Spellbrawl on " + map;
			}
			else if (status.Match == MatchPhase::Ended)
			{
				presence.Token = "#Status_Results";
				presence.Status = "Spellbrawl results on " + map;
			}
			else if (status.Brawl.Phase == BrawlPhase::Draft)
			{
				presence.Token = "#Status_Draft";
				presence.Status = "Picking spells for " + map;
			}
			else
			{
				presence.Token = "#Status_Brawl";
				presence.Round = status.Brawl.Round + (status.Brawl.Phase == BrawlPhase::Pick ? 1u : 0u);
				presence.Rounds = status.Brawl.Rounds;
				presence.Status = "Spellbrawl: " + map + ", round " + std::to_string(presence.Round) + " of " + std::to_string(presence.Rounds);
			}
			break;
		case GameMode::Arena:
		case GameMode::Royale:
			presence.Token = "#Status_Arena";
			presence.Status = "Fighting in the " + map;
			presence.Map = map;
			break;
		default:
			presence.Token = "#Status_Mission";
			presence.Status = "On a mission in " + map;
			presence.Map = map;
			break;
		}
		return presence;
	}

	void SessionState::DrawInvitePage(glm::vec2 center)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		SteamPlatform& steam = m_App.GetSteam();
		const UiRect panel = UiRect::Centered(center, { 620.0f, std::min(700.0f, ui.GetSize().y - 40.0f) });
		ui.Panel(panel, 18.0f);
		UiRect content = panel.Shrink(36.0f, 30.0f);
		TextStyle title = Theme::Title(30.0f);
		title.Tracking = 2.5f;
		title.GlowColor = WithAlpha(Theme::Accent, 0.55f);
		title.GlowBlur = 10.0f;
		draw.Text({ content.X, content.Y }, "Invite Friends", title);
		const char* subtitle = steam.IsLobbyOpenToFriends() ? "Steam friends can also join you from their friends list." : "Your game is invite only. Only friends you invite can join.";
		draw.TextWrapped({ content.X, content.Y + 44.0f, content.W, 40.0f }, subtitle, Theme::Body(15.0f, Theme::Muted));
		ui.Separator({ content.X, content.Y + 74.0f, content.W, 10.0f });
		content.CutTop(100.0f);
		UiRect footer = content.CutBottom(44.0f);
		content.CutBottom(14.0f);
		if (ui.Button(Ui::MakeId("invite.back"), footer.CutLeft(170.0f), "Back"))
			m_PauseInvite = false;
		if (steam.IsOverlayEnabled() && ui.Button(Ui::MakeId("invite.overlay"), footer.CutRight(220.0f), "Steam Overlay", ButtonKind::Normal, true, UiIcon::MagicPortal))
			steam.ShowInviteOverlay();

		const std::vector<SteamFriend>& friends = steam.GetFriends();
		if (friends.empty())
		{
			draw.TextWrapped(content.CutTop(60.0f), "None of your Steam friends are online right now.", Theme::Body(15.5f, Theme::Faint));
			return;
		}
		const size_t visible = size_t(std::max(content.H / FriendRow, 1.0f));
		const size_t maxFirst = friends.size() > visible ? friends.size() - visible : 0;
		if (const float scroll = Rdn::Input::GetScrollVertical(); scroll != 0.0f && content.Contains(ui.GetMouse()))
			m_InviteScroll = size_t(std::clamp(int64_t(m_InviteScroll) - int64_t(std::lround(scroll)), int64_t(0), int64_t(maxFirst)));
		const size_t first = std::min(m_InviteScroll, maxFirst);
		const uint64_t lobby = steam.GetLobbyId();
		for (size_t i = first; i < friends.size() && i < first + visible; i++)
		{
			const SteamFriend& person = friends[i];
			UiRect row = content.CutTop(FriendRow).Shrink(0.0f, 4.0f);
			const UiInteraction interaction = ui.Interact(Ui::MakeId("invite.row", person.Id), row);
			ui.Card(row, ui.Animate(Ui::MakeId("invite.row", person.Id), interaction.Hovered), 0.0f);
			row = row.Shrink(14.0f, 0.0f);
			const bool together = lobby != 0 && person.Lobby == lobby;
			const auto invited = std::find_if(m_Invited.begin(), m_Invited.end(), [&](const auto& entry) { return entry.first == person.Id; });
			const UiRect button = row.CutRight(130.0f).Shrink(0.0f, 4.0f);
			if (together)
				draw.TextAligned(button, "In your game", Theme::Body(14.0f, Theme::Success), { 1.0f, 0.5f });
			else if (ui.Button(Ui::MakeId("invite.send", person.Id), button, invited != m_Invited.end() ? "Invited" : "Invite", ButtonKind::Primary, invited == m_Invited.end()) && steam.InviteFriend(person.Id))
				m_Invited.push_back({ person.Id, InviteSeconds });
			const UiColor tone = person.Activity == FriendActivity::Away ? Theme::Faint : person.Activity == FriendActivity::Online ? Theme::Astral : Theme::Success;
			draw.Circle({ row.X + 5.0f, row.Center().y }, 4.0f, tone);
			draw.TextFit({ row.X + 20.0f, row.Y + 3.0f, row.W - 30.0f, 20.0f }, person.Name, Theme::Bold(16.0f, Theme::Text));
			draw.TextFit({ row.X + 20.0f, row.Y + 22.0f, row.W - 30.0f, 16.0f }, DescribeActivity(person.Activity), Theme::Body(13.0f, Theme::Muted));
		}
		if (maxFirst > 0)
			draw.TextAligned(footer, "Scroll for more", Theme::Body(13.0f, Theme::Faint), { 0.5f, 0.5f });
	}

	void SessionState::DrawAchievementsPage(glm::vec2 center)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const CharacterProfile& profile = m_Client.GetProfile();
		const UiRect panel = UiRect::Centered(center, { 900.0f, std::min(760.0f, ui.GetSize().y - 40.0f) });
		ui.Panel(panel, 18.0f);
		UiRect content = panel.Shrink(36.0f, 30.0f);
		TextStyle title = Theme::Title(30.0f);
		title.Tracking = 2.5f;
		title.GlowColor = WithAlpha(Theme::Accent, 0.55f);
		title.GlowBlur = 10.0f;
		draw.Text({ content.X, content.Y }, "Achievements", title);
		const uint32_t unlocked = uint32_t(std::popcount(profile.Achievements & AllAchievements));
		const std::string summary = std::to_string(unlocked) + " of " + std::to_string(uint32_t(Achievement::Count)) + " unlocked" + (m_App.GetSteam().IsReady() && m_App.GetSettings().SaveProfiles ? ", and Steam keeps them too." : ".");
		draw.TextWrapped({ content.X, content.Y + 44.0f, content.W, 40.0f }, summary, Theme::Body(15.0f, Theme::Muted));
		ui.Separator({ content.X, content.Y + 74.0f, content.W, 10.0f });
		content.CutTop(96.0f);
		UiRect footer = content.CutBottom(44.0f);
		content.CutBottom(10.0f);
		if (ui.Button(Ui::MakeId("achievements.back"), footer.CutLeft(170.0f), "Back"))
			m_PauseAchievements = false;
		draw.TextAligned(footer, "Esc returns", Theme::Body(13.5f, Theme::Faint), { 1.0f, 0.5f });

		constexpr float Gap = 14.0f;
		const size_t rows = (std::size(Achievements) + 1) / 2;
		const float cellWidth = (content.W - Gap) * 0.5f;
		const float cellHeight = std::min((content.H - Gap * float(rows - 1)) / float(rows), 74.0f);
		for (size_t i = 0; i < std::size(Achievements); i++)
		{
			const AchievementDef& def = Achievements[i];
			const bool earned = (profile.Achievements & AchievementBit(def.Id)) != 0;
			const UiRect cell{ content.X + float(i % 2) * (cellWidth + Gap), content.Y + float(i / 2) * (cellHeight + Gap), cellWidth, cellHeight };
			ui.Card(cell, 0.0f, earned ? 1.0f : 0.0f, Theme::Gold);
			const glm::vec2 badge(cell.X + 34.0f, cell.Center().y);
			if (earned)
				draw.Sigil(badge, 24.0f, { .Color = WithAlpha(Theme::Gold, 0.5f), .Accent = WithAlpha(Theme::GoldLight, 0.35f), .Speed = 0.5f, .Phase = float(i) * 0.13f, .LineWidth = 1.0f, .Glow = 0.4f, .Layers = SigilRings });
			draw.Icon(AchievementIcons[i], UiRect::Centered(badge, glm::vec2(30.0f)), earned
				? IconStyle{ .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.7f), .GlowColor = WithAlpha(Theme::Gold, 0.35f) }
				: IconStyle{ .Color = WithAlpha(Theme::Faint, 0.8f), .ColorBottom = WithAlpha(Theme::Faint, 0.5f) });
			const bool secret = def.Hidden && !earned;
			UiRect text{ cell.X + 66.0f, cell.Y + 9.0f, cell.W - 78.0f, cell.H - 18.0f };
			const uint32_t progress = ProgressOf(def.Id, profile.Falls, profile.Scholar);
			if (def.Goal > 0 && !earned)
			{
				const UiRect count = text.CutRight(64.0f);
				draw.TextAligned({ count.X, count.Y, count.W, 20.0f }, std::to_string(progress) + " / " + std::to_string(def.Goal), Theme::Bold(13.5f, Theme::Muted), { 1.0f, 0.5f });
				const UiRect bar{ count.X + 6.0f, count.Y + 26.0f, count.W - 6.0f, 5.0f };
				draw.Rect(bar, { .Fill = Rgba(1.0f, 1.0f, 1.0f, 0.08f), .Radius = 2.5f });
				draw.Rect({ bar.X, bar.Y, bar.W * float(progress) / float(def.Goal), bar.H }, { .Fill = WithAlpha(Theme::Gold, 0.85f), .Radius = 2.5f });
			}
			draw.TextFit({ text.X, text.Y, text.W, 22.0f }, secret ? "Hidden achievement" : def.Name, Theme::Bold(16.0f, earned ? Theme::GoldLight : Theme::Text));
			draw.TextWrapped({ text.X, text.Y + 24.0f, text.W, text.H - 24.0f }, secret ? "A secret of Emberhold." : def.Description, Theme::Body(13.5f, earned ? Theme::Muted : Theme::Faint));
		}
	}

	void SessionState::DrawAchievementToasts()
	{
		if (m_Toasts.empty())
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const glm::vec2 size = ui.GetSize();
		for (size_t i = 0; i < m_Toasts.size() && i < MaxToasts; i++)
		{
			const Toast& toast = m_Toasts[i];
			const AchievementDef& def = GetAchievement(toast.Id);
			const float enter = std::clamp(toast.Age / 0.35f, 0.0f, 1.0f);
			const float ease = 1.0f - (1.0f - enter) * (1.0f - enter) * (1.0f - enter);
			const float alpha = ease * std::clamp((ToastSeconds - toast.Age) / 0.6f, 0.0f, 1.0f);
			const UiRect box{ size.x - ToastWidth - 24.0f + (1.0f - ease) * 70.0f, size.y - 150.0f - ToastHeight - float(i) * (ToastHeight + 10.0f), ToastWidth, ToastHeight };
			draw.Rect(box.Translate({ 0.0f, 5.0f }), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.45f * alpha), .Softness = 12.0f, .Chamfer = 11.0f });
			draw.Rect(box, { .Fill = Hex(0x1C1532, 0.95f * alpha), .FillBottom = Hex(0x0B0918, 0.96f * alpha), .Border = WithAlpha(Theme::Gold, 0.85f * alpha), .BorderWidth = 1.2f, .Glow = WithAlpha(Theme::Gold, 0.22f * alpha), .GlowSize = 22.0f, .Chamfer = 11.0f, .Aether = 0.35f, .Seed = 0.37f + float(i) * 0.11f });
			const glm::vec2 badge(box.X + 40.0f, box.Center().y);
			draw.Sigil(badge, 27.0f, { .Color = WithAlpha(Theme::Gold, 0.6f * alpha), .Accent = WithAlpha(Theme::GoldLight, 0.45f * alpha), .Speed = 1.2f, .Phase = float(i) * 0.21f, .LineWidth = 1.0f, .Glow = 0.5f, .Layers = SigilRings | SigilRunes, .Progress = std::min(toast.Age / 0.8f, 1.0f) });
			draw.Icon(AchievementIcons[size_t(toast.Id)], UiRect::Centered(badge, glm::vec2(32.0f)), { .Color = WithAlpha(Theme::GoldLight, alpha), .ColorBottom = WithAlpha(Theme::Gold, alpha), .ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.7f * alpha), .GlowColor = WithAlpha(Theme::Gold, 0.4f * alpha) });
			const float left = box.X + 78.0f;
			draw.Text({ left, box.Y + 11.0f }, IsHonor(toast.Id) ? "ACHIEVEMENT AND TITLE" : "ACHIEVEMENT UNLOCKED", Theme::Caps(11.5f, WithAlpha(Theme::Gold, alpha), 2.0f));
			draw.TextFit({ left, box.Y + 26.0f, box.Right() - left - 14.0f, 26.0f }, def.Name, Theme::Bold(19.0f, WithAlpha(Theme::GoldLight, alpha)));
			draw.TextFit({ left, box.Y + 52.0f, box.Right() - left - 14.0f, 18.0f }, def.Description, Theme::Body(13.5f, WithAlpha(Theme::Muted, alpha)));
		}
	}
}
