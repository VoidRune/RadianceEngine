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

namespace Game
{
	namespace
	{
		constexpr float FeedSeconds = 7.0f;
		constexpr size_t FeedLines = 5;
		constexpr float MinimapSize = 196.0f;
		constexpr int32_t MinimapCell = 2;
		constexpr glm::vec3 StormColor{ 0.62f, 0.28f, 0.95f };
		constexpr float StormDrawRange = 36.0f;

		std::string Clock(float seconds)
		{
			const int total = int(std::ceil(std::max(seconds, 0.0f)));
			char text[16];
			std::snprintf(text, sizeof(text), "%d:%02d", total / 60, total % 60);
			return text;
		}

		std::string Ordinal(uint32_t place)
		{
			const char* suffix = place % 100 >= 11 && place % 100 <= 13 ? "th" : place % 10 == 1 ? "st" : place % 10 == 2 ? "nd" : place % 10 == 3 ? "rd" : "th";
			return std::to_string(place) + suffix;
		}

		UiRect Strip(float y, float width, float right)
		{
			return { right - width, y, width, 26.0f };
		}
	}

	bool SessionState::DescribeOutcome(std::string& title, std::string& text) const
	{
		const GameStatus& status = m_Client.GetStatus();
		const SessionInfo& session = m_Client.GetSession();
		const uint32_t self = m_Client.GetPlayerId();
		if (session.Mode == GameMode::Arena && session.Rules.Teams)
		{
			if (status.WinningTeam == NoTeam)
			{
				title = "Draw";
				text = "Neither team pulled ahead";
				return false;
			}
			const bool won = status.WinningTeam == m_Client.GetTeam(self);
			title = won ? "Victory" : "Defeat";
			text = std::string("The ") + GetTeamName(status.WinningTeam) + " team wins " + std::to_string(status.TeamScores[0]) + " to " + std::to_string(status.TeamScores[1]);
			return won;
		}
		if (status.Winner == NoPlayer)
		{
			title = "Draw";
			text = session.Mode == GameMode::Royale ? "Nobody walked out of the storm" : "Nobody came out ahead";
			return false;
		}
		if (status.Winner == self)
		{
			title = "Victory";
			text = session.Mode == GameMode::Royale ? "You are the last one standing" : "You won the arena";
			return true;
		}
		const PlayerInfo* you = m_Client.FindPlayer(self);
		title = session.Mode == GameMode::Royale && you && you->Placement > 0 ? Ordinal(you->Placement) + " Place" : "Defeat";
		text = PlayerName(status.Winner) + " wins";
		return false;
	}

