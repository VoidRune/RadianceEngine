#include "MusicDirector.h"
#include <algorithm>
#include <span>

namespace Game
{
	namespace
	{
		constexpr float CrossfadeOut = 3.0f;
		constexpr float CrossfadeIn = 3.5f;
		constexpr float MinimumPlaySeconds = 100.0f;
		constexpr float VictorySeconds = 10.5f;

		constexpr const char* TrackFiles[] = {
			"res/Audio/Music/Cosmos.ogg",
			"res/Audio/Music/Cathedral.ogg",
			"res/Audio/Music/HubPavane.ogg",
			"res/Audio/Music/HubFlow.ogg",
			"res/Audio/Music/HubAdventure.ogg",
			"res/Audio/Music/OldTown.ogg",
			"res/Audio/Music/OldTownDark.ogg",
			"res/Audio/Music/Caves.ogg",
			"res/Audio/Music/Boss.ogg",
			"res/Audio/Music/Battle.ogg",
			"res/Audio/Music/BattleHurry.ogg",
			"res/Audio/Music/BattleStorm.ogg",
			"res/Audio/Music/BattleClash.ogg",
		};

		constexpr const char* StingerFiles[] = {
			"res/Audio/Music/Victory.ogg",
			"res/Audio/Music/Defeat.ogg",
		};
	}

	MusicDirector::MusicDirector(Timbre::AudioEngine& engine, Timbre::Bus bus)
	{
		static_assert(std::size(TrackFiles) == size_t(Track::Count));
		static_assert(std::size(StingerFiles) == size_t(MusicStinger::Count));
		Timbre::SoundDesc desc;
		desc.Mode = Timbre::LoadMode::Stream;
		desc.Bus = bus;
		desc.Looping = true;
		desc.Priority = 255;
		for (size_t i = 0; i < m_Tracks.size(); i++)
			m_Tracks[i] = engine.LoadSound(TrackFiles[i], desc);
		for (size_t i = 0; i < m_Stingers.size(); i++)
		{
			desc.Looping = MusicStinger(i) == MusicStinger::Victory;
			m_Stingers[i] = engine.LoadSound(StingerFiles[i], desc);
		}
	}

	std::span<const MusicDirector::Track> MusicDirector::Playlist(MusicMood mood)
	{
		static constexpr Track Menu[] = { Track::Cosmos, Track::Cathedral };
		static constexpr Track Hub[] = { Track::HubPavane, Track::HubFlow, Track::HubAdventure };
		static constexpr Track Town[] = { Track::OldTown, Track::OldTownDark };
		static constexpr Track Caves[] = { Track::Caves };
		static constexpr Track Boss[] = { Track::Boss };
		static constexpr Track Horde[] = { Track::Battle, Track::BattleHurry };
		static constexpr Track Arena[] = { Track::Battle, Track::BattleClash, Track::BattleStorm };
		static constexpr Track Royale[] = { Track::BattleHurry, Track::BattleStorm, Track::Battle };
		switch (mood)
		{
		case MusicMood::Menu:
			return Menu;
		case MusicMood::Hub:
			return Hub;
		case MusicMood::Town:
			return Town;
		case MusicMood::Caves:
			return Caves;
		case MusicMood::Boss:
			return Boss;
		case MusicMood::Horde:
			return Horde;
		case MusicMood::Arena:
			return Arena;
		case MusicMood::Royale:
			return Royale;
		default:
			return {};
		}
	}

	void MusicDirector::SetMood(MusicMood mood)
	{
		if (mood == m_Mood)
			return;
		m_Mood = mood;
		m_Position = 0;
		m_Voice.Stop(CrossfadeOut);
		m_Voice = {};
		m_PendingStart = m_Suspended && mood != MusicMood::None;
		if (!m_Suspended && mood != MusicMood::None)
			StartTrack(CrossfadeIn, 0.6f);
	}

	void MusicDirector::StartTrack(float fadeIn, float delay)
	{
		m_TrackTime = 0.0f;
		m_PendingStart = false;
		const std::span<const Track> playlist = Playlist(m_Mood);
		if (playlist.empty())
			return;
		m_Voice = m_Tracks[size_t(playlist[m_Position % playlist.size()])].Play({ .FadeIn = fadeIn, .Delay = delay });
	}

	void MusicDirector::PlayStinger(MusicStinger stinger)
	{
		m_StingerVoice.Stop(0.5f);
		if (!m_Suspended)
			m_Voice.SetPaused(true, 1.0f);
		m_Suspended = true;
		m_Stinger = stinger;
		m_StingerTime = 0.0f;
		m_StingerVoice = m_Stingers[size_t(stinger)].Play({ .Delay = 0.4f });
	}

	void MusicDirector::Update(float dt)
	{
		m_TrackTime += dt;
		if (m_Suspended)
		{
			const float before = m_StingerTime;
			m_StingerTime += dt;
			if (m_Stinger == MusicStinger::Victory && before < VictorySeconds && m_StingerTime >= VictorySeconds)
				m_StingerVoice.Stop(3.0f);
			if (m_StingerVoice.IsPlaying())
				return;
			m_Suspended = false;
			if (m_PendingStart || !m_Voice.IsPlaying())
				StartTrack(CrossfadeIn, 0.3f);
			else
				m_Voice.SetPaused(false, 2.5f);
			return;
		}

		const std::span<const Track> playlist = Playlist(m_Mood);
		if (playlist.size() < 2)
			return;
		const float duration = m_Tracks[size_t(playlist[m_Position % playlist.size()])].GetDuration();
		if (m_TrackTime < std::max(MinimumPlaySeconds, 2.0f * duration))
			return;
		m_Voice.Stop(CrossfadeOut);
		m_Position++;
		StartTrack(CrossfadeIn, 1.0f);
	}
}
