#pragma once
#include <cstdint>
#include <vector>

namespace Timbre
{
	class Effect;
}

namespace Timbre::Internal
{
	struct SoundData;
	class StreamInstance;

	constexpr uint32_t MaxVoiceSends = 4;
	constexpr uint32_t NoBus = 0xFFFF;
	constexpr float MaxLowPassHz = 20000.0f;

	// Immutable list of effects handed to the audio thread. Replaced as a whole when effects change.
	struct EffectChain
	{
		std::vector<Effect*> Effects;
	};

	enum class CommandType : uint8_t
	{
		EndBatch,          // Frame: batch id
		PlayVoice,         // Play
		StopVoice,         // Param.Frames: fade
		PauseVoice,        // Param.Flag: paused, Param.Frames: fade
		SetVoiceVolume,    // Param.Value, Param.Frames: fade
		SetVoicePitch,     // Param.Value
		SetVoicePan,       // Param.Value
		SetVoiceLooping,   // Param.Flag
		SetVoicePosition,  // Vector
		SetVoiceVelocity,  // Vector
		SetVoiceDirection, // Vector
		SetVoiceLowPass,   // Param.Value: cutoff
		SetVoiceHighPass,  // Param.Value: cutoff
		SeekVoice,         // Seconds
		SetVoiceEffects,   // Effects
		SetVoiceSend,      // Send
		CreateBus,         // Parent
		SetBusVolume,      // Param.Value, Param.Frames: fade
		SetBusMuted,       // Param.Flag
		SetBusPaused,      // Param.Flag
		SetBusPitch,       // Param.Value
		SetBusEffects,     // Effects
		SetBusLowPass,     // Param.Value: cutoff, Param.Frames: fade
		SetBusSnapshot,    // Snapshot
		SetBusDucking,     // Ducking
		StopBus,           // Param.Frames: fade
		StopAll,           // Param.Frames: fade
		SetListener,       // Listener: position, forward, up, velocity
		SetSpatialMode,    // Param.Flag: binaural
	};

	struct PlayCommand
	{
		SoundData* Sound;
		StreamInstance* Stream;
		double StartTime;
		float Volume;
		float Pitch;
		float Pan;
		uint32_t DelayFrames;
		uint32_t FadeInFrames;
		uint16_t Bus;
		bool Looping;
		bool Paused;
	};

	struct ParamCommand
	{
		float Value;
		uint32_t Frames;
		bool Flag;
	};

	struct SendCommand
	{
		uint32_t Bus;
		float Level;
		uint32_t Frames;
	};

	struct SnapshotCommand
	{
		float Gain;
		float LowPass;
		uint32_t Frames;
	};

	struct DuckingCommand
	{
		uint32_t Trigger;
		float Gain;
		float Threshold;
		float Attack;
		float Release;
	};

	struct Command
	{
		CommandType Type = CommandType::EndBatch;
		// Voice or bus index
		uint32_t Index = 0;
		// Voice generation, commands for an older generation of a voice slot are ignored
		uint32_t Generation = 0;
		union
		{
			PlayCommand Play{};
			ParamCommand Param;
			SendCommand Send;
			SnapshotCommand Snapshot;
			DuckingCommand Ducking;
			float Vector[3];
			float Listener[12];
			double Seconds;
			uint64_t Frame;
			EffectChain* Effects;
			uint32_t Parent;
		};
	};

	static_assert(sizeof(Command) <= 64, "Commands are copied through a ring buffer, keep them small");
}
