#include "MainMenuState.h"
#include "App/GameApp.h"
#include "Audio/GameAudio.h"
#include "Online/SteamPlatform.h"
#include "Rendering/MenuBackdrop.h"
#include "States/SessionState.h"
#include "UI/SettingsUi.h"
#include "UI/Ui.h"
#include <GameShared/Content/Items.h>
#include <GameShared/Save/CharacterProfile.h>
#include <RadianceEngine/Window/Input.h>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>

namespace Game
{
	namespace
	{
		constexpr float MenuX = 64.0f;
		constexpr float MenuTop = 300.0f;
		constexpr float PageX = 648.0f;
		constexpr float IdleSeconds = 20.0f;
		constexpr float OrbitHold = 2.5f;
		constexpr float PageWidth = 480.0f;
		constexpr float PageTop = 112.0f;
		constexpr float PageHeight = 676.0f;
		constexpr float SettingsPageHeight = 814.0f;

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

		UiRect PageHeader(Ui& ui, UiRect area, std::string_view title, std::string_view subtitle)
		{
			UiDrawList& draw = ui.Draw();
			TextStyle style = Theme::Title(30.0f);
			style.Tracking = 2.5f;
			draw.Text({ area.X, area.Y }, title, style);
			draw.TextWrapped({ area.X, area.Y + 44.0f, area.W, 40.0f }, subtitle, Theme::Body(15.0f, Theme::Muted));
			ui.Separator({ area.X, area.Y + 78.0f, area.W, 10.0f });
			area.CutTop(104.0f);
			return area;
		}

		void FieldLabel(Ui& ui, UiRect& area, std::string_view text)
		{
			ui.Draw().Text({ area.X, area.Y }, text, Theme::Caps(13.0f, Theme::Accent, 1.6f));
			area.CutTop(24.0f);
		}
	}

