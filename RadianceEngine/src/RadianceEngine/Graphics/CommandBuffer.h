#pragma once
#include "Handle.h"
#include "Common.h"
#include "Attachment.h"
#include "Resources/RaytracingPipeline.h"
#include <span>
#include <optional>
#include <vector>

namespace Rdn
{
    struct ImageBarrier
    {
        ImageHandle     Handle;
        ImageLayout     OldLayout;
        ImageLayout     NewLayout;
        PipelineStage   SrcStage = PipelineStage::AllCommands;
        PipelineStage   DstStage = PipelineStage::AllCommands;
        AccessMask      SrcAccess = AccessMask::MemoryWrite;
        AccessMask      DstAccess = AccessMask::MemoryRead | AccessMask::MemoryWrite;
        ImageAspect     Aspect = ImageAspect::Color;
    };

    struct BufferBarrier
    {
        BufferHandle    Handle;
        PipelineStage   SrcStage = PipelineStage::AllCommands;
        PipelineStage   DstStage = PipelineStage::AllCommands;
        AccessMask      SrcAccess = AccessMask::MemoryWrite;
        AccessMask      DstAccess = AccessMask::MemoryRead;
        uint64_t        Offset = 0;
        uint64_t        Size = WholeSize; // a size of 0 is invalid in a buffer barrier
    };

	struct BufferWrite
	{
		uint32_t Binding = 0;
		DescriptorType Type = {};
		BufferHandle Buffer = {};
		uint32_t Offset = 0;
		uint32_t Size = 0;
		uint32_t ArrayElement = 0;
	};

	struct ImageWrite
	{
		uint32_t Binding = 0;
		DescriptorType Type = {};
		ImageViewHandle ImageView = {};
		ImageLayout ImageLayout = ImageLayout::Undefined;
		SamplerHandle Sampler = {};
		uint32_t ArrayElement = 0;
	};

	struct AccelerationStructureWrite
	{
		uint32_t Binding = 0;
		DescriptorType Type = {};
		AccelerationStructureHandle AccelerationStructure = {};
		uint32_t ArrayElement = 0;
	};

	class DescriptorWrite
	{
	public:
		DescriptorWrite& AddWrite(const BufferWrite& write)
		{
			m_BufferWrites.push_back(write);
			return *this;
		}
		DescriptorWrite& AddWrite(const ImageWrite& write)
		{
			m_ImageWrites.push_back(write);
			return *this;
		}
		DescriptorWrite& AddWrite(const AccelerationStructureWrite& write)
		{
			m_AccelerationStructureWrites.push_back(write);
			return *this;
		}

	private:
		std::vector<BufferWrite> m_BufferWrites;
		std::vector<ImageWrite> m_ImageWrites;
		std::vector<AccelerationStructureWrite> m_AccelerationStructureWrites;

		friend class Device;
		friend class CommandBuffer;
		friend class ResourceAllocator;
	};

	class CommandBuffer
	{
	public:
		CommandBuffer(CommandBufferHandle handle);
		~CommandBuffer() = default;
		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer& operator=(const CommandBuffer&) = delete;
		CommandBuffer(CommandBuffer&&) = default;
		CommandBuffer& operator=(CommandBuffer&&) = default;

		void Begin(bool oneTimeSubmit = true);
		void End();

        void SetViewport(const Extent2D area);
        void SetScissors(const Extent2D area);

        void BeginRendering(const std::vector<ColorAttachment>& colorAttachments, std::optional<DepthAttachment> depthAttachment, const Extent2D area);
        void EndRendering();

		void BindDescriptorSets(PipelineBindPoint bindPoint, PipelineLayoutHandle layout, uint32_t firstSet, const std::vector<DescriptorSetHandle>& descriptorSets);
        void PushDescriptorSets(PipelineBindPoint bindPoint, PipelineLayoutHandle layout, uint32_t set, const DescriptorWrite& descriptorWrite);
        void PushConstants(ShaderStage shaderStage, PipelineLayoutHandle layout, const void* data, uint32_t size);

        void BindPipeline(PipelineHandle pipeline);
        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t vertexOffset, uint32_t firstInstance);
		void BindComputePipeline(PipelineHandle pipeline);
		void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
		void BindRayTracingPipeline(PipelineHandle pipeline);
		void TraceRays(RayTracingPipeline* pipeline, uint32_t width, uint32_t height, uint32_t depth);

        void Barrier(std::span<const ImageBarrier>  imageBarriers, std::span<const BufferBarrier> bufferBarriers);

        void TransitionImage(ImageHandle image,
            ImageLayout oldLayout, ImageLayout newLayout,
            PipelineStage srcStage, PipelineStage dstStage,
            ImageAspect aspect);
        void ClearColorImage(ImageHandle image, const float clearColor[4], ImageLayout layout);
        void BlitImageToImage(ImageHandle src, const Extent3D srcExtent, ImageHandle dst, const Extent3D dstExtent);

        // Named regions for RenderDoc/Nsight. Requires VK_EXT_debug_utils (Device::IsDebugUtilsEnabled()).
        void BeginDebugLabel(const char* name);
        void EndDebugLabel();

		CommandBufferHandle GetHandle() const { return m_Handle; }
	private:
		CommandBufferHandle m_Handle;
	};
}