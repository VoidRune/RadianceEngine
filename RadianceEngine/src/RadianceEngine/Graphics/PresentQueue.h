#pragma once
#include "Handle.h"
#include "Common.h"
#include "CommandBuffer.h"
#include <mutex>
#include <optional>
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
		Extent2D            PresentExtent;
		Format              PresentFormat;
	};

	class Device;
	class PresentQueue
	{
	public:
		static constexpr PipelineStage AcquireWaitStage = PipelineStage::ColorAttachmentOutput;

		PresentQueue(Device* device, PresentMode presentMode);
		~PresentQueue();
		PresentQueue(const PresentQueue&) = delete;
		PresentQueue& operator=(const PresentQueue&) = delete;

		[[nodiscard]] std::optional<FrameContext> BeginFrame();
		void Submit();

		bool NeedsRecreate() const { return m_NeedsRecreate; }
		bool Recreate();
		void SetPresentMode(PresentMode presentMode);

		Format GetSurfaceFormat() const { return m_SurfaceFormat; }
		Extent2D GetExtent() const { return m_Extent; }
		PresentMode GetPresentMode() const { return m_PresentMode; }
		uint32_t GetImageCount() const { return uint32_t(m_Images.size()); }
		ImageHandle GetImage(uint32_t index) const { return m_Images[index]; }
		ImageViewHandle GetImageView(uint32_t index) const { return m_ImageViews[index]; }

	private:
		struct PerFrame {
			CommandPoolHandle   CmdPool;
			CommandBufferHandle Cmd;
			SemaphoreHandle     AcquireSemaphore;
			FenceHandle         InFlightFence;
		};

		bool CreateSwapchain();
		void DestroySwapchainResources();

		Device* m_Device;
		DeviceHandle m_LogicalDevice;
		QueueHandle m_GraphicsQueue;
		QueueHandle m_PresentQueue;
		std::mutex* m_PresentQueueMutex;

		std::vector<PerFrame> m_Frames;
		uint32_t m_CurrentFrame = 0;
		uint32_t m_ImageIndex = 0;
		bool m_FrameInProgress = false;

		SwapchainHandle m_Swapchain;
		PresentMode m_RequestedPresentMode;
		PresentMode m_PresentMode;
		Format m_SurfaceFormat = Format::Undefined;
		Extent2D m_Extent;
		std::vector<ImageHandle> m_Images;
		std::vector<ImageViewHandle> m_ImageViews;
		std::vector<SemaphoreHandle> m_PresentSemaphores;
		bool m_NeedsRecreate = false;
	};
}
