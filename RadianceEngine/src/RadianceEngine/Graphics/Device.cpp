#include "Device.h"

#include "VulkanInternal/VulkanUtilities.h"
#include <algorithm>

namespace Rdn
{
	Device::Device(const DeviceConfig& config)
	{
		InstanceCreateInfo instanceCreateInfo = {
			.applicationName = "Application",
			.engineName = "Radiance Renderer",
			.apiVersion = ApiVersion(1, 4),
			.instanceExtensions = config.InstanceExtensions,
			.enableValidationLayers = config.EnableValidation
		};
		m_Instance = CreateInstanceHandle(instanceCreateInfo);
		m_DebugUtilsEnabled = config.EnableValidation; // VK_EXT_debug_utils is enabled together with validation

		DebugUtilsMessengerCreateInfo debugUtilsMessengerInfo = {
			.instance = m_Instance,
			.enableDebugUtilsMessenger = config.EnableValidation
		};
		m_DebugUtilsMessenger = CreateDebugUtilsMessengerHandle(debugUtilsMessengerInfo);

		PhysicalDeviceSelectInfo physicalDeviceSelectInfo = {
			.instance = m_Instance,
		};
		m_PhysicalDevice = SelectPhysicalDeviceHandle(physicalDeviceSelectInfo);

		SurfaceCreateInfo surfaceCreateInfo = {
			.instance = m_Instance,
			.windowHandle = config.WindowHandle
		};
		m_Surface = CreateSurfaceHandle(surfaceCreateInfo);

		// Independent of the swapchain image count: PresentQueue keeps per-image and per-frame state apart
		m_FramesInFlight = std::max(config.FramesInFlight, 1u);

		QueueFamilySelectInfo queueFamilySelectInfo = {
			.physicalDevice = m_PhysicalDevice,
			.surface = m_Surface
		};
		QueueFamilyIndices queueFamilyIndices = SelectQueueFamilies(queueFamilySelectInfo);
		m_GraphicsQueue.FamilyIndex = queueFamilyIndices.GraphicsIndex;
		m_PresentQueue.FamilyIndex = queueFamilyIndices.PresentIndex;

		DeviceCreateInfo deviceCreateInfo = {
			.physicalDevice = m_PhysicalDevice,
			.queueFamilyIndices = queueFamilyIndices
		};

		m_LogicalDevice = CreateLogicalDeviceHandle(deviceCreateInfo);

		VkQueue graphicsQueue;
		VkQueue presentQueue;
		vkGetDeviceQueue(toVk(m_LogicalDevice), m_GraphicsQueue.FamilyIndex, 0, &graphicsQueue);
		vkGetDeviceQueue(toVk(m_LogicalDevice), m_PresentQueue.FamilyIndex, 0, &presentQueue);
		m_GraphicsQueue.Handle = fromVk(graphicsQueue);
		m_PresentQueue.Handle = fromVk(presentQueue);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = m_GraphicsQueue.FamilyIndex;

		VkCommandPool commandPool;
		VK_CHECK(vkCreateCommandPool(toVk(m_LogicalDevice), &poolInfo, nullptr, &commandPool));
		m_ImmediateCmdPool = fromVk(commandPool);
	}

	Device::~Device()
	{
		WaitIdle();
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
		vkDeviceWaitIdle(toVk(m_LogicalDevice));
	}

	void Device::ImmediateSubmit(std::function<void(CommandBuffer&)> fn)
	{
		VkDevice dev = toVk(m_LogicalDevice);

		VkCommandBufferAllocateInfo allocCI{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocCI.commandPool = toVk(m_ImmediateCmdPool);
		allocCI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocCI.commandBufferCount = 1;

		VkCommandBuffer vkCmd;
		VK_CHECK(vkAllocateCommandBuffers(dev, &allocCI, &vkCmd));

		VkCommandBufferBeginInfo beginCI{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginCI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(vkCmd, &beginCI));
		CommandBuffer cmd(fromVk(vkCmd));
		fn(cmd);
		VK_CHECK(vkEndCommandBuffer(vkCmd));


		VkCommandBufferSubmitInfo cmdInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
		cmdInfo.commandBuffer = vkCmd;

		VkSubmitInfo2 submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submitInfo.commandBufferInfoCount = 1;
		submitInfo.pCommandBufferInfos = &cmdInfo;

		VkFence uploadFence;
		VkFenceCreateInfo uploadFenceCreateInfo{};
		uploadFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		uploadFenceCreateInfo.pNext = nullptr;
		uploadFenceCreateInfo.flags = 0;
		VK_CHECK(vkCreateFence(dev, &uploadFenceCreateInfo, nullptr, &uploadFence));

		{
			std::lock_guard<std::mutex> lock(m_GraphicsQueue.Mutex);
			VK_CHECK(vkQueueSubmit2(toVk(m_GraphicsQueue.Handle), 1, &submitInfo, uploadFence));
		}

		VK_CHECK(vkWaitForFences(dev, 1, &uploadFence, VK_TRUE, UINT64_MAX));
		vkDestroyFence(dev, uploadFence, nullptr);

		vkFreeCommandBuffers(dev, toVk(m_ImmediateCmdPool), 1, &vkCmd);
	}
}