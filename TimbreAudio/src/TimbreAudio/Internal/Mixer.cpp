#include "Mixer.h"
#include "SoundAsset.h"
#include "Streamer.h"
#include <algorithm>
#include <chrono>
#include <cstring>

namespace Timbre::Internal
{
	namespace
	{
		constexpr uint64_t UnitStep = 1ull << 32;
		constexpr float FractionScale = 1.0f / 4294967296.0f;
		// About -100 dB, anything quieter is not worth mixing.
		constexpr float SilenceThreshold = 1e-5f;
		constexpr float OpenLowPassLog = 14.2877123795f;
		constexpr float RearShelfHz = 4000.0f;
		constexpr float HeadRadius = 0.0875f;
		constexpr float SpeedOfSoundInAir = 343.0f;
		constexpr float BinauralGain = 0.70710678f;

		float EvaluateCurve(const std::vector<AttenuationPoint>& curve, float distance)
		{
			if (curve.empty())
				return 1.0f;
			if (distance <= curve.front().Distance)
				return curve.front().Volume;
			for (size_t i = 1; i < curve.size(); i++)
			{
				const AttenuationPoint& a = curve[i - 1];
				const AttenuationPoint& b = curve[i];
				if (distance < b.Distance)
					return a.Volume + (b.Volume - a.Volume) * (distance - a.Distance) / (b.Distance - a.Distance);
			}
			return curve.back().Volume;
		}

		float Attenuate(const SpatialSettings& settings, float distance)
		{
			const float minDistance = std::max(settings.MinDistance, 1e-4f);
			const float maxDistance = std::max(settings.MaxDistance, minDistance);
			const float d = std::clamp(distance, minDistance, maxDistance);
			switch (settings.Model)
			{
			case AttenuationModel::None:
				return 1.0f;
			case AttenuationModel::Inverse:
				return minDistance / (minDistance + settings.Rolloff * (d - minDistance));
			case AttenuationModel::Linear:
				return maxDistance > minDistance ? std::max(0.0f, 1.0f - settings.Rolloff * (d - minDistance) / (maxDistance - minDistance)) : 1.0f;
			case AttenuationModel::Exponential:
				return std::pow(d / minDistance, -settings.Rolloff);
			case AttenuationModel::Custom:
				return EvaluateCurve(settings.Curve, distance);
			}
			return 1.0f;
		}

		float AirAbsorptionCutoff(float amount, float distance)
		{
			const float loss = 0.1f * amount * distance;
			const float excess = std::pow(10.0f, loss * 0.1f) - 1.0f;
			if (excess <= 0.16f)
				return 0.0f;
			return std::max(8000.0f / std::sqrt(excess), 1000.0f);
		}

		float OnePoleCoefficient(float cutoff, float sampleRate)
		{
			return std::exp(-2.0f * Pi * cutoff / sampleRate);
		}

		float LowPassToLog(float cutoff)
		{
			return cutoff > 0.0f ? std::log2(std::clamp(cutoff, 20.0f, MaxLowPassHz)) : OpenLowPassLog;
		}

		float HeadShadowAlpha(float theta)
		{
			return 1.05f + 0.95f * std::cos(1.2f * theta);
		}

		float WoodworthDelay(float theta)
		{
			const float headDelay = HeadRadius / SpeedOfSoundInAir;
			return headDelay * (theta < Pi * 0.5f ? 1.0f - std::cos(theta) : theta - Pi * 0.5f + 1.0f);
		}

		struct SpatialFilter
		{
			float Air;
			float Rear;
			float RearGain;
			float RearStep;
		};

		template<uint32_t Channels, bool Air, bool Rear>
		void SpatialFilterKernel(float* samples, uint32_t frames, const SpatialFilter& filter, float* airState, float* rearState)
		{
			const float airInput = 1.0f - filter.Air;
			const float rearInput = 1.0f - filter.Rear;
			float air[Channels];
			float rear[Channels];
			for (uint32_t c = 0; c < Channels; c++)
			{
				air[c] = airState[c];
				rear[c] = rearState[c];
			}
			for (uint32_t i = 0; i < frames; i++)
			{
				const float gain = filter.RearGain + filter.RearStep * float(i + 1);
				for (uint32_t c = 0; c < Channels; c++)
				{
					float x = samples[i * Channels + c];
					if constexpr (Air)
					{
						air[c] = filter.Air * air[c] + airInput * x;
						x = air[c];
					}
					if constexpr (Rear)
					{
						rear[c] = filter.Rear * rear[c] + rearInput * x;
						x = rear[c] + gain * (x - rear[c]);
					}
					samples[i * Channels + c] = x;
				}
			}
			for (uint32_t c = 0; c < Channels; c++)
			{
				airState[c] = air[c];
				rearState[c] = rear[c];
			}
		}

		void AddScaled(float* destination, const float* source, uint32_t frames, float from, float to)
		{
			const float step = (to - from) / float(frames);
			for (uint32_t i = 0; i < frames; i++)
			{
				const float gain = from + step * float(i + 1);
				destination[i * 2 + 0] += source[i * 2 + 0] * gain;
				destination[i * 2 + 1] += source[i * 2 + 1] * gain;
			}
		}

		// Output frame k is interpolated at source position fraction + k * step (32.32 fixed point),
		// relative to source[Channels], so source[0] is the frame before the cursor.
		template<uint32_t Channels, bool Cubic>
		void ResampleKernel(const float* source, float* destination, uint32_t frames, uint64_t position, uint64_t step)
		{
			for (uint32_t k = 0; k < frames; k++, position += step)
			{
				const float t = float(uint32_t(position)) * FractionScale;
				const float* s = source + (position >> 32) * Channels;
				for (uint32_t c = 0; c < Channels; c++)
				{
					const float s0 = s[c];
					const float s1 = s[Channels + c];
					const float s2 = s[2 * Channels + c];
					if constexpr (Cubic)
					{
						const float s3 = s[3 * Channels + c];
						destination[k * Channels + c] = s1 + 0.5f * t * (s2 - s0 + t * (2.0f * s0 - 5.0f * s1 + 4.0f * s2 - s3 + t * (3.0f * (s1 - s2) + s3 - s0)));
					}
					else
						destination[k * Channels + c] = s1 + t * (s2 - s1);
				}
			}
		}

		// Mixes a mono or stereo voice into interleaved stereo, ramping the 2x2 gain matrix
		// (left to left, right to left, left to right, right to right; mono uses the first two).
		template<bool Accumulate>
		void Pan(const float* source, uint32_t channels, float* destination, uint32_t frames, const float* from, const float* to)
		{
			const float inverse = 1.0f / float(frames);
			if (channels == 1)
			{
				const float stepLeft = (to[0] - from[0]) * inverse;
				const float stepRight = (to[1] - from[1]) * inverse;
				for (uint32_t i = 0; i < frames; i++)
				{
					const float left = from[0] + stepLeft * float(i + 1);
					const float right = from[1] + stepRight * float(i + 1);
					const float x = source[i];
					if constexpr (Accumulate)
					{
						destination[i * 2 + 0] += x * left;
						destination[i * 2 + 1] += x * right;
					}
					else
					{
						destination[i * 2 + 0] = x * left;
						destination[i * 2 + 1] = x * right;
					}
				}
				return;
			}

			float step[4];
			for (int j = 0; j < 4; j++)
				step[j] = (to[j] - from[j]) * inverse;
			for (uint32_t i = 0; i < frames; i++)
			{
				const float t = float(i + 1);
				const float l = source[i * 2 + 0];
				const float r = source[i * 2 + 1];
				const float left = l * (from[0] + step[0] * t) + r * (from[1] + step[1] * t);
				const float right = l * (from[2] + step[2] * t) + r * (from[3] + step[3] * t);
				if constexpr (Accumulate)
				{
					destination[i * 2 + 0] += left;
					destination[i * 2 + 1] += right;
				}
				else
				{
					destination[i * 2 + 0] = left;
					destination[i * 2 + 1] = right;
				}
			}
		}

		// Custom effects can produce NaN or infinity. Silence it before it reaches shared buses and
		// poisons effects like reverb for good.
		void Sanitize(float* samples, size_t count)
		{
			for (size_t i = 0; i < count; i++)
				if (!std::isfinite(samples[i]))
					samples[i] = 0.0f;
		}

		void ApplyGain(float* samples, uint32_t frames, float from, float to)
		{
			if (from == to)
			{
				if (to != 1.0f)
					for (uint32_t i = 0; i < frames * 2; i++)
						samples[i] *= to;
				return;
			}
			const float step = (to - from) / float(frames);
			for (uint32_t i = 0; i < frames; i++)
			{
				const float gain = from + step * float(i + 1);
				samples[i * 2 + 0] *= gain;
				samples[i * 2 + 1] *= gain;
			}
		}
	}

