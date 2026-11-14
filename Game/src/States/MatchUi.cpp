#include "SessionState.h"
#include "App/GameApp.h"
#include "Audio/GameAudio.h"
#include "Rendering/Models.h"
#include "Rendering/WorldRenderer.h"
#include "UI/Palette.h"
#include "UI/Ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <tuple>

namespace Game
{
	namespace
	{
		constexpr float FeedSeconds = 7.0f;
		constexpr size_t FeedLines = 5;

		std::string Clock(float seconds)
		{
			const int total = int(std::ceil(std::max(seconds, 0.0f)));
			char text[16];
			std::snprintf(text, sizeof(text), "%d:%02d", total / 60, total % 60);
			return text;
		}

		UiRect Strip(float y, float width, float right)
		{
			return { right - width, y, width, 26.0f };
		}
	}

	bool SessionState::BrawlRanksAbove(const PlayerInfo& a, const PlayerInfo& b) const
	{
		if (m_Client.GetSession().Rules.Mode == BrawlMode::Damage)
			return std::tuple(a.Score, a.Assists, a.Rounds, a.Damage) > std::tuple(b.Score, b.Assists, b.Rounds, b.Damage);
		return std::tuple(a.Score, a.Rounds, a.Kills) > std::tuple(b.Score, b.Rounds, b.Kills);
	}

	bool SessionState::DescribeOutcome(std::string& title, std::string& text) const
	{
		const GameStatus& status = m_Client.GetStatus();
		const SessionInfo& session = m_Client.GetSession();
		const uint32_t self = m_Client.GetPlayerId();
		const bool watching = m_Client.IsSpectating();
		if (session.Rules.Teams >= 2)
		{
			if (status.WinningTeam >= MaxTeams)
			{
				title = "Draw";
				text = "No team came out on top";
				return false;
			}
			const uint8_t mine = m_Client.GetTeam(self);
			const bool won = mine == status.WinningTeam;
			const uint16_t rounds = status.TeamScores[status.WinningTeam];
			title = watching || mine == NoTeam ? std::string(GetTeamName(status.WinningTeam)) + " Wins" : won ? "Victory" : "Defeat";
			text = std::string(won ? "Your team rules " : std::string("The ") + GetTeamName(status.WinningTeam) + " team rules ") + TermsFor(session.Map).Title + " with " + std::to_string(rounds) + (rounds == 1 ? " round won" : " rounds won");
			return won;
		}
		if (status.Winner == NoPlayer)
		{
			title = "Draw";
			text = "Nobody ended up on top";
			return false;
		}
		const PlayerInfo* winner = m_Client.FindPlayer(status.Winner);
		const std::string points = winner ? " with " + std::to_string(winner->Score) + " points" : std::string();
		if (status.Winner == self)
		{
			title = "Victory";
			text = std::string("You rule ") + TermsFor(session.Map).Title + points;
			return true;
		}
		title = watching ? "Match Over" : "Defeat";
		text = PlayerName(status.Winner) + " rules " + TermsFor(session.Map).Title + points;
		return false;
	}

	void SessionState::UpdateMatchFeedback(float dt)
	{
		for (FeedEntry& entry : m_Feed)
			entry.Age += dt;
		std::erase_if(m_Feed, [](const FeedEntry& entry) { return entry.Age >= FeedSeconds; });
		GameAudio& audio = m_App.GetGameAudio();
		const uint32_t self = m_Client.GetPlayerId();
		const bool brawl = m_Client.GetSession().Mode == GameMode::Brawl;
		for (const KillRecord& kill : m_Client.TakeKills())
		{
			m_Feed.push_back({ kill });
			if (kill.Killer == self && kill.Victim != self)
			{
				audio.PlayEvent(AudioEvent::Kill);
				const bool shoved = brawl && m_Client.GetSession().Rules.Mode == BrawlMode::Knockback;
				m_Notices.push_back({ (shoved ? "You knocked " + PlayerName(kill.Victim) + " into the " + TermsFor(m_Client.GetSession().Map).Hazard : "You slew " + PlayerName(kill.Victim)), Theme::AccentLight });
			}
		}
		if (m_Feed.size() > FeedLines)
			m_Feed.erase(m_Feed.begin(), m_Feed.end() - FeedLines);

		const GameStatus& status = m_Client.GetStatus();
		if (!IsPvpMode(status.Mode))
			return;
		UpdateBrawlFeedback();
		if (status.Match != m_SeenMatch)
		{
			if (status.Match == MatchPhase::Ended)
			{
				std::string title;
				std::string text;
				const bool won = DescribeOutcome(title, text);
				m_BannerTitle = title;
				m_BannerText = text;
				m_BannerColor = won ? Hex(0xFFD36A) : Hex(0xC8C0B4);
				m_BannerTime = 4.0f;
				audio.PlayEvent(won ? AudioEvent::MatchWon : AudioEvent::MatchLost);
			}
			m_SeenMatch = status.Match;
		}
	}

	void SessionState::DrawMatchStatus()
	{
		DrawBrawlStatus();
	}

