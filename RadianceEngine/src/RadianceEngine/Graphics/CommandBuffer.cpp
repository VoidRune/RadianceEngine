#include "CommandBuffer.h"
#include "Resources/RaytracingPipeline.h"
#include "VulkanInternal/VulkanUtilities.h"
#include <vector>

namespace Rdn
{
    static_assert(sizeof(DrawIndirectCommand) == sizeof(VkDrawIndirectCommand));
    static_assert(sizeof(DrawIndexedIndirectCommand) == sizeof(VkDrawIndexedIndirectCommand));
    static_assert(sizeof(DispatchIndirectCommand) == sizeof(VkDispatchIndirectCommand));

    namespace
    {
        template<typename T, size_t N>
        class InlineArray
        {
        public:
            explicit InlineArray(size_t size)
                : m_Size(uint32_t(size))
            {
                if (size > N)
                {
                    m_Heap.resize(size);
                    m_Data = m_Heap.data();
                }
            }
            InlineArray(const InlineArray&) = delete;
            InlineArray& operator=(const InlineArray&) = delete;

            T& operator[](size_t index) { return m_Data[index]; }
            const T* Data() const { return m_Data; }
            uint32_t Size() const { return m_Size; }

        private:
            T m_Inline[N];
            std::vector<T> m_Heap;
            T* m_Data = m_Inline;
            uint32_t m_Size;
        };

        VkOffset3D ToOffset(Extent3D extent)
        {
            return { int32_t(extent.Width), int32_t(extent.Height), int32_t(extent.Depth) };
        }