	struct Mixer::Voice
	{
		uint32_t Generation = 0;
		bool Active = false;
		bool Listed = false;
		bool Started = false;
		bool Stopping = false;
		bool Finished = false;
		bool Paused = false;
		bool PauseAfterFade = false;
		bool Looping = false;
		bool Spatial = false;
		bool HasDirection = false;
		bool Real = false;
		bool GainsValid = false;
		bool InstantStart = false;
		bool FiltersDirty = false;
		bool LowPassActive = false;
		bool HighPassActive = false;
		bool Binaural = false;
		bool AirActive = false;
		bool AirFresh = false;

		SoundData* Sound = nullptr;
		const SoundAsset* Asset = nullptr;
		StreamInstance* Stream = nullptr;
		EffectChain* Effects = nullptr;
		uint16_t Bus = 0;
		uint8_t Priority = 128;
		uint32_t Channels = 0;
		uint32_t SampleRate = 0;

		uint32_t DelayFrames = 0;
		uint32_t StartOffset = 0;
		uint32_t FadeInFrames = 0;
		double StartTime = 0.0;
		double PendingSeek = -1.0;

		// Static sounds: frame in the sound. Streams: frame in the ring.
		int64_t Cursor = 0;
		uint32_t Fraction = 0;
		uint64_t Step = UnitStep;
		uint64_t FrameCount = 0;
		uint64_t LoopStart = 0;
		uint64_t LoopEnd = 0;

		Ramp Volume;
		Ramp Fade;
		// Short declick fade used for pausing and seeking
		Ramp Gate;
		float Pitch = 1.0f;
		float Pan = 0.0f;
		Vec3 Position;
		Vec3 Velocity;
		Vec3 Direction = { 0.0f, 0.0f, 1.0f };

		float LowPass = 0.0f;
		float HighPass = 0.0f;
		BiquadCoefficients LowPassCoefficients;
		BiquadCoefficients HighPassCoefficients;
		BiquadState LowPassState[2];
		BiquadState HighPassState[2];

		float AirCoefficient = 0.0f;
		float AirState[2] = {};
		float RearGain = 1.0f;
		float TargetRearGain = 1.0f;
		float RearState[2] = {};

		// Gain matrix applied at the end of the previous block, and the one to reach by the end of this block
		float Gains[4] = {};
		float TargetGains[4] = {};
		float Audibility = 0.0f;

		float Delay[2] = {};
		float TargetDelay[2] = {};
		float ShadowB0[2] = { 1.0f, 1.0f };
		float ShadowB1[2] = {};
		float ShadowA1[2] = {};
		float ShadowX[2] = {};
		float ShadowY[2] = {};
		float History[BinauralHistory] = {};

		struct Send
		{
			uint16_t Bus = 0;
			Ramp Level;
			float Gain = 0.0f;
			float Target = 0.0f;
		};
		Send Sends[MaxVoiceSends * 2];
		uint32_t SendCount = 0;

		// Streams
		uint64_t StreamStart = 0;
		uint64_t StreamEnd = UINT64_MAX;
		uint64_t SegmentRing = 0;
		uint64_t SegmentSource = 0;
		// Length of the loop region when the current segment repeats, 0 when it plays linearly
		uint64_t SegmentLoop = 0;
		uint32_t SeekSerial = 0;
		// Streams played in the same batch start together, this is the batch's sequence number
		uint32_t Group = 0;
		uint32_t GroupWait = 0;
		bool FlushPending = false;
		bool Buffering = true;
		bool LoopingChanged = false;
	};

	struct Mixer::Bus
	{
		bool Active = false;
		uint16_t Parent = 0;
		uint16_t Depth = 0;
		bool Paused = false;
		float Pitch = 1.0f;
		Ramp Volume;
		Ramp Mute;
		EffectChain* Effects = nullptr;
		float* Buffer = nullptr;
		bool HasInput = false;
		bool Dirty = false;
		// Gain applied at the end of the previous block
		float Gain = 1.0f;

		Ramp Snapshot;
		Ramp LowPass{ OpenLowPassLog, OpenLowPassLog, 0 };
		Ramp SnapshotLowPass{ OpenLowPassLog, OpenLowPassLog, 0 };
		bool LowPassActive = false;
		float LowPassLog = OpenLowPassLog;
		BiquadCoefficients LowPassCoefficients;
		BiquadState LowPassState[2];

		uint16_t DuckTrigger = uint16_t(NoBus);
		float DuckGain = 1.0f;
		float DuckThreshold = 1.0f;
		float DuckAttack = 0.05f;
		float DuckRelease = 0.6f;
		float Duck = 1.0f;
		float Level = 0.0f;

		// Computed at the start of every block
		bool EffectivePaused = false;
		float EffectivePitch = 1.0f;
		float EffectiveGain = 1.0f;
		float GainEnd = 1.0f;
	};

	Mixer::Mixer(const MixerConfig& config, Streamer* streamer)
		: m_Config(config), m_Streamer(streamer), m_Commands(config.CommandCapacity)
	{
		m_Voices.resize(config.MaxVoices);
		m_Status = std::make_unique<VoiceStatus[]>(config.MaxVoices);
		m_ActiveVoices.reserve(config.MaxVoices);
		m_Candidates.reserve(config.MaxVoices);

		m_Buses.resize(config.MaxBuses);
		m_BusOrder.reserve(config.MaxBuses);
		m_BusBuffers.assign(size_t(config.MaxBuses) * BlockFrames * 2, 0.0f);
		for (uint32_t i = 0; i < config.MaxBuses; i++)
			m_Buses[i].Buffer = m_BusBuffers.data() + size_t(i) * BlockFrames * 2;
		m_Buses[0].Active = true;
		m_BusOrder.push_back(0);

		m_SourceBuffer.assign((size_t(BlockFrames) * MaxStep + 8) * 2, 0.0f);
		m_VoiceBuffer.assign(size_t(BlockFrames) * 2, 0.0f);
		m_EffectBuffer.assign(size_t(BlockFrames) * 2, 0.0f);
		m_BinauralBuffer.assign(size_t(BinauralHistory) + BlockFrames + 1, 0.0f);
		m_DeclickFrames = std::max(1u, config.SampleRate / 200);
		m_Binaural = config.SpatialMode == SpatialMode::Binaural;
		m_RearCoefficient = OnePoleCoefficient(RearShelfHz, float(config.SampleRate));

		m_Limiter = std::make_unique<Limiter>(-0.3f, 80.0f);
		static_cast<Effect*>(m_Limiter.get())->Initialize(config.SampleRate);

		m_Dying.reserve(MaxDyingVoices);
		m_Groups.reserve(MaxStartGroups);
		m_MaxGroupWait = config.SampleRate / 4;
	}

	Mixer::~Mixer() = default;

	void Mixer::Render(float* output, uint32_t frames)
	{
		const auto start = std::chrono::steady_clock::now();
		DenormalGuard denormals;
		m_Commands.ConsumeAll([this](const Command& command) { Execute(command); });

		for (uint32_t offset = 0; offset < frames;)
		{
			const uint32_t count = std::min(BlockFrames, frames - offset);
			RenderBlock(output + size_t(offset) * 2, count);
			offset += count;
		}
		m_Dying.clear();
		PublishStatus();
		// Published only now: stolen voices fading out in this call still used their resources.
		m_CompletedBatch.store(m_ProcessedBatch, std::memory_order_release);
		if (frames == 0)
			return;

		const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		const double budget = double(frames) / double(m_Config.SampleRate);
		m_LoadAverage += (float(elapsed / budget) - m_LoadAverage) * 0.05f;
		m_CpuLoad.store(m_LoadAverage, std::memory_order_relaxed);
	}

