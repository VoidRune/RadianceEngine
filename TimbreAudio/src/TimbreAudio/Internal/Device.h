#pragma once
#include "TimbreAudio/AudioEngine.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Timbre::Internal
{
	// The output device. Calls the render callback from the audio thread with interleaved stereo float frames.
	class Device
	{
	public:
		using RenderCallback = void (*)(void* userData, float* output, uint32_t frames);

		Device(RenderCallback callback, void* userData);
		~Device();
		Device(const Device&) = delete;
		Device& operator=(const Device&) = delete;

		// Opens and starts the device. Falls back to the system default, and then to a silent device
		// so the game keeps running on machines without audio. A sample rate of 0 uses the device's.
		bool Open(const std::string& name, uint32_t sampleRate, uint32_t bufferSizeMs);
		void Close();

		uint32_t GetSampleRate() const;
		std::string GetName() const;
		std::vector<AudioDeviceInfo> GetDevices();

		// Returns true once after the device reported that it stopped unexpectedly.
		bool ConsumeStopped() { return m_Stopped.exchange(false, std::memory_order_relaxed); }

	private:
		struct State;

		bool OpenOn(bool nullBackend, const std::string& name, uint32_t sampleRate, uint32_t bufferSizeMs);

		RenderCallback m_Callback;
		void* m_UserData;
		std::unique_ptr<State> m_State;
		std::atomic<bool> m_Stopped = false;
	};
}
