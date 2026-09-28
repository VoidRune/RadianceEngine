#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

namespace Timbre
{
	namespace Internal
	{
		class EngineImpl;
		class Mixer;
	}

	// Base class of all effects. An effect is attached to one bus or voice at a time and always
	// processes interleaved stereo at the engine's sample rate.
	//
	// To write a custom effect, derive from Effect and override Process(). Keep parameters in
	// std::atomic members so setters can be called from any thread while the audio thread runs.
	class Effect
	{
	public:
		virtual ~Effect() = default;
		Effect(const Effect&) = delete;
		Effect& operator=(const Effect&) = delete;

		void SetEnabled(bool enabled) { m_Enabled.store(enabled, std::memory_order_relaxed); }
		bool IsEnabled() const { return m_Enabled.load(std::memory_order_relaxed); }

	protected:
		Effect() = default;

		// Called on the attaching thread before the audio thread uses the effect. Allocate buffers
		// and reset state here.
		virtual void Initialize(uint32_t sampleRate) { (void)sampleRate; }

		// Called on the audio thread. Must be real-time safe: no locks, allocations or file I/O.
		virtual void Process(float* samples, uint32_t frames) = 0;

	private:
		std::atomic<bool> m_Enabled = true;
		std::atomic<bool> m_Attached = false;

		friend class Internal::EngineImpl;
		friend class Internal::Mixer;
	};

	enum class FilterType : uint8_t
	{
		LowPass,
		HighPass,
		BandPass,
		Notch,
		Peak,
		LowShelf,
		HighShelf,
	};

	// Second order filter, the building block for EQs and low pass "muffling".
	class BiquadFilter : public Effect
	{
	public:
		explicit BiquadFilter(FilterType type = FilterType::LowPass, float frequency = 1000.0f, float q = 0.7071f, float gainDb = 0.0f);

		void SetType(FilterType type) { m_Type.store(type, std::memory_order_relaxed); }
		void SetFrequency(float frequency) { m_Frequency.store(frequency, std::memory_order_relaxed); }
		void SetQ(float q) { m_Q.store(q, std::memory_order_relaxed); }
		// Boost or cut for the Peak, LowShelf and HighShelf types.
		void SetGain(float decibels) { m_Gain.store(decibels, std::memory_order_relaxed); }

		FilterType GetType() const { return m_Type.load(std::memory_order_relaxed); }
		float GetFrequency() const { return m_Frequency.load(std::memory_order_relaxed); }
		float GetQ() const { return m_Q.load(std::memory_order_relaxed); }
		float GetGain() const { return m_Gain.load(std::memory_order_relaxed); }

	protected:
		void Initialize(uint32_t sampleRate) override;
		void Process(float* samples, uint32_t frames) override;

	private:
		void UpdateCoefficients();

		std::atomic<FilterType> m_Type;
		std::atomic<float> m_Frequency;
		std::atomic<float> m_Q;
		std::atomic<float> m_Gain;

		float m_SampleRate = 48000.0f;
		FilterType m_ActiveType = FilterType::LowPass;
		float m_ActiveFrequency = -1.0f;
		float m_ActiveQ = -1.0f;
		float m_ActiveGain = 0.0f;
		float m_Coefficients[5] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		float m_State[2][2] = {};
	};

	class LowPassFilter : public BiquadFilter
	{
	public:
		explicit LowPassFilter(float cutoff = 1000.0f, float q = 0.7071f) : BiquadFilter(FilterType::LowPass, cutoff, q) {}
	};

	class HighPassFilter : public BiquadFilter
	{
	public:
		explicit HighPassFilter(float cutoff = 200.0f, float q = 0.7071f) : BiquadFilter(FilterType::HighPass, cutoff, q) {}
	};

	class BandPassFilter : public BiquadFilter
	{
	public:
		explicit BandPassFilter(float center = 1000.0f, float q = 1.0f) : BiquadFilter(FilterType::BandPass, center, q) {}
	};

	// Echo with feedback.
	class Delay : public Effect
	{
	public:
		explicit Delay(float delaySeconds = 0.25f, float feedback = 0.35f, float wet = 0.3f, float maxDelaySeconds = 2.0f);

		void SetDelay(float seconds) { m_Delay.store(seconds, std::memory_order_relaxed); }
		void SetFeedback(float feedback) { m_Feedback.store(feedback, std::memory_order_relaxed); }
		void SetWet(float wet) { m_Wet.store(wet, std::memory_order_relaxed); }
		void SetDry(float dry) { m_Dry.store(dry, std::memory_order_relaxed); }

		float GetDelay() const { return m_Delay.load(std::memory_order_relaxed); }
		float GetFeedback() const { return m_Feedback.load(std::memory_order_relaxed); }
		float GetWet() const { return m_Wet.load(std::memory_order_relaxed); }
		float GetDry() const { return m_Dry.load(std::memory_order_relaxed); }

	protected:
		void Initialize(uint32_t sampleRate) override;
		void Process(float* samples, uint32_t frames) override;

	private:
		std::atomic<float> m_Delay;
		std::atomic<float> m_Feedback;
		std::atomic<float> m_Wet;
		std::atomic<float> m_Dry = 1.0f;

		float m_MaxDelay;
		float m_SampleRate = 48000.0f;
		std::vector<float> m_Buffer;
		uint32_t m_Length = 0;
		uint32_t m_Position = 0;
		float m_CurrentDelay = -1.0f;
	};

	// Room reverb. Put it on a bus to give every sound on it a shared space.
	class Reverb : public Effect
	{
	public:
		explicit Reverb(float roomSize = 0.7f, float damping = 0.5f, float wet = 0.25f, float width = 1.0f);

		void SetRoomSize(float roomSize) { m_RoomSize.store(roomSize, std::memory_order_relaxed); }
		void SetDamping(float damping) { m_Damping.store(damping, std::memory_order_relaxed); }
		void SetWet(float wet) { m_Wet.store(wet, std::memory_order_relaxed); }
		void SetDry(float dry) { m_Dry.store(dry, std::memory_order_relaxed); }
		void SetWidth(float width) { m_Width.store(width, std::memory_order_relaxed); }
		void SetPreDelay(float seconds) { m_PreDelay.store(seconds, std::memory_order_relaxed); }
		void SetDecayTime(float seconds) { m_DecayTime.store(seconds, std::memory_order_relaxed); }

		float GetRoomSize() const { return m_RoomSize.load(std::memory_order_relaxed); }
		float GetDamping() const { return m_Damping.load(std::memory_order_relaxed); }
		float GetWet() const { return m_Wet.load(std::memory_order_relaxed); }
		float GetDry() const { return m_Dry.load(std::memory_order_relaxed); }
		float GetWidth() const { return m_Width.load(std::memory_order_relaxed); }
		float GetPreDelay() const { return m_PreDelay.load(std::memory_order_relaxed); }
		float GetDecayTime() const { return m_DecayTime.load(std::memory_order_relaxed); }

	protected:
		void Initialize(uint32_t sampleRate) override;
		void Process(float* samples, uint32_t frames) override;

	private:
		struct Line
		{
			std::vector<float> Buffer;
			uint32_t Index = 0;
			float Length = 0.0f;
			float Delay = 0.0f;
			float Damping = 0.0f;
		};

		struct Diffuser
		{
			std::vector<float> Buffer;
			uint32_t Index = 0;
			float Gain = 0.0f;
		};

		static constexpr uint32_t LineCount = 8;
		static constexpr uint32_t DiffuserCount = 4;

		std::atomic<float> m_RoomSize;
		std::atomic<float> m_Damping;
		std::atomic<float> m_Wet;
		std::atomic<float> m_Dry = 1.0f;
		std::atomic<float> m_Width;
		std::atomic<float> m_PreDelay = 0.02f;
		std::atomic<float> m_DecayTime = 0.0f;

		float m_SampleRate = 48000.0f;
		std::vector<float> m_PreDelayBuffer;
		uint32_t m_PreDelayIndex = 0;
		Diffuser m_Diffusers[DiffuserCount];
		Line m_Lines[LineCount];
		float m_ModulationDepth = 0.0f;
		double m_ModulationTime = 0.0;
	};

	// Evens out loudness: reduces the level of everything above the threshold by the ratio.
	class Compressor : public Effect
	{
	public:
		explicit Compressor(float thresholdDb = -18.0f, float ratio = 4.0f, float attackMs = 5.0f, float releaseMs = 120.0f, float makeupDb = 0.0f);

