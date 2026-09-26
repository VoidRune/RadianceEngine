#include "Device.h"

#include "VulkanInternal/VulkanUtilities.h"
#include <RadianceEngine/Core/Log.h>
#include <algorithm>
#include <format>

namespace Rdn
{
	namespace
	{
		DeviceProperties QueryDeviceProperties(VkPhysicalDevice physicalDevice)
		{
			VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
			VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
			rayTracingProperties.pNext = &accelerationStructureProperties;
			VkPhysicalDeviceDriverProperties driverProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
			driverProperties.pNext = &rayTracingProperties;
			VkPhysicalDeviceProperties2 properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			properties.pNext = &driverProperties;
			vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

			VkPhysicalDeviceMemoryProperties memoryProperties;
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

			DeviceProperties result;
			result.Name = properties.properties.deviceName;
			result.Driver = std::format("{} {}", driverProperties.driverName, driverProperties.driverInfo);
			result.ApiVersion = properties.properties.apiVersion;
			for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; i++)
			{
				if (memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
					result.DeviceLocalMemory = std::max(result.DeviceLocalMemory, uint64_t(memoryProperties.memoryHeaps[i].size));
			}
			result.MinUniformBufferOffsetAlignment = properties.properties.limits.minUniformBufferOffsetAlignment;
			result.MinStorageBufferOffsetAlignment = properties.properties.limits.minStorageBufferOffsetAlignment;
			result.MinAccelerationStructureScratchOffsetAlignment = accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment;
			result.ShaderGroupHandleSize = rayTracingProperties.shaderGroupHandleSize;
			result.ShaderGroupHandleAlignment = rayTracingProperties.shaderGroupHandleAlignment;
			result.ShaderGroupBaseAlignment = rayTracingProperties.shaderGroupBaseAlignment;
			return result;
		}
	}

	Device::Device(const DeviceConfig& config)
	{
		InstanceCreateInfo instanceCreateInfo = {
			.applicationName = "Application",
			.engineName = "Radiance Renderer",
			.apiVersion = ApiVersion(1, 4),
			.instanceExtensions = config.InstanceExtensions,
			.enableValidationLayers = config.EnableValidation
		};
		InstanceOutput instance = CreateInstanceHandle(instanceCreateInfo);
		m_Instance = instance.instance;
		m_DebugUtilsEnabled = instance.debugUtilsEnabled;

		DebugUtilsMessengerCreateInfo debugUtilsMessengerInfo = {
			.instance = m_Instance,
			.enableDebugUtilsMessenger = m_DebugUtilsEnabled
		};
		m_DebugUtilsMessenger = CreateDebugUtilsMessengerHandle(debugUtilsMessengerInfo);

		SurfaceCreateInfo surfaceCreateInfo = {
			.instance = m_Instance,
			.windowHandle = config.WindowHandle
		};
		m_Surface = CreateSurfaceHandle(surfaceCreateInfo);

		PhysicalDeviceSelectInfo physicalDeviceSelectInfo = {
			.instance = m_Instance,
			.surface = m_Surface,
		};
		PhysicalDeviceSelection selection = SelectPhysicalDevice(physicalDeviceSelectInfo);
		m_PhysicalDevice = selection.physicalDevice;
		m_Properties = QueryDeviceProperties(toVk(m_PhysicalDevice));

		m_FramesInFlight = std::max(config.FramesInFlight, 1u);

		DeviceCreateInfo deviceCreateInfo = {
			.physicalDevice = m_PhysicalDevice,
			.queueFamilyIndices = selection.queueFamilyIndices
		};
		DeviceOutput device = CreateLogicalDeviceHandle(deviceCreateInfo);
		m_LogicalDevice = device.device;
		m_EnabledExtensions.assign(device.enabledExtensions.begin(), device.enabledExtensions.end());

		VkQueue graphicsQueue;
		vkGetDeviceQueue(toVk(m_LogicalDevice), selection.queueFamilyIndices.GraphicsIndex, 0, &graphicsQueue);
		m_GraphicsQueue.Handle = fromVk(graphicsQueue);
		m_GraphicsQueue.FamilyIndex = selection.queueFamilyIndices.GraphicsIndex;

		if (selection.queueFamilyIndices.PresentIndex != selection.queueFamilyIndices.GraphicsIndex)
		{
			VkQueue presentQueue;
			vkGetDeviceQueue(toVk(m_LogicalDevice), selection.queueFamilyIndices.PresentIndex, 0, &presentQueue);
			m_SeparatePresentQueue.Handle = fromVk(presentQueue);
			m_SeparatePresentQueue.FamilyIndex = selection.queueFamilyIndices.PresentIndex;
			m_PresentQueue = &m_SeparatePresentQueue;
		}

		VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = m_GraphicsQueue.FamilyIndex;
		VkCommandPool commandPool;
		VK_CHECK(vkCreateCommandPool(toVk(m_LogicalDevice), &poolInfo, nullptr, &commandPool));
		m_ImmediateCmdPool = fromVk(commandPool);

		VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer;
		VK_CHECK(vkAllocateCommandBuffers(toVk(m_LogicalDevice), &allocInfo, &commandBuffer));
		m_ImmediateCmd = fromVk(commandBuffer);

		FenceCreateInfo fenceInfo = { .logicalDevice = m_LogicalDevice };
		m_ImmediateFence = CreateFenceHandle(fenceInfo);

		RDN_LOG("GPU: {} ({}, Vulkan {}.{}.{}, {} MiB device local{})", m_Properties.Name, m_Properties.Driver,
			VK_API_VERSION_MAJOR(m_Properties.ApiVersion), VK_API_VERSION_MINOR(m_Properties.ApiVersion), VK_API_VERSION_PATCH(m_Properties.ApiVersion),
			m_Properties.DeviceLocalMemory >> 20, instance.validationEnabled ? ", validation on" : "");
	}

	Device::~Device()
	{
		WaitIdle();
		vkDestroyFence(toVk(m_LogicalDevice), toVk(m_ImmediateFence), nullptr);
		vkDestroyCommandPool(toVk(m_LogicalDevice), toVk(m_ImmediateCmdPool), nullptr);
		vkDestroyDevice(toVk(m_LogicalDevice), nullptr);
		vkDestroySurfaceKHR(toVk(m_Instance), toVk(m_Surface), nullptr);
		if (m_DebugUtilsMessenger)
		{
			vkDestroyDebugUtilsMessengerEXT(toVk(m_Instance), toVk(m_DebugUtilsMessenger), nullptr);
		}
		vkDestroyInstance(toVk(m_Instance), nullptr);
	}

	void Device::WaitIdle()
	{
		if (m_PresentQueue != &m_GraphicsQueue)
		{
			std::scoped_lock lock(m_GraphicsQueue.Mutex, m_PresentQueue->Mutex);
			VK_CHECK(vkDeviceWaitIdle(toVk(m_LogicalDevice)));
		}
		else
		{
			std::scoped_lock lock(m_GraphicsQueue.Mutex);
			VK_CHECK(vkDeviceWaitIdle(toVk(m_LogicalDevice)));
		}
	}

	bool Device::IsExtensionEnabled(std::string_view extension) const
	{
		return std::ranges::find(m_EnabledExtensions, extension) != m_EnabledExtensions.end();
	}

	void Device::ImmediateSubmit(std::function<void(CommandBuffer&)> fn)
	{
		std::scoped_lock immediateLock(m_ImmediateMutex);

		const VkDevice device = toVk(m_LogicalDevice);
		const VkCommandBuffer vkCmd = toVk(m_ImmediateCmd);
		const VkFence fence = toVk(m_ImmediateFence);

		VK_CHECK(vkResetCommandBuffer(vkCmd, 0));
		CommandBuffer cmd(m_ImmediateCmd);
		cmd.Begin();
		fn(cmd);
		cmd.End();

		VkCommandBufferSubmitInfo cmdInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
		cmdInfo.commandBuffer = vkCmd;

		VkSubmitInfo2 submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submitInfo.commandBufferInfoCount = 1;
		submitInfo.pCommandBufferInfos = &cmdInfo;

		VK_CHECK(vkResetFences(device, 1, &fence));
		{
			std::scoped_lock queueLock(m_GraphicsQueue.Mutex);
			VK_CHECK(vkQueueSubmit2(toVk(m_GraphicsQueue.Handle), 1, &submitInfo, fence));
		}
		VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
	}
}
