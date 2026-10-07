#include "MainMenuState.h"
#include "App/GameApp.h"
#include "States/SessionState.h"
#include "UI/Ui.h"
#include <GameShared/Content/Items.h>
#include <GameShared/Save/CharacterProfile.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Game
{
	namespace
	{
		constexpr float PanelWidth = 480.0f;
		constexpr float PanelHeight = 566.0f;
		constexpr float PanelGap = 28.0f;
		constexpr float PanelTop = 196.0f;
		constexpr uint32_t Embers = 48;

		bool NameCharacter(uint32_t codepoint)
		{
			return codepoint >= 32 && codepoint != '/' && codepoint != '\\' && codepoint != ':' && codepoint != '*' && codepoint != '?' && codepoint != '"' && codepoint != '<' && codepoint != '>' && codepoint != '|';
		}

		bool DigitCharacter(uint32_t codepoint)
		{
			return codepoint >= '0' && codepoint <= '9';
		}

		bool AddressCharacter(uint32_t codepoint)
		{
			return codepoint > 32 && codepoint < 127;
		}

		float Noise(uint32_t seed)
		{
			seed = (seed ^ 61u) ^ (seed >> 16);
			seed *= 9u;
			seed ^= seed >> 4;
			seed *= 0x27D4EB2Du;
			seed ^= seed >> 15;
			return float(seed & 0xFFFFFF) / float(0xFFFFFF);
		}

		UiRect Section(Ui& ui, const UiRect& rect, std::string_view title, UiIcon icon)
		{
			ui.Panel(rect, 12.0f);
			UiDrawList& draw = ui.Draw();
			const UiRect header{ rect.X + 22.0f, rect.Y + 14.0f, rect.W - 44.0f, 34.0f };
			draw.Icon(icon, { header.X, header.Center().y - 13.0f, 26.0f, 26.0f }, { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f) });
			draw.TextAligned({ header.X + 36.0f, header.Y, header.W - 36.0f, header.H }, title, Theme::Title(24.0f), { 0.0f, 0.5f });
			ui.Separator({ rect.X + 16.0f, header.Bottom() + 4.0f, rect.W - 32.0f, 8.0f });
			return { rect.X + 24.0f, header.Bottom() + 20.0f, rect.W - 48.0f, rect.Bottom() - header.Bottom() - 38.0f };
		}

		bool ChoiceCard(Ui& ui, UiId id, const UiRect& rect, bool selected, UiIcon icon, std::string_view title, std::string_view description, float textSize)
		{
			const UiInteraction interaction = ui.Interact(id, rect);
			const float hover = ui.Animate(id, interaction.Hovered);
			const float chosen = ui.Animate(id ^ 0x51ED270B27ull, selected, 14.0f);
			UiDrawList& draw = ui.Draw();
			if (chosen > 0.01f)
				draw.Shadow(rect, 8.0f, 10.0f, WithAlpha(Theme::Gold, 0.22f * chosen), { 0.0f, 0.0f });
			draw.Rect(rect, { .Fill = glm::mix(Hex(0x1B191F), Hex(0x2C2418), chosen), .FillBottom = glm::mix(Hex(0x121115), Hex(0x1A140C), chosen), .Radius = 8.0f, .Border = glm::mix(WithAlpha(Theme::GoldDark, 0.55f + 0.35f * hover), Theme::Gold, chosen), .BorderWidth = 1.3f + 0.7f * chosen, .Glow = WithAlpha(Theme::GoldLight, 0.05f * hover + 0.08f * chosen), .GlowSize = 14.0f });
			const float iconSize = std::min(rect.H - 20.0f, 44.0f);
			const UiRect iconRect{ rect.X + 14.0f, rect.Y + 12.0f, iconSize, iconSize };
			draw.Icon(icon, iconRect, { .Color = glm::mix(Theme::Muted, Theme::GoldLight, std::max(chosen, hover * 0.6f)), .ColorBottom = glm::mix(Theme::Faint, Theme::Gold, std::max(chosen, hover * 0.6f)), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f) });
			const float textX = iconRect.Right() + 14.0f;
			draw.Text({ textX, rect.Y + 9.0f }, title, Theme::Bold(18.0f, glm::mix(Theme::Text, Theme::GoldLight, chosen)));
			draw.PushClip(rect.Shrink(2.0f));
			draw.TextWrapped({ textX, rect.Y + 33.0f, rect.Right() - textX - 12.0f, rect.H - 36.0f }, description, Theme::Body(textSize, Theme::Muted), 0.0f, 0.92f);
			draw.PopClip();
			if (selected)
				draw.Icon(UiIcon::CheckMark, { rect.Right() - 26.0f, rect.Y + 8.0f, 18.0f, 18.0f }, { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold });
			return interaction.Clicked;
		}

		void FieldLabel(Ui& ui, float x, float y, std::string_view text)
		{
			ui.Draw().Text({ x, y }, text, Theme::Label(14.0f));
		}
	}

	MainMenuState::MainMenuState(GameApp& app, std::string message)
		: m_App(app), m_Message(std::move(message))
	{
		const Settings& settings = app.GetSettings();
		m_Name = settings.PlayerName;
		m_Address = settings.JoinAddress;
		m_Port = std::to_string(settings.Port);
		m_Kit = int(settings.Kit);
		m_Unlimited = settings.UnlimitedMaterials;
		m_Volume = settings.Volume;
		m_Muted = settings.Muted;
		m_LagMs = float(settings.Simulation.LagMs);
		m_LossPercent = settings.Simulation.LossPercent;
	}

	void MainMenuState::RefreshCharacter()
	{
		if (m_CheckedName == m_Name)
			return;
		m_CheckedName = m_Name;
		m_HasSave = false;
		m_SaveSummary.clear();
		if (!m_App.GetSettings().SaveProfiles)
			return;
		std::string error;
		if (const std::optional<CharacterProfile> profile = LoadProfile(GetProfilePath(m_CheckedName), error))
		{
			m_HasSave = true;
			m_SaveSummary = DescribeProfile(*profile);
		}
	}

	void MainMenuState::DrawBackground(Ui& ui)
	{
		ui.SetLayer(UiLayer::Background);
		UiDrawList& draw = ui.Draw();
		const UiRect screen = ui.GetScreen();
		const float time = ui.GetTime();
		draw.Rect(screen, { .Fill = Hex(0x1C1522), .FillBottom = Hex(0x09080B) });
		draw.Shadow(UiRect::Centered({ screen.W * 0.5f, 120.0f }, { screen.W * 0.6f, 160.0f }), 80.0f, 220.0f, Rgba(1.0f, 0.62f, 0.25f, 0.12f), { 0.0f, 0.0f });
		draw.Shadow(UiRect::Centered({ screen.W * 0.5f, screen.H + 40.0f }, { screen.W * 0.9f, 120.0f }), 60.0f, 200.0f, Rgba(0.55f, 0.25f, 0.85f, 0.10f), { 0.0f, 0.0f });
		for (uint32_t i = 0; i < Embers; i++)
		{
			const float speed = 12.0f + 26.0f * Noise(i * 3u + 1u);
			const float travel = screen.H + 60.0f;
			const float y = screen.H + 30.0f - std::fmod(time * speed + Noise(i * 3u + 2u) * travel, travel);
			const float x = Noise(i * 3u) * screen.W + std::sin(time * 0.6f + float(i)) * 18.0f;
			const float flicker = 0.55f + 0.45f * std::sin(time * (2.0f + Noise(i) * 3.0f) + float(i) * 1.7f);
			const float size = 1.2f + 2.2f * Noise(i * 7u + 5u);
			const float fade = std::clamp((screen.H - y) / 260.0f, 0.0f, 1.0f) * std::clamp(y / 200.0f, 0.0f, 1.0f);
			const UiColor color = Rgba(1.0f, 0.7f + 0.2f * Noise(i * 11u), 0.35f, 0.65f * flicker * fade);
			draw.Shadow(UiRect::Centered({ x, y }, glm::vec2(size * 2.0f)), size, size * 3.5f, WithAlpha(color, 0.5f), { 0.0f, 0.0f });
			draw.Circle({ x, y }, size * 0.6f, color);
		}
		draw.Rect(screen, { .Glow = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .GlowSize = 240.0f });
	}

	void MainMenuState::DrawUi()
	{
		RefreshCharacter();
		Ui& ui = m_App.GetUi();
		DrawBackground(ui);

		ui.SetLayer(UiLayer::Hud);
		UiDrawList& draw = ui.Draw();
		const UiRect screen = ui.GetScreen();
		const float center = screen.W * 0.5f;
		draw.Shadow(UiRect::Centered({ center, 56.0f }, { 34.0f, 22.0f }), 11.0f, 22.0f, Rgba(1.0f, 0.7f, 0.3f, 0.35f), { 0.0f, 0.0f });
		draw.Icon(UiIcon::Crown, UiRect::Centered({ center, 54.0f }, { 46.0f, 46.0f }), { .Color = Theme::GoldLight, .ColorBottom = Theme::GoldDark, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
		TextStyle title = Theme::Title(78.0f);
		title.Tracking = 6.0f;
		title.Weight = 0.25f;
		title.ShadowOffset = { 0.0f, 3.0f };
		title.ShadowBlur = 4.0f;
		draw.Shadow(UiRect::Centered({ center, 118.0f }, { draw.MeasureWidth("RADIANCE", title) * 0.8f, 30.0f }), 15.0f, 40.0f, Rgba(1.0f, 0.62f, 0.2f, 0.16f), { 0.0f, 0.0f });
		draw.TextAligned({ 0.0f, 78.0f, screen.W, 80.0f }, "RADIANCE", title, { 0.5f, 0.5f });
		TextStyle subtitle = Theme::Title(17.0f);
		subtitle.Tracking = 5.0f;
		subtitle.Color = Theme::Muted;
		subtitle.ColorBottom = Theme::Faint;
		draw.TextAligned({ 0.0f, 152.0f, screen.W, 22.0f }, "CO-OP ACTION RPG", subtitle, { 0.5f, 0.5f });

		const float left = center - PanelWidth - PanelGap * 0.5f;
		DrawCharacterPanel(ui, { left, PanelTop, PanelWidth, PanelHeight });
		DrawPlayPanel(ui, { center + PanelGap * 0.5f, PanelTop, PanelWidth, PanelHeight });

		float y = PanelTop + PanelHeight + 18.0f;
		if (!m_Message.empty())
		{
			const TextStyle error = Theme::Body(16.0f, Theme::Danger);
			const float width = 2.0f * PanelWidth + PanelGap;
			const float height = draw.TextWrapped({ left + 12.0f, y + 10.0f, width - 24.0f, 200.0f }, m_Message, error, 0.5f);
			draw.Rect({ left, y, width, height + 20.0f }, { .Fill = Rgba(0.35f, 0.06f, 0.05f, 0.35f), .Radius = 8.0f, .Border = WithAlpha(Theme::Danger, 0.5f), .BorderWidth = 1.0f });
			y += height + 34.0f;
		}
		if (ui.Button(Ui::MakeId("quit"), UiRect::Centered({ center, std::max(y + 24.0f, screen.H - 56.0f) }, { 200.0f, 42.0f }), "Quit", ButtonKind::Normal, true, UiIcon::ExitDoor))
			m_App.Quit();
	}

	void MainMenuState::DrawCharacterPanel(Ui& ui, UiRect area)
	{
		UiRect content = Section(ui, area, "Character", UiIcon::PointyHat);
		UiDrawList& draw = ui.Draw();
		FieldLabel(ui, content.X, content.Y, "Name");
		content.CutTop(22.0f);
		ui.TextField(Ui::MakeId("name"), content.CutTop(44.0f), m_Name, 24, "Your name", NameCharacter);
		content.CutTop(18.0f);

		const bool showKits = !m_HasSave || m_Fresh;
		if (m_HasSave)
		{
			const UiRect status = content.CutTop(24.0f);
			draw.Icon(UiIcon::CheckedShield, { status.X, status.Y + 1.0f, 22.0f, 22.0f }, { .Color = Theme::Success, .ColorBottom = Darken(Theme::Success, 0.3f) });
			draw.TextAligned({ status.X + 30.0f, status.Y, status.W - 30.0f, status.H }, "Saved character found", Theme::Bold(17.0f, Theme::Success), { 0.0f, 0.5f });
			content.CutTop(draw.TextWrapped({ content.X, content.Y + 4.0f, content.W, 60.0f }, m_SaveSummary, Theme::Body(15.0f, Theme::Muted)) + 12.0f);
			ui.Checkbox(Ui::MakeId("fresh"), content.CutTop(28.0f), "Start over with a new starting kit (replaces the save)", m_Fresh);
			content.CutTop(14.0f);
		}
		else
		{
			const char* note = m_App.GetSettings().SaveProfiles ? "New character. Pick a starting kit, your gear is saved as you play." : "Saving is off for this run (--no-save). Pick a starting kit:";
			content.CutTop(draw.TextWrapped({ content.X, content.Y, content.W, 60.0f }, note, Theme::Body(15.0f, Theme::Muted)) + 14.0f);
		}
		if (!showKits)
		{
			draw.TextWrapped({ content.X, content.Y + 8.0f, content.W, 80.0f }, "Your saved gear, gems and backpack come with you into the game.", Theme::Body(15.0f, Theme::Faint));
			return;
		}
		const float cardHeight = std::min(86.0f, (content.H - 20.0f) / 3.0f);
		for (const KitDef& kit : Kits)
		{
			const UiRect card = content.CutTop(cardHeight);
			if (ChoiceCard(ui, Ui::MakeId("kit", uint64_t(kit.Id)), card, m_Kit == int(kit.Id), IconFor(kit.Id), kit.Name, kit.Description, 14.5f))
				m_Kit = int(kit.Id);
			content.CutTop(10.0f);
		}
	}

	void MainMenuState::DrawPlayPanel(Ui& ui, UiRect area)
	{
		UiRect content = Section(ui, area, "Play", UiIcon::CrossedSwords);
		UiDrawList& draw = ui.Draw();
		ui.Separator(content.CutTop(20.0f), "HOST A GAME");
		content.CutTop(8.0f);
		UiRect hub = content.CutTop(92.0f);
		draw.Rect(hub, { .Fill = Hex(0x1B191F), .FillBottom = Hex(0x121115), .Radius = 8.0f, .Border = WithAlpha(Theme::GoldDark, 0.7f), .BorderWidth = 1.2f });
		draw.Icon(UiIcon::AnvilImpact, { hub.X + 14.0f, hub.Y + 14.0f, 40.0f, 40.0f }, { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f) });
		draw.Text({ hub.X + 68.0f, hub.Y + 10.0f }, GetMapDef(MapKind::Hub).Name, Theme::Bold(18.0f, Theme::GoldLight));
		draw.TextWrapped({ hub.X + 68.0f, hub.Y + 34.0f, hub.W - 80.0f, hub.H - 38.0f }, "Your party starts in the hub town. Store loot in the stash, craft at the forge and lapidary, and pick missions at the board.", Theme::Body(14.0f, Theme::Muted));
		content.CutTop(8.0f);
		if (ui.Checkbox(Ui::MakeId("unlimited"), content.CutTop(28.0f), "Unlimited crafting materials (testing)", m_Unlimited))
			m_App.GetSettings().UnlimitedMaterials = m_Unlimited;
		content.CutTop(8.0f);
		UiRect hostRow = content.CutTop(44.0f);
		const UiRect portLabel = hostRow.CutLeft(38.0f);
		draw.TextAligned(portLabel, "Port", Theme::Label(15.0f), { 0.0f, 0.5f });
		ui.TextField(Ui::MakeId("port"), hostRow.CutLeft(92.0f), m_Port, 5, "27015", DigitCharacter);
		hostRow.CutLeft(12.0f);
		if (ui.Button(Ui::MakeId("host"), hostRow, "Host game", ButtonKind::Primary, true, UiIcon::CrossedSwords))
			StartSession(true);

		content.CutTop(20.0f);
		ui.Separator(content.CutTop(20.0f), "JOIN A GAME");
		content.CutTop(10.0f);
		UiRect joinRow = content.CutTop(44.0f);
		const UiRect joinButton = joinRow.CutRight(150.0f);
		joinRow.CutRight(12.0f);
		ui.TextField(Ui::MakeId("address"), joinRow, m_Address, 63, "Host address", AddressCharacter);
		if (ui.Button(Ui::MakeId("join"), joinButton, "Join game", ButtonKind::Normal, !m_Address.empty(), UiIcon::MagicPortal))
			StartSession(false);

		content.CutTop(20.0f);
		ui.Separator(content.CutTop(20.0f), "SETTINGS");
		content.CutTop(8.0f);
		char text[32];
		const auto sliderRow = [&](std::string_view label)
		{
			UiRect row = content.CutTop(32.0f);
			draw.TextAligned(row.CutLeft(132.0f), label, Theme::Body(15.0f, Theme::Text), { 0.0f, 0.5f });
			return row;
		};
		std::snprintf(text, sizeof(text), "%.0f%%", double(m_Volume) * 100.0);
		bool audioChanged = ui.Slider(Ui::MakeId("volume"), sliderRow("Volume"), m_Volume, 0.0f, 1.0f, text);
		audioChanged |= ui.Checkbox(Ui::MakeId("mute"), content.CutTop(30.0f), "Mute all sound", m_Muted);
		if (audioChanged)
		{
			m_App.GetSettings().Volume = m_Volume;
			m_App.GetSettings().Muted = m_Muted;
			m_App.ApplyAudioSettings();
		}
		std::snprintf(text, sizeof(text), "%.0f ms", double(m_LagMs));
		if (ui.Slider(Ui::MakeId("lag"), sliderRow("Simulated lag"), m_LagMs, 0.0f, 500.0f, text))
			m_LagMs = std::round(m_LagMs / 5.0f) * 5.0f;
		std::snprintf(text, sizeof(text), "%.1f%%", double(m_LossPercent));
		if (ui.Slider(Ui::MakeId("loss"), sliderRow("Simulated loss"), m_LossPercent, 0.0f, 25.0f, text))
			m_LossPercent = std::round(m_LossPercent * 2.0f) * 0.5f;
	}

	void MainMenuState::StartSession(bool host)
	{
		Settings& settings = m_App.GetSettings();
		settings.PlayerName = m_Name.empty() ? "Player" : m_Name;
		settings.JoinAddress = m_Address;
		const int port = m_Port.empty() ? int(DefaultPort) : std::atoi(m_Port.c_str());
		settings.Port = uint16_t(std::clamp(port, 1024, 65535));
		settings.Kit = StarterKit(m_Kit);
		settings.Simulation = { int32_t(m_LagMs), m_LossPercent };

		SessionConfig config;
		config.Host = host;
		config.Port = settings.Port;
		config.Address = settings.JoinAddress;
		config.Map = settings.Map;
		config.StartMode = GameMode::Hub;
		config.Profile = m_App.LoadCharacter(settings.PlayerName, settings.Kit, m_HasSave && m_Fresh);
		m_App.ChangeState(std::make_unique<SessionState>(m_App, config));
	}
}
