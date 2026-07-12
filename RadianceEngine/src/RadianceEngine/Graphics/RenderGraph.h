#pragma once
#include "Handle.h"
#include "Device.h"
#include "Common.h"
#include "CommandBuffer.h"
#include "Attachment.h"

#include <functional>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

namespace Rdn
{
	struct ImageAccess
	{
		ImageHandle   Image;
		ImageLayout   Layout;
		PipelineStage Stage;
		AccessMask    Access;
		ImageAspect   Aspect = ImageAspect::Color;
	};

	struct BufferAccess
	{
		BufferHandle  Buffer;
		PipelineStage Stage;
		AccessMask    Access;
	};

	class PassBuilder
	{
	public:
		void UseImage(ImageHandle image, ImageLayout layout,
			PipelineStage stage, AccessMask access,
			ImageAspect aspect = ImageAspect::Color);

		void UseBuffer(BufferHandle buffer, PipelineStage stage, AccessMask access);

		void AddColorAttachment(ImageViewHandle view, ImageHandle backingImage,
			AttachmentLoadOp loadOp = AttachmentLoadOp::Clear,
			AttachmentStoreOp storeOp = AttachmentStoreOp::Store,
			float r = 0, float g = 0, float b = 0, float a = 1);

		void SetDepthAttachment(ImageViewHandle view, ImageHandle backingImage,
			AttachmentLoadOp loadOp = AttachmentLoadOp::Clear,
			AttachmentStoreOp storeOp = AttachmentStoreOp::DontCare,
			float clearDepth = 1.0f);

	private:
		friend class RenderGraph;
		struct PassData* m_Data = nullptr;
	};

	using PassSetupFn = std::function<void(PassBuilder&)>;
	using PassExecuteFn = std::function<void(CommandBuffer&, uint32_t frameIndex)>;

	struct PassData
	{
		std::string Name;
		PassExecuteFn Execute;

		std::vector<ImageAccess> ImageAccesses;
		std::vector<BufferAccess> BufferAccesses;

		std::vector<ColorAttachment> ColorAttachments;
		std::vector<ImageHandle> ColorAttachmentImages;
		std::optional<DepthAttachment> DepthAttach;
		ImageHandle DepthAttachImage;
		bool UsesDynamicRendering = false;
	};

	class RenderGraph
	{
	public:
		RenderGraph(Device* device);
		~RenderGraph();

		void AddPass(std::string name, PassSetupFn setup, PassExecuteFn execute);

		void SetPresentPass(std::string name,
			PassSetupFn setup,
			AttachmentLoadOp loadOp,
			float clearR, float clearG, float clearB, float clearA,
			PassExecuteFn execute);

		void Execute(CommandBuffer& cmd, uint32_t frameIndex,
			ImageHandle swapchainImage, ImageViewHandle swapchainView,
			const Extent2D area);

		void Reset();

	private:
		struct ResourceState
		{
			ImageLayout   Layout = ImageLayout::Undefined;
			PipelineStage Stage = PipelineStage::TopOfPipe;
			AccessMask    Access = AccessMask::None;
		};
		struct BufferState
		{
			PipelineStage Stage = PipelineStage::TopOfPipe;
			AccessMask    Access = AccessMask::None;
		};

		void AddBarriersForPass(CommandBuffer& cmd, PassData& pass);
		void RecordPass(CommandBuffer& cmd, PassData& pass, uint32_t frameIndex, const Extent2D area);

		Device* m_Device;
		DeviceHandle m_LogicalDevice;

		std::vector<PassData> m_Passes;
		std::optional<PassData> m_PresentPass;

		std::unordered_map<uint64_t, ResourceState> m_ImageStates;
		std::unordered_map<uint64_t, BufferState> m_BufferStates;
	};
}