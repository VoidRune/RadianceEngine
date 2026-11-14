#include "SessionState.h"
#include "App/GameApp.h"
#include "Rendering/Models.h"
#include "UI/Palette.h"
#include "UI/Ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace Game
{
	namespace
	{
		template<typename... Arguments>
		std::string Format(const char* format, Arguments... arguments)
		{
			char text[192];
			std::snprintf(text, sizeof(text), format, arguments...);
			return text;
		}
	}

	void SessionState::DrawStationWindow()
	{
		if (!m_Station)
			return;
		switch (*m_Station)
		{
		case StationKind::Spellbook:
			DrawSpellbook();
			break;
		case StationKind::CalderaGate:
			DrawCalderaGate();
			break;
		case StationKind::ShardRift:
			DrawShardRift();
			break;
		case StationKind::Chess:
			DrawChess();
			break;
		default:
			break;
		}
	}

	void SessionState::DrawTitleOrnament(float center, float y, float titleWidth)
	{
		UiDrawList& draw = m_App.GetUi().Draw();
		for (const float side : { -1.0f, 1.0f })
		{
			const float start = center + side * (titleWidth * 0.5f + 20.0f);
			constexpr float Length = 96.0f;
			draw.Rect({ side < 0.0f ? start - Length : start, y, Length, 1.0f }, { .Fill = WithAlpha(Theme::AccentLight, side < 0.0f ? 0.0f : 0.75f), .FillBottom = WithAlpha(Theme::AccentLight, side < 0.0f ? 0.75f : 0.0f), .Horizontal = true });
			draw.Rect(UiRect::Centered({ start, y + 0.5f }, glm::vec2(14.0f)), { .Fill = WithAlpha(Theme::Accent, 0.45f), .Radius = 7.0f, .Softness = 5.0f, .Additive = 1.0f });
			draw.Diamond({ start, y + 0.5f }, 9.0f, Theme::AccentLight, WithAlpha(Theme::Ink, 0.9f), 1.0f);
			draw.Diamond({ start + side * 14.0f, y + 0.5f }, 4.5f, WithAlpha(Theme::Accent, 0.85f));
		}
	}

	void SessionState::DrawBanner()
	{
		if (m_BannerTime <= 0.0f || m_BannerTitle.empty())
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const float alpha = std::clamp(m_BannerTime / 0.8f, 0.0f, 1.0f) * std::clamp((4.0f - m_BannerTime) / 0.4f, 0.0f, 1.0f);
		const float y = ui.GetSize().y * 0.26f;
		TextStyle title = Theme::Title(54.0f);
		title.Tracking = 5.0f;
		title.Color = WithAlpha(Lighten(m_BannerColor, 0.35f), alpha);
		title.ColorBottom = WithAlpha(m_BannerColor, alpha);
		title.ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f * alpha);
		title.ShadowBlur = 10.0f;
		draw.Shadow(UiRect::Centered({ ui.GetSize().x * 0.5f, y + 30.0f }, { draw.MeasureWidth(m_BannerTitle, title) + 200.0f, 90.0f }), 45.0f, 60.0f, Rgba(0.0f, 0.0f, 0.0f, 0.5f * alpha), { 0.0f, 0.0f });
		draw.TextAligned({ 0.0f, y, ui.GetSize().x, 60.0f }, m_BannerTitle, title, { 0.5f, 0.5f });
		draw.TextAligned({ 0.0f, y + 62.0f, ui.GetSize().x, 26.0f }, m_BannerText, Theme::Floating(19.0f, WithAlpha(Theme::Text, alpha), UiFont::Body), { 0.5f, 0.5f });
	}

	void SessionState::DrawInteractionLabels()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const Level* level = m_Client.GetLevel();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (!level || !self || m_Overview)
			return;
		const auto label = [&](UiId id, Vec2 position, float lift, UiIcon icon, const std::string& title, const char* action, bool near) -> bool
		{
			const std::optional<glm::vec2> pixels = ProjectToScreen(Ground(position, lift));
			if (!pixels)
				return false;
			const glm::vec2 point = ui.ToLogical(*pixels);
			const TextStyle titleStyle = Theme::Bold(15.0f, Theme::AccentLight);
			const TextStyle actionStyle = Theme::Body(13.0f, Theme::Text);
			const float width = std::max(draw.MeasureWidth(title, titleStyle), action ? draw.MeasureWidth(action, actionStyle) : 0.0f) + 54.0f;
			const float height = action ? 46.0f : 30.0f;
			const UiRect box{ point.x - width * 0.5f, point.y - height, width, height };
			const UiInteraction interaction = ui.Interact(id, box);
			const bool lit = near || interaction.Hovered;
			draw.Rect(box.Translate({ 0.0f, 3.0f }), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.5f), .Softness = 8.0f, .Chamfer = 8.0f });
			draw.Rect(box, { .Fill = Rgba(0.06f, 0.05f, 0.11f, 0.92f), .FillBottom = Rgba(0.03f, 0.025f, 0.06f, 0.92f), .Border = WithAlpha(Theme::Accent, lit ? 1.0f : 0.6f), .BorderWidth = lit ? 1.8f : 1.2f, .Glow = WithAlpha(Theme::Accent, lit ? 0.12f : 0.0f), .GlowSize = 10.0f, .Chamfer = 8.0f});
			draw.Icon(icon, { box.X + 9.0f, box.Y + 6.0f, 18.0f, 18.0f }, { .Color = Theme::AccentLight, .ColorBottom = Theme::Accent });
			draw.Text({ box.X + 34.0f, box.Y + 5.0f }, title, titleStyle);
			if (action)
				draw.Text({ box.X + 34.0f, box.Y + 25.0f }, action, actionStyle);
			return interaction.Clicked;
		};
		for (size_t i = 0; i < level->Stations.size(); i++)
		{
			const Station& station = level->Stations[i];
			if (LengthSquared(station.Position - self->Position) > 81.0f)
				continue;
			const bool near = int32_t(i) == m_NearStation;
			const StationDef& def = GetStation(station.Kind);
			const std::string action = std::string("Press E to ") + def.Action;
			if (label(Ui::MakeId("stationlabel", i), station.Position, 2.6f, IconFor(station.Kind), def.Name, near ? action.c_str() : nullptr, near) && near)
				OpenStation(station.Kind);
		}
		if (const Landmark* observatory = level->FindLandmark(LandmarkKind::Observatory); observatory && m_AimPoint && m_Client.GetSession().Mode == GameMode::Hub && LengthSquared(self->Position - observatory->Position) <= observatory->Radius * observatory->Radius)
			for (const Constellation& star : level->Constellations)
			{
				if (LengthSquared(Vec2(m_AimPoint->x, m_AimPoint->z) - star.Position) > star.Radius * star.Radius)
					continue;
				const bool lit = ((m_Client.GetProfile().Starlit >> size_t(star.Spell)) & 1u) != 0;
				const std::string name = std::string(GetAbility(star.Spell).Name) + " constellation";
				const std::string hint = lit ? std::string("It burns for you") : std::string("Cast ") + GetAbility(star.Spell).Name + " at it";
				label(Ui::MakeId("starlabel", size_t(star.Spell)), star.Position, star.Height + 1.6f - GroundHeight(star.Position), IconFor(star.Spell), name, hint.c_str(), lit);
			}
		if (const Landmark* board = level->FindLandmark(LandmarkKind::FallBoard); board && m_Client.GetSession().Mode == GameMode::Hub && LengthSquared(board->Position - self->Position) <= board->Radius * board->Radius)
		{
			const std::string tally = Format("Falls today: %u", unsigned(m_Client.GetStatus().HubFalls));
			label(Ui::MakeId("fallboard"), board->Position, 2.2f, UiIcon::SkullCrossedBones, "Days since someone fell off: 0", tally.c_str(), false);
		}
	}
}
