#pragma once
#include "Handle.h"
#include "Common.h"
#include "Attachment.h"
#include "DescriptorWrite.h"
#include <array>
#include <initializer_list>
#include <optional>
#include <span>

namespace Rdn
{
    class RayTracingPipeline;

    struct ImageBarrier
    {
        ImageHandle     Handle;
        ImageLayout     OldLayout = ImageLayout::Undefined;
        ImageLayout     NewLayout = ImageLayout::Undefined;
        PipelineStage   SrcStage = PipelineStage::AllCommands;
        PipelineStage   DstStage = PipelineStage::AllCommands;
        AccessMask      SrcAccess = AccessMask::MemoryWrite;
        AccessMask      DstAccess = AccessMask::MemoryRead | AccessMask::MemoryWrite;
        ImageAspect     Aspect = ImageAspect::Color;
        uint32_t        BaseMip = 0;
        uint32_t        MipCount = RemainingMips;
        uint32_t        BaseLayer = 0;
        uint32_t        LayerCount = RemainingLayers;
    };

    struct BufferBarrier
    {
        BufferHandle    Handle;
        PipelineStage   SrcStage = PipelineStage::AllCommands;
        PipelineStage   DstStage = PipelineStage::AllCommands;
        AccessMask      SrcAccess = AccessMask::MemoryWrite;
        AccessMask      DstAccess = AccessMask::MemoryRead | AccessMask::MemoryWrite;
        uint64_t        Offset = 0;
        uint64_t        Size = WholeSize; // a size of 0 is invalid in a buffer barrier
    };

    struct GlobalBarrier
    {
        PipelineStage   SrcStage = PipelineStage::AllCommands;
        PipelineStage   DstStage = PipelineStage::AllCommands;
        AccessMask      SrcAccess = AccessMask::MemoryWrite;
        AccessMask      DstAccess = AccessMask::MemoryRead | AccessMask::MemoryWrite;
    };

    struct DrawIndirectCommand
    {
        uint32_t VertexCount = 0;
        uint32_t InstanceCount = 1;
        uint32_t FirstVertex = 0;
        uint32_t FirstInstance = 0;
    };

    struct DrawIndexedIndirectCommand
    {
        uint32_t IndexCount = 0;
        uint32_t InstanceCount = 1;
        uint32_t FirstIndex = 0;
        int32_t  VertexOffset = 0;
        uint32_t FirstInstance = 0;
    };

    struct DispatchIndirectCommand
    {
        uint32_t GroupCountX = 1;
        uint32_t GroupCountY = 1;
        uint32_t GroupCountZ = 1;
    };

	class CommandBuffer
	{
	public:
		explicit CommandBuffer(CommandBufferHandle handle);
		~CommandBuffer() = default;
		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer& operator=(const CommandBuffer&) = delete;
		CommandBuffer(CommandBuffer&&) = default;
		CommandBuffer& operator=(CommandBuffer&&) = default;

		void Begin(bool oneTimeSubmit = true);
		void End();

        void BeginRendering(std::span<const ColorAttachment> colorAttachments, const std::optional<DepthAttachment>& depthAttachment, const Extent2D area);
        void EndRendering();
        void SetViewport(const Extent2D area);
        void SetScissors(const Extent2D area);

        void BindPipeline(PipelineHandle pipeline);
        void BindComputePipeline(PipelineHandle pipeline);
        void BindRayTracingPipeline(PipelineHandle pipeline);
        void BindDescriptorSets(PipelineBindPoint bindPoint, PipelineLayoutHandle layout, uint32_t firstSet, std::span<const DescriptorSetHandle> descriptorSets);
        void BindDescriptorSets(PipelineBindPoint bindPoint, PipelineLayoutHandle layout, uint32_t firstSet, std::initializer_list<DescriptorSetHandle> descriptorSets);
        void PushDescriptorSets(PipelineBindPoint bindPoint, PipelineLayoutHandle layout, uint32_t set, const DescriptorWrite& descriptorWrite);
        void PushConstants(ShaderStage shaderStage, PipelineLayoutHandle layout, const void* data, uint32_t size, uint32_t offset = 0);

        void BindVertexBuffer(uint32_t binding, BufferHandle buffer, uint64_t offset = 0);
        void BindIndexBuffer(BufferHandle buffer, IndexType indexType, uint64_t offset = 0);
        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
        void DrawIndirect(BufferHandle buffer, uint64_t offset = 0, uint32_t drawCount = 1, uint32_t stride = sizeof(DrawIndirectCommand));
        void DrawIndexedIndirect(BufferHandle buffer, uint64_t offset = 0, uint32_t drawCount = 1, uint32_t stride = sizeof(DrawIndexedIndirectCommand));
        void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
        void DispatchIndirect(BufferHandle buffer, uint64_t offset = 0);
        void TraceRays(RayTracingPipeline* pipeline, uint32_t width, uint32_t height, uint32_t depth);

        void Barrier(std::span<const ImageBarrier> imageBarriers, std::span<const BufferBarrier> bufferBarriers = {}, std::span<const GlobalBarrier> globalBarriers = {});
        void Barrier(const ImageBarrier& barrier);
        void Barrier(const BufferBarrier& barrier);
        void Barrier(const GlobalBarrier& barrier);

        void CopyBuffer(BufferHandle src, BufferHandle dst, uint64_t size, uint64_t srcOffset = 0, uint64_t dstOffset = 0);
        void CopyBufferToImage(BufferHandle src, ImageHandle dst, const Extent3D extent, uint32_t mipLevel = 0, uint64_t bufferOffset = 0);
        void CopyImageToBuffer(ImageHandle src, BufferHandle dst, const Extent3D extent, uint32_t mipLevel = 0, uint64_t bufferOffset = 0);
        void BlitImage(ImageHandle src, const Extent3D srcExtent, ImageHandle dst, const Extent3D dstExtent, Filter filter = Filter::Linear, uint32_t srcMip = 0, uint32_t dstMip = 0);
        void FillBuffer(BufferHandle buffer, uint32_t value, uint64_t offset = 0, uint64_t size = WholeSize);
        void ClearColorImage(ImageHandle image, const std::array<float, 4>& clearColor, ImageLayout layout);

        void BeginDebugLabel(const char* name);
        void EndDebugLabel();

		CommandBufferHandle GetHandle() const { return m_Handle; }
	private:
		CommandBufferHandle m_Handle;
	};
}