	MainMenuState::MainMenuState(GameApp& app, std::string message, uint8_t page)
		: m_App(app), m_Message(std::move(message)), m_Page(Page(page))
	{
		const Settings& settings = app.GetSettings();
		m_Name = settings.PlayerName;
		m_Address = settings.JoinAddress;
		m_Port = std::to_string(settings.Port);
		m_Kit = int(settings.Kit);
		m_Unlimited = settings.UnlimitedMaterials;
		m_FriendsCanJoin = settings.FriendsCanJoin;
		app.GetGameAudio().EnterMenu();
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
			m_Style = profile->Style;
		}
	}

	void MainMenuState::OpenPage(Page page)
	{
		if (page == m_Page)
			return;
		static constexpr float PageTones[] = { 1.0f, 1.0f, 1.1225f, 0.8409f, 0.7492f, 0.9439f };
		GameAudio& audio = m_App.GetGameAudio();
		audio.PlayUi(Cue::UiShatter, page == Page::None ? 0.5f : 0.8f);
		if (page == Page::None)
			audio.PlayUi(Cue::UiClose, 0.8f);
		else
			audio.PlayUi(Cue::UiOpen, 0.8f, PageTones[size_t(page)], 0.04f);
		m_Page = page;
		m_Reveal = 0.0f;
	}

	void MainMenuState::Update(float dt)
	{
		m_Time += dt;
		SteamPlatform& steam = m_App.GetSteam();
		steam.SetPresence({});
		if (std::string error = steam.TakeError(); !error.empty())
			m_Message = std::move(error);
		const Ui& ui = m_App.GetUi();
		const glm::vec2 pointer = ui.GetMouse();
		const glm::vec2 mouse = glm::clamp(pointer / glm::max(ui.GetSize(), glm::vec2(1.0f)) * 2.0f - 1.0f, glm::vec2(-1.0f), glm::vec2(1.0f));
		m_Parallax += (mouse - m_Parallax) * (1.0f - std::exp(-dt * 1.2f));
		m_Reveal = std::min(m_Reveal + dt / 0.6f, 1.0f);
		const bool open = m_Page != Page::None;
		m_PageShown += (float(open) - m_PageShown) * (1.0f - std::exp(-dt * (open ? 3.2f : 5.0f)));

		const bool rightDown = Rdn::Input::IsKeyDown(Rdn::KeyCode::MouseRight);
		const bool moved = glm::length(pointer - m_LastPointer) > 0.5f;
		const bool active = moved || rightDown || Rdn::Input::IsKeyDown(Rdn::KeyCode::MouseLeft) || !Rdn::Input::GetTypedCharacters().empty()
			|| Rdn::Input::IsKeyPressed(Rdn::KeyCode::Escape) || Rdn::Input::IsKeyPressed(Rdn::KeyCode::Enter) || Rdn::Input::GetScrollVertical() != 0.0f;
		m_Idle = active ? 0.0f : m_Idle + dt;
		const bool touring = !open && m_Idle > IdleSeconds;
		m_Tour += ((touring ? 1.0f : 0.0f) - m_Tour) * (1.0f - std::exp(-dt * (touring ? 0.45f : 6.0f)));

		if (rightDown && (m_Dragging || !ui.WantsMouse()))
		{
			if (m_Dragging && dt > 0.0f)
			{
				const glm::vec2 step = (pointer - m_LastPointer) * glm::vec2(0.0025f, 0.0018f);
				m_OrbitTarget += step;
				m_OrbitFling = glm::mix(m_OrbitFling, step / dt, 1.0f - std::exp(-dt * 12.0f));
			}
			m_Dragging = true;
			m_SinceDrag = 0.0f;
		}
		else
		{
			m_Dragging = false;
			m_SinceDrag += dt;
			m_OrbitTarget += m_OrbitFling * 0.35f * dt;
			m_OrbitFling *= std::exp(-dt * 3.5f);
		}
		m_OrbitTarget.y = std::clamp(m_OrbitTarget.y, -0.55f, 0.7f);
		const bool returning = !m_Dragging && m_Tour < 0.02f && m_SinceDrag > OrbitHold;
		if (returning)
		{
			const float turns = std::round(m_OrbitTarget.x / 6.2831853f) * 6.2831853f;
			m_OrbitTarget.x -= turns;
			m_Orbit.x -= turns;
			m_OrbitTarget = glm::vec2(0.0f);
			m_OrbitFling = glm::vec2(0.0f);
		}
		const float omega = returning ? 2.4f : 5.5f;
		const glm::vec2 pull = omega * omega * (m_OrbitTarget - m_Orbit) - 2.0f * omega * m_OrbitVelocity;
		m_OrbitVelocity += pull * dt;
		m_Orbit += m_OrbitVelocity * dt;

		m_LastPointer = pointer;
	}

	bool MainMenuState::Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex)
	{
		static_assert(uint8_t(Page::Host) == uint8_t(BackdropMood::Kindle) && uint8_t(Page::Settings) == uint8_t(BackdropMood::Attune) && uint8_t(Page::Wardrobe) == uint8_t(BackdropMood::Wardrobe));
		const Ui& ui = m_App.GetUi();
		BackdropView view;
		view.Time = ui.GetTime() + 30.0f;
		view.Mood = BackdropMood(m_Page);
		view.Mouse = ui.GetMouse();
		view.Parallax = m_Parallax;
		view.Fade = std::min(m_Time / 1.6f, 1.0f);
		view.UiScale = ui.GetScale();
		view.Title = m_TitleGlyphs;
		view.TitleTime = m_Time;
		view.Cloud = glm::vec4(m_PageRect.X - 34.0f, m_PageRect.Y - 30.0f, m_PageRect.W + 68.0f, m_PageRect.H + 60.0f);
		view.PageShown = m_PageShown;
		view.Orbit = m_Orbit;
		const float tour = glm::smoothstep(0.0f, 1.0f, m_Tour);
		view.Tour = tour;
		view.TitleShown = 1.0f - tour;
		return m_App.GetMenuBackdrop().Render(graph, target, extent, frameIndex, view);
	}

	void MainMenuState::DrawUi()
	{
		RefreshCharacter();
		Ui& ui = m_App.GetUi();
		ui.SetMotion(1.0f);
		ui.SetOpacity(1.0f - glm::smoothstep(0.0f, 1.0f, m_Tour));
		if (m_Page != Page::None && !ui.WantsKeyboard() && Rdn::Input::IsKeyPressed(Rdn::KeyCode::Escape))
			OpenPage(Page::None);
		ui.SetLayer(UiLayer::Hud);
		DrawTitle(ui);
		DrawMenu(ui, { MenuX, MenuTop, 420.0f, ui.GetSize().y - MenuTop - 24.0f });
		if (m_Page != Page::None)
		{
			ui.SetLayer(UiLayer::Windows);
			DrawPage(ui);
		}
	}

	void MainMenuState::DrawTitle(Ui& ui)
	{
		UiDrawList& draw = ui.Draw();
		const glm::vec2 anchor(MenuX + 300.0f, 140.0f);
		const UiRect titleRect{ anchor.x - 320.0f, anchor.y - 52.0f, 640.0f, 84.0f };
		TextStyle title = Theme::Title(78.0f);
		title.Tracking = 10.0f;
		title.ShadowColor = UiColor(0.0f);
		title.GlowColor = UiColor(0.0f);
		UiDrawList glyphs(draw.GetAssets());
		glyphs.Reset(ui.GetScale());
		glyphs.TextAligned(titleRect, "RADIANCE", title, { 0.5f, 0.5f });
		m_TitleGlyphs = glyphs.GetPrimitives();
		m_TitleWarmth.resize(m_TitleGlyphs.size(), 0.0f);
		const float cooling = std::exp(-ui.GetDeltaTime() * 1.2f);
		for (size_t i = 0; i < m_TitleGlyphs.size(); i++)
		{
			UiPrimitive glyph = m_TitleGlyphs[i];
			const float progress = std::clamp((m_Time - 0.5f - 0.13f * float(i)) / 1.6f, 0.0f, 1.0f);
			const glm::vec2 offset = glm::vec2(glyph.Rect) - ui.GetMouse();
			m_TitleWarmth[i] = std::max(std::exp(-glm::dot(offset, offset) / (2.0f * 110.0f * 110.0f)), m_TitleWarmth[i] * cooling);
			if (progress <= 0.0f)
				continue;
			const float warmth = m_TitleWarmth[i];
			const float ease = 1.0f - (1.0f - progress) * (1.0f - progress) * (1.0f - progress);
			glyph.Rect.y += (1.0f - ease) * 12.0f;
			const float flare = std::sin(3.14159265f * std::min(progress * 1.3f, 1.0f));
			UiPrimitive halo = glyph;
			const UiColor haloColor = WithAlpha(glm::mix(Hex(0xC8DCFF), Hex(0xFFC27A), warmth), (0.6f * flare + 0.1f * ease + 0.3f * warmth) * ease);
			halo.Colors.x = PackColor(haloColor);
			halo.Colors.y = PackColor(haloColor);
			halo.Params = glm::vec4(0.0f, 0.0f, 9.0f, 0.0f);
			halo.Info.z = 0u;
			halo.Info.w = 255u;
			draw.Primitive(halo);
			const float alpha = glm::smoothstep(0.1f, 0.8f, progress);
			glyph.Colors.x = PackColor(WithAlpha(glm::mix(Hex(0xFFF8EC), Hex(0xFFDDA6), warmth), (0.9f + 0.1f * warmth) * alpha));
			glyph.Colors.y = PackColor(WithAlpha(glm::mix(Hex(0xEAD7AE), Hex(0xF5B860), warmth), (0.82f + 0.18f * warmth) * alpha));
			draw.Primitive(glyph);
		}
		TextStyle subtitle = Theme::Caps(15.0f, Theme::Muted, 6.0f);
		subtitle.ColorBottom = Theme::Faint;
		draw.SetHalo(Rgba(0.0f, 0.0f, 0.02f, 0.45f));
		draw.TextAligned({ anchor.x - 200.0f, anchor.y + 34.0f, 400.0f, 22.0f }, "co-op action rpg", subtitle, { 0.5f, 0.5f });
		draw.SetHalo(UiColor(0.0f));
		ui.Separator({ anchor.x - 140.0f, anchor.y + 64.0f, 280.0f, 10.0f });
	}

	void MainMenuState::DrawMenu(Ui& ui, UiRect)
	{
		UiDrawList& draw = ui.Draw();
		const float height = ui.GetSize().y;
		const glm::vec2 hub(-470.0f, height * 0.56f);
		constexpr float Radius = 640.0f;
		constexpr float Step = 0.105f;
		const std::string wanderer = (m_Name.empty() ? std::string("Nameless") : m_Name) + "  \xC2\xB7  " + (m_HasSave && !m_Fresh ? std::string("saved character") : std::string(GetKit(StarterKit(m_Kit)).Name));
		struct Entry
		{
			Page Target;
			const char* Title;
			std::string Subtitle;
			float Angle;
		};
		const Entry entries[] = {
			{ Page::Host, "Open a Rift", "Host a game for your party", -0.3f },
			{ Page::Join, "Follow a Rift", "Join a game someone is hosting", -0.3f + Step },
			{ Page::Character, "The Wanderer", wanderer, -0.3f + Step * 2.0f },
			{ Page::Wardrobe, "Wardrobe", std::string(GetOutfit(m_Style.Body).Name) + "  \xC2\xB7  " + GetFocus(m_Style.Cast).Name, -0.3f + Step * 3.0f },
			{ Page::Settings, "Attunement", "Sound, visuals and diagnostics", -0.3f + Step * 4.0f },
			{ Page::None, "Depart", "Quit to desktop", -0.3f + Step * 5.0f + 0.05f },
		};
		const auto direction = [](float angle) { return glm::vec2(std::cos(angle), std::sin(angle)); };
		const auto entryRect = [&](const Entry& entry)
		{
			const glm::vec2 node = hub + direction(entry.Angle) * Radius;
			return UiRect{ node.x - 14.0f, node.y - 19.0f, 430.0f, 58.0f };
		};

		draw.Rect({ 0.0f, 0.0f, 760.0f, height }, { .Fill = Rgba(0.0f, 0.0f, 0.015f, 0.62f), .FillBottom = Rgba(0.0f, 0.0f, 0.015f, 0.0f), .Horizontal = true });
		const float intro = glm::smoothstep(0.2f, 2.6f, m_Time);
		const UiRect orbit = UiRect::Centered(hub, glm::vec2(Radius * 2.0f));
		draw.Rect(orbit, { .Radius = Radius, .Glow = WithAlpha(Theme::Astral, 0.1f * intro), .GlowSize = 4.0f, .Additive = 1.0f });
		draw.Rect(orbit, { .Radius = Radius, .Border = WithAlpha(Theme::Accent, 0.32f * intro), .BorderWidth = 1.0f });
		draw.Rect(orbit.Expand(46.0f), { .Radius = Radius + 46.0f, .Border = WithAlpha(Theme::Edge, 0.12f * intro), .BorderWidth = 1.0f });
		for (int i = 0; i < 3; i++)
		{
			const float angle = std::fmod(m_Time * (0.014f + 0.006f * float(i)) + float(i) * 0.31f, 1.4f) - 0.7f;
			const float fade = intro * glm::smoothstep(0.7f, 0.5f, std::abs(angle));
			const glm::vec2 moon = hub + direction(angle) * (Radius + (i == 1 ? 46.0f : 0.0f));
			draw.Rect(UiRect::Centered(moon, glm::vec2(18.0f)), { .Fill = WithAlpha(Theme::Astral, 0.28f * fade), .Radius = 9.0f, .Softness = 7.0f, .Additive = 1.0f });
			draw.Rect(UiRect::Centered(moon, glm::vec2(3.0f)), { .Fill = WithAlpha(Lighten(Theme::Astral, 0.6f), 0.9f * fade), .Radius = 1.5f });
		}

		for (size_t i = 0; i < std::size(entries); i++)
		{
			const Entry& entry = entries[i];
			const float appear = std::clamp((m_Time - 0.6f - 0.12f * float(i)) / 0.7f, 0.0f, 1.0f);
			const float ease = 1.0f - (1.0f - appear) * (1.0f - appear);
			const UiRect rect = entryRect(entry);
			draw.PushClip({ rect.X - 20.0f, rect.Y - 14.0f, (rect.W + 40.0f) * ease, rect.H + 28.0f });
			const bool quit = entry.Target == Page::None;
			if (ui.MenuEntry(quit ? Ui::MakeId("menu.quit") : Ui::MakeId("menu", i), rect, entry.Title, entry.Subtitle, !quit && m_Page == entry.Target))
			{
				if (quit)
					m_App.Quit();
				else
					OpenPage(m_Page == entry.Target ? Page::None : entry.Target);
			}
			draw.PopClip();
		}

		if (!m_Message.empty())
		{
			const UiRect last = entryRect(entries[std::size(entries) - 1]);
			const TextStyle style = Theme::Body(15.5f, Hex(0xFFB8C2));
			const float width = 330.0f;
			const float lines = float(draw.WrapText(m_Message, style, width).size()) * draw.LineHeight(style);
			const UiRect box{ last.X + 20.0f, last.Bottom() + 18.0f, width + 34.0f, lines + 22.0f };
			draw.Rect(box, { .Fill = Rgba(0.3f, 0.04f, 0.1f, 0.45f), .FillBottom = Rgba(0.15f, 0.02f, 0.06f, 0.45f), .Radius = 2.0f, .Border = WithAlpha(Theme::Danger, 0.65f), .BorderWidth = 1.0f });
			draw.Rect(UiRect::Centered({ box.X, box.Center().y }, glm::vec2(10.0f)), { .Fill = Theme::Ink, .Radius = 5.0f, .Border = Theme::Danger, .BorderWidth = 1.0f });
			draw.TextWrapped({ box.X + 18.0f, box.Y + 11.0f, width, lines }, m_Message, style);
		}
		if (!m_App.GetSettings().SaveProfiles)
			draw.Text({ MenuX + 34.0f, ui.GetSize().y - 36.0f }, "Saving is off for this run", Theme::Body(13.5f, Theme::Faint));
	}

	void MainMenuState::DrawPage(Ui& ui)
	{
		const float ease = 1.0f - std::pow(1.0f - m_Reveal, 3.0f);
		const bool settings = m_Page == Page::Settings;
		const float top = settings ? std::clamp(ui.GetSize().y - 24.0f - SettingsPageHeight, 24.0f, PageTop) : PageTop;
		const UiRect panel{ PageX, top, PageWidth, std::min(settings ? SettingsPageHeight : PageHeight, ui.GetSize().y - top - 24.0f) };
		const float height = panel.H * ease;
		UiDrawList& draw = ui.Draw();
		draw.PushClip({ panel.X - 40.0f, panel.Center().y - height * 0.5f - 40.0f, panel.W + 80.0f, height + 80.0f });
		ui.Block(panel);
		m_PageRect = panel;
		draw.SetHalo(Rgba(0.0f, 0.0f, 0.02f, 0.4f));
		const UiRect content = panel.Shrink(34.0f, 30.0f);
		switch (m_Page)
		{
		case Page::Host:
			DrawHostPage(ui, content);
			break;
		case Page::Join:
			DrawJoinPage(ui, content);
			break;
		case Page::Character:
			DrawCharacterPage(ui, content);
			break;
		case Page::Wardrobe:
			DrawWardrobePage(ui, content);
			break;
		case Page::Settings:
			DrawSettings(ui, m_App, PageHeader(ui, content, "Attunement", "Changes apply at once and are remembered."));
			break;
		default:
			break;
		}
		draw.SetHalo(UiColor(0.0f));
		draw.PopClip();
	}

	void MainMenuState::DrawHostPage(Ui& ui, UiRect area)
	{
		const bool steam = m_App.GetSteam().IsReady();
		area = PageHeader(ui, area, "Open a Rift", steam ? "Host a game. Steam friends can join you, or use your address." : "Host a game. Friends follow your rift with your address and port.");
		UiDrawList& draw = ui.Draw();
		const UiRect hub = area.CutTop(108.0f);
		ui.Card(hub, 0.0f, 0.0f);
		const glm::vec2 seal(hub.X + 50.0f, hub.Center().y);
		draw.Sigil(seal, 36.0f, { .Color = WithAlpha(Theme::Gold, 0.55f), .Accent = WithAlpha(Theme::AccentLight, 0.5f), .Speed = 0.8f, .LineWidth = 1.0f, .Glow = 0.45f, .Layers = SigilRings });
		draw.Icon(UiIcon::AnvilImpact, UiRect::Centered(seal, glm::vec2(32.0f)), { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.7f), .GlowColor = WithAlpha(Theme::Gold, 0.3f) });
		draw.Text({ hub.X + 102.0f, hub.Y + 14.0f }, GetMapDef(MapKind::Hub).Name, Theme::Caps(19.0f, Theme::GoldLight, 1.5f));
		draw.TextWrapped({ hub.X + 102.0f, hub.Y + 44.0f, hub.W - 116.0f, hub.H - 48.0f }, "Your party gathers in the hub town. Store loot in the stash, craft at the forge and lapidary, and choose missions at the board.", Theme::Body(14.5f, Theme::Muted));
		area.CutTop(24.0f);
		FieldLabel(ui, area, "PORT");
		UiRect portRow = area.CutTop(44.0f);
		ui.TextField(Ui::MakeId("port"), portRow.CutLeft(140.0f), m_Port, 5, "27020", DigitCharacter);
		portRow.CutLeft(18.0f);
		draw.TextWrapped({ portRow.X, portRow.Y + 3.0f, portRow.W, portRow.H }, "UDP. Others need to reach it to follow you in.", Theme::Body(14.0f, Theme::Muted));
		area.CutTop(20.0f);
		if (ui.Checkbox(Ui::MakeId("unlimited"), area.CutTop(30.0f), "Unlimited crafting materials (testing)", m_Unlimited))
			m_App.GetSettings().UnlimitedMaterials = m_Unlimited;
		if (steam)
		{
			area.CutTop(10.0f);
			ui.Checkbox(Ui::MakeId("friends"), area.CutTop(30.0f), "Steam friends can join without an invite", m_FriendsCanJoin);
		}
		const UiRect go = area.CutBottom(54.0f);
		const std::string who = "You enter as " + (m_Name.empty() ? std::string("Player") : m_Name) + ", " + (m_HasSave && !m_Fresh ? std::string("with your saved gear") : std::string("the ") + GetKit(StarterKit(m_Kit)).Name);
		draw.TextAligned(area.CutBottom(34.0f), who, Theme::Body(14.5f, Theme::Muted), { 0.5f, 0.5f });
		if (ui.Button(Ui::MakeId("host"), go, "Open the Rift", ButtonKind::Primary, true, UiIcon::MagicPortal))
			StartSession(true);
	}

	void MainMenuState::DrawJoinPage(Ui& ui, UiRect area)
	{
		area = PageHeader(ui, area, "Follow a Rift", "Join a game that someone else is hosting.");
		UiDrawList& draw = ui.Draw();
		FieldLabel(ui, area, "HOST ADDRESS");
		ui.TextField(Ui::MakeId("address"), area.CutTop(44.0f), m_Address, 63, "Address, like 192.168.1.20", AddressCharacter);
		area.CutTop(12.0f);
		draw.TextWrapped(area.CutTop(44.0f), "Add :port after the address when the host doesn't use the default port.", Theme::Body(14.0f, Theme::Muted));
		const UiRect go = area.CutBottom(54.0f);
		const std::string who = "You enter as " + (m_Name.empty() ? std::string("Player") : m_Name) + ", " + (m_HasSave && !m_Fresh ? std::string("with your saved gear") : std::string("the ") + GetKit(StarterKit(m_Kit)).Name);
		draw.TextAligned(area.CutBottom(34.0f), who, Theme::Body(14.5f, Theme::Muted), { 0.5f, 0.5f });
		if (ui.Button(Ui::MakeId("join"), go, "Step Through", ButtonKind::Primary, !m_Address.empty(), UiIcon::MagicPortal))
			StartSession(false);
		if (m_App.GetSteam().IsReady())
		{
			area.CutTop(10.0f);
			DrawFriendGames(ui, area);
		}
	}

	void MainMenuState::DrawFriendGames(Ui& ui, UiRect area)
	{
		constexpr float Row = 50.0f;
		UiDrawList& draw = ui.Draw();
		SteamPlatform& steam = m_App.GetSteam();
		FieldLabel(ui, area, "STEAM FRIENDS PLAYING");
		if (steam.IsJoining())
		{
			draw.TextWrapped(area.CutTop(30.0f), "Joining through Steam...", Theme::Body(15.0f, Theme::AccentLight));
			return;
		}
		uint32_t shown = 0;
		for (const SteamFriend& person : steam.GetFriends())
		{
			if (person.Activity != FriendActivity::Joinable && person.Activity != FriendActivity::Playing)
				break;
			if (area.H < Row)
				break;
			UiRect row = area.CutTop(Row).Shrink(0.0f, 4.0f);
			ui.Card(row, 0.0f, 0.0f);
			row = row.Shrink(14.0f, 0.0f);
			const UiRect button = row.CutRight(120.0f).Shrink(0.0f, 5.0f);
			if (person.Activity == FriendActivity::Joinable && ui.Button(Ui::MakeId("friend.join", person.Id), button, "Join", ButtonKind::Primary))
			{
				m_App.SaveCharacter(PrepareProfile());
				steam.JoinLobby(person.Lobby);
			}
			draw.TextFit({ row.X, row.Y + 4.0f, row.W - 12.0f, 20.0f }, person.Name, Theme::Bold(16.0f, Theme::Text));
			draw.TextFit({ row.X, row.Y + 23.0f, row.W - 12.0f, 16.0f }, person.Activity == FriendActivity::Joinable ? "In a game you can join" : "Playing, not in a party", Theme::Body(13.0f, Theme::Muted));
			shown++;
		}
		if (shown == 0)
			draw.TextWrapped(area.CutTop(44.0f), "No Steam friends are playing right now. When one is, join them here or from your Steam friends list.", Theme::Body(14.0f, Theme::Muted));
	}

	void MainMenuState::DrawWardrobePage(Ui& ui, UiRect area)
	{
		area = PageHeader(ui, area, "Wardrobe", "How others see you. The pause menu changes it mid-game too.");
		FieldLabel(ui, area, "OUTFIT");
		DrawOutfitPicker(ui, area.CutTop(58.0f), m_Style.Body);
		area.CutTop(18.0f);
		FieldLabel(ui, area, "CASTING FOCUS");
		DrawFocusPicker(ui, area.CutTop(58.0f), m_Style.Cast);
		area.CutTop(18.0f);
		FieldLabel(ui, area, "AURA");
		DrawAuraPicker(ui, area.CutTop(58.0f), m_Style.Glow);
		area.CutTop(18.0f);
		FieldLabel(ui, area, "HEADWEAR");
		DrawHeadpiecePicker(ui, area.CutTop(58.0f), m_Style.Crown);
	}

	void MainMenuState::DrawCharacterPage(Ui& ui, UiRect area)
	{
		const bool saving = m_App.GetSettings().SaveProfiles;
		area = PageHeader(ui, area, "The Wanderer", saving ? "Your gear, stash and materials are bound to this name." : "Saving is off for this run, nothing you find is kept.");
		UiDrawList& draw = ui.Draw();
		FieldLabel(ui, area, "NAME");
		ui.TextField(Ui::MakeId("name"), area.CutTop(44.0f), m_Name, 24, "Your name", NameCharacter);
		area.CutTop(18.0f);
		if (m_HasSave)
		{
			const UiRect status = area.CutTop(26.0f);
			draw.Icon(UiIcon::CheckedShield, { status.X, status.Y + 2.0f, 22.0f, 22.0f }, { .Color = Theme::Success, .ColorBottom = Darken(Theme::Success, 0.3f) });
			draw.TextAligned({ status.X + 32.0f, status.Y, status.W - 32.0f, status.H }, "A saved character answers to this name", Theme::Bold(16.5f, Theme::Success), { 0.0f, 0.5f });
			area.CutTop(draw.TextWrapped({ area.X, area.Y + 4.0f, area.W, 60.0f }, m_SaveSummary, Theme::Body(14.5f, Theme::Muted)) + 12.0f);
			ui.Checkbox(Ui::MakeId("fresh"), area.CutTop(30.0f), "Start over with a new kit (replaces the save)", m_Fresh);
			area.CutTop(16.0f);
			if (!m_Fresh)
			{
				draw.TextWrapped(area, "Your saved gear, gems and backpack come with you.", Theme::Body(14.5f, Theme::Muted));
				return;
			}
		}
		FieldLabel(ui, area, "STARTING KIT");
		constexpr float Gap = 12.0f;
		const float kits = float(std::size(Kits));
		const float cardWidth = (area.W - Gap * (kits - 1.0f)) / kits;
		UiRect row = area.CutTop(150.0f);
		std::optional<size_t> hovered;
		for (size_t i = 0; i < std::size(Kits); i++)
		{
			const KitDef& kit = Kits[i];
			const UiRect card = row.CutLeft(cardWidth);
			row.CutLeft(Gap);
			const UiId id = Ui::MakeId("kit", uint64_t(kit.Id));
			const UiInteraction interaction = ui.Interact(id, card);
			const bool selected = m_Kit == int(kit.Id);
			const float hover = ui.Animate(id, interaction.Hovered);
			const float chosen = ui.Animate(id ^ 0x51ED270B27ull, selected, 10.0f);
			if (interaction.Hovered)
				hovered = i;
			if (interaction.Clicked)
				m_Kit = int(kit.Id);
			ui.Card(card, hover, chosen, Theme::Gold);
			const glm::vec2 seal(card.Center().x, card.Y + 56.0f);
			draw.Sigil(seal, 38.0f, { .Color = WithAlpha(glm::mix(Theme::AccentLight, Theme::Gold, chosen), 0.25f + 0.3f * hover + 0.4f * chosen), .Accent = WithAlpha(Theme::Astral, 0.3f + 0.4f * chosen), .Speed = 0.6f + 1.2f * chosen,
				.Phase = float(i) * 0.17f, .LineWidth = 1.0f, .Glow = 0.3f + 0.4f * chosen, .Layers = SigilRings });
			const float lit = std::max(chosen, hover * 0.7f);
			draw.Icon(IconFor(kit.Id), UiRect::Centered(seal, glm::vec2(40.0f)), { .Color = glm::mix(Theme::Muted, chosen > 0.5f ? Theme::GoldLight : Theme::AccentLight, lit), .ColorBottom = glm::mix(Theme::Faint, chosen > 0.5f ? Theme::Gold : Theme::Accent, lit),
				.ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.7f), .GlowColor = WithAlpha(chosen > 0.5f ? Theme::Gold : Theme::Accent, 0.35f * lit) });
			draw.TextAligned({ card.X, card.Y + 106.0f, card.W, 24.0f }, kit.Name, Theme::Caps(15.5f, glm::mix(Theme::Text, Theme::GoldLight, chosen), 1.2f), { 0.5f, 0.5f });
		}
		area.CutTop(16.0f);
		const KitDef& shown = Kits[hovered.value_or(size_t(std::clamp(m_Kit, 0, int(std::size(Kits)) - 1)))];
		draw.Text({ area.X, area.Y }, shown.Name, Theme::Caps(17.0f, Theme::GoldLight, 1.4f));
		draw.TextWrapped({ area.X, area.Y + 28.0f, area.W, 60.0f }, shown.Description, Theme::Body(15.0f, Theme::Text));
	}

	CharacterProfile MainMenuState::PrepareProfile()
	{
		Settings& settings = m_App.GetSettings();
		settings.PlayerName = m_Name.empty() ? "Player" : m_Name;
		settings.JoinAddress = m_Address;
		const int port = m_Port.empty() ? int(DefaultPort) : std::atoi(m_Port.c_str());
		settings.Port = uint16_t(std::clamp(port, 1024, 65535));
		settings.Kit = StarterKit(m_Kit);
		settings.FriendsCanJoin = m_FriendsCanJoin;
		m_App.RememberSettings(SettingsGroup::Identity);
		CharacterProfile profile = m_App.LoadCharacter(settings.PlayerName, settings.Kit, m_HasSave && m_Fresh);
		if (profile.Style != m_Style)
		{
			profile.Style = m_Style;
			if (m_HasSave && !m_Fresh)
				m_App.SaveCharacter(profile);
		}
		return profile;
	}

	void MainMenuState::StartSession(bool host)
	{
		SessionConfig config;
		config.Profile = PrepareProfile();
		const Settings& settings = m_App.GetSettings();
		config.Host = host;
		config.Port = settings.Port;
		config.Address = settings.JoinAddress;
		config.Map = settings.Map;
		config.StartMode = GameMode::Hub;
		m_App.GetGameAudio().PlayUi(Cue::UiRift, 0.9f);
		m_App.ChangeState(std::make_unique<SessionState>(m_App, config));
	}
}
