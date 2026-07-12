#include "PresentQueue.h"
#include "Device.h"
#include <RadianceEngine/Core/Log.h>
#include "VulkanInternal/VulkanUtilities.h"

namespace Rdn
{
	PresentQueue::PresentQueue(Device* device, PresentMode presentMode)
	{
		if (!device)
		{
			RDN_LOG_FATAL("Failed to create PresentQueue object: Device pointer is not valid!");
		}

		m_Device = device;
		m_LogicalDevice = device->GetLogicalDevice();
		m_GraphicsQueue = device->GetGraphicsQueueContext().Handle;
		m_PresentQueue = device->GetPresentQueueContext().Handle;
		SwapchainCreateInfo swapchainCreateInfo =
		{
			.instance = device->GetInstance(),
			.physicalDevice = device->GetPhysicalDevice(),
			.logicalDevice = m_LogicalDevice,
			.surface = device->GetSurface(),
			.graphicsFamilyIndex = device->GetGraphicsQueueContext().FamilyIndex,
			.presentFamilyIndex = device->GetPresentQueueContext().FamilyIndex,
			.desiredImageCount = device->GetFramesInFlightCount(),
			.presentMode = presentMode,
		};
		SwapchainOutput swapchainOutput = CreateSwapchainHandle(swapchainCreateInfo);
		m_Swapchain = swapchainOutput.swapchain;
		m_ImageCount = swapchainOutput.imageCount;
		m_SurfaceFormat = swapchainOutput.surfaceFormat;
		m_SwapchainSize.Width = swapchainOutput.extent[0];
		m_SwapchainSize.Height = swapchainOutput.extent[1];
		m_SwapchainSize.Depth = swapchainOutput.extent[2];

		SwapchainImagesRetreiveInfo swapchainImageRetreiveInfo =
		{
			.logicalDevice = m_LogicalDevice,
			.swapchain = m_Swapchain,
		};
		m_SwapchainImages = RetreiveSwapchainImages(swapchainImageRetreiveInfo);

		m_SwapchainImageViews.resize(m_SwapchainImages.size());
		for (size_t i = 0; i < m_SwapchainImageViews.size(); i++) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = toVk(m_SwapchainImages[i]);
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = toVk(swapchainOutput.surfaceFormat);
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
			vkCreateImageView(toVk(m_LogicalDevice), &createInfo, nullptr, &imageView);
			m_SwapchainImageViews[i] = fromVk(imageView);
		}

