#pragma once
#include <TimbreAudio/AudioEngine.h>
#include <TimbreAudio/SoundContainer.h>
#include <array>
#include <cstdint>

namespace Game
{
	enum class Cue : uint8_t
	{
		SwordSwing,
		HeavySwing,
		DaggerSwing,
		BowDraw,
		BowShoot,
		ArrowHit,
		MetalHit,
		HeavyHit,
		DaggerHit,
		Latch,
		Click,
		FireBuildup,
		FireLaunch,
		FireImpact,
		IceBuildup,
		IceLaunch,
		IceImpact,
		ShockBuildup,
		ShockLaunch,
		ShockHit,
		Heal,
		Shield,
		TeleportOut,
		TeleportIn,
		ManaStart,
		ManaEnd,
		Explosion,
		Thunder,
		Rumble,
		Rock,
		Wood,
		Plank,
		Glass,
		GlassLight,
		Body,
		BodyLight,
		Soft,
		Plate,
		PlateLight,
		Bone,
		Bell,
		WhooshLight,
		WhooshHeavy,
		GhoulGrowl,
		GhoulSnarl,
		SkeletonHiss,
		Rattle,
		Wraith,
		OgreRoar,
		Grunt,
		Growl,
		Spit,
		Collapse,
		WyrmRoar,
		DemonCall,
		DemonGrowl,
		BeastDeath,
		SpiritShout,
		StepStone,
		StepGrass,
		StepWood,
		StepCarpet,
		StepSand,
		StepDirt,
		Cloth,
		Coins,
		Leather,
		Drop,
		Belt,
		Unsheathe,
		Creak,
		BookOpen,
		BookClose,
		BookFlip,
		DoorOpen,
		DoorClose,
		Anvil,
		Crow,
		Gong,
		Horn,
		Drum,
		UiClick,
		UiConfirm,
		UiError,
		UiNotify,
		UiReward,
		UiRare,
		UiSuccess,
		UiFail,
		FireLoop,
		ShockLoop,
		IceLoop,
		Chant,
		GatewayHum,
		Campfire,
		Heartbeat,
		Count,
	};

	enum class CueBus : uint8_t
	{
		World,
		Interface,
		Stinger,
	};

	enum class Reach : uint8_t
	{
		Flat,
		Close,
		Medium,
		Wide,
	};

	struct AudioBuses
	{
		Timbre::Bus Music;
		Timbre::Bus Ambience;
		Timbre::Bus World;
		Timbre::Bus Interface;
		Timbre::Bus Stingers;
		Timbre::Bus Reverb;
	};

	inline constexpr float ListenerDistance = 7.0f;

	class SoundBank
	{
	public:
		SoundBank(Timbre::AudioEngine& engine, const AudioBuses& buses);

		Timbre::Voice Play(Cue cue, const Timbre::PlayParams& params = {}) const;

		static Timbre::SpatialSettings MakeSpatial(Reach reach);
		static const char* GetPath(Cue cue);

	private:
		std::array<Timbre::SoundContainer, size_t(Cue::Count)> m_Cues;
	};
}
