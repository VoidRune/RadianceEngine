#include "SettingsUi.h"
#include "App/GameApp.h"
#include "UI/Ui.h"
#include <GameShared/Net/Transport.h>
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
		visuals |= ui.Checkbox(Ui::MakeId("settings.controls"), row("Hints", 34.0f), "Show the controls in a corner", settings.ShowControls);
		if (visuals)
		{
			app.ApplyVisualSettings();
			app.RememberSettings(SettingsGroup::Visuals);
		}

		area.CutTop(14.0f);
		ui.Separator(area.CutTop(22.0f), "NETWORK TESTING");
		area.CutTop(6.0f);
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
}
