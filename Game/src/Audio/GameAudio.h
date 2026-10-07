#pragma once
#include <GameShared/Client/GameClient.h>
#include <TimbreAudio/AudioEngine.h>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>

namespace Game
{
	enum class Sfx : uint8_t
	{
		Bolt,
		Arrow,
		EnemyShot,
		Slash,
		Nova,
		Dash,
		Blink,
		Hit,
		EnemyDeath,
		Hurt,
		PlayerDeath,
		Windup,
		BruteWindup,
		Pickup,
		PowerUp,
		Heal,
		WaveStart,
		WaveClear,
		Defeat,
		Spawn,
		ItemPickup,
		RareDrop,
		Upgrade,
		Equip,
		Crit,
		Door,
		Fireball,
		Explosion,
		Zap,
		Frost,
		Kill,
		Storm,
		Count,
	};

	class GameAudio
	{
	public:
		explicit GameAudio(Timbre::AudioEngine& engine);

		void Play(Sfx sfx, std::optional<Vec2> position = {}, float volume = 1.0f, float pitch = 1.0f);
		void Observe(const GameClient& client);
		void Reset();
		uint32_t GetPlayedCount() const { return m_Played; }

	private:
		struct Tracked
		{
			EntityKind Kind = EntityKind::Player;
			uint8_t Variant = 0;
			uint32_t Owner = NoPlayer;
			uint8_t Health = 255;
			uint8_t Flags = 0;
			Vec2 Position;
			bool Seen = false;
		};

		void OnAppear(const EntityState& entity, bool own);
		void OnChange(const EntityState& entity, const Tracked& before, bool self);
		void OnDisappear(const Tracked& last, const GameClient& client);
		void ObserveLocalPlayer(const GameClient& client);
		void ObserveInventory(const ServerInventory& inventory);

		Timbre::AudioEngine& m_Engine;
		Timbre::Bus m_Bus;
		std::array<Timbre::Sound, size_t(Sfx::Count)> m_Sounds;
		std::unordered_map<uint32_t, Tracked> m_Tracked;
		std::set<std::pair<uint32_t, uint32_t>> m_FrameVolleys;
		GameStatus m_Status;
		Vec2 m_Listener;
		LocalPlayerState m_Local;
		ServerInventory m_Inventory;
		bool m_Primed = false;
		uint32_t m_Played = 0;
	};
}
