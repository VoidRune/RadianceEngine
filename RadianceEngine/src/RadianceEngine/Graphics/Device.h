#pragma once
#include <vector>
#include "Handle.h"
#include "CommandBuffer.h"
#include <functional>
#include <mutex>

namespace Rdn
{
	struct DeviceConfig 
	{
	public:
		void* WindowHandle;
		std::vector<const char*> InstanceExtensions;
		uint32_t FramesInFlight = 2;
		bool EnableValidation = true;
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
		QueueContext& GetPresentQueueContext() { return m_PresentQueue; }
		uint32_t GetFramesInFlightCount() { return m_FramesInFlight; }
		bool IsDebugUtilsEnabled() const { return m_DebugUtilsEnabled; }


		void ImmediateSubmit(std::function<void(CommandBuffer&)> fn);

	private:

		InstanceHandle m_Instance;
		DebugUtilsMessengerHandle m_DebugUtilsMessenger;
		PhysicalDeviceHandle m_PhysicalDevice;
		SurfaceHandle m_Surface;
		DeviceHandle m_LogicalDevice;
		
		uint32_t m_FramesInFlight;
		bool m_DebugUtilsEnabled = false;

		QueueContext m_GraphicsQueue;
		QueueContext m_PresentQueue;

		std::mutex  m_GraphicsQueueMutex;
		CommandPoolHandle m_ImmediateCmdPool;
	};
}