#pragma once
#include <TimbreAudio/AudioEngine.h>
#include <array>
#include <cstdint>
#include <span>

namespace Game
{
	enum class MusicMood : uint8_t
	{
		None,
		Menu,
		Hub,
		Town,
		Caves,
		Boss,
		Horde,
		Arena,
		Royale,
		Count,
	};

	enum class MusicStinger : uint8_t
	{
		Victory,
		Defeat,
		Count,
	};

	class MusicDirector
	{
	public:
		MusicDirector(Timbre::AudioEngine& engine, Timbre::Bus bus);

		void SetMood(MusicMood mood);
		MusicMood GetMood() const { return m_Mood; }
		void PlayStinger(MusicStinger stinger);
		void Update(float dt);

	private:
		enum class Track : uint8_t
		{
			Cosmos,
			Cathedral,
			HubPavane,
			HubFlow,
			HubAdventure,
			OldTown,
			OldTownDark,
			Caves,
			Boss,
			Battle,
			BattleHurry,
			BattleStorm,
			BattleClash,
			Count,
		};

		static std::span<const Track> Playlist(MusicMood mood);
		void StartTrack(float fadeIn, float delay);

		std::array<Timbre::Sound, size_t(Track::Count)> m_Tracks;
		std::array<Timbre::Sound, size_t(MusicStinger::Count)> m_Stingers;
		MusicMood m_Mood = MusicMood::None;
		size_t m_Position = 0;
		float m_TrackTime = 0.0f;
		Timbre::Voice m_Voice;
		Timbre::Voice m_StingerVoice;
		MusicStinger m_Stinger = MusicStinger::Victory;
		float m_StingerTime = 0.0f;
		bool m_Suspended = false;
		bool m_PendingStart = false;
	};
}