		m_Frames.resize(m_ImageCount);
		for (uint32_t i = 0; i < m_ImageCount; i++)
		{
			PerFrame& frame = m_Frames[i];

			VkCommandPoolCreateInfo poolCI{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			poolCI.queueFamilyIndex = device->GetGraphicsQueueContext().FamilyIndex;
			poolCI.flags = 0;

			VkCommandPool pool;
			vkCreateCommandPool(toVk(m_LogicalDevice), &poolCI, nullptr, &pool);
			frame.CmdPool = fromVk(pool);

			VkCommandBufferAllocateInfo allocCI{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			allocCI.commandPool = pool;
			allocCI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocCI.commandBufferCount = 1;

			VkCommandBuffer cmd;
			vkAllocateCommandBuffers(toVk(m_LogicalDevice), &allocCI, &cmd);
			frame.Cmd = fromVk(cmd);

			VkSemaphoreCreateInfo semCI{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

			VkSemaphore imageAvailable, renderFinished;
			vkCreateSemaphore(toVk(m_LogicalDevice), &semCI, nullptr, &imageAvailable);
			vkCreateSemaphore(toVk(m_LogicalDevice), &semCI, nullptr, &renderFinished);
			frame.ImageAvailableSemaphore = fromVk(imageAvailable);
			frame.RenderFinishedSemaphore = fromVk(renderFinished);

			VkFenceCreateInfo fenceCI{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			VkFence fence;
			vkCreateFence(toVk(m_LogicalDevice), &fenceCI, nullptr, &fence);
			frame.InFlightFence = fromVk(fence);
		}

		m_OutOfDate = false;
		m_CurrentFrame = 0;
		m_PresentImageIndex = 0;
	}

	PresentQueue::~PresentQueue()
	{
		VkDevice device = toVk(m_LogicalDevice);
		if (device == VK_NULL_HANDLE) return;

		vkDeviceWaitIdle(device);

		for (PerFrame& frame : m_Frames)
		{
			if (frame.CmdPool) vkDestroyCommandPool(device, toVk(frame.CmdPool), nullptr);
			if (frame.ImageAvailableSemaphore) vkDestroySemaphore(device, toVk(frame.ImageAvailableSemaphore), nullptr);
			if (frame.RenderFinishedSemaphore) vkDestroySemaphore(device, toVk(frame.RenderFinishedSemaphore), nullptr);
			if (frame.InFlightFence) vkDestroyFence(device, toVk(frame.InFlightFence), nullptr);
		}
		m_Frames.clear();

		for (ImageViewHandle view : m_SwapchainImageViews)
		{
			if (view)
			{
				vkDestroyImageView(device, toVk(view), nullptr);
			}
		}
		m_SwapchainImageViews.clear();
		m_SwapchainImages.clear();

		if (m_Swapchain)
		{
			vkDestroySwapchainKHR(device, toVk(m_Swapchain), nullptr);
		}
	}

	FrameContext PresentQueue::BeginFrame()
	{
		PerFrame& frame = m_Frames[m_CurrentFrame];

		VkFence inFlightFence = toVk(frame.InFlightFence);
		VkResult waitResult = vkWaitForFences(toVk(m_LogicalDevice), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
		if (waitResult != VK_SUCCESS)
			RDN_LOG_ERROR("PresentQueue::BeginFrame: vkWaitForFences returned {0}", (int)waitResult);

		VkResult result = vkAcquireNextImageKHR(
			toVk(m_LogicalDevice), 
			toVk(m_Swapchain),
			UINT64_MAX, 
			toVk(frame.ImageAvailableSemaphore),
			VK_NULL_HANDLE, 
			&m_PresentImageIndex);

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
			RDN_LOG_ERROR("PresentQueue::BeginFrame: vkAcquireNextImageKHR returned {0}", (int)result);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_OutOfDate = true;
		}

		vkResetFences(toVk(m_LogicalDevice), 1, &inFlightFence);

		vkResetCommandPool(toVk(m_LogicalDevice), toVk(frame.CmdPool), 0);

		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(toVk(frame.Cmd), &beginInfo);

		return FrameContext{
			.Cmd = CommandBuffer(frame.Cmd),
			.FrameIndex = m_CurrentFrame,
			.ImageIndex = m_PresentImageIndex,
			.PresentImage = m_SwapchainImages[m_PresentImageIndex],
			.PresentImageView = m_SwapchainImageViews[m_PresentImageIndex],
		};
	}

	void PresentQueue::Submit()
	{
		if (m_OutOfDate)
		{
			return;
		}

		PerFrame& frame = m_Frames[m_CurrentFrame];
		vkEndCommandBuffer(toVk(frame.Cmd));

		VkSemaphoreSubmitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		waitInfo.semaphore = toVk(frame.ImageAvailableSemaphore);
		waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSemaphoreSubmitInfo signalInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		signalInfo.semaphore = toVk(frame.RenderFinishedSemaphore);
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
			VkResult submitResult = vkQueueSubmit2(toVk(m_GraphicsQueue), 1, &submitInfo, toVk(frame.InFlightFence));
			if (submitResult != VK_SUCCESS)
				RDN_LOG_ERROR("PresentQueue::Submit: vkQueueSubmit2 returned {0}", (int)submitResult);
		}

		VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };

		VkSemaphore renderingFinishedSemaphore = toVk(frame.RenderFinishedSemaphore);
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderingFinishedSemaphore;

		presentInfo.swapchainCount = 1;
		VkSwapchainKHR swapchain = toVk(m_Swapchain);
		presentInfo.pSwapchains = &swapchain;
		presentInfo.pImageIndices = &m_PresentImageIndex;

		VkResult presentResult = vkQueuePresentKHR(toVk(m_PresentQueue), &presentInfo);

		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
		{
			m_OutOfDate = true;
		}
		else
		{
			VK_CHECK(presentResult);
		}

		m_CurrentFrame = (m_CurrentFrame + 1) % m_ImageCount;
	}
}