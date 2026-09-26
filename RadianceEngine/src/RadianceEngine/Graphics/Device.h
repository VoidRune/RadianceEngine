#pragma once
#include <vector>
#include "Handle.h"
#include "CommandBuffer.h"
#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace Rdn
{
	struct DeviceConfig
	{
	public:
		void* WindowHandle = nullptr;
		std::vector<const char*> InstanceExtensions;
		uint32_t FramesInFlight = 2;
		bool EnableValidation = true;
	};

	struct DeviceProperties
	{
		std::string Name;
		std::string Driver;
		uint32_t ApiVersion = 0;
		uint64_t DeviceLocalMemory = 0;
		uint64_t MinUniformBufferOffsetAlignment = 1;
		uint64_t MinStorageBufferOffsetAlignment = 1;
		uint64_t MinAccelerationStructureScratchOffsetAlignment = 1;
		uint32_t ShaderGroupHandleSize = 0;
		uint32_t ShaderGroupHandleAlignment = 1;
		uint32_t ShaderGroupBaseAlignment = 1;
	};

	class Device
	{
	public:
		Device(const DeviceConfig& config);
		~Device();
		Device(const Device&) = delete;
		Device& operator=(const Device&) = delete;

		void WaitIdle();

		InstanceHandle GetInstance() { return m_Instance; }
		PhysicalDeviceHandle GetPhysicalDevice() { return m_PhysicalDevice; }
		SurfaceHandle GetSurface() { return m_Surface; }
		DeviceHandle GetLogicalDevice() { return m_LogicalDevice; }

		QueueContext& GetGraphicsQueueContext() { return m_GraphicsQueue; }
		QueueContext& GetPresentQueueContext() { return *m_PresentQueue; }
		uint32_t GetFramesInFlightCount() { return m_FramesInFlight; }
		const DeviceProperties& GetProperties() const { return m_Properties; }
		bool IsDebugUtilsEnabled() const { return m_DebugUtilsEnabled; }
		bool IsExtensionEnabled(std::string_view extension) const;

		void ImmediateSubmit(std::function<void(CommandBuffer&)> fn);

	private:

		InstanceHandle m_Instance;
		DebugUtilsMessengerHandle m_DebugUtilsMessenger;
		PhysicalDeviceHandle m_PhysicalDevice;
		SurfaceHandle m_Surface;
		DeviceHandle m_LogicalDevice;

		DeviceProperties m_Properties;
		std::vector<std::string> m_EnabledExtensions;
		uint32_t m_FramesInFlight;
		bool m_DebugUtilsEnabled = false;

		QueueContext m_GraphicsQueue;
		QueueContext m_SeparatePresentQueue;
		QueueContext* m_PresentQueue = &m_GraphicsQueue;

		std::mutex m_ImmediateMutex;
		CommandPoolHandle m_ImmediateCmdPool;
		CommandBufferHandle m_ImmediateCmd;
		FenceHandle m_ImmediateFence;
	};
}