	void SessionState::UpdateMatchFeedback(float dt)
	{
		for (FeedEntry& entry : m_Feed)
			entry.Age += dt;
		std::erase_if(m_Feed, [](const FeedEntry& entry) { return entry.Age >= FeedSeconds; });
		GameAudio& audio = m_App.GetGameAudio();
		const uint32_t self = m_Client.GetPlayerId();
		for (const KillRecord& kill : m_Client.TakeKills())
		{
			m_Feed.push_back({ kill });
			if (kill.Killer == self && kill.Victim != self)
			{
				audio.PlayEvent(AudioEvent::Kill);
				m_Notices.push_back({ "You slew " + PlayerName(kill.Victim), Theme::GoldLight });
			}
		}
		if (m_Feed.size() > FeedLines)
			m_Feed.erase(m_Feed.begin(), m_Feed.end() - FeedLines);

		const GameStatus& status = m_Client.GetStatus();
		if (!IsPvpMode(status.Mode))
			return;
		if (status.Match == MatchPhase::Countdown)
		{
			const int32_t second = int32_t(std::ceil(status.MatchTime));
			if (second != m_SeenCountdown && second > 0 && second <= 3)
				audio.PlayEvent(AudioEvent::Countdown);
			m_SeenCountdown = second;
		}
		if (status.Match != m_SeenMatch)
		{
			if (status.Match == MatchPhase::Fight)
			{
				m_BannerTitle = "Fight!";
				m_BannerText = status.Mode == GameMode::Royale ? "The ash storm is awake. Loot, hunt, survive" : "Kill your rivals, hold the shrines";
				m_BannerColor = Hex(0xFF7A5A);
				m_BannerTime = 3.0f;
				audio.PlayEvent(AudioEvent::FightStart);
			}
			else if (status.Match == MatchPhase::Ended)
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
		if (status.Mode == GameMode::Royale && status.Match == MatchPhase::Fight)
		{
			if (status.Storm.Shrinking && !m_SeenStormShrinking)
			{
				m_Notices.push_back({ "The ash storm closes in", Hex(0xD9A6FF) });
				audio.PlayEvent(AudioEvent::StormClosing);
			}
			m_SeenStormShrinking = status.Storm.Shrinking;
			m_SeenStormStage = status.Storm.Stage;
		}
	}

	void SessionState::DrawMatchStatus()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const GameStatus& status = m_Client.GetStatus();
		const SessionInfo& session = m_Client.GetSession();
		const float width = ui.GetSize().x;
		const float center = width * 0.5f;
		const bool royale = session.Mode == GameMode::Royale;
		const std::string title = royale ? "Battle Royale" : session.Rules.Teams ? "Team Arena" : "Arena";
		std::string subtitle;
		UiColor subtitleColor = Theme::Text;
		switch (status.Match)
		{
		case MatchPhase::Countdown:
			subtitle = std::string(royale ? "Grab what you can, the fight starts in " : "The fight starts in ") + std::to_string(int(std::ceil(status.MatchTime)));
			subtitleColor = Theme::GoldLight;
			break;
		case MatchPhase::Fight:
			if (royale)
				subtitle = std::to_string(status.Alive) + " of " + std::to_string(status.Entrants) + " still standing";
			else
				subtitle = std::string(session.Rules.Teams ? "First team to " : "First to ") + std::to_string(session.Rules.ScoreGoal) + " kills   " + Clock(status.MatchTime) + " left";
			break;
		case MatchPhase::Ended:
		{
			std::string outcome;
			subtitleColor = DescribeOutcome(outcome, subtitle) ? Theme::Success : Theme::Danger;
			subtitle = outcome + ". " + subtitle;
			break;
		}
		}
		TextStyle titleStyle = Theme::Title(28.0f);
		titleStyle.Tracking = 2.0f;
		const float titleWidth = draw.MeasureWidth(title, titleStyle);
		draw.Shadow(UiRect::Centered({ center, 42.0f }, { titleWidth + 160.0f, 52.0f }), 26.0f, 30.0f, Rgba(0.0f, 0.0f, 0.0f, 0.55f), { 0.0f, 0.0f });
		draw.TextAligned({ 0.0f, 16.0f, width, 34.0f }, title, titleStyle, { 0.5f, 0.5f });
		draw.TextAligned({ 0.0f, 54.0f, width, 22.0f }, subtitle, Theme::Floating(16.0f, subtitleColor, UiFont::Body), { 0.5f, 0.5f });

		float y = 84.0f;
		if (!royale && session.Rules.Teams)
		{
			const float pill = 150.0f;
			for (uint8_t team = 0; team < TeamCount; team++)
			{
				const UiRect box{ center + (team == 0 ? -pill - 8.0f : 8.0f), y, pill, 32.0f };
				const UiColor color = WithAlpha(TeamColor(team));
				const bool mine = m_Client.GetTeam(m_Client.GetPlayerId()) == team;
				draw.Rect(box, { .Fill = WithAlpha(Darken(color, 0.65f), 0.85f), .Radius = 8.0f, .Border = mine ? Lighten(color, 0.3f) : WithAlpha(color, 0.6f), .BorderWidth = mine ? 2.0f : 1.0f });
				draw.TextAligned(box.Shrink(12.0f, 0.0f), GetTeamName(team), Theme::Bold(15.0f, Lighten(color, 0.4f)), { team == 0 ? 0.0f : 1.0f, 0.5f });
				draw.TextAligned(box.Shrink(12.0f, 0.0f), std::to_string(status.TeamScores[team]), Theme::Title(22.0f), { team == 0 ? 1.0f : 0.0f, 0.5f });
			}
			y += 40.0f;
		}
		else if (!royale && status.Match == MatchPhase::Fight)
		{
			const PlayerInfo* leader = nullptr;
			for (const PlayerInfo& player : m_Client.GetPlayers())
				if (!leader || player.Kills > leader->Kills)
					leader = &player;
			const PlayerInfo* you = m_Client.FindPlayer(m_Client.GetPlayerId());
			if (leader && you)
			{
				const std::string line = leader == you || leader->Kills == you->Kills ? "You lead with " + std::to_string(you->Kills) + " kills" : "You have " + std::to_string(you->Kills) + ", " + leader->Name + " leads with " + std::to_string(leader->Kills);
				draw.TextAligned({ 0.0f, y, width, 20.0f }, line, Theme::Floating(14.5f, Theme::Muted, UiFont::Body), { 0.5f, 0.5f });
			}
		}
		if (royale && status.Match == MatchPhase::Fight)
		{
			const StormState& storm = status.Storm;
			std::string line;
			if (storm.Stage >= StormStageCount)
				line = "The storm has swallowed everything. Last one standing wins";
			else if (storm.Shrinking)
				line = "The ash storm is closing, " + Clock(storm.Timer) + " until it settles";
			else
				line = "The ash storm closes in " + Clock(storm.Timer);
			const UiColor color = storm.Shrinking || storm.Stage >= StormStageCount ? Hex(0xE7B8FF) : Hex(0xB89BD6);
			const TextStyle style = Theme::Floating(15.0f, color, UiFont::Bold);
			const float lineWidth = draw.MeasureWidth(line, style) + 54.0f;
			const UiRect pill = UiRect::Centered({ center, y + 13.0f }, { lineWidth, 28.0f });
			draw.Rect(pill, { .Fill = Rgba(0.12f, 0.05f, 0.18f, 0.75f), .Radius = 14.0f, .Border = WithAlpha(color, 0.7f), .BorderWidth = 1.0f });
			draw.Icon(UiIcon::Waves, { pill.X + 10.0f, pill.Center().y - 9.0f, 18.0f, 18.0f }, { .Color = color });
			draw.TextAligned({ pill.X + 34.0f, pill.Y, pill.W - 44.0f, pill.H }, line, style, { 0.0f, 0.5f });
		}
	}

	void SessionState::DrawKillFeed()
	{
		if (m_Feed.empty())
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const float right = ui.GetSize().x - 14.0f;
		float y = m_Client.GetSession().Mode == GameMode::Royale ? 14.0f + MinimapSize + 14.0f : 14.0f;
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
			const char* tail = m_Client.GetSession().Mode == GameMode::Royale ? "fell to the storm" : "fell";
			const float icon = 20.0f;
			const float killerWidth = storm ? 0.0f : draw.MeasureWidth(killer, killerStyle) + 8.0f;
			const float victimWidth = draw.MeasureWidth(victim, victimStyle);
			const float tailWidth = storm ? draw.MeasureWidth(tail, tailStyle) + 8.0f : 0.0f;
			const float total = killerWidth + icon + 8.0f + victimWidth + tailWidth + 24.0f;
			const UiRect pill = Strip(y, total, right);
			draw.Rect(pill, { .Fill = Rgba(0.06f, 0.05f, 0.07f, 0.75f * alpha), .Radius = 13.0f, .Border = WithAlpha(involved ? Theme::Gold : Theme::GoldDark, (involved ? 0.9f : 0.5f) * alpha), .BorderWidth = involved ? 1.6f : 1.0f });
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

	void SessionState::DrawMinimap()
	{
		const Level* level = m_Client.GetLevel();
		if (!level)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const TileMap& map = level->Map;
		if (m_MinimapTravel != m_Client.GetTravelCount())
		{
			m_MinimapTravel = m_Client.GetTravelCount();
			m_MinimapLand.clear();
			for (int32_t y = 0; y < map.GetHeight(); y += MinimapCell)
			{
				int32_t start = -1;
				for (int32_t x = 0; x <= map.GetWidth(); x += MinimapCell)
				{
					uint32_t walkable = 0;
					for (int32_t dy = 0; dy < MinimapCell && x < map.GetWidth(); dy++)
						for (int32_t dx = 0; dx < MinimapCell; dx++)
							walkable += map.IsWalkable(x + dx, y + dy) ? 1u : 0u;
					const bool land = x < map.GetWidth() && walkable * 2 >= uint32_t(MinimapCell * MinimapCell);
					if (land && start < 0)
						start = x;
					else if (!land && start >= 0)
					{
						m_MinimapLand.push_back({ float(start), float(y), float(x), float(y + MinimapCell) });
						start = -1;
					}
				}
			}
		}
		const glm::vec2 size = ui.GetSize();
		const UiRect frame{ size.x - MinimapSize - 14.0f, 14.0f, MinimapSize, MinimapSize };
		const float extent = float(std::max(map.GetWidth(), map.GetHeight()));
		const float scale = frame.W / extent;
		const auto toMap = [&](Vec2 position) { return glm::vec2(frame.X + position.x * scale, frame.Bottom() - position.y * scale); };
		draw.Shadow(frame, 8.0f, 12.0f, Rgba(0.0f, 0.0f, 0.0f, 0.45f), { 0.0f, 3.0f });
		draw.Rect(frame, { .Fill = Rgba(0.07f, 0.12f, 0.18f, 0.88f), .Radius = 8.0f });
		draw.PushClip(frame.Shrink(2.0f));
		for (const glm::vec4& run : m_MinimapLand)
		{
			const glm::vec2 from = toMap({ run.x, run.w });
			const glm::vec2 to = toMap({ run.z, run.y });
			draw.Rect({ from.x, from.y, to.x - from.x + 0.5f, to.y - from.y + 0.5f }, { .Fill = Rgba(0.33f, 0.38f, 0.26f, 0.95f) });
		}
		for (const Region& region : level->Regions)
		{
			if (region.Min.x <= 0.0f && region.Max.x >= extent)
				continue;
			draw.Circle(toMap(region.GetCenter()), 2.0f, Rgba(0.95f, 0.85f, 0.6f, 0.55f));
		}
		const GameStatus& status = m_Client.GetStatus();
		const StormState& storm = status.Storm;
		if (status.Match != MatchPhase::Countdown && storm.Radius > 0.0f)
		{
			draw.Circle(toMap(storm.Center), storm.Radius * scale, UiColor(0.0f), WithAlpha(StormColor, 0.95f), 2.0f);
			if (storm.Stage < StormStageCount && storm.TargetRadius < storm.Radius - 0.5f)
				draw.Circle(toMap(storm.Target), std::max(storm.TargetRadius * scale, 1.5f), UiColor(0.0f), Rgba(1.0f, 1.0f, 1.0f, 0.8f), 1.3f);
		}
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (entity.Kind == EntityKind::Chest && ChestKind(entity.Tag) == ChestKind::Cache && entity.Variant == 0)
				draw.Circle(toMap(entity.Position), 3.5f, Hex(0xFF8A3A), Rgba(0.0f, 0.0f, 0.0f, 0.8f), 1.0f);
			if (entity.Kind != EntityKind::Player || entity.Has(EntityFlags::Dead))
				continue;
			const bool self = entity.NetId == m_Client.GetPlayerNetId();
			const bool watched = !local.State.Alive && entity.Owner == local.Watching;
			if (!self && !watched)
				continue;
			const glm::vec2 point = toMap(entity.Position);
			const glm::vec2 forward(std::cos(entity.Facing), -std::sin(entity.Facing));
			draw.Triangle(point + forward * 2.0f, { 9.0f, 11.0f }, std::atan2(forward.x, -forward.y), self ? Theme::GoldLight : WithAlpha(ColorOf(entity.Owner)), Rgba(0.0f, 0.0f, 0.0f, 0.9f), 1.2f);
		}
		draw.PopClip();
		draw.Rect(frame, { .Radius = 8.0f, .Border = WithAlpha(Theme::GoldDark, 0.9f), .BorderWidth = 1.5f });
		if (const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId()))
			if (const Region* region = level->FindRegion(self->Position))
				draw.TextAligned({ frame.X, frame.Bottom() + 2.0f, frame.W, 18.0f }, region->Name, Theme::Floating(13.0f, Theme::Muted, UiFont::Body), { 0.5f, 0.0f });
		ui.Block(frame);
	}

	void SessionState::DrawStorm(WorldRenderer& renderer) const
	{
		const GameStatus& status = m_Client.GetStatus();
		if (status.Mode != GameMode::Royale || status.Match == MatchPhase::Countdown)
			return;
		const StormState& storm = status.Storm;
		const Vec2 focus = { m_CameraTarget.x, m_CameraTarget.z };
		const float pulse = 0.65f + 0.35f * std::sin(m_Time * 2.5f);
		if (storm.Radius > 0.05f)
		{
			const float segment = 0.8f;
			const uint32_t count = std::clamp(uint32_t(2.0f * Pi * storm.Radius / segment), 12u, 4096u);
			for (uint32_t i = 0; i < count; i++)
			{
				const float angle = 2.0f * Pi * float(i) / float(count);
				const Vec2 outward = FromAngle(angle);
				const Vec2 point = storm.Center + outward * storm.Radius;
				if (LengthSquared(point - focus) > StormDrawRange * StormDrawRange)
					continue;
				const float length = 2.0f * Pi * storm.Radius / float(count);
				const float wave = std::sin(m_Time * 1.7f + float(i) * 0.37f);
				const float height = 1.4f + 0.5f * wave;
				renderer.DrawBox(Ground(point, height), { length * 0.5f + 0.02f, height, 0.1f }, StormColor * (0.75f + 0.25f * wave), angle + HalfPi, 0.5f + 0.3f * pulse);
				for (const float band : { 0.9f, 2.6f, 4.6f })
					renderer.DrawBox(Ground(point + outward * band, 0.03f), { length * 0.5f + 0.05f, 0.02f, 0.9f + 0.1f * band }, glm::mix(StormColor * 0.55f, glm::vec3(0.12f, 0.08f, 0.16f), band / 5.0f), angle + HalfPi, 0.35f - 0.06f * band);
			}
			const float cell = 1.6f;
			for (float y = std::floor((focus.y - 22.0f) / cell) * cell; y < focus.y + 22.0f; y += cell)
				for (float x = std::floor((focus.x - 26.0f) / cell) * cell; x < focus.x + 26.0f; x += cell)
				{
					const uint32_t hash = uint32_t(int32_t(x / cell) * 73856093) ^ uint32_t(int32_t(y / cell) * 19349663);
					const float u = float(hash & 0xFFFF) / 65535.0f;
					const float v = float((hash >> 16) & 0xFFFF) / 65535.0f;
					const Vec2 point = { x + u * cell, y + v * cell };
					if (storm.Contains(point))
						continue;
					const float rise = std::fmod(m_Time * (0.25f + 0.2f * u) + v, 1.0f);
					renderer.DrawBox(Ground(point + FromAngle(m_Time * 0.8f + u * 6.0f) * 0.3f, 0.15f + 2.6f * rise), glm::vec3(0.07f * (1.0f - rise) + 0.02f), glm::mix(StormColor, glm::vec3(0.35f, 0.3f, 0.38f), v), m_Time + u * 10.0f, 0.8f * (1.0f - rise) + 0.2f);
				}
		}
		if (storm.Stage >= StormStageCount || storm.TargetRadius >= storm.Radius - 0.5f)
			return;
		const uint32_t dashes = std::clamp(uint32_t(2.0f * Pi * storm.TargetRadius / 1.4f), 8u, 2048u);
		for (uint32_t i = 0; i < dashes; i += 2)
		{
			const float angle = 2.0f * Pi * float(i) / float(dashes);
			const Vec2 point = storm.Target + FromAngle(angle) * storm.TargetRadius;
			if (LengthSquared(point - focus) > StormDrawRange * StormDrawRange)
				continue;
			const float length = 2.0f * Pi * storm.TargetRadius / float(dashes);
			renderer.DrawBox(Ground(point, 0.06f), { length * 0.5f, 0.02f, 0.06f }, glm::vec3(0.95f, 0.92f, 1.0f), angle + HalfPi, 0.6f);
		}
	}

	void SessionState::DrawMatchResults()
	{
		const GameStatus& status = m_Client.GetStatus();
		if (!IsPvpMode(status.Mode) || status.Match != MatchPhase::Ended || m_Paused)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const bool royale = status.Mode == GameMode::Royale;
		std::vector<PlayerInfo> players = m_Client.GetPlayers();
		if (royale)
			std::stable_sort(players.begin(), players.end(), [](const PlayerInfo& a, const PlayerInfo& b) { return (a.Placement == 0) != (b.Placement == 0) ? b.Placement == 0 : a.Placement != b.Placement ? a.Placement < b.Placement : a.Kills > b.Kills; });
		else
			std::stable_sort(players.begin(), players.end(), [](const PlayerInfo& a, const PlayerInfo& b) { return a.Kills != b.Kills ? a.Kills > b.Kills : a.Deaths < b.Deaths; });
		constexpr float RowHeight = 30.0f;
		const float height = 168.0f + float(players.size()) * RowHeight;
		const UiRect panel = UiRect::Centered({ ui.GetSize().x * 0.5f, ui.GetSize().y * 0.5f + 40.0f }, { 520.0f, height });
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
		draw.TextAligned({ header.X, header.Y, 60.0f, header.H }, royale ? "Place" : "#", headStyle, { 0.0f, 0.5f });
		draw.TextAligned({ header.X + 60.0f, header.Y, 200.0f, header.H }, "Player", headStyle, { 0.0f, 0.5f });
		draw.TextAligned({ header.Right() - 160.0f, header.Y, 70.0f, header.H }, "Kills", headStyle, { 0.5f, 0.5f });
		draw.TextAligned({ header.Right() - 80.0f, header.Y, 80.0f, header.H }, "Deaths", headStyle, { 0.5f, 0.5f });
		ui.Separator(table.CutTop(6.0f));
		for (size_t i = 0; i < players.size(); i++)
		{
			const PlayerInfo& player = players[i];
			const UiRect row = table.CutTop(RowHeight);
			const bool you = player.PlayerId == m_Client.GetPlayerId();
			if (you)
				draw.Rect(row, { .Fill = Rgba(1.0f, 0.85f, 0.5f, 0.07f), .Radius = 6.0f, .Border = WithAlpha(Theme::GoldDark, 0.6f), .BorderWidth = 1.0f });
			const std::string place = royale ? (player.Placement > 0 ? Ordinal(player.Placement) : "-") : std::to_string(i + 1);
			const bool first = royale ? player.Placement == 1 : i == 0;
			draw.TextAligned({ row.X + 8.0f, row.Y, 52.0f, row.H }, place, Theme::Bold(15.0f, first ? Theme::GoldLight : Theme::Text), { 0.0f, 0.5f });
			if (first)
				draw.Icon(UiIcon::Crown, { row.X + 40.0f, row.Center().y - 8.0f, 16.0f, 16.0f }, { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold });
			const UiColor color = WithAlpha(ColorOf(player.PlayerId));
			draw.Circle({ row.X + 68.0f, row.Center().y }, 5.0f, color, Rgba(0.0f, 0.0f, 0.0f, 0.6f), 1.0f);
			draw.TextFit({ row.X + 80.0f, row.Y, 220.0f, row.H }, player.Name + (you ? "  (you)" : ""), Theme::Bold(15.0f, Lighten(color, 0.45f)));
			draw.TextAligned({ row.Right() - 160.0f, row.Y, 70.0f, row.H }, std::to_string(player.Kills), Theme::Body(15.0f, Theme::Text), { 0.5f, 0.5f });
			draw.TextAligned({ row.Right() - 80.0f, row.Y, 80.0f, row.H }, std::to_string(player.Deaths), Theme::Body(15.0f, Theme::Muted), { 0.5f, 0.5f });
		}
		draw.TextAligned({ panel.X, panel.Bottom() - 32.0f, panel.W, 22.0f }, "Returning to Emberhold in " + std::to_string(int(std::ceil(status.MatchTime))), Theme::Body(14.0f, Theme::Faint), { 0.5f, 0.5f });
	}
}