	void SessionState::DrawKillFeed()
	{
		if (m_Feed.empty())
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const float right = ui.GetSize().x - 14.0f;
		float y = 14.0f;
		const uint32_t self = m_Client.GetPlayerId();
		for (auto it = m_Feed.rbegin(); it != m_Feed.rend(); ++it)
		{
			const KillRecord& kill = it->Kill;
			const float alpha = std::clamp((FeedSeconds - it->Age) / 0.8f, 0.0f, 1.0f) * std::clamp(it->Age / 0.15f, 0.0f, 1.0f);
			const bool involved = kill.Killer == self || kill.Victim == self;
			const TextStyle victimStyle = Theme::Bold(14.5f, WithAlpha(Lighten(WithAlpha(ColorOf(kill.Victim)), 0.35f), alpha));
			const std::string victim = PlayerName(kill.Victim);
			const bool storm = kill.Killer == NoPlayer;
			const std::string killer = storm ? std::string() : PlayerName(kill.Killer);
			const TextStyle killerStyle = Theme::Bold(14.5f, WithAlpha(Lighten(WithAlpha(ColorOf(kill.Killer)), 0.35f), alpha));
			const TextStyle tailStyle = Theme::Body(14.0f, WithAlpha(Theme::Muted, alpha));
			const char* tail = TermsFor(m_Client.GetSession().Map).Fell;
			const float icon = 20.0f;
			const float killerWidth = storm ? 0.0f : draw.MeasureWidth(killer, killerStyle) + 8.0f;
			const float victimWidth = draw.MeasureWidth(victim, victimStyle);
			const float tailWidth = storm ? draw.MeasureWidth(tail, tailStyle) + 8.0f : 0.0f;
			const float total = killerWidth + icon + 8.0f + victimWidth + tailWidth + 24.0f;
			const UiRect pill = Strip(y, total, right);
			draw.Rect(pill, { .Fill = Rgba(0.05f, 0.04f, 0.1f, 0.78f * alpha), .Border = WithAlpha(involved ? Theme::Accent : Theme::Edge, (involved ? 0.9f : 0.5f) * alpha), .BorderWidth = involved ? 1.5f : 1.0f, .Chamfer = 7.0f });
			float x = pill.X + 12.0f;
			if (!storm)
			{
				draw.TextAligned({ x, pill.Y, killerWidth, pill.H }, killer, killerStyle, { 0.0f, 0.5f });
				x += killerWidth;
			}
			const UiIcon glyph = kill.Ability < uint8_t(AbilityId::Count) ? IconFor(AbilityId(kill.Ability)) : UiIcon::SkullCrossedBones;
			const UiColor glyphColor = kill.Ability < uint8_t(AbilityId::Count) ? WithAlpha(SkillColor(AbilityId(kill.Ability)), alpha) : WithAlpha(Theme::Danger, alpha);
			draw.Icon(glyph, { x, pill.Center().y - icon * 0.5f, icon, icon }, { .Color = Lighten(glyphColor, 0.3f), .ColorBottom = glyphColor, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f * alpha) });
			x += icon + 8.0f;
			draw.TextAligned({ x, pill.Y, victimWidth, pill.H }, victim, victimStyle, { 0.0f, 0.5f });
			x += victimWidth + 8.0f;
			if (storm)
				draw.TextAligned({ x, pill.Y, tailWidth, pill.H }, tail, tailStyle, { 0.0f, 0.5f });
			y += 31.0f;
		}
	}

	void SessionState::DrawMatchResults()
	{
		const GameStatus& status = m_Client.GetStatus();
		if (!IsPvpMode(status.Mode) || status.Match != MatchPhase::Ended || m_Paused)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const bool damage = m_Client.GetSession().Rules.Mode == BrawlMode::Damage;
		std::vector<PlayerInfo> players;
		for (const PlayerInfo& player : m_Client.GetPlayers())
			if (!player.Spectator)
				players.push_back(player);
		std::stable_sort(players.begin(), players.end(), [this](const PlayerInfo& a, const PlayerInfo& b) { return BrawlRanksAbove(a, b); });
		const std::vector<uint8_t> teams = TeamOrder();
		if (!teams.empty())
			std::stable_sort(players.begin(), players.end(), [&](const PlayerInfo& a, const PlayerInfo& b)
			{
				const auto place = [&](uint8_t team) { return size_t(std::find(teams.begin(), teams.end(), team) - teams.begin()); };
				return place(a.Team) < place(b.Team);
			});
		const std::vector<const char*> labels = damage ? std::vector<const char*>{ "Points", "Rounds", "Kills", "Assists", "Damage" } : std::vector<const char*>{ "Points", "Rounds", "Kills" };
		const auto cells = [&](const PlayerInfo& player)
		{
			std::vector<std::string> values = { std::to_string(player.Score), std::to_string(player.Rounds), std::to_string(player.Kills), std::to_string(player.Assists), std::to_string(player.Damage) };
			values.resize(labels.size());
			return values;
		};
		constexpr float RowHeight = 30.0f;
		constexpr float ColumnWidth = 74.0f;
		const float columns = float(labels.size()) * ColumnWidth;
		uint32_t bands = 0;
		for (size_t i = 0; i < players.size() && !teams.empty(); i++)
			bands += i == 0 || players[i].Team != players[i - 1].Team ? 1u : 0u;
		const float height = 168.0f + float(players.size()) * RowHeight + float(bands) * 34.0f;
		const UiRect panel = UiRect::Centered({ ui.GetSize().x * 0.5f, ui.GetSize().y * 0.5f + 40.0f }, { 380.0f + columns, height });
		ui.Panel(panel, 12.0f, false);
		std::string title;
		std::string text;
		const bool won = DescribeOutcome(title, text);
		TextStyle titleStyle = Theme::Title(34.0f);
		titleStyle.Tracking = 3.0f;
		if (!won)
		{
			titleStyle.Color = Hex(0xE6DED2);
			titleStyle.ColorBottom = Hex(0x9C948A);
		}
		draw.TextAligned({ panel.X, panel.Y + 14.0f, panel.W, 40.0f }, title, titleStyle, { 0.5f, 0.5f });
		draw.TextAligned({ panel.X, panel.Y + 54.0f, panel.W, 22.0f }, text, Theme::Body(15.5f, won ? Theme::Success : Theme::Muted), { 0.5f, 0.5f });
		UiRect table{ panel.X + 24.0f, panel.Y + 88.0f, panel.W - 48.0f, panel.H - 120.0f };
		const UiRect header = table.CutTop(24.0f);
		const TextStyle headStyle = Theme::Label(13.5f);
		draw.TextAligned({ header.X, header.Y, 60.0f, header.H }, "#", headStyle, { 0.0f, 0.5f });
		draw.TextAligned({ header.X + 60.0f, header.Y, 200.0f, header.H }, "Player", headStyle, { 0.0f, 0.5f });
		for (size_t i = 0; i < labels.size(); i++)
			draw.TextAligned({ header.Right() - columns + float(i) * ColumnWidth, header.Y, ColumnWidth, header.H }, labels[i], headStyle, { 0.5f, 0.5f });
		ui.Separator(table.CutTop(6.0f));
		size_t place = 0;
		for (size_t i = 0; i < players.size(); i++)
		{
			const PlayerInfo& player = players[i];
			if (!teams.empty() && (i == 0 || player.Team != players[i - 1].Team))
			{
				place = 0;
				table.CutTop(4.0f);
				const UiRect band = table.CutTop(30.0f);
				const UiColor color = player.Team < MaxTeams ? WithAlpha(TeamColor(player.Team)) : Theme::Muted;
				const bool winners = player.Team == status.WinningTeam;
				draw.Rect(band, { .Fill = WithAlpha(Darken(color, 0.72f), 0.75f), .FillBottom = WithAlpha(Darken(color, 0.85f), 0.4f), .Border = WithAlpha(color, winners ? 0.95f : 0.45f), .BorderWidth = winners ? 1.6f : 1.0f, .Chamfer = 6.0f });
				UiRect label = band.Shrink(12.0f, 0.0f);
				if (winners)
					draw.Icon(UiIcon::Crown, label.CutLeft(20.0f).Shrink(0.0f, 6.0f), { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold });
				draw.TextAligned(label.Shrink(winners ? 6.0f : 0.0f, 0.0f), GetTeamName(player.Team), Theme::Bold(16.0f, Lighten(color, 0.45f)), { 0.0f, 0.5f });
				if (player.Team < MaxTeams)
					draw.TextAligned(label, std::to_string(status.TeamScores[player.Team]) + (status.TeamScores[player.Team] == 1 ? " round won" : " rounds won"), Theme::Bold(15.0f, Theme::Text), { 1.0f, 0.5f });
			}
			place++;
			const UiRect row = table.CutTop(RowHeight);
			const bool you = player.PlayerId == m_Client.GetPlayerId();
			if (you)
				draw.Rect(row, { .Fill = Rgba(0.75f, 0.65f, 1.0f, 0.07f), .Border = WithAlpha(Theme::Edge, 0.6f), .BorderWidth = 1.0f, .Chamfer = 6.0f });
			const std::string placeText = std::to_string(place);
			const bool first = teams.empty() && i == 0;
			draw.TextAligned({ row.X + 8.0f, row.Y, 52.0f, row.H }, placeText, Theme::Bold(15.0f, first ? Theme::AccentLight : Theme::Text), { 0.0f, 0.5f });
			if (first)
				draw.Icon(UiIcon::Crown, { row.X + 40.0f, row.Center().y - 8.0f, 16.0f, 16.0f }, { .Color = Theme::AccentLight, .ColorBottom = Theme::Accent });
			const UiColor color = WithAlpha(ColorOf(player.PlayerId));
			draw.Circle({ row.X + 68.0f, row.Center().y }, 5.0f, color, Rgba(0.0f, 0.0f, 0.0f, 0.6f), 1.0f);
			draw.TextFit({ row.X + 80.0f, row.Y, row.W - 88.0f - columns, row.H }, player.Name + (you ? "  (you)" : ""), Theme::Bold(15.0f, Lighten(color, 0.45f)));
			const std::vector<std::string> values = cells(player);
			for (size_t column = 0; column < values.size(); column++)
				draw.TextAligned({ row.Right() - columns + float(column) * ColumnWidth, row.Y, ColumnWidth, row.H }, values[column], Theme::Body(15.0f, column == 0 ? Theme::Text : Theme::Muted), { 0.5f, 0.5f });
		}
		draw.TextAligned({ panel.X, panel.Bottom() - 32.0f, panel.W, 22.0f }, "Returning to Emberhold in " + std::to_string(int(std::ceil(status.MatchTime))), Theme::Body(14.0f, Theme::Faint), { 0.5f, 0.5f });
	}
}
