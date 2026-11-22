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

	enum class LatencyMode
	{
		Off,
		Auto,
		One,
		Two,
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

		void WaitForLatency(double refreshRate);
		void SetLatencyMode(LatencyMode mode) { m_LatencyMode = mode; }
		LatencyMode GetLatencyMode() const { return m_LatencyMode; }
		bool SupportsPresentWait() const { return m_PresentWaitSupported; }
		uint32_t GetLatencyFrames() const { return m_Latency; }
		float GetDisplayLatency() const { return m_DisplayLatency; }
		float GetFrameCost() const { return m_CostAverage; }

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
			QueryPoolHandle     Timestamps;
			bool                TimestampsWritten = false;
		};

		bool CreateSwapchain();
		void DestroySwapchainResources();
		void UpdateLatency(double cost);

		static constexpr size_t SampleHistory = 8;

		bool m_PresentWaitSupported = false;
		LatencyMode m_LatencyMode = LatencyMode::Auto;
		uint32_t m_Latency = 0;
		uint64_t m_PresentId = 0;
		uint64_t m_DisplayedId = 0;
		double m_SampleTimes[SampleHistory] = {};
		double m_Period = 0.0;
		double m_Wake = 0.0;
		double m_LastWake = 0.0;
		double m_Blocked = 0.0;
		double m_GpuCost = -1.0;
		bool m_Missed = false;
		bool m_WasOver = false;
		double m_Over = 0.0;
		uint32_t m_Hold = 120;
		uint32_t m_Good = 0;
		uint32_t m_Backoff = 300;
		float m_DisplayLatency = 0.0f;
		float m_CostAverage = 0.0f;

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
		bool m_PresentModeChanged = false;
	};
}
