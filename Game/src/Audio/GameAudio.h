#pragma once
#include "Audio/MusicDirector.h"
#include "Audio/SoundBank.h"
#include "Audio/Soundscape.h"
#include <GameShared/Client/GameClient.h>
#include <GameShared/Core/Random.h>
#include <TimbreAudio/AudioEngine.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Game
{
	enum class AudioEvent : uint8_t
	{
		Travel,
		Teleport,
		Kill,
		Countdown,
		FightStart,
		MatchWon,
		MatchLost,
		StormClosing,
		RoundWon,
		RoundLost,
		ApexClaimed,
	};

	class GameAudio
	{
	public:
		explicit GameAudio(Timbre::AudioEngine& engine);
		GameAudio(const GameAudio&) = delete;
		GameAudio& operator=(const GameAudio&) = delete;

		void SetMix(float music, float effects);
		void EnterMenu();
		void Reset();
		void Update(float dt);
		void Observe(const GameClient& client, float dt, const glm::vec3& focus, const glm::vec3& eye);
		void SetOverlay(bool inventory, bool station, bool paused);
		void PlayStation(StationKind kind);
		void PlayNotice(bool success, std::optional<StationKind> station);
		void PlayEvent(AudioEvent event);
		void PlayUi(Cue cue, float volume = 1.0f, float pitch = 1.0f, float delay = 0.0f);
		void PlayHover(int index);
		void PlayCrumble(Vec2 position, bool ice = false);
		uint32_t GetPlayedCount() const { return m_Played; }
		void SetTrace(bool trace) { m_Trace = trace; }

	private:
		struct Tracked
		{
			EntityState Last;
			float Stride = 0.0f;
			float Idle = 0.0f;
			float SinceHit = 1e9f;
			Timbre::Voice Loop;
			bool Seen = false;
			bool LeftFoot = false;
		};

		Timbre::Voice PlayAt(Cue cue, Vec2 position, float volume = 1.0f, float pitch = 1.0f, float delay = 0.0f, float lift = 1.0f);
		Timbre::Voice PlayFlat(Cue cue, Timbre::Bus bus, float volume = 1.0f, float pitch = 1.0f, float delay = 0.0f);
		void PlayStinger(MusicStinger stinger);
		float GroundAt(Vec2 position) const;
		TerrainId TerrainAt(Vec2 position) const;
		bool HitNear(Vec2 position, float radius) const;

		void UpdateListener(const glm::vec3& focus, const glm::vec3& eye);
		void UpdateScene(const GameClient& client, float dt);
		void ApplyReverb(AmbienceScene scene);
		void OnAppear(const EntityState& entity, Tracked& tracked, const GameClient& client);
		void OnChange(const EntityState& entity, Tracked& tracked, float dt);
		void OnVanish(const Tracked& tracked, const GameClient& client);
		void PlayLaunch(const EntityState& entity, const GameClient& client);
		void PlayProjectileImpact(const EntityState& entity);
		void PlayStrike(const EntityState& entity);
		void StartTelegraph(const EntityState& entity, Tracked& tracked);
		void StartCast(const EntityState& entity, Tracked& tracked, bool self);
		void StartWindup(const EntityState& entity, Tracked& tracked);
		void PlayAggro(const EntityState& entity);
		void PlayFootstep(const EntityState& entity, Tracked& tracked, float moved, float volume);
		void PlayMonsterStep(const EntityState& entity, Tracked& tracked, float moved);
		void PlayBossChange(const EntityState& entity, const EntityState& before);
		void AnnounceBoss(const EntityState& boss);
		void PlayEnemyDeath(const EntityState& last);
		void PlayLootDrop(const EntityState& entity, const GameClient& client);
		void PlayLootTaken(const EntityState& last);
		void PlayHits();
		void PlayItemGain(const Item& item, bool equipped);
		void ObserveLocalPlayer(const GameClient& client);
		void ObserveInventory(const GameClient& client);
		void ObserveStatus(const GameClient& client);

		Timbre::AudioEngine& m_Engine;
		AudioBuses m_Buses;
		std::shared_ptr<Timbre::Reverb> m_Reverb;
		std::unique_ptr<SoundBank> m_Bank;
		std::unique_ptr<MusicDirector> m_Music;
		std::unique_ptr<Soundscape> m_Soundscape;
		Timbre::Snapshot m_Downed;
		Timbre::Snapshot m_Overlay;
		Timbre::Snapshot m_Danger;
		std::unordered_map<uint32_t, Tracked> m_Tracked;
		std::unordered_map<uint32_t, float> m_LastSwing;
		std::set<std::pair<uint32_t, uint32_t>> m_FrameVolleys;
		std::vector<HitEvent> m_Hits;
		std::vector<HitEvent> m_PreviousHits;
		const Level* m_Level = nullptr;
		const Level* m_SceneLevel = nullptr;
		GameStatus m_Status;
		LocalPlayerState m_Local;
		ServerInventory m_Inventory;
		MaterialStock m_Materials{};
		Vec2 m_Focus;
		float m_Ground = 0.0f;
		uint32_t m_Self = 0;
		uint32_t m_AnnouncedBoss = 0;
		Random m_Random{ 0xA0D10u };
		float m_Time = 0.0f;
		float m_MusicVolume = 1.0f;
		float m_EffectsVolume = 1.0f;
		float m_ReverbLevel = 0.0f;
		float m_LastGrowl = -10.0f;
		float m_LastCoins = -10.0f;
		float m_LastBossVoice = -10.0f;
		float m_NextThunder = 0.0f;
		uint32_t m_FrameImpacts = 0;
		uint32_t m_Played = 0;
		bool m_Primed = false;
		bool m_Trace = false;
		bool m_Overlaid = false;
		bool m_InventoryOpen = false;
		bool m_StationOpen = false;
		bool m_PauseOpen = false;
	};
}