	void Mixer::Execute(const Command& command)
	{
		switch (command.Type)
		{
		case CommandType::EndBatch:
			m_ProcessedBatch = command.Frame;
			m_BatchSequence = m_BatchSequence + 1 == 0 ? 1 : m_BatchSequence + 1;
			return;
		case CommandType::PlayVoice:
			StartVoice(command.Index, command.Generation, command.Play);
			return;
		case CommandType::CreateBus:
			CreateBus(command.Index, command.Parent);
			return;
		case CommandType::StopAll:
			for (uint32_t index : m_ActiveVoices)
				if (m_Voices[index].Active)
					StopVoice(m_Voices[index], command.Param.Frames);
			return;
		case CommandType::SetListener:
			SetListener(command.Listener);
			return;
		case CommandType::SetSpatialMode:
			m_Binaural = command.Param.Flag;
			return;
		default:
			break;
		}

		if (command.Type >= CommandType::SetBusVolume && command.Type <= CommandType::StopBus)
		{
			if (command.Index >= m_Buses.size() || !m_Buses[command.Index].Active)
				return;
			Bus& bus = m_Buses[command.Index];
			switch (command.Type)
			{
			case CommandType::SetBusVolume:
				bus.Volume.Start(command.Param.Value, command.Param.Frames);
				break;
			case CommandType::SetBusMuted:
				bus.Mute.Start(command.Param.Flag ? 0.0f : 1.0f, m_DeclickFrames);
				break;
			case CommandType::SetBusPaused:
				bus.Paused = command.Param.Flag;
				break;
			case CommandType::SetBusPitch:
				bus.Pitch = command.Param.Value;
				break;
			case CommandType::SetBusEffects:
				bus.Effects = command.Effects;
				break;
			case CommandType::SetBusLowPass:
				bus.LowPass.Start(LowPassToLog(command.Param.Value), command.Param.Frames);
				break;
			case CommandType::SetBusSnapshot:
				bus.Snapshot.Start(command.Snapshot.Gain, command.Snapshot.Frames);
				bus.SnapshotLowPass.Start(LowPassToLog(command.Snapshot.LowPass), command.Snapshot.Frames);
				break;
			case CommandType::SetBusDucking:
				bus.DuckTrigger = command.Ducking.Trigger < m_Buses.size() && m_Buses[command.Ducking.Trigger].Active ? uint16_t(command.Ducking.Trigger) : uint16_t(NoBus);
				bus.DuckGain = command.Ducking.Gain;
				bus.DuckThreshold = command.Ducking.Threshold;
				bus.DuckAttack = command.Ducking.Attack;
				bus.DuckRelease = command.Ducking.Release;
				break;
			case CommandType::StopBus:
				for (uint32_t index : m_ActiveVoices)
				{
					Voice& voice = m_Voices[index];
					if (voice.Active && IsInBus(voice.Bus, command.Index))
						StopVoice(voice, command.Param.Frames);
				}
				break;
			default:
				break;
			}
			return;
		}

		if (command.Index >= m_Voices.size())
			return;
		Voice& voice = m_Voices[command.Index];
		if (!voice.Active || voice.Generation != command.Generation)
			return;
		switch (command.Type)
		{
		case CommandType::StopVoice:
			StopVoice(voice, command.Param.Frames);
			break;
		case CommandType::PauseVoice:
			PauseVoice(voice, command.Param.Flag, command.Param.Frames);
			break;
		case CommandType::SetVoiceVolume:
			voice.Volume.Start(command.Param.Value, command.Param.Frames);
			break;
		case CommandType::SetVoicePitch:
			voice.Pitch = command.Param.Value;
			break;
		case CommandType::SetVoicePan:
			voice.Pan = command.Param.Value;
			break;
		case CommandType::SetVoiceLooping:
			voice.Looping = command.Param.Flag;
			voice.LoopingChanged = true;
			if (voice.Stream)
				voice.Stream->Looping.store(command.Param.Flag, std::memory_order_relaxed);
			break;
		case CommandType::SetVoicePosition:
			voice.Position = { command.Vector[0], command.Vector[1], command.Vector[2] };
			voice.Spatial = true;
			break;
		case CommandType::SetVoiceVelocity:
			voice.Velocity = { command.Vector[0], command.Vector[1], command.Vector[2] };
			break;
		case CommandType::SetVoiceDirection:
			voice.Direction = Normalize({ command.Vector[0], command.Vector[1], command.Vector[2] }, { 0.0f, 0.0f, 1.0f });
			voice.HasDirection = true;
			break;
		case CommandType::SetVoiceLowPass:
			voice.LowPass = command.Param.Value;
			voice.FiltersDirty = true;
			break;
		case CommandType::SetVoiceHighPass:
			voice.HighPass = command.Param.Value;
			voice.FiltersDirty = true;
			break;
		case CommandType::SeekVoice:
			voice.PendingSeek = std::max(0.0, command.Seconds);
			break;
		case CommandType::SetVoiceSend:
			SetSend(voice, command.Send);
			break;
		case CommandType::SetVoiceEffects:
			voice.Effects = command.Effects;
			break;
		default:
			break;
		}
	}

	void Mixer::StartVoice(uint32_t index, uint32_t generation, const PlayCommand& play)
	{
		if (index >= m_Voices.size())
			return;
		Voice& voice = m_Voices[index];
		// A slot that is still playing was stolen by the game thread and the new voice replaces it.
		// If the old one was audible it fades out over the next block instead of cutting off. Its
		// resources stay alive until then because the batch is only reported done after rendering.
		if (voice.Active && voice.Real && voice.GainsValid && m_Dying.size() < MaxDyingVoices)
		{
			m_Dying.push_back(voice);
			m_Dying.back().StartOffset = 0;
		}
		const bool listed = voice.Listed;
		voice = Voice{};
		voice.Listed = true;
		if (!listed)
			m_ActiveVoices.push_back(index);

		voice.Active = true;
		voice.Generation = generation;
		voice.Sound = play.Sound;
		voice.Asset = play.Sound->Asset.get();
		voice.Stream = play.Stream;
		voice.Group = play.Stream != nullptr ? m_BatchSequence : 0;
		voice.Bus = play.Bus < m_Buses.size() && m_Buses[play.Bus].Active ? play.Bus : 0;
		voice.Priority = play.Sound->Desc.Priority;
		voice.Volume.Set(play.Volume);
		voice.Fade.Set(1.0f);
		voice.Gate.Set(1.0f);
		voice.Pitch = play.Pitch;
		voice.Pan = play.Pan;
		voice.Looping = play.Looping;
		voice.Paused = play.Paused;
		voice.DelayFrames = play.DelayFrames;
		voice.FadeInFrames = play.FadeInFrames;
		voice.StartTime = play.StartTime;
		m_Status[index].Position.store(0, std::memory_order_relaxed);
		m_Status[index].Audibility.store(0.0f, std::memory_order_relaxed);
		m_Status[index].State.store(PackVoiceState(generation, VoiceState::Waiting), std::memory_order_release);
	}

	void Mixer::StopVoice(Voice& voice, uint32_t fadeFrames)
	{
		// Voices that haven't started, or are silent while held (paused), have nothing to fade.
		if (!voice.Started || (voice.Gate.Value <= 0.0f && voice.Gate.Target <= 0.0f))
		{
			EndVoice(voice);
			return;
		}
		voice.Stopping = true;
		voice.Fade.Start(0.0f, std::max(fadeFrames, m_DeclickFrames));
	}

	void Mixer::PauseVoice(Voice& voice, bool paused, uint32_t fadeFrames)
	{
		if (paused)
		{
			if (voice.Paused || voice.PauseAfterFade)
				return;
			if (fadeFrames > 0 && voice.Started && !voice.Stopping)
			{
				voice.PauseAfterFade = true;
				voice.Fade.Start(0.0f, fadeFrames);
			}
			else
				voice.Paused = true;
			return;
		}

		const bool wasPaused = voice.Paused || voice.PauseAfterFade;
		voice.Paused = false;
		voice.PauseAfterFade = false;
		if (!wasPaused || voice.Stopping)
			return;
		// A voice that went silent without a fade still fades back in when asked to.
		if (fadeFrames > 0 && voice.Gate.Value <= 0.0f && voice.Gate.Target <= 0.0f)
			voice.Fade.Set(0.0f);
		if (voice.Fade.Target < 1.0f)
			voice.Fade.Start(1.0f, fadeFrames);
	}

	void Mixer::EndVoice(Voice& voice)
	{
		voice.Active = false;
		const uint32_t index = uint32_t(&voice - m_Voices.data());
		m_Status[index].Audibility.store(0.0f, std::memory_order_relaxed);
		m_Status[index].State.store(PackVoiceState(voice.Generation, VoiceState::Ended), std::memory_order_release);
	}

	void Mixer::SetSend(Voice& voice, const SendCommand& command)
	{
		if (command.Bus >= m_Buses.size() || !m_Buses[command.Bus].Active)
			return;
		const uint32_t frames = voice.Started ? std::max(command.Frames, m_DeclickFrames) : command.Frames;
		for (uint32_t i = 0; i < voice.SendCount; i++)
		{
			if (voice.Sends[i].Bus == command.Bus)
			{
				voice.Sends[i].Level.Start(command.Level, frames);
				return;
			}
		}
		if (command.Level <= 0.0f || voice.SendCount == std::size(voice.Sends))
			return;
		Voice::Send& send = voice.Sends[voice.SendCount++];
		send = {};
		send.Bus = uint16_t(command.Bus);
		send.Level.Set(voice.Started ? 0.0f : command.Level);
		send.Level.Start(command.Level, voice.Started ? frames : 0);
		send.Gain = send.Target = send.Level.Value;
	}

