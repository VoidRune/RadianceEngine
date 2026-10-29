#include "SettingsUi.h"
#include "App/GameApp.h"
#include "UI/Ui.h"
#include "UI/UiAssets.h"
#include <GameShared/Net/Transport.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace Game
{
	namespace
	{
		constexpr const char* ShadowOptions[] = { "Off", "Low", "High" };
		constexpr const char* GiOptions[] = { "Off", "Low", "Medium", "High" };
		static_assert(std::size(ShadowOptions) == size_t(ShadowQuality::Count) && std::size(GiOptions) == size_t(GiQuality::Count));
	}

	void DrawSettings(Ui& ui, GameApp& app, UiRect area)
	{
		Settings& settings = app.GetSettings();
		UiDrawList& draw = ui.Draw();
		char text[32];
		const auto row = [&](std::string_view label, float height = 36.0f)
		{
			UiRect line = area.CutTop(height);
			draw.TextAligned(line.CutLeft(170.0f), label, Theme::Body(16.0f, Theme::Text), { 0.0f, 0.5f });
			return line;
		};
		const auto percent = [&](float value)
		{
			std::snprintf(text, sizeof(text), "%.0f%%", double(value) * 100.0);
			return std::string_view(text);
		};

		ui.Separator(area.CutTop(22.0f), "SOUND");
		area.CutTop(6.0f);
		bool audio = ui.Slider(Ui::MakeId("settings.volume"), row("Master"), settings.Volume, 0.0f, 1.0f, percent(settings.Volume));
		audio |= ui.Slider(Ui::MakeId("settings.music"), row("Music"), settings.MusicVolume, 0.0f, 1.0f, percent(settings.MusicVolume));
		audio |= ui.Slider(Ui::MakeId("settings.effects"), row("Effects"), settings.EffectsVolume, 0.0f, 1.0f, percent(settings.EffectsVolume));
		UiRect mute = row("Silence", 34.0f);
		audio |= ui.Checkbox(Ui::MakeId("settings.mute"), mute, settings.Muted ? "All sound is muted" : "Sound is on", settings.Muted);
		if (audio)
		{
			app.ApplyAudioSettings();
			app.RememberSettings(SettingsGroup::Audio);
		}

		area.CutTop(14.0f);
		ui.Separator(area.CutTop(22.0f), "VISUALS");
		area.CutTop(6.0f);
		bool visuals = false;
		int shadows = int(settings.Shadows);
		if (ui.Choice(Ui::MakeId("settings.shadows"), row("Shadows", 40.0f).CutLeft(230.0f), shadows, ShadowOptions))
		{
			settings.Shadows = ShadowQuality(shadows);
			visuals = true;
		}
		int gi = int(settings.Gi);
		if (ui.Choice(Ui::MakeId("settings.gi"), row("Indirect light", 40.0f).CutLeft(230.0f), gi, GiOptions))
		{
			settings.Gi = GiQuality(gi);
			visuals = true;
		}
		visuals |= ui.Checkbox(Ui::MakeId("settings.fullscreen"), row("Display", 34.0f), "Fullscreen", settings.Fullscreen);
		visuals |= ui.Checkbox(Ui::MakeId("settings.vsync"), row("Vsync", 34.0f), settings.Vsync ? "Synced to the monitor's refresh rate" : "Off, the frame cap below applies", settings.Vsync);
		const UiRect cap = row("Frame cap");
		if (settings.Vsync)
			draw.TextAligned(cap, "Turn vsync off to cap the frame rate", Theme::Body(15.0f, Theme::Faint), { 0.0f, 0.5f });
		else
		{
			float frames = float(settings.FrameCap);
			std::snprintf(text, sizeof(text), "%u fps", unsigned(settings.FrameCap));
			if (ui.Slider(Ui::MakeId("settings.framecap"), cap, frames, float(MinFrameCap), float(MaxFrameCap), text))
			{
				settings.FrameCap = uint32_t(std::lround(frames));
				visuals = true;
			}
		}
		if (ui.Slider(Ui::MakeId("settings.shake"), row("Screen shake"), settings.ScreenShake, 0.0f, 1.0f, percent(settings.ScreenShake)))
		{
			settings.ScreenShake = std::round(settings.ScreenShake * 20.0f) / 20.0f;
			visuals = true;
		}
		visuals |= ui.Checkbox(Ui::MakeId("settings.controls"), row("Hints", 34.0f), "Show the controls in a corner", settings.ShowControls);
		if (visuals)
		{
			app.ApplyVisualSettings();
			app.RememberSettings(SettingsGroup::Visuals);
		}

		area.CutTop(14.0f);
		ui.Separator(area.CutTop(22.0f), "DIAGNOSTICS");
		area.CutTop(6.0f);
		ui.Checkbox(Ui::MakeId("settings.performance"), row("Performance", 34.0f), "Show frame and GPU timings (F2)", settings.ShowPerformance);
		bool network = false;
		float lag = float(settings.Simulation.LagMs);
		std::snprintf(text, sizeof(text), "%.0f ms", double(lag));
		if (ui.Slider(Ui::MakeId("settings.lag"), row("Added lag"), lag, 0.0f, 500.0f, text))
		{
			settings.Simulation.LagMs = int32_t(std::round(lag / 5.0f) * 5.0f);
			network = true;
		}
		float loss = settings.Simulation.LossPercent;
		std::snprintf(text, sizeof(text), "%.1f%%", double(loss));
		if (ui.Slider(Ui::MakeId("settings.loss"), row("Packet loss"), loss, 0.0f, 25.0f, text))
		{
			settings.Simulation.LossPercent = std::round(loss * 2.0f) * 0.5f;
			network = true;
		}
		if (network)
			NetLibrary::SetSimulation(settings.Simulation);
	}

	bool DrawHeadwearPicker(Ui& ui, UiRect row, Headwear& headwear)
	{
		UiDrawList& draw = ui.Draw();
		constexpr float Gap = 12.0f;
		const float count = float(std::size(Headwears));
		const float width = (row.W - Gap * (count - 1.0f)) / count;
		bool changed = false;
		for (const HeadwearDef& def : Headwears)
		{
			const UiRect card = row.CutLeft(width);
			row.CutLeft(Gap);
			const UiId id = Ui::MakeId("headwear", uint64_t(def.Id));
			const UiInteraction interaction = ui.Interact(id, card);
			const bool selected = headwear == def.Id;
			const float hover = ui.Animate(id, interaction.Hovered);
			const float chosen = ui.Animate(id ^ 0x4EAD3EA7ull, selected, 10.0f);
			if (interaction.Clicked && !selected)
			{
				headwear = def.Id;
				changed = true;
			}
			ui.Card(card, hover, chosen, Theme::Gold);
			const float lit = std::max(chosen, hover * 0.7f);
			const UiColor tone = chosen > 0.5f ? Theme::Gold : Theme::Accent;
			draw.Icon(IconFor(def.Id), { card.X + 12.0f, card.Center().y - 16.0f, 32.0f, 32.0f }, { .Color = glm::mix(Theme::Muted, chosen > 0.5f ? Theme::GoldLight : Theme::AccentLight, lit), .ColorBottom = glm::mix(Theme::Faint, tone, lit),
				.ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.7f), .GlowColor = WithAlpha(tone, 0.35f * lit) });
			draw.TextAligned({ card.X + 52.0f, card.Y, card.W - 56.0f, card.H }, def.Name, Theme::Caps(12.5f, glm::mix(Theme::Text, Theme::GoldLight, chosen), 1.0f), { 0.0f, 0.5f });
		}
		return changed;
	}
}