        VkBufferImageCopy ColorCopyRegion(Extent3D extent, uint32_t mipLevel, uint64_t bufferOffset)
        {
            VkBufferImageCopy region{};
            region.bufferOffset = bufferOffset;
            region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0, 1 };
            region.imageExtent = { extent.Width, extent.Height, extent.Depth };
            return region;
        }
    }

	CommandBuffer::CommandBuffer(CommandBufferHandle handle)
		: m_Handle(handle)
	{

	}

	void CommandBuffer::Begin(bool oneTimeSubmit)
	{
		VkCommandBufferBeginInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		info.flags = oneTimeSubmit ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;
		VK_CHECK(vkBeginCommandBuffer(toVk(m_Handle), &info));
	}

	void CommandBuffer::End()
	{
		VK_CHECK(vkEndCommandBuffer(toVk(m_Handle)));
	}

    void CommandBuffer::BeginRendering(std::span<const ColorAttachment> colorAttachments, const std::optional<DepthAttachment>& depthAttachment, const Extent2D area)
    {
        InlineArray<VkRenderingAttachmentInfo, 8> colorInfos(colorAttachments.size());
        for (size_t i = 0; i < colorAttachments.size(); i++)
        {
            const ColorAttachment& attachment = colorAttachments[i];
            VkRenderingAttachmentInfo& info = colorInfos[i];
            info = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            info.imageView = toVk(attachment.ImageView);
            info.imageLayout = toVk(attachment.ImageLayout);
            info.loadOp = toVk(attachment.LoadOp);
            info.storeOp = toVk(attachment.StoreOp);
            info.clearValue.color = { { attachment.ClearColor[0], attachment.ClearColor[1], attachment.ClearColor[2], attachment.ClearColor[3] } };
        }

        VkRenderingAttachmentInfo depthInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        if (depthAttachment)
        {
            depthInfo.imageView = toVk(depthAttachment->ImageView);
            depthInfo.imageLayout = toVk(depthAttachment->ImageLayout);
            depthInfo.loadOp = toVk(depthAttachment->LoadOp);
            depthInfo.storeOp = toVk(depthAttachment->StoreOp);
            depthInfo.clearValue.depthStencil = { depthAttachment->ClearDepth, 0 };
        }

        VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderingInfo.renderArea = { { 0, 0 }, { area.Width, area.Height } };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = colorInfos.Size();
        renderingInfo.pColorAttachments = colorInfos.Data();
        renderingInfo.pDepthAttachment = depthAttachment ? &depthInfo : nullptr;
        vkCmdBeginRendering(toVk(m_Handle), &renderingInfo);
    }

    void CommandBuffer::EndRendering()
    {
        vkCmdEndRendering(toVk(m_Handle));
    }

    void CommandBuffer::SetViewport(const Extent2D area)
    {
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(area.Width);
        viewport.height = static_cast<float>(area.Height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(toVk(m_Handle), 0, 1, &viewport);
    }

    void CommandBuffer::SetScissors(const Extent2D area)
    {
        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = { area.Width, area.Height };
        vkCmdSetScissor(toVk(m_Handle), 0, 1, &scissor);
    }

    void CommandBuffer::BindPipeline(PipelineHandle pipeline)
    {
        vkCmdBindPipeline(toVk(m_Handle), VK_PIPELINE_BIND_POINT_GRAPHICS, toVk(pipeline));
    }

    void CommandBuffer::BindComputePipeline(PipelineHandle pipeline)
    {
        vkCmdBindPipeline(toVk(m_Handle), VK_PIPELINE_BIND_POINT_COMPUTE, toVk(pipeline));
    }

    void CommandBuffer::BindRayTracingPipeline(PipelineHandle pipeline)
    {
        vkCmdBindPipeline(toVk(m_Handle), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, toVk(pipeline));
    }

    void CommandBuffer::BindDescriptorSets(PipelineBindPoint bindPoint, PipelineLayoutHandle layout, uint32_t firstSet, std::span<const DescriptorSetHandle> descriptorSets)
    {
        InlineArray<VkDescriptorSet, 8> sets(descriptorSets.size());
        for (size_t i = 0; i < descriptorSets.size(); i++)
            sets[i] = toVk(descriptorSets[i]);

        vkCmdBindDescriptorSets(toVk(m_Handle), toVk(bindPoint), toVk(layout), firstSet, sets.Size(), sets.Data(), 0, nullptr);
    }

    void CommandBuffer::BindDescriptorSets(PipelineBindPoint bindPoint, PipelineLayoutHandle layout, uint32_t firstSet, std::initializer_list<DescriptorSetHandle> descriptorSets)
    {
        BindDescriptorSets(bindPoint, layout, firstSet, std::span(descriptorSets.begin(), descriptorSets.size()));
    }

    void CommandBuffer::PushDescriptorSets(PipelineBindPoint bindPoint, PipelineLayoutHandle layout, uint32_t set, const DescriptorWrite& descriptorWrite)
    {
        const VulkanDescriptorWrites writes(descriptorWrite, VK_NULL_HANDLE);
        vkCmdPushDescriptorSet(toVk(m_Handle), toVk(bindPoint), toVk(layout), set, uint32_t(writes.Writes.size()), writes.Writes.data());
    }

    void CommandBuffer::PushConstants(ShaderStage shaderStage, PipelineLayoutHandle layout, const void* data, uint32_t size, uint32_t offset)
    {
        vkCmdPushConstants(toVk(m_Handle), toVk(layout), toVk(shaderStage), offset, size, data);
    }

    void CommandBuffer::BindVertexBuffer(uint32_t binding, BufferHandle buffer, uint64_t offset)
    {
        const VkBuffer vkBuffer = toVk(buffer);
        vkCmdBindVertexBuffers(toVk(m_Handle), binding, 1, &vkBuffer, &offset);
    }

    void CommandBuffer::BindIndexBuffer(BufferHandle buffer, IndexType indexType, uint64_t offset)
    {
        vkCmdBindIndexBuffer(toVk(m_Handle), toVk(buffer), offset, toVk(indexType));
    }

    void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        vkCmdDraw(toVk(m_Handle), vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        vkCmdDrawIndexed(toVk(m_Handle), indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void CommandBuffer::DrawIndirect(BufferHandle buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
    {
        vkCmdDrawIndirect(toVk(m_Handle), toVk(buffer), offset, drawCount, stride);
    }

    void CommandBuffer::DrawIndexedIndirect(BufferHandle buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
    {
        vkCmdDrawIndexedIndirect(toVk(m_Handle), toVk(buffer), offset, drawCount, stride);
    }

    void CommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        vkCmdDispatch(toVk(m_Handle), groupCountX, groupCountY, groupCountZ);
    }

    void CommandBuffer::DispatchIndirect(BufferHandle buffer, uint64_t offset)
    {
        vkCmdDispatchIndirect(toVk(m_Handle), toVk(buffer), offset);
    }

    void CommandBuffer::TraceRays(RayTracingPipeline* pipeline, uint32_t width, uint32_t height, uint32_t depth)
    {
        auto toVkRegion = [](const RayTracingPipeline::StridedDeviceAddressRegion& region) {
            return VkStridedDeviceAddressRegionKHR{ region.DeviceAddress, region.Stride, region.Size };
        };
        const VkStridedDeviceAddressRegionKHR rayGen = toVkRegion(pipeline->GetRayGenShaderBindingTable());
        const VkStridedDeviceAddressRegionKHR miss = toVkRegion(pipeline->GetRayMissShaderBindingTable());
        const VkStridedDeviceAddressRegionKHR closestHit = toVkRegion(pipeline->GetRayClosestHitShaderBindingTable());
        const VkStridedDeviceAddressRegionKHR callable{};

        vkCmdTraceRaysKHR(toVk(m_Handle), &rayGen, &miss, &closestHit, &callable, width, height, depth);
    }

    void CommandBuffer::Barrier(std::span<const ImageBarrier> imageBarriers, std::span<const BufferBarrier> bufferBarriers, std::span<const GlobalBarrier> globalBarriers)
    {
        if (imageBarriers.empty() && bufferBarriers.empty() && globalBarriers.empty())
            return;

        InlineArray<VkImageMemoryBarrier2, 16> images(imageBarriers.size());
        for (size_t i = 0; i < imageBarriers.size(); i++)
        {
            const ImageBarrier& b = imageBarriers[i];
            VkImageMemoryBarrier2& vkb = images[i];
            vkb = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
            vkb.srcStageMask = toVk(b.SrcStage);
            vkb.srcAccessMask = toVk(b.SrcAccess);
            vkb.dstStageMask = toVk(b.DstStage);
            vkb.dstAccessMask = toVk(b.DstAccess);
            vkb.oldLayout = toVk(b.OldLayout);
            vkb.newLayout = toVk(b.NewLayout);
            vkb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkb.image = toVk(b.Handle);
            vkb.subresourceRange = { toVk(b.Aspect), b.BaseMip, b.MipCount, b.BaseLayer, b.LayerCount };
        }

        InlineArray<VkBufferMemoryBarrier2, 16> buffers(bufferBarriers.size());
        for (size_t i = 0; i < bufferBarriers.size(); i++)
        {
            const BufferBarrier& b = bufferBarriers[i];
            VkBufferMemoryBarrier2& vkb = buffers[i];
            vkb = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
            vkb.srcStageMask = toVk(b.SrcStage);
            vkb.srcAccessMask = toVk(b.SrcAccess);
            vkb.dstStageMask = toVk(b.DstStage);
            vkb.dstAccessMask = toVk(b.DstAccess);
            vkb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkb.buffer = toVk(b.Handle);
            vkb.offset = b.Offset;
            vkb.size = b.Size;
        }

        InlineArray<VkMemoryBarrier2, 4> globals(globalBarriers.size());
        for (size_t i = 0; i < globalBarriers.size(); i++)
        {
            const GlobalBarrier& b = globalBarriers[i];
            VkMemoryBarrier2& vkb = globals[i];
            vkb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
            vkb.srcStageMask = toVk(b.SrcStage);
            vkb.srcAccessMask = toVk(b.SrcAccess);
            vkb.dstStageMask = toVk(b.DstStage);
            vkb.dstAccessMask = toVk(b.DstAccess);
        }

        VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dependency.memoryBarrierCount = globals.Size();
        dependency.pMemoryBarriers = globals.Data();
        dependency.bufferMemoryBarrierCount = buffers.Size();
        dependency.pBufferMemoryBarriers = buffers.Data();
        dependency.imageMemoryBarrierCount = images.Size();
        dependency.pImageMemoryBarriers = images.Data();
        vkCmdPipelineBarrier2(toVk(m_Handle), &dependency);
    }

    void CommandBuffer::Barrier(const ImageBarrier& barrier)
    {
        Barrier(std::span(&barrier, 1));
    }

    void CommandBuffer::Barrier(const BufferBarrier& barrier)
    {
        Barrier({}, std::span(&barrier, 1));
    }

    void CommandBuffer::Barrier(const GlobalBarrier& barrier)
    {
        Barrier({}, {}, std::span(&barrier, 1));
    }

    void CommandBuffer::CopyBuffer(BufferHandle src, BufferHandle dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        const VkBufferCopy region{ srcOffset, dstOffset, size };
        vkCmdCopyBuffer(toVk(m_Handle), toVk(src), toVk(dst), 1, &region);
    }

    void CommandBuffer::CopyBufferToImage(BufferHandle src, ImageHandle dst, const Extent3D extent, uint32_t mipLevel, uint64_t bufferOffset)
    {
        const VkBufferImageCopy region = ColorCopyRegion(extent, mipLevel, bufferOffset);
        vkCmdCopyBufferToImage(toVk(m_Handle), toVk(src), toVk(dst), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    void CommandBuffer::CopyImageToBuffer(ImageHandle src, BufferHandle dst, const Extent3D extent, uint32_t mipLevel, uint64_t bufferOffset)
    {
        const VkBufferImageCopy region = ColorCopyRegion(extent, mipLevel, bufferOffset);
        vkCmdCopyImageToBuffer(toVk(m_Handle), toVk(src), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, toVk(dst), 1, &region);
    }

    void CommandBuffer::BlitImage(ImageHandle src, const Extent3D srcExtent, ImageHandle dst, const Extent3D dstExtent, Filter filter, uint32_t srcMip, uint32_t dstMip)
    {
        VkImageBlit2 region{ VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
        region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, srcMip, 0, 1 };
        region.srcOffsets[1] = ToOffset(srcExtent);
        region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, dstMip, 0, 1 };
        region.dstOffsets[1] = ToOffset(dstExtent);

        VkBlitImageInfo2 blitInfo{ VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2 };
        blitInfo.srcImage = toVk(src);
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.dstImage = toVk(dst);
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &region;
        blitInfo.filter = toVk(filter);
        vkCmdBlitImage2(toVk(m_Handle), &blitInfo);
    }

    void CommandBuffer::FillBuffer(BufferHandle buffer, uint32_t value, uint64_t offset, uint64_t size)
    {
        vkCmdFillBuffer(toVk(m_Handle), toVk(buffer), offset, size, value);
    }

    void CommandBuffer::ClearColorImage(ImageHandle image, const std::array<float, 4>& clearColor, ImageLayout layout)
    {
        const VkClearColorValue clearValue{ { clearColor[0], clearColor[1], clearColor[2], clearColor[3] } };
        const VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
        vkCmdClearColorImage(toVk(m_Handle), toVk(image), toVk(layout), &clearValue, 1, &range);
    }

    void CommandBuffer::BeginDebugLabel(const char* name)
    {
        if (!vkCmdBeginDebugUtilsLabelEXT)
            return;

        VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName = name;
        vkCmdBeginDebugUtilsLabelEXT(toVk(m_Handle), &label);
    }

    void CommandBuffer::EndDebugLabel()
    {
        if (vkCmdEndDebugUtilsLabelEXT)
            vkCmdEndDebugUtilsLabelEXT(toVk(m_Handle));
    }
}
