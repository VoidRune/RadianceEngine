#include "Device.h"
#include "Log.h"
#include "Miniaudio.h"

namespace Timbre::Internal
{
	struct Device::State
	{
		Device* Owner = nullptr;
		ma_context Context{};
		ma_context NullContext{};
		ma_device Output{};
		bool ContextReady = false;
		bool NullContextReady = false;
		bool OutputReady = false;
		bool UsingNull = false;
		std::atomic<bool> Closing = false;

		static void DataCallback(ma_device* device, void* output, const void* input, ma_uint32 frames)
		{
			(void)input;
			const Device& owner = *static_cast<State*>(device->pUserData)->Owner;
			owner.m_Callback(owner.m_UserData, static_cast<float*>(output), frames);
		}

		static void NotificationCallback(const ma_device_notification* notification)
		{
			State& state = *static_cast<State*>(notification->pDevice->pUserData);
			if (notification->type == ma_device_notification_type_stopped && !state.Closing.load(std::memory_order_relaxed))
				state.Owner->m_Stopped.store(true, std::memory_order_relaxed);
		}
	};

	Device::Device(RenderCallback callback, void* userData)
		: m_Callback(callback), m_UserData(userData), m_State(std::make_unique<State>())
	{
		m_State->Owner = this;
		m_State->ContextReady = ma_context_init(nullptr, 0, nullptr, &m_State->Context) == MA_SUCCESS;
		if (!m_State->ContextReady)
			TIMBRE_LOG_WARNING("Failed to initialize the audio backend");
	}

	Device::~Device()
	{
		Close();
		if (m_State->ContextReady)
			ma_context_uninit(&m_State->Context);
		if (m_State->NullContextReady)
			ma_context_uninit(&m_State->NullContext);
	}

	bool Device::Open(const std::string& name, uint32_t sampleRate, uint32_t bufferSizeMs)
	{
		Close();
		if (OpenOn(false, name, sampleRate, bufferSizeMs))
			return true;
		if (!name.empty())
		{
			TIMBRE_LOG_WARNING("Failed to open the output device \"{}\", using the default device", name);
			if (OpenOn(false, {}, sampleRate, bufferSizeMs))
				return true;
		}
		TIMBRE_LOG_WARNING("No audio output device is available, audio will be silent");
		return OpenOn(true, {}, sampleRate, bufferSizeMs);
	}

	bool Device::OpenOn(bool nullBackend, const std::string& name, uint32_t sampleRate, uint32_t bufferSizeMs)
	{
		State& state = *m_State;
		if (nullBackend && !state.NullContextReady)
		{
			const ma_backend backend = ma_backend_null;
			state.NullContextReady = ma_context_init(&backend, 1, nullptr, &state.NullContext) == MA_SUCCESS;
		}
		if (nullBackend ? !state.NullContextReady : !state.ContextReady)
			return false;
		ma_context* context = nullBackend ? &state.NullContext : &state.Context;

		ma_device_id id{};
		bool hasId = false;
		if (!name.empty())
		{
			ma_device_info* devices = nullptr;
			ma_uint32 count = 0;
			if (ma_context_get_devices(context, &devices, &count, nullptr, nullptr) == MA_SUCCESS)
			{
				for (ma_uint32 i = 0; i < count && !hasId; i++)
				{
					if (name == devices[i].name)
					{
						id = devices[i].id;
						hasId = true;
					}
				}
			}
			if (!hasId)
				return false;
		}

		ma_device_config config = ma_device_config_init(ma_device_type_playback);
		config.playback.pDeviceID = hasId ? &id : nullptr;
		config.playback.format = ma_format_f32;
		config.playback.channels = 2;
		config.sampleRate = sampleRate;
		config.periodSizeInMilliseconds = bufferSizeMs;
		config.performanceProfile = ma_performance_profile_low_latency;
		config.noPreSilencedOutputBuffer = MA_TRUE;
		config.noClip = MA_TRUE;
		config.dataCallback = &State::DataCallback;
		config.notificationCallback = &State::NotificationCallback;
		config.pUserData = &state;

		if (ma_device_init(context, &config, &state.Output) != MA_SUCCESS)
			return false;
		if (ma_device_start(&state.Output) != MA_SUCCESS)
		{
			state.Closing.store(true, std::memory_order_relaxed);
			ma_device_uninit(&state.Output);
			state.Closing.store(false, std::memory_order_relaxed);
			return false;
		}
		state.OutputReady = true;
		state.UsingNull = nullBackend;
		return true;
	}

	void Device::Close()
	{
		State& state = *m_State;
		if (!state.OutputReady)
			return;
		state.Closing.store(true, std::memory_order_relaxed);
		ma_device_uninit(&state.Output);
		state.Closing.store(false, std::memory_order_relaxed);
		state.OutputReady = false;
	}

	uint32_t Device::GetSampleRate() const
	{
		return m_State->OutputReady ? m_State->Output.sampleRate : 0;
	}

	std::string Device::GetName() const
	{
		if (!m_State->OutputReady)
			return {};
		if (m_State->UsingNull)
			return "None";
		char name[MA_MAX_DEVICE_NAME_LENGTH + 1] = {};
		if (ma_device_get_name(&m_State->Output, ma_device_type_playback, name, sizeof(name), nullptr) != MA_SUCCESS)
			return {};
		return name;
	}

	std::vector<AudioDeviceInfo> Device::GetDevices()
	{
		std::vector<AudioDeviceInfo> result;
		if (!m_State->ContextReady)
			return result;
		ma_device_info* devices = nullptr;
		ma_uint32 count = 0;
		if (ma_context_get_devices(&m_State->Context, &devices, &count, nullptr, nullptr) != MA_SUCCESS)
			return result;
		for (ma_uint32 i = 0; i < count; i++)
			result.push_back({ devices[i].name, devices[i].isDefault != 0 });
		return result;
	}
}