		void SetThreshold(float decibels) { m_Threshold.store(decibels, std::memory_order_relaxed); }
		void SetRatio(float ratio) { m_Ratio.store(ratio, std::memory_order_relaxed); }
		void SetAttack(float milliseconds) { m_Attack.store(milliseconds, std::memory_order_relaxed); }
		void SetRelease(float milliseconds) { m_Release.store(milliseconds, std::memory_order_relaxed); }
		void SetMakeupGain(float decibels) { m_Makeup.store(decibels, std::memory_order_relaxed); }
		void SetKnee(float decibels) { m_Knee.store(decibels, std::memory_order_relaxed); }

		float GetThreshold() const { return m_Threshold.load(std::memory_order_relaxed); }
		float GetRatio() const { return m_Ratio.load(std::memory_order_relaxed); }
		float GetAttack() const { return m_Attack.load(std::memory_order_relaxed); }
		float GetRelease() const { return m_Release.load(std::memory_order_relaxed); }
		float GetMakeupGain() const { return m_Makeup.load(std::memory_order_relaxed); }
		float GetKnee() const { return m_Knee.load(std::memory_order_relaxed); }
		// Current gain reduction in decibels (0 or positive), for meters.
		float GetGainReduction() const { return m_GainReduction.load(std::memory_order_relaxed); }

	protected:
		void Initialize(uint32_t sampleRate) override;
		void Process(float* samples, uint32_t frames) override;

	private:
		std::atomic<float> m_Threshold;
		std::atomic<float> m_Ratio;
		std::atomic<float> m_Attack;
		std::atomic<float> m_Release;
		std::atomic<float> m_Makeup;
		std::atomic<float> m_Knee = 6.0f;
		std::atomic<float> m_GainReduction = 0.0f;

		float m_SampleRate = 48000.0f;
		float m_Envelope = 0.0f;
	};

	// Look-ahead peak limiter. Keeps the signal below the ceiling without audible clipping.
	class Limiter : public Effect
	{
	public:
		explicit Limiter(float ceilingDb = -0.3f, float releaseMs = 80.0f);

		void SetCeiling(float decibels) { m_Ceiling.store(decibels, std::memory_order_relaxed); }
		void SetRelease(float milliseconds) { m_Release.store(milliseconds, std::memory_order_relaxed); }

		float GetCeiling() const { return m_Ceiling.load(std::memory_order_relaxed); }
		float GetRelease() const { return m_Release.load(std::memory_order_relaxed); }
		// Current gain reduction in decibels (0 or positive), for meters.
		float GetGainReduction() const { return m_GainReduction.load(std::memory_order_relaxed); }

	protected:
		void Initialize(uint32_t sampleRate) override;
		void Process(float* samples, uint32_t frames) override;

	private:
		std::atomic<float> m_Ceiling;
		std::atomic<float> m_Release;
		std::atomic<float> m_GainReduction = 0.0f;

		float m_SampleRate = 48000.0f;
		uint32_t m_Lookahead = 1;
		uint64_t m_Time = 0;
		std::vector<float> m_Delay;
		std::vector<float> m_Required;
		std::vector<float> m_Window;
		std::vector<uint64_t> m_Queue;
		uint32_t m_QueueHead = 0;
		uint32_t m_QueueSize = 0;
		double m_WindowSum = 0.0;
		float m_Gain = 1.0f;
	};

	// Soft clipping distortion, from warm saturation to a broken radio.
	class Distortion : public Effect
	{
	public:
		explicit Distortion(float drive = 0.5f, float mix = 1.0f, float outputDb = 0.0f);

		// 0 is clean, 1 is heavily driven.
		void SetDrive(float drive) { m_Drive.store(drive, std::memory_order_relaxed); }
		void SetMix(float mix) { m_Mix.store(mix, std::memory_order_relaxed); }
		void SetOutputGain(float decibels) { m_Output.store(decibels, std::memory_order_relaxed); }

		float GetDrive() const { return m_Drive.load(std::memory_order_relaxed); }
		float GetMix() const { return m_Mix.load(std::memory_order_relaxed); }
		float GetOutputGain() const { return m_Output.load(std::memory_order_relaxed); }

	protected:
		void Process(float* samples, uint32_t frames) override;

	private:
		std::atomic<float> m_Drive;
		std::atomic<float> m_Mix;
		std::atomic<float> m_Output;
	};
}
