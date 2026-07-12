#pragma once
#include "Handle.h"
#include "Common.h"
#include "CommandBuffer.h"
#include <vector>

namespace Rdn
{
	struct FrameContext 
	{
		CommandBuffer		Cmd;
		uint32_t			FrameIndex;
		uint32_t			ImageIndex;
		ImageHandle         PresentImage;
		ImageViewHandle     PresentImageView;
	};

	class Device;
	class PresentQueue
	{
	public:
		PresentQueue(Device* device, PresentMode presentMode);
		~PresentQueue();
		PresentQueue(const PresentQueue&) = delete;
		PresentQueue& operator=(const PresentQueue&) = delete;

		bool OutOfDate() { return m_OutOfDate; }
		FrameContext BeginFrame();
		void Submit();

		Format GetSurfaceFormat() { return m_SurfaceFormat; }
		ImageHandle GetImage(uint32_t index) { return m_SwapchainImages[index]; };
		Extent2D GetExtent() { return m_SwapchainSize.To2D(); };
		ImageViewHandle GetImageView(uint32_t index) { return m_SwapchainImageViews[index]; };

	private:
		struct PerFrame {
			CommandPoolHandle   CmdPool;
			CommandBufferHandle Cmd;
			SemaphoreHandle     ImageAvailableSemaphore;
			SemaphoreHandle     RenderFinishedSemaphore;
			FenceHandle         InFlightFence;
		};
		std::vector<PerFrame> m_Frames;
		uint32_t m_CurrentFrame;
		uint32_t m_PresentImageIndex;
		bool m_OutOfDate;

		Device* m_Device;
		DeviceHandle m_LogicalDevice;
		SwapchainHandle m_Swapchain;
		QueueHandle m_GraphicsQueue;
		QueueHandle m_PresentQueue;
		uint32_t m_ImageCount;
		Format m_SurfaceFormat;
		Extent3D m_SwapchainSize;
		std::vector<ImageHandle> m_SwapchainImages;
		std::vector<ImageViewHandle> m_SwapchainImageViews;
	};
}