#include "PresentQueue.h"
#include "Device.h"
#include <RadianceEngine/Core/Log.h>
#include "VulkanInternal/VulkanUtilities.h"
#include <vulkan/vk_enum_string_helper.h>
#include <algorithm>
#include <chrono>

namespace Rdn
{
	static double Now()
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

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
			std::abort();
		}

		m_LogicalDevice = device->GetLogicalDevice();
		QueueContext& graphics = device->GetGraphicsQueueContext();
		QueueContext& present = device->GetPresentQueueContext();
		m_GraphicsQueue = graphics.Handle;
		m_PresentQueue = present.Handle;
		m_PresentQueueMutex = &present.Mutex;

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

			if (device->GetProperties().Timestamps)
			{
				VkQueryPoolCreateInfo queryInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
				queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
				queryInfo.queryCount = 2;
				VkQueryPool queryPool;
				VK_CHECK(vkCreateQueryPool(toVk(m_LogicalDevice), &queryInfo, nullptr, &queryPool));
				frame.Timestamps = fromVk(queryPool);
			}
		}
		m_PresentWaitSupported = device->Supports(DeviceCapability::PresentWait);

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
			if (frame.Timestamps)
				vkDestroyQueryPool(device, toVk(frame.Timestamps), nullptr);
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
		m_Extent = swapchainOutput.extent;
		m_PresentId = 0;
		m_DisplayedId = 0;
		m_LastWake = 0.0;

		SwapchainImagesRetrieveInfo swapchainImagesInfo =
		{
			.logicalDevice = m_LogicalDevice,
			.swapchain = m_Swapchain,
		};
		m_Images = RetrieveSwapchainImages(swapchainImagesInfo);

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
		if (m_PresentModeChanged && m_Swapchain)
		{
			DestroySwapchainResources();
			vkDestroySwapchainKHR(toVk(m_LogicalDevice), toVk(m_Swapchain), nullptr);
			m_Swapchain = {};
		}
		m_PresentModeChanged = false;
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
		m_PresentModeChanged = true;
		m_NeedsRecreate = true;
	}

	void PresentQueue::WaitForLatency(double refreshRate)
	{
		m_Period = refreshRate > 0.0 ? 1.0 / refreshRate : 0.0;
		m_Blocked = 0.0;
		const bool fifo = m_PresentMode == PresentMode::Fifo || m_PresentMode == PresentMode::FifoRelaxed;
		if (!m_PresentWaitSupported || m_LatencyMode == LatencyMode::Off || !m_Swapchain || !fifo)
		{
			m_Latency = 0;
			m_LastWake = 0.0;
			m_Wake = Now();
			return;
		}
		if (m_LatencyMode == LatencyMode::One)
			m_Latency = 1;
		else if (m_LatencyMode == LatencyMode::Two || m_Latency == 0)
			m_Latency = 2;

		const uint64_t target = m_PresentId + 1 > m_Latency ? m_PresentId + 1 - m_Latency : 0;
		if (target > m_DisplayedId)
		{
			const VkResult result = vkWaitForPresentKHR(toVk(m_LogicalDevice), toVk(m_Swapchain), target, 100'000'000);
			if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
			{
				const float latency = float((Now() - m_SampleTimes[target % SampleHistory]) * 1000.0);
				m_DisplayLatency = m_DisplayLatency > 0.0f ? m_DisplayLatency + (latency - m_DisplayLatency) * 0.05f : latency;
			}
			else if (result != VK_TIMEOUT)
				m_NeedsRecreate = true;
			m_DisplayedId = target;
		}

		m_Wake = Now();
		m_Missed = m_Latency == 1 && m_Period > 0.0 && m_LastWake > 0.0 && m_Wake - m_LastWake > m_Period * 1.5;
		m_LastWake = m_Wake;
	}

	void PresentQueue::UpdateLatency(double cost)
	{
		m_CostAverage += (float(cost * 1000.0) - m_CostAverage) * 0.05f;
		if (m_LatencyMode != LatencyMode::Auto)
			return;
		if (m_Period <= 0.0 || m_GpuCost < 0.0)
		{
			m_Latency = 2;
			return;
		}

		const bool over = cost > m_Period * 0.8 || (m_Missed && !m_WasOver);
		m_WasOver = over;
		m_Over = m_Over * 0.95 + (over ? 0.05 : 0.0);
		if (m_Latency == 1)
		{
			if (m_Over > 0.08)
			{
				m_Latency = 2;
				m_Hold = m_Backoff;
				m_Backoff = std::min(m_Backoff * 2, 7200u);
				m_Good = 0;
			}
			else if (++m_Good >= 3600)
			{
				m_Backoff = 300;
				m_Good = 0;
			}
			return;
		}
		if (m_Hold > 0)
			m_Hold--;
		m_Good = cost < m_Period * 0.65 ? m_Good + 1 : 0;
		if (m_Hold == 0 && m_Good >= 120)
		{
			m_Latency = 1;
			m_Over = 0.0;
			m_Good = 0;
		}
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
		const double blockStart = Now();
		VkFence inFlightFence = toVk(frame.InFlightFence);
		VK_CHECK(vkWaitForFences(toVk(m_LogicalDevice), 1, &inFlightFence, VK_TRUE, UINT64_MAX));
		if (frame.TimestampsWritten)
		{
			uint64_t ticks[2] = {};
			if (vkGetQueryPoolResults(toVk(m_LogicalDevice), toVk(frame.Timestamps), 0, 2, sizeof(ticks), ticks, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS)
				m_GpuCost = double(ticks[1] - ticks[0]) * double(m_Device->GetProperties().TimestampPeriod) * 1e-9;
			frame.TimestampsWritten = false;
		}

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
		m_Blocked += Now() - blockStart;

		VK_CHECK(vkResetFences(toVk(m_LogicalDevice), 1, &inFlightFence));
		VK_CHECK(vkResetCommandPool(toVk(m_LogicalDevice), toVk(frame.CmdPool), 0));

		CommandBuffer cmd(frame.Cmd);
		cmd.Begin();
		if (frame.Timestamps)
		{
			vkCmdResetQueryPool(toVk(frame.Cmd), toVk(frame.Timestamps), 0, 2);
			vkCmdWriteTimestamp2(toVk(frame.Cmd), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, toVk(frame.Timestamps), 0);
		}

		m_ImageIndex = imageIndex;
		m_FrameInProgress = true;

		return FrameContext{
			.Cmd = std::move(cmd),
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
		if (frame.Timestamps)
		{
			vkCmdWriteTimestamp2(toVk(frame.Cmd), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, toVk(frame.Timestamps), 1);
			frame.TimestampsWritten = true;
		}
		CommandBuffer(frame.Cmd).End();

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

		const uint64_t presentId = m_PresentId + 1;
		VkPresentIdKHR presentIdInfo{ VK_STRUCTURE_TYPE_PRESENT_ID_KHR };
		presentIdInfo.swapchainCount = 1;
		presentIdInfo.pPresentIds = &presentId;
		if (m_PresentWaitSupported)
			presentInfo.pNext = &presentIdInfo;

		const double presentStart = Now();
		VkResult presentResult;
		{
			std::lock_guard lock(*m_PresentQueueMutex);
			presentResult = vkQueuePresentKHR(toVk(m_PresentQueue), &presentInfo);
		}
		m_PresentId = presentId;
		m_SampleTimes[presentId % SampleHistory] = m_Wake;
		if (m_Latency > 0)
			UpdateLatency(presentStart - m_Wake - m_Blocked + std::max(m_GpuCost, 0.0));

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