	void Mixer::CreateBus(uint32_t index, uint32_t parent)
	{
		if (index == 0 || index >= m_Buses.size() || m_Buses[index].Active)
			return;
		Bus& bus = m_Buses[index];
		float* buffer = bus.Buffer;
		bus = Bus{};
		bus.Buffer = buffer;
		bus.Active = true;
		bus.Parent = uint16_t(parent < m_Buses.size() && m_Buses[parent].Active ? parent : 0);
		bus.Depth = uint16_t(m_Buses[bus.Parent].Depth + 1);
		const auto position = std::upper_bound(m_BusOrder.begin(), m_BusOrder.end(), bus.Depth, [this](uint16_t depth, uint16_t other) { return depth < m_Buses[other].Depth; });
		m_BusOrder.insert(position, uint16_t(index));
	}

	void Mixer::SetListener(const float* values)
	{
		m_ListenerPosition = { values[0], values[1], values[2] };
		const Vec3 forward = Normalize({ values[3], values[4], values[5] }, { 0.0f, 0.0f, 1.0f });
		const Vec3 upHint = { values[6], values[7], values[8] };
		const Vec3 fallback = std::abs(forward.y) < 0.99f ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(0.0f, 0.0f, 1.0f);
		Vec3 up = Normalize(upHint - forward * Dot(upHint, forward), fallback);
		up = Normalize(up - forward * Dot(up, forward), fallback);
		m_ListenerVelocity = { values[9], values[10], values[11] };
		m_ListenerForward = forward;
		m_ListenerRight = m_Config.Handedness == Handedness::LeftHanded ? Cross(up, forward) : Cross(forward, up);
	}

	bool Mixer::IsInBus(uint32_t bus, uint32_t ancestor) const
	{
		for (uint32_t depth = 0; depth <= m_Buses.size(); depth++)
		{
			if (bus == ancestor)
				return true;
			if (bus == 0)
				return false;
			bus = m_Buses[bus].Parent;
		}
		return false;
	}

	void Mixer::RenderBlock(float* output, uint32_t frames)
	{
		UpdateBuses(frames);
		UpdateStartGroups();

		m_Candidates.clear();
		uint32_t streams = 0;
		for (uint32_t index : m_ActiveVoices)
		{
			Voice& voice = m_Voices[index];
			if (!voice.Active)
				continue;
			if (voice.Stream)
				streams++;
			if (PrepareVoice(voice, frames))
				m_Candidates.push_back({ index, voice.Priority, voice.Audibility * (voice.Real ? 1.25f : 1.0f) });
			else if (voice.Real)
			{
				// Held voices keep their gains at zero, they ramp back up when they resume.
				voice.Real = false;
				voice.GainsValid = false;
			}
		}

		// Only the most important voices are mixed, the others advance silently.
		const size_t maxAudible = m_Config.MaxAudibleVoices;
		if (m_Candidates.size() > maxAudible)
		{
			std::nth_element(m_Candidates.begin(), m_Candidates.begin() + ptrdiff_t(maxAudible), m_Candidates.end(), [](const Candidate& a, const Candidate& b)
			{
				return a.Priority != b.Priority ? a.Priority > b.Priority : a.Score > b.Score;
			});
		}

		uint32_t audible = 0;
		for (size_t i = 0; i < m_Candidates.size(); i++)
		{
			Voice& voice = m_Voices[m_Candidates[i].Index];
			const bool loud = voice.Audibility >= SilenceThreshold;
			if (i < maxAudible && (loud || voice.Real))
			{
				RenderVoice(voice, frames, false);
				voice.Real = loud;
				audible++;
			}
			else if (voice.Real)
			{
				// Fade out over one block when a voice becomes virtual.
				RenderVoice(voice, frames, true);
				voice.Real = false;
			}
			else
				AdvanceVoice(voice, frames);

			if (!voice.Real)
				voice.GainsValid = false;
			if (voice.Finished && voice.Active)
				EndVoice(voice);
		}

		for (Voice& voice : m_Dying)
			RenderVoice(voice, frames, true);
		m_Dying.clear();

		MixBuses(output, frames);

		size_t kept = 0;
		for (uint32_t index : m_ActiveVoices)
		{
			Voice& voice = m_Voices[index];
			if (voice.Active)
				m_ActiveVoices[kept++] = index;
			else
				voice.Listed = false;
		}
		m_ActiveVoices.resize(kept);
		m_AudibleCount.store(audible, std::memory_order_relaxed);
		m_StreamCount.store(streams, std::memory_order_relaxed);
	}

	void Mixer::UpdateBuses(uint32_t frames)
	{
		const float seconds = float(frames) / float(m_Config.SampleRate);
		for (uint16_t index : m_BusOrder)
		{
			Bus& bus = m_Buses[index];
			const Bus* parent = index == 0 ? nullptr : &m_Buses[bus.Parent];
			bus.EffectivePaused = bus.Paused || (parent != nullptr && parent->EffectivePaused);
			bus.EffectivePitch = bus.Pitch * (parent != nullptr ? parent->EffectivePitch : 1.0f);

			const bool ducked = bus.DuckTrigger != NoBus && m_Buses[bus.DuckTrigger].Level > bus.DuckThreshold;
			const float duckTarget = ducked ? bus.DuckGain : 1.0f;
			const float duckTime = duckTarget < bus.Duck ? bus.DuckAttack : bus.DuckRelease;
			bus.Duck += (duckTarget - bus.Duck) * (1.0f - std::exp(-seconds / duckTime));

			bus.GainEnd = bus.Volume.Advance(frames) * bus.Mute.Advance(frames) * bus.Snapshot.Advance(frames) * bus.Duck;
			bus.EffectiveGain = bus.GainEnd * (parent != nullptr ? parent->EffectiveGain : 1.0f);

			const float lowPass = std::min(bus.LowPass.Advance(frames), bus.SnapshotLowPass.Advance(frames));
			if (lowPass < OpenLowPassLog - 0.01f)
			{
				if (!bus.LowPassActive || std::abs(lowPass - bus.LowPassLog) > 1e-4f)
					bus.LowPassCoefficients = ComputeBiquad(FilterType::LowPass, std::exp2(lowPass), 0.7071f, 0.0f, float(m_Config.SampleRate));
				if (!bus.LowPassActive)
					bus.LowPassState[0] = bus.LowPassState[1] = {};
				bus.LowPassActive = true;
				bus.LowPassLog = lowPass;
			}
			else
				bus.LowPassActive = false;

			if (bus.Dirty)
				std::fill(bus.Buffer, bus.Buffer + size_t(BlockFrames) * 2, 0.0f);
			bus.Dirty = false;
			bus.HasInput = false;
		}
	}

	void Mixer::UpdateStartGroups()
	{
		m_Groups.clear();
		for (uint32_t index : m_ActiveVoices)
		{
			Voice& voice = m_Voices[index];
			if (!voice.Active || voice.Started || voice.Group == 0 || voice.Paused || m_Buses[voice.Bus].EffectivePaused)
				continue;
			// A seek before the start changes what has to be buffered, so it's done first.
			if (voice.PendingSeek >= 0.0 && !voice.FlushPending)
				PerformSeek(voice);
			const bool ready = UpdateStream(voice);
			if (!voice.Active)
				continue;

			auto group = std::find_if(m_Groups.begin(), m_Groups.end(), [&](const StartGroup& g) { return g.Id == voice.Group; });
			if (group == m_Groups.end())
			{
				if (m_Groups.size() == MaxStartGroups)
				{
					voice.Group = 0;
					continue;
				}
				m_Groups.push_back({ voice.Group, 0, 0 });
				group = m_Groups.end() - 1;
			}
			group->Members++;
			group->Ready += ready ? 1 : 0;
		}
	}

	bool Mixer::PrepareVoice(Voice& voice, uint32_t frames)
	{
		voice.StartOffset = 0;
		if (!voice.Started)
		{
			// Voices played paused, or into a paused bus, wait without using up their delay.
			if (voice.Paused || m_Buses[voice.Bus].EffectivePaused)
				return false;
			// Streams played in the same batch wait until all of them are buffered, then start in
			// the same block, so layered music stays in sync. A slow one doesn't hold the rest forever.
			if (voice.Group != 0)
			{
				const auto group = std::find_if(m_Groups.begin(), m_Groups.end(), [&](const StartGroup& g) { return g.Id == voice.Group; });
				if (group != m_Groups.end() && group->Ready < group->Members && voice.GroupWait < m_MaxGroupWait)
				{
					voice.GroupWait += frames;
					return false;
				}
				voice.Group = 0;
			}
			uint32_t offset = 0;
			if (voice.DelayFrames > 0)
			{
				if (voice.DelayFrames >= frames)
				{
					voice.DelayFrames -= frames;
					return false;
				}
				offset = voice.DelayFrames;
				voice.DelayFrames = 0;
			}
			if (!StartPlayback(voice))
				return false;
			voice.StartOffset = offset;
		}

		if (voice.Stream && !UpdateStream(voice))
		{
			if (voice.Active && voice.Stopping)
				EndVoice(voice);
			return false;
		}

		if (voice.LoopingChanged)
		{
			voice.LoopingChanged = false;
			// Restarting the stream where it is makes it loop even if it already decoded past the loop end.
			if (voice.Stream && voice.Looping && NeedsLoopRestart(voice) && voice.PendingSeek < 0.0)
				voice.PendingSeek = double(GetPosition(voice)) / voice.SampleRate;
		}

		const Bus& bus = m_Buses[voice.Bus];
		const bool hold = voice.Paused || bus.EffectivePaused || voice.PendingSeek >= 0.0;
		const float gate = hold ? 0.0f : 1.0f;
		if (voice.Gate.Target != gate)
			voice.Gate.Start(gate, m_DeclickFrames);
		if (hold && voice.Gate.Value <= 0.0f)
		{
			// A voice paused while fading out stays paused with its fade frozen.
			if (voice.PendingSeek >= 0.0)
			{
				if (voice.Stopping)
				{
					EndVoice(voice);
					return false;
				}
				PerformSeek(voice);
			}
			return false;
		}

		ComputeTargets(voice, bus, frames);
		return true;
	}

	bool Mixer::NeedsLoopRestart(const Voice& voice) const
	{
		const StreamInstance& stream = *voice.Stream;
		if (voice.SegmentLoop > 0)
			return false;
		const size_t count = stream.Markers.Size();
		for (size_t i = 0; i < count; i++)
			if (stream.Markers.Peek(i).Type == StreamMarker::Kind::Loop)
				return false;

		uint64_t loopStart = 0;
		uint64_t loopEnd = 0;
		voice.Sound->GetLoopRegion(loopStart, loopEnd);
		const uint64_t position = GetPosition(voice);
		// Past the loop end it plays to the end, the same as decompressed sounds.
		if (position >= loopEnd)
			return false;
		if (voice.StreamEnd != UINT64_MAX)
			return true;
		const uint64_t buffered = stream.WriteFrame.load(std::memory_order_acquire) - uint64_t(voice.Cursor);
		return position + buffered > loopEnd;
	}

	bool Mixer::StartPlayback(Voice& voice)
	{
		const LoadState state = voice.Asset->State.load(std::memory_order_acquire);
		if (state == LoadState::Loading)
			return false;
		if (state == LoadState::Failed)
		{
			EndVoice(voice);
			return false;
		}

		if (voice.Stream)
		{
			// A seek before the start restarts the stream at the new position right away.
			if (voice.PendingSeek >= 0.0 && !voice.FlushPending)
				PerformSeek(voice);
			if (!UpdateStream(voice))
				return false;
		}
		else
		{
			if (voice.PendingSeek >= 0.0)
			{
				voice.StartTime = voice.PendingSeek;
				voice.PendingSeek = -1.0;
			}
			const AudioFormat& format = voice.Asset->Format;
			voice.Channels = format.Channels;
			voice.SampleRate = format.SampleRate;
			voice.FrameCount = format.FrameCount;
			voice.Sound->GetLoopRegion(voice.LoopStart, voice.LoopEnd);
			voice.Cursor = int64_t(std::min(SecondsToFrames(voice.StartTime, voice.SampleRate), voice.FrameCount));
		}

		voice.Started = true;
		// Starting anywhere but the beginning, including after a seek, fades in to avoid a click.
		const bool midSound = voice.StartTime > 0.0 || voice.SeekSerial > 0;
		voice.InstantStart = voice.FadeInFrames == 0 && !midSound;
		if (voice.FadeInFrames > 0)
		{
			voice.Fade.Set(0.0f);
			voice.Fade.Start(1.0f, voice.FadeInFrames);
		}
		else if (midSound)
		{
			voice.Gate.Set(0.0f);
			voice.Gate.Start(1.0f, m_DeclickFrames);
		}
		const uint32_t index = uint32_t(&voice - m_Voices.data());
		m_Status[index].State.store(PackVoiceState(voice.Generation, VoiceState::Playing), std::memory_order_release);
		return true;
	}

	bool Mixer::UpdateStream(Voice& voice)
	{
		StreamInstance& stream = *voice.Stream;
		const StreamStatus status = stream.Status.load(std::memory_order_acquire);
		if (status == StreamStatus::Opening)
			return false;
		if (status == StreamStatus::Failed)
		{
			EndVoice(voice);
			return false;
		}
		if (voice.Channels == 0)
		{
			voice.Channels = stream.Channels;
			voice.SampleRate = stream.SampleRate;
			voice.SegmentSource = SecondsToFrames(voice.StartTime, stream.SampleRate);
		}

		if (voice.FlushPending)
		{
			// Everything before the answer to our seek request is stale.
			while (voice.FlushPending && stream.Markers.Size() > 0)
			{
				const StreamMarker marker = stream.Markers.Peek(0);
				stream.Markers.Pop();
				if (marker.Type != StreamMarker::Kind::Flush || marker.Serial != voice.SeekSerial)
					continue;
				voice.Cursor = int64_t(marker.RingFrame);
				voice.Fraction = 0;
				voice.StreamStart = marker.RingFrame;
				voice.StreamEnd = UINT64_MAX;
				voice.SegmentRing = marker.RingFrame;
				voice.SegmentSource = marker.SourceFrame;
				voice.SegmentLoop = 0;
				voice.FlushPending = false;
				voice.Buffering = true;
				stream.ReadFrame.store(marker.RingFrame, std::memory_order_release);
			}
			if (voice.FlushPending)
				return false;
		}

		if (voice.StreamEnd == UINT64_MAX)
		{
			const size_t count = stream.Markers.Size();
			for (size_t i = 0; i < count; i++)
			{
				const StreamMarker& marker = stream.Markers.Peek(i);
				if (marker.Type == StreamMarker::Kind::End)
				{
					voice.StreamEnd = marker.RingFrame;
					break;
				}
			}
		}

		if (voice.Buffering)
		{
			const uint64_t available = stream.WriteFrame.load(std::memory_order_acquire) - uint64_t(voice.Cursor);
			const uint64_t block = ((uint64_t(voice.Fraction) + uint64_t(BlockFrames) * voice.Step) >> 32) + 8;
			const uint64_t start = std::max<uint64_t>(std::min<uint64_t>(stream.Capacity / 2, stream.SampleRate / 10), block);
			if (available < start && voice.StreamEnd == UINT64_MAX)
			{
				m_Streamer->Wake();
				return false;
			}
			voice.Buffering = false;
		}
		return true;
	}

	void Mixer::PerformSeek(Voice& voice)
	{
		const double seconds = voice.PendingSeek;
		voice.PendingSeek = -1.0;
		if (voice.Stream)
		{
			voice.SeekSerial++;
			voice.Stream->SeekTime.store(seconds, std::memory_order_relaxed);
			voice.Stream->SeekSerial.store(voice.SeekSerial, std::memory_order_release);
			voice.FlushPending = true;
			m_Streamer->Wake();
			return;
		}
		voice.Cursor = int64_t(std::min(SecondsToFrames(seconds, voice.SampleRate), voice.FrameCount));
		voice.Fraction = 0;
	}

	void Mixer::ComputeTargets(Voice& voice, const Bus& bus, uint32_t frames)
	{
		float gain = voice.Volume.Advance(frames) * voice.Fade.Advance(frames) * voice.Gate.Advance(frames);
		if (voice.Fade.IsDone() && voice.Fade.Value <= 0.0f)
		{
			if (voice.Stopping)
				voice.Finished = true;
			else if (voice.PauseAfterFade)
			{
				// Already silent, so the gate closes at once and the voice holds from the next block.
				voice.PauseAfterFade = false;
				voice.Paused = true;
				voice.Gate.Set(0.0f);
			}
		}

		float pitch = voice.Pitch * bus.EffectivePitch;
		float pan = voice.Pan;
		float rear = 0.0f;
		float airCutoff = 0.0f;
		if (voice.Spatial)
		{
			float doppler = 1.0f;
			gain *= Spatialize(voice, pan, rear, airCutoff, doppler);
			pitch *= doppler;
		}

		const bool binaural = voice.Spatial && m_Binaural;
		if (binaural != voice.Binaural)
		{
			voice.Binaural = binaural;
			voice.GainsValid = false;
		}

		float matrix[4] = {};
		if (binaural)
		{
			SetBinauralTargets(voice, pan);
			matrix[0] = matrix[1] = BinauralGain;
		}
		else
		{
			float left = 1.0f;
			float right = 1.0f;
			if (voice.Spatial)
				ConstantPowerGains(pan, left, right);
			else
				PanGains(pan, left, right);

			// Mono: left and right gains. Stereo: a 2x2 matrix; 3D voices fold stereo to a mono point
			// source, 2D voices keep the channels and pan as a balance control.
			matrix[0] = left;
			matrix[1] = right;
			if (voice.Channels == 2)
			{
				if (voice.Spatial)
				{
					matrix[0] = matrix[1] = 0.5f * left;
					matrix[2] = matrix[3] = 0.5f * right;
				}
				else
				{
					matrix[0] = left;
					matrix[1] = 0.0f;
					matrix[2] = 0.0f;
					matrix[3] = right;
				}
			}
		}

		// The game side rejects NaN and infinity, this is the last line of defense: a bad step would
		// make the voice read far past its buffers.
		if (!std::isfinite(gain))
			gain = 0.0f;
		float loudest = 0.0f;
		for (int i = 0; i < 4; i++)
		{
			voice.TargetGains[i] = matrix[i] * gain;
			loudest = std::max(loudest, std::abs(voice.TargetGains[i]));
		}

		float audibility = loudest * bus.EffectiveGain;
		for (uint32_t i = 0; i < voice.SendCount;)
		{
			Voice::Send& send = voice.Sends[i];
			send.Target = send.Level.Advance(frames);
			if (send.Target <= 0.0f && send.Gain <= 0.0f && send.Level.IsDone())
				send = voice.Sends[--voice.SendCount];
			else
			{
				audibility = std::max(audibility, loudest * std::max(send.Target, send.Gain) * m_Buses[send.Bus].EffectiveGain);
				i++;
			}
		}
		voice.Audibility = audibility;

		voice.TargetRearGain = rear > 0.0f && m_Config.RearHighCutDb > 0.0f ? DecibelsToLinear(-m_Config.RearHighCutDb * rear) : 1.0f;
		if (airCutoff > 0.0f)
		{
			voice.AirCoefficient = OnePoleCoefficient(airCutoff, float(m_Config.SampleRate));
			voice.AirFresh = voice.AirFresh || !voice.AirActive;
			voice.AirActive = true;
		}
		else
			voice.AirActive = false;

		double maxStep = double(MaxStep);
		if (voice.Stream && voice.StreamEnd == UINT64_MAX)
			maxStep = std::min(maxStep, double(voice.Stream->Capacity / 2 - 8) / double(BlockFrames));
		double step = double(pitch) * voice.SampleRate / m_Config.SampleRate;
		if (!std::isfinite(step))
			step = 1.0;
		step = std::clamp(step, 1.0 / 1024.0, maxStep);
		voice.Step = uint64_t(step * double(UnitStep) + 0.5);
	}

	void Mixer::SetBinauralTargets(Voice& voice, float lateral) const
	{
		const float rate = float(m_Config.SampleRate);
		const float k = rate * HeadRadius / SpeedOfSoundInAir;
		const float normalize = 1.0f / (1.0f + k);
		const float thetaRight = std::acos(std::clamp(lateral, -1.0f, 1.0f));
		const float theta[2] = { Pi - thetaRight, thetaRight };
		for (int ear = 0; ear < 2; ear++)
		{
			voice.TargetDelay[ear] = std::min(WoodworthDelay(theta[ear]) * rate, float(BinauralHistory - 2));
			const float alpha = HeadShadowAlpha(theta[ear]);
			voice.ShadowB0[ear] = (1.0f + alpha * k) * normalize;
			voice.ShadowB1[ear] = (1.0f - alpha * k) * normalize;
			voice.ShadowA1[ear] = (1.0f - k) * normalize;
		}
	}

	float Mixer::Spatialize(const Voice& voice, float& pan, float& rear, float& airCutoff, float& doppler) const
	{
		const SpatialSettings& settings = voice.Sound->Desc.Spatial;
		const Vec3 offset = voice.Position - m_ListenerPosition;
		const float distance = Length(offset);
		float gain = Attenuate(settings, distance);
		pan = 0.0f;
		rear = 0.0f;
		doppler = 1.0f;
		airCutoff = AirAbsorptionCutoff(settings.AirAbsorption, distance);
		if (distance <= 1e-4f)
			return gain;

		const Vec3 direction = offset * (1.0f / distance);
		if (voice.HasDirection && settings.ConeOuterAngle < 360.0f)
		{
			// Full cone angle between the voice's facing and the listener, in degrees.
			const float angle = std::acos(std::clamp(-Dot(voice.Direction, direction), -1.0f, 1.0f)) * (360.0f / Pi);
			const float inner = settings.ConeInnerAngle;
			const float outer = std::max(settings.ConeOuterAngle, inner);
			if (angle >= outer)
				gain *= settings.ConeOuterVolume;
			else if (angle > inner)
				gain *= 1.0f + (settings.ConeOuterVolume - 1.0f) * (angle - inner) / (outer - inner);
		}

		// Sources very close to the listener pan towards the center instead of jumping sides.
		const float closeness = std::min(1.0f, distance / std::max(settings.MinDistance * 0.25f, 1e-3f));
		pan = Dot(direction, m_ListenerRight) * closeness;
		rear = std::max(0.0f, -Dot(direction, m_ListenerForward)) * closeness;

		if (settings.DopplerFactor > 0.0f)
		{
			const float speed = m_Config.SpeedOfSound;
			const float limit = 0.9f * speed / settings.DopplerFactor;
			const float listenerSpeed = std::min(-Dot(m_ListenerVelocity, direction), limit);
			const float sourceSpeed = std::min(-Dot(voice.Velocity, direction), limit);
			doppler = std::clamp((speed - settings.DopplerFactor * listenerSpeed) / (speed - settings.DopplerFactor * sourceSpeed), 0.25f, 4.0f);
		}
		return gain;
	}

	bool Mixer::HasStreamData(const Voice& voice, uint32_t frames) const
	{
		if (voice.StreamEnd != UINT64_MAX)
			return true;
		const uint64_t needed = ((uint64_t(voice.Fraction) + uint64_t(frames) * voice.Step) >> 32) + 3;
		return voice.Stream->WriteFrame.load(std::memory_order_acquire) - uint64_t(voice.Cursor) >= needed;
	}

	void Mixer::RenderVoice(Voice& voice, uint32_t frames, bool fadeOut)
	{
		const uint32_t offset = voice.StartOffset;
		const uint32_t count = frames - offset;
		if (voice.Stream && !HasStreamData(voice, count))
		{
			// The streaming thread fell behind: wait for it, then resume with a short fade in.
			voice.Buffering = true;
			voice.GainsValid = false;
			m_Underruns.fetch_add(1, std::memory_order_relaxed);
			m_Streamer->Wake();
			return;
		}

		float* samples = m_VoiceBuffer.data();
		if (voice.Step == UnitStep && voice.Fraction == 0)
			Fetch(voice, voice.Cursor, count, samples);
		else
		{
			const uint32_t needed = uint32_t((uint64_t(voice.Fraction) + uint64_t(count - 1) * voice.Step) >> 32) + 4;
			Fetch(voice, voice.Cursor - 1, needed, m_SourceBuffer.data());
			Resample(voice, m_SourceBuffer.data(), samples, count);
		}
		ApplyFilters(voice, samples, count);

		if (!voice.GainsValid)
		{
			for (int i = 0; i < 4; i++)
				voice.Gains[i] = voice.InstantStart ? voice.TargetGains[i] : 0.0f;
			for (int ear = 0; ear < 2; ear++)
			{
				voice.Delay[ear] = voice.TargetDelay[ear];
				voice.ShadowX[ear] = voice.ShadowY[ear] = 0.0f;
			}
			std::fill(std::begin(voice.History), std::end(voice.History), 0.0f);
			for (uint32_t i = 0; i < voice.SendCount; i++)
				voice.Sends[i].Gain = voice.Sends[i].Target;
			voice.GainsValid = true;
		}
		voice.InstantStart = false;
		const float silence[4] = {};
		const float* target = fadeOut ? silence : voice.TargetGains;

		Bus& bus = m_Buses[voice.Bus];
		const bool hasEffects = voice.Effects != nullptr && !voice.Effects->Effects.empty();
		if (!hasEffects && voice.SendCount == 0)
		{
			float* destination = bus.Buffer + size_t(offset) * 2;
			if (voice.Binaural)
				RenderBinaural<true>(voice, samples, destination, count, target);
			else
				Pan<true>(samples, voice.Channels, destination, count, voice.Gains, target);
		}
		else
		{
			float* wet = m_EffectBuffer.data();
			std::fill(wet, wet + size_t(frames) * 2, 0.0f);
			if (voice.Binaural)
				RenderBinaural<false>(voice, samples, wet + size_t(offset) * 2, count, target);
			else
				Pan<false>(samples, voice.Channels, wet + size_t(offset) * 2, count, voice.Gains, target);
			if (hasEffects)
			{
				for (Effect* effect : voice.Effects->Effects)
					if (effect->IsEnabled())
						effect->Process(wet, frames);
				Sanitize(wet, size_t(frames) * 2);
			}
			for (uint32_t i = 0; i < frames * 2; i++)
				bus.Buffer[i] += wet[i];
			MixSends(voice, wet, frames);
		}
		bus.HasInput = true;

		std::memcpy(voice.Gains, target, sizeof(voice.Gains));
		Advance(voice, count);
	}

	template<bool Accumulate>
	void Mixer::RenderBinaural(Voice& voice, const float* samples, float* destination, uint32_t frames, const float* target)
	{
		float* buffer = m_BinauralBuffer.data();
		std::memcpy(buffer, voice.History, sizeof(voice.History));
		float* input = buffer + BinauralHistory;
		if (voice.Channels == 2)
		{
			for (uint32_t i = 0; i < frames; i++)
				input[i] = 0.5f * (samples[i * 2] + samples[i * 2 + 1]);
		}
		else
			std::memcpy(input, samples, size_t(frames) * sizeof(float));
		input[frames] = input[frames - 1];

		const float inverse = 1.0f / float(frames);
		float fromDelay[2];
		float stepDelay[2];
		float fromGain[2];
		float stepGain[2];
		float x1[2];
		float y1[2];
		float b0[2];
		float b1[2];
		float a1[2];
		for (uint32_t ear = 0; ear < 2; ear++)
		{
			fromDelay[ear] = float(BinauralHistory) - voice.Delay[ear];
			stepDelay[ear] = (voice.Delay[ear] - voice.TargetDelay[ear]) * inverse;
			fromGain[ear] = voice.Gains[ear];
			stepGain[ear] = (target[ear] - fromGain[ear]) * inverse;
			x1[ear] = voice.ShadowX[ear];
			y1[ear] = voice.ShadowY[ear];
			b0[ear] = voice.ShadowB0[ear];
			b1[ear] = voice.ShadowB1[ear];
			a1[ear] = voice.ShadowA1[ear];
		}
		for (uint32_t i = 0; i < frames; i++)
		{
			const float t = float(i + 1);
			for (uint32_t ear = 0; ear < 2; ear++)
			{
				const float position = float(i) + fromDelay[ear] + stepDelay[ear] * t;
				const uint32_t index = uint32_t(position);
				const float fraction = position - float(index);
				const float x = buffer[index] + fraction * (buffer[index + 1] - buffer[index]);
				const float y = b0[ear] * x + b1[ear] * x1[ear] - a1[ear] * y1[ear];
				x1[ear] = x;
				y1[ear] = y;
				const float out = y * (fromGain[ear] + stepGain[ear] * t);
				if constexpr (Accumulate)
					destination[i * 2 + ear] += out;
				else
					destination[i * 2 + ear] = out;
			}
		}
		for (uint32_t ear = 0; ear < 2; ear++)
		{
			voice.ShadowX[ear] = x1[ear];
			voice.ShadowY[ear] = y1[ear];
			voice.Delay[ear] = voice.TargetDelay[ear];
		}
		std::memcpy(voice.History, buffer + frames, sizeof(voice.History));
	}

	void Mixer::MixSends(Voice& voice, const float* wet, uint32_t frames)
	{
		for (uint32_t i = 0; i < voice.SendCount; i++)
		{
			Voice::Send& send = voice.Sends[i];
			const float from = send.Gain;
			send.Gain = send.Target;
			if (from <= 0.0f && send.Target <= 0.0f)
				continue;
			Bus& target = m_Buses[send.Bus];
			AddScaled(target.Buffer, wet, frames, from, send.Target);
			target.HasInput = true;
		}
	}

	void Mixer::AdvanceVoice(Voice& voice, uint32_t frames)
	{
		// A voice that becomes audible mid-sound fades in instead of starting at full volume.
		voice.InstantStart = false;
		const uint32_t count = frames - voice.StartOffset;
		if (voice.Stream && !HasStreamData(voice, count))
		{
			voice.Buffering = true;
			m_Underruns.fetch_add(1, std::memory_order_relaxed);
			m_Streamer->Wake();
			return;
		}
		Advance(voice, count);
	}

	void Mixer::Fetch(const Voice& voice, int64_t start, uint32_t count, float* destination) const
	{
		if (voice.Stream)
			FetchStream(voice, start, count, destination);
		else
			FetchStatic(voice, start, count, destination);
	}

	void Mixer::FetchStatic(const Voice& voice, int64_t start, uint32_t count, float* destination) const
	{
		const uint32_t channels = voice.Channels;
		const int16_t* pcm = voice.Asset->Pcm.data();
		const int64_t length = int64_t(voice.FrameCount);
		const int64_t loopStart = int64_t(voice.LoopStart);
		const int64_t loopEnd = int64_t(voice.LoopEnd);
		const bool loop = voice.Looping && voice.Cursor < loopEnd;

		int64_t frame = start;
		uint32_t done = 0;
		while (done < count)
		{
			float* out = destination + size_t(done) * channels;
			uint32_t run = 0;
			if (frame < 0)
			{
				run = uint32_t(std::min<int64_t>(count - done, -frame));
				std::fill(out, out + size_t(run) * channels, 0.0f);
			}
			else if (loop && frame >= loopEnd)
			{
				frame = loopStart + (frame - loopStart) % (loopEnd - loopStart);
				continue;
			}
			else if (frame >= length)
			{
				std::fill(out, destination + size_t(count) * channels, 0.0f);
				return;
			}
			else
			{
				run = uint32_t(std::min<int64_t>(count - done, (loop ? loopEnd : length) - frame));
				const int16_t* in = pcm + frame * channels;
				for (size_t i = 0; i < size_t(run) * channels; i++)
					out[i] = float(in[i]) * (1.0f / 32768.0f);
			}
			frame += run;
			done += run;
		}
	}

	void Mixer::FetchStream(const Voice& voice, int64_t start, uint32_t count, float* destination) const
	{
		const StreamInstance& stream = *voice.Stream;
		const uint32_t channels = voice.Channels;
		const uint64_t mask = stream.Capacity - 1;
		const int16_t* ring = stream.Ring.data();

		uint32_t done = 0;
		while (done < count)
		{
			const int64_t frame = start + int64_t(done);
			float* out = destination + size_t(done) * channels;
			if (frame < int64_t(voice.StreamStart))
			{
				const uint32_t run = uint32_t(std::min<int64_t>(count - done, int64_t(voice.StreamStart) - frame));
				std::fill(out, out + size_t(run) * channels, 0.0f);
				done += run;
				continue;
			}
			if (uint64_t(frame) >= voice.StreamEnd)
			{
				std::fill(out, destination + size_t(count) * channels, 0.0f);
				return;
			}
			const uint64_t slot = uint64_t(frame) & mask;
			const uint64_t run = std::min<uint64_t>({ uint64_t(count - done), stream.Capacity - slot, voice.StreamEnd - uint64_t(frame) });
			const int16_t* in = ring + slot * channels;
			for (size_t i = 0; i < size_t(run) * channels; i++)
				out[i] = float(in[i]) * (1.0f / 32768.0f);
			done += uint32_t(run);
		}
	}

	void Mixer::Resample(const Voice& voice, const float* source, float* destination, uint32_t frames) const
	{
		const bool cubic = m_Config.Interpolation == Interpolation::Cubic;
		if (voice.Channels == 1)
		{
			if (cubic)
				ResampleKernel<1, true>(source, destination, frames, voice.Fraction, voice.Step);
			else
				ResampleKernel<1, false>(source, destination, frames, voice.Fraction, voice.Step);
		}
		else
		{
			if (cubic)
				ResampleKernel<2, true>(source, destination, frames, voice.Fraction, voice.Step);
			else
				ResampleKernel<2, false>(source, destination, frames, voice.Fraction, voice.Step);
		}
	}

	void Mixer::ApplyFilters(Voice& voice, float* samples, uint32_t frames)
	{
		if (voice.FiltersDirty)
		{
			const float rate = float(m_Config.SampleRate);
			const bool lowPass = voice.LowPass > 0.0f && voice.LowPass < rate * 0.45f;
			const bool highPass = voice.HighPass > 10.0f && voice.HighPass < rate * 0.45f;
			if (lowPass)
			{
				voice.LowPassCoefficients = ComputeBiquad(FilterType::LowPass, voice.LowPass, 0.7071f, 0.0f, rate);
				if (!voice.LowPassActive)
					voice.LowPassState[0] = voice.LowPassState[1] = {};
			}
			if (highPass)
			{
				voice.HighPassCoefficients = ComputeBiquad(FilterType::HighPass, voice.HighPass, 0.7071f, 0.0f, rate);
				if (!voice.HighPassActive)
					voice.HighPassState[0] = voice.HighPassState[1] = {};
			}
			voice.LowPassActive = lowPass;
			voice.HighPassActive = highPass;
			voice.FiltersDirty = false;
		}

		const uint32_t channels = voice.Channels;
		for (uint32_t c = 0; c < channels; c++)
		{
			if (voice.LowPassActive)
			{
				BiquadState state = voice.LowPassState[c];
				for (uint32_t i = 0; i < frames; i++)
					samples[i * channels + c] = ProcessBiquad(voice.LowPassCoefficients, state, samples[i * channels + c]);
				voice.LowPassState[c] = state;
			}
			if (voice.HighPassActive)
			{
				BiquadState state = voice.HighPassState[c];
				for (uint32_t i = 0; i < frames; i++)
					samples[i * channels + c] = ProcessBiquad(voice.HighPassCoefficients, state, samples[i * channels + c]);
				voice.HighPassState[c] = state;
			}
		}

		const bool rear = voice.RearGain < 1.0f || voice.TargetRearGain < 1.0f;
		if (voice.AirActive || rear)
		{
			for (uint32_t c = 0; c < channels; c++)
			{
				if (voice.AirFresh)
					voice.AirState[c] = samples[c];
				if (voice.RearGain >= 1.0f)
					voice.RearState[c] = samples[c];
			}
			const SpatialFilter filter = { voice.AirCoefficient, m_RearCoefficient, voice.RearGain, (voice.TargetRearGain - voice.RearGain) / float(frames) };
			if (channels == 1)
			{
				if (voice.AirActive && rear)
					SpatialFilterKernel<1, true, true>(samples, frames, filter, voice.AirState, voice.RearState);
				else if (voice.AirActive)
					SpatialFilterKernel<1, true, false>(samples, frames, filter, voice.AirState, voice.RearState);
				else
					SpatialFilterKernel<1, false, true>(samples, frames, filter, voice.AirState, voice.RearState);
			}
			else
			{
				if (voice.AirActive && rear)
					SpatialFilterKernel<2, true, true>(samples, frames, filter, voice.AirState, voice.RearState);
				else if (voice.AirActive)
					SpatialFilterKernel<2, true, false>(samples, frames, filter, voice.AirState, voice.RearState);
				else
					SpatialFilterKernel<2, false, true>(samples, frames, filter, voice.AirState, voice.RearState);
			}
		}
		voice.AirFresh = false;
		voice.RearGain = voice.TargetRearGain;
	}

	void Mixer::Advance(Voice& voice, uint32_t frames)
	{
		const int64_t start = voice.Cursor;
		const uint64_t position = uint64_t(voice.Fraction) + uint64_t(frames) * voice.Step;
		voice.Cursor += int64_t(position >> 32);
		voice.Fraction = uint32_t(position);

		if (voice.Stream)
		{
			StreamInstance& stream = *voice.Stream;
			const uint64_t cursor = uint64_t(voice.Cursor);
			while (stream.Markers.Size() > 0)
			{
				const StreamMarker& marker = stream.Markers.Peek(0);
				if (marker.RingFrame > cursor || marker.Type == StreamMarker::Kind::Flush)
					break;
				if (marker.Type == StreamMarker::Kind::Segment || marker.Type == StreamMarker::Kind::Loop)
				{
					voice.SegmentRing = marker.RingFrame;
					voice.SegmentSource = marker.SourceFrame;
					voice.SegmentLoop = marker.Type == StreamMarker::Kind::Loop ? marker.LoopLength : 0;
				}
				stream.Markers.Pop();
			}
			// Keep the frame before the cursor for interpolation, the streamer may overwrite the rest.
			stream.ReadFrame.store(cursor > voice.StreamStart ? cursor - 1 : voice.StreamStart, std::memory_order_release);
			if (cursor >= voice.StreamEnd)
				voice.Finished = true;
			else if (stream.WriteFrame.load(std::memory_order_acquire) - cursor < stream.Capacity / 2)
				m_Streamer->Wake();
			return;
		}

		const bool loop = voice.Looping && start < int64_t(voice.LoopEnd);
		if (loop && voice.Cursor >= int64_t(voice.LoopEnd))
		{
			const int64_t length = int64_t(voice.LoopEnd - voice.LoopStart);
			voice.Cursor = int64_t(voice.LoopStart) + (voice.Cursor - int64_t(voice.LoopStart)) % length;
		}
		else if (!loop && voice.Cursor >= int64_t(voice.FrameCount))
			voice.Finished = true;
	}

	void Mixer::MixBuses(float* output, uint32_t frames)
	{
		const size_t samples = size_t(frames) * 2;
		// Children before parents
		for (auto it = m_BusOrder.rbegin(); it != m_BusOrder.rend(); ++it)
		{
			const uint16_t index = *it;
			Bus& bus = m_Buses[index];
			const bool hasEffects = bus.Effects != nullptr && !bus.Effects->Effects.empty();
			if (!bus.HasInput && !hasEffects)
			{
				bus.Gain = bus.GainEnd;
				bus.Level = 0.0f;
				bus.LowPassState[0] = bus.LowPassState[1] = {};
				continue;
			}

			if (hasEffects)
			{
				for (Effect* effect : bus.Effects->Effects)
					if (effect->IsEnabled())
						effect->Process(bus.Buffer, frames);
				Sanitize(bus.Buffer, samples);
			}
			if (bus.LowPassActive)
			{
				for (uint32_t c = 0; c < 2; c++)
				{
					BiquadState state = bus.LowPassState[c];
					for (uint32_t i = 0; i < frames; i++)
						bus.Buffer[i * 2 + c] = ProcessBiquad(bus.LowPassCoefficients, state, bus.Buffer[i * 2 + c]);
					bus.LowPassState[c] = state;
				}
			}
			ApplyGain(bus.Buffer, frames, bus.Gain, bus.GainEnd);
			bus.Gain = bus.GainEnd;
			bus.Dirty = true;
			float peak = 0.0f;
			for (size_t i = 0; i < samples; i++)
				peak = std::max(peak, std::abs(bus.Buffer[i]));
			bus.Level = peak;
			if (index == 0)
				continue;

			Bus& parent = m_Buses[bus.Parent];
			for (size_t i = 0; i < samples; i++)
				parent.Buffer[i] += bus.Buffer[i];
			parent.HasInput = true;
		}

		Bus& master = m_Buses[0];
		float* buffer = master.Buffer;
		master.Dirty = true;
		// A broken custom effect must not take the limiter or the speakers down with it.
		for (size_t i = 0; i < samples; i++)
		{
			const float sample = buffer[i];
			buffer[i] = sample == sample ? std::clamp(sample, -16.0f, 16.0f) : 0.0f;
		}
		if (m_Config.MasterLimiter)
			static_cast<Effect*>(m_Limiter.get())->Process(buffer, frames);
		for (size_t i = 0; i < samples; i++)
			output[i] = std::clamp(buffer[i], -1.0f, 1.0f);
	}

	uint64_t Mixer::GetPosition(const Voice& voice) const
	{
		if (!voice.Stream)
			return uint64_t(std::max<int64_t>(voice.Cursor, 0));
		const uint64_t cursor = uint64_t(voice.Cursor);
		uint64_t offset = cursor > voice.SegmentRing ? cursor - voice.SegmentRing : 0;
		if (voice.SegmentLoop > 0)
			offset %= voice.SegmentLoop;
		return voice.SegmentSource + offset;
	}

	void Mixer::PublishStatus()
	{
		for (uint32_t index : m_ActiveVoices)
		{
			const Voice& voice = m_Voices[index];
			if (!voice.Started)
				continue;
			m_Status[index].Position.store(GetPosition(voice), std::memory_order_relaxed);
			m_Status[index].Audibility.store(voice.Audibility, std::memory_order_relaxed);
		}
	}
}
