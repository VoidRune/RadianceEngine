#include "PresentQueue.h"
#include "Device.h"
#include <RadianceEngine/Core/Log.h>
#include "VulkanInternal/VulkanUtilities.h"
#include <vulkan/vk_enum_string_helper.h>

namespace Rdn
{
	static const char* PresentModeName(PresentMode mode)
	{
		switch (mode)
		{
		case PresentMode::Immediate:   return "Immediate";
		case PresentMode::Mailbox:     return "Mailbox";
		case PresentMode::Fifo:        return "Fifo";
		case PresentMode::FifoRelaxed: return "FifoRelaxed";
		}
		return "?";
	}

	PresentQueue::PresentQueue(Device* device, PresentMode presentMode)
		: m_Device(device)
		, m_RequestedPresentMode(presentMode)
		, m_PresentMode(presentMode)
	{
		if (!device)
		{
			RDN_LOG_FATAL("Failed to create PresentQueue object: Device pointer is not valid!");
		}

		m_LogicalDevice = device->GetLogicalDevice();
		QueueContext& graphics = device->GetGraphicsQueueContext();
		QueueContext& present = device->GetPresentQueueContext();
		m_GraphicsQueue = graphics.Handle;
		m_PresentQueue = present.Handle;
		m_PresentQueueMutex = (present.Handle == graphics.Handle) ? &graphics.Mutex : &present.Mutex;

		m_Frames.resize(device->GetFramesInFlightCount());
		for (PerFrame& frame : m_Frames)
		{
			VkCommandPoolCreateInfo poolCI{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			poolCI.queueFamilyIndex = graphics.FamilyIndex;

			VkCommandPool pool;
			VK_CHECK(vkCreateCommandPool(toVk(m_LogicalDevice), &poolCI, nullptr, &pool));
			frame.CmdPool = fromVk(pool);

			VkCommandBufferAllocateInfo allocCI{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			allocCI.commandPool = pool;
			allocCI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocCI.commandBufferCount = 1;

			VkCommandBuffer cmd;
			VK_CHECK(vkAllocateCommandBuffers(toVk(m_LogicalDevice), &allocCI, &cmd));
			frame.Cmd = fromVk(cmd);

			SemaphoreCreateInfo semaphoreInfo = { .logicalDevice = m_LogicalDevice };
			frame.AcquireSemaphore = CreateSemaphoreHandle(semaphoreInfo);

			FenceCreateInfo fenceInfo = { .logicalDevice = m_LogicalDevice, .createSignaled = true };
			frame.InFlightFence = CreateFenceHandle(fenceInfo);
		}

		if (!CreateSwapchain())
			m_NeedsRecreate = true;
	}

	PresentQueue::~PresentQueue()
	{
		VkDevice device = toVk(m_LogicalDevice);
		if (device == VK_NULL_HANDLE) return;

		vkDeviceWaitIdle(device);

		DestroySwapchainResources();
		if (m_Swapchain)
			vkDestroySwapchainKHR(device, toVk(m_Swapchain), nullptr);

		for (PerFrame& frame : m_Frames)
		{
			vkDestroyCommandPool(device, toVk(frame.CmdPool), nullptr);
			vkDestroySemaphore(device, toVk(frame.AcquireSemaphore), nullptr);
			vkDestroyFence(device, toVk(frame.InFlightFence), nullptr);
		}
	}

	bool PresentQueue::CreateSwapchain()
	{
		SwapchainCreateInfo swapchainCreateInfo =
		{
			.instance = m_Device->GetInstance(),
			.physicalDevice = m_Device->GetPhysicalDevice(),
			.logicalDevice = m_LogicalDevice,
			.surface = m_Device->GetSurface(),
			.graphicsFamilyIndex = m_Device->GetGraphicsQueueContext().FamilyIndex,
			.presentFamilyIndex = m_Device->GetPresentQueueContext().FamilyIndex,
			.presentMode = m_RequestedPresentMode,
			.oldSwapchain = m_Swapchain,
		};
		SwapchainOutput swapchainOutput = CreateSwapchainHandle(swapchainCreateInfo);
		if (!swapchainOutput.swapchain)
			return false;

		DestroySwapchainResources();
		if (m_Swapchain)
			vkDestroySwapchainKHR(toVk(m_LogicalDevice), toVk(m_Swapchain), nullptr);

		m_Swapchain = swapchainOutput.swapchain;
		m_SurfaceFormat = swapchainOutput.surfaceFormat;
		m_PresentMode = swapchainOutput.presentMode;
		m_Extent = { swapchainOutput.extent[0], swapchainOutput.extent[1] };

		SwapchainImagesRetreiveInfo swapchainImageRetreiveInfo =
		{
			.logicalDevice = m_LogicalDevice,
			.swapchain = m_Swapchain,
		};
		m_Images = RetreiveSwapchainImages(swapchainImageRetreiveInfo);

		for (ImageHandle image : m_Images)
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = toVk(image);
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = toVk(m_SurfaceFormat);
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VkImageView imageView;
			VK_CHECK(vkCreateImageView(toVk(m_LogicalDevice), &createInfo, nullptr, &imageView));
			m_ImageViews.push_back(fromVk(imageView));

			SemaphoreCreateInfo semaphoreInfo = { .logicalDevice = m_LogicalDevice };
			m_PresentSemaphores.push_back(CreateSemaphoreHandle(semaphoreInfo));
		}

		RDN_LOG("Swapchain: {}x{}, {} images, {}, {}", m_Extent.Width, m_Extent.Height, m_Images.size(),
			string_VkFormat(toVk(m_SurfaceFormat)), PresentModeName(m_PresentMode));
		return true;
	}

	void PresentQueue::DestroySwapchainResources()
	{
		VkDevice device = toVk(m_LogicalDevice);
		for (ImageViewHandle view : m_ImageViews)
			vkDestroyImageView(device, toVk(view), nullptr);
		for (SemaphoreHandle semaphore : m_PresentSemaphores)
			vkDestroySemaphore(device, toVk(semaphore), nullptr);
		m_ImageViews.clear();
		m_PresentSemaphores.clear();
		m_Images.clear();
	}

	bool PresentQueue::Recreate()
	{
		if (m_FrameInProgress)
		{
			RDN_LOG_ERROR("PresentQueue::Recreate() called between BeginFrame() and Submit()");
			return false;
		}

		m_Device->WaitIdle();
		if (!CreateSwapchain())
			return false;

		m_NeedsRecreate = false;
		return true;
	}

	void PresentQueue::SetPresentMode(PresentMode presentMode)
	{
		if (presentMode == m_RequestedPresentMode)
			return;
		m_RequestedPresentMode = presentMode;
		m_NeedsRecreate = true;
	}

	std::optional<FrameContext> PresentQueue::BeginFrame()
	{
		if (m_FrameInProgress)
		{
			RDN_LOG_ERROR("PresentQueue::BeginFrame() called twice without Submit()");
			return std::nullopt;
		}
		if (!m_Swapchain)
			return std::nullopt;

		PerFrame& frame = m_Frames[m_CurrentFrame];

		// The slot's previous submission must finish before its command pool, fence and acquire semaphore are reused
		VkFence inFlightFence = toVk(frame.InFlightFence);
		VK_CHECK(vkWaitForFences(toVk(m_LogicalDevice), 1, &inFlightFence, VK_TRUE, UINT64_MAX));

		uint32_t imageIndex = 0;
		VkResult result = vkAcquireNextImageKHR(
			toVk(m_LogicalDevice),
			toVk(m_Swapchain),
			UINT64_MAX,
			toVk(frame.AcquireSemaphore),
			VK_NULL_HANDLE,
			&imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_NeedsRecreate = true;
			return std::nullopt;
		}
		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			RDN_LOG_ERROR("PresentQueue::BeginFrame: vkAcquireNextImageKHR returned {}", GetVulkanResultString(result));
			return std::nullopt;
		}
		if (result == VK_SUBOPTIMAL_KHR)
			m_NeedsRecreate = true;

		VK_CHECK(vkResetFences(toVk(m_LogicalDevice), 1, &inFlightFence));
		VK_CHECK(vkResetCommandPool(toVk(m_LogicalDevice), toVk(frame.CmdPool), 0));

		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(toVk(frame.Cmd), &beginInfo));

		m_ImageIndex = imageIndex;
		m_FrameInProgress = true;

		return FrameContext{
			.Cmd = CommandBuffer(frame.Cmd),
			.FrameIndex = m_CurrentFrame,
			.ImageIndex = imageIndex,
			.PresentImage = m_Images[imageIndex],
			.PresentImageView = m_ImageViews[imageIndex],
			.PresentExtent = m_Extent,
			.PresentFormat = m_SurfaceFormat,
		};
	}

	void PresentQueue::Submit()
	{
		if (!m_FrameInProgress)
		{
			RDN_LOG_ERROR("PresentQueue::Submit() called without a frame from BeginFrame()");
			return;
		}
		m_FrameInProgress = false;

		PerFrame& frame = m_Frames[m_CurrentFrame];
		VK_CHECK(vkEndCommandBuffer(toVk(frame.Cmd)));

		VkSemaphore presentSemaphore = toVk(m_PresentSemaphores[m_ImageIndex]);

		VkSemaphoreSubmitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		waitInfo.semaphore = toVk(frame.AcquireSemaphore);
		waitInfo.stageMask = toVk(AcquireWaitStage);

		VkSemaphoreSubmitInfo signalInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		signalInfo.semaphore = presentSemaphore;
		signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

		VkCommandBufferSubmitInfo cmdInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
		cmdInfo.commandBuffer = toVk(frame.Cmd);

		VkSubmitInfo2 submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = 1,
			.pWaitSemaphoreInfos = &waitInfo,
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &cmdInfo,
			.signalSemaphoreInfoCount = 1,
			.pSignalSemaphoreInfos = &signalInfo,
		};

		{
			std::lock_guard lock(m_Device->GetGraphicsQueueContext().Mutex);
			VK_CHECK(vkQueueSubmit2(toVk(m_GraphicsQueue), 1, &submitInfo, toVk(frame.InFlightFence)));
		}

		VkSwapchainKHR swapchain = toVk(m_Swapchain);
		VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &presentSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.pImageIndices = &m_ImageIndex;

		VkResult presentResult;
		{
			std::lock_guard lock(*m_PresentQueueMutex);
			presentResult = vkQueuePresentKHR(toVk(m_PresentQueue), &presentInfo);
		}

		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
		{
			m_NeedsRecreate = true;
		}
		else
		{
			VK_CHECK(presentResult);
		}

		m_CurrentFrame = (m_CurrentFrame + 1) % uint32_t(m_Frames.size());
	}
}
