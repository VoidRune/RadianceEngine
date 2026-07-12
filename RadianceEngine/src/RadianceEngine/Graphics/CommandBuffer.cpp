#include "CommandBuffer.h"
#include "VulkanInternal/VulkanUtilities.h"

namespace Rdn
{
	CommandBuffer::CommandBuffer(CommandBufferHandle handle)
		: m_Handle(handle)
	{

	}

	void CommandBuffer::Begin(bool oneTimeSubmit)
	{
		VkCommandBufferBeginInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		info.flags = oneTimeSubmit ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;
		vkBeginCommandBuffer(toVk(m_Handle), &info);
	}

	void CommandBuffer::End()
	{
		vkEndCommandBuffer(toVk(m_Handle));
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

    void CommandBuffer::BeginRendering(const std::vector<ColorAttachment>& colorAttachments, std::optional<DepthAttachment> depthAttachment, const Extent2D area)
    {
        std::vector<VkRenderingAttachmentInfo> colorInfo(colorAttachments.size());
        for (size_t i = 0; i < colorInfo.size(); i++)
        {
            auto& info = colorInfo[i];
            auto& colorAttachment = colorAttachments[i];
            info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            info.imageView = toVk(colorAttachment.ImageView);
            info.imageLayout = toVk(colorAttachment.ImageLayout);
            info.loadOp = toVk(colorAttachment.LoadOp);
            info.storeOp = toVk(colorAttachment.StoreOp);
            info.clearValue = { colorAttachment.ClearColor[0], colorAttachment.ClearColor[1], colorAttachment.ClearColor[2], colorAttachment.ClearColor[3] };
        }

        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = VkRect2D{ {0, 0}, { area.Width, area.Height } };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = (uint32_t)colorInfo.size();
        renderingInfo.pColorAttachments = colorInfo.data();
        if (depthAttachment.has_value())
        {
            auto& da = depthAttachment.value();
            VkRenderingAttachmentInfo depthInfo = {};
            depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthInfo.imageView = toVk(da.ImageView);
            depthInfo.imageLayout = toVk(da.ImageLayout);
            depthInfo.loadOp = toVk(da.LoadOp);
            depthInfo.storeOp = toVk(da.StoreOp);
            depthInfo.clearValue.depthStencil = { da.ClearDepth, uint32_t(0) };
            renderingInfo.pDepthAttachment = &depthInfo;
        }

        vkCmdBeginRendering(toVk(m_Handle), &renderingInfo);
    }

    void CommandBuffer::EndRendering()
    {
        vkCmdEndRendering(toVk(m_Handle));
    }

    void CommandBuffer::BindDescriptorSets(PipelineBindPoint bindPoint, PipelineLayoutHandle layout, uint32_t firstSet, const std::vector<DescriptorSetHandle>& descriptorSets)
    {
        std::vector<VkDescriptorSet> sets(descriptorSets.size());
        for (size_t i = 0; i < descriptorSets.size(); i++)
        {
            sets[i] = toVk(descriptorSets[i]);
        }

        vkCmdBindDescriptorSets(toVk(m_Handle), toVk(bindPoint), toVk(layout), firstSet, (uint32_t)sets.size(), sets.data(), 0, nullptr);
    }

    void CommandBuffer::PushDescriptorSets(PipelineBindPoint bindPoint, PipelineLayoutHandle layout, uint32_t set, const DescriptorWrite& descriptorWrite)
    {
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(descriptorWrite.m_BufferWrites.size() + descriptorWrite.m_ImageWrites.size());

        std::vector<VkDescriptorBufferInfo> bufferInfos;
        bufferInfos.reserve(descriptorWrite.m_BufferWrites.size());

        for (auto& bw : descriptorWrite.m_BufferWrites)
        {
            VkDescriptorBufferInfo bufferInfo = {};
            bufferInfo.buffer = toVk(bw.Buffer);
            bufferInfo.offset = bw.Offset;
            bufferInfo.range = bw.Size == 0 ? VK_WHOLE_SIZE : bw.Size;
            bufferInfos.push_back(bufferInfo);

            VkWriteDescriptorSet writeInfo = {};
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.dstSet = nullptr;
            writeInfo.dstBinding = bw.Binding;
            writeInfo.dstArrayElement = bw.ArrayElement;
            writeInfo.descriptorType = toVk(bw.Type);
            writeInfo.descriptorCount = 1;
            writeInfo.pBufferInfo = &bufferInfos[bufferInfos.size() - 1];
            writes.push_back(writeInfo);
        }

        std::vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.reserve(descriptorWrite.m_ImageWrites.size());

        for (auto& iw : descriptorWrite.m_ImageWrites)
        {
            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageView = toVk(iw.ImageView);
            imageInfo.imageLayout = toVk(iw.ImageLayout);
            imageInfo.sampler = toVk(iw.Sampler);
            imageInfos.push_back(imageInfo);

            VkWriteDescriptorSet writeInfo = {};
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.dstSet = nullptr;
            writeInfo.dstBinding = iw.Binding;
            writeInfo.dstArrayElement = iw.ArrayElement;
            writeInfo.descriptorType = toVk(iw.Type);
            writeInfo.descriptorCount = 1;
            writeInfo.pImageInfo = &imageInfos[imageInfos.size() - 1];
            writes.push_back(writeInfo);
        }

        std::vector<VkAccelerationStructureKHR> accelerationHandles;
        accelerationHandles.reserve(descriptorWrite.m_AccelerationStructureWrites.size());

        std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accelerationInfos;
        accelerationInfos.reserve(descriptorWrite.m_AccelerationStructureWrites.size());

        for (auto& sw : descriptorWrite.m_AccelerationStructureWrites)
        {
            accelerationHandles.push_back(toVk(sw.AccelerationStructure));
            VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
            descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
            descriptorAccelerationStructureInfo.pAccelerationStructures = &accelerationHandles[accelerationHandles.size() - 1];
            accelerationInfos.push_back(descriptorAccelerationStructureInfo);

            VkWriteDescriptorSet writeInfo = {};
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.pNext = &accelerationInfos[accelerationInfos.size() - 1];
            writeInfo.dstSet = nullptr;
            writeInfo.dstBinding = sw.Binding;
            writeInfo.dstArrayElement = sw.ArrayElement;
            writeInfo.descriptorType = toVk(sw.Type);
            writeInfo.descriptorCount = 1;
            writes.push_back(writeInfo);
        }

        vkCmdPushDescriptorSet(toVk(m_Handle), toVk(bindPoint), toVk(layout), set, (uint32_t)writes.size(), writes.data());
    }

    void CommandBuffer::PushConstants(ShaderStage shaderStage, PipelineLayoutHandle layout, const void* data, uint32_t size)
    {
        vkCmdPushConstants(toVk(m_Handle), toVk(layout), toVk(shaderStage), 0, size, data);
    }

    void CommandBuffer::BindPipeline(PipelineHandle pipeline)
    {
        vkCmdBindPipeline(toVk(m_Handle), VK_PIPELINE_BIND_POINT_GRAPHICS, toVk(pipeline));
    }

    void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        vkCmdDraw(toVk(m_Handle), vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t vertexOffset, uint32_t firstInstance)
    {
        vkCmdDrawIndexed(toVk(m_Handle), indexCount, instanceCount, firstVertex, vertexOffset, firstInstance);
    }

    void CommandBuffer::BindComputePipeline(PipelineHandle pipeline)
    {
        vkCmdBindPipeline(toVk(m_Handle), VK_PIPELINE_BIND_POINT_COMPUTE, toVk(pipeline));
    }

    void CommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        vkCmdDispatch(toVk(m_Handle), groupCountX, groupCountY, groupCountZ);
    }

    void CommandBuffer::BindRayTracingPipeline(PipelineHandle pipeline)
    {
        vkCmdBindPipeline(toVk(m_Handle), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, toVk(pipeline));
    }

    void CommandBuffer::TraceRays(RayTracingPipeline* pipeline, uint32_t width, uint32_t height, uint32_t depth)
    {
        VkStridedDeviceAddressRegionKHR rayGenSBT{};
        rayGenSBT.deviceAddress = pipeline->GetRayGenShaderBindingTable().DeviceAddress;
        rayGenSBT.stride = pipeline->GetRayGenShaderBindingTable().Stride;
        rayGenSBT.size = pipeline->GetRayGenShaderBindingTable().Size;

        VkStridedDeviceAddressRegionKHR rayMissSBT{};
        rayMissSBT.deviceAddress = pipeline->GetRayMissShaderBindingTable().DeviceAddress;
        rayMissSBT.stride = pipeline->GetRayMissShaderBindingTable().Stride;
        rayMissSBT.size = pipeline->GetRayMissShaderBindingTable().Size;

        VkStridedDeviceAddressRegionKHR rayClosestHitSBT{};
        rayClosestHitSBT.deviceAddress = pipeline->GetRayClosestHitShaderBindingTable().DeviceAddress;
        rayClosestHitSBT.stride = pipeline->GetRayClosestHitShaderBindingTable().Stride;
        rayClosestHitSBT.size = pipeline->GetRayClosestHitShaderBindingTable().Size;

        VkStridedDeviceAddressRegionKHR rayCallableSBT{};

        vkCmdTraceRaysKHR(toVk(m_Handle),
            &rayGenSBT,
            &rayMissSBT,
            &rayClosestHitSBT,
            &rayCallableSBT,
            width, height, depth);
    }


    void CommandBuffer::Barrier(std::span<const ImageBarrier>  imageBarriers,
        std::span<const BufferBarrier> bufferBarriers)
    {
        VkImageMemoryBarrier2  vkImgBarriers[16];
        VkBufferMemoryBarrier2 vkBufBarriers[16];

        std::vector<VkImageMemoryBarrier2>  imgHeap;
        std::vector<VkBufferMemoryBarrier2> bufHeap;

        VkImageMemoryBarrier2* pImg = vkImgBarriers;
        VkBufferMemoryBarrier2* pBuf = vkBufBarriers;

        if (imageBarriers.size() > 16) { imgHeap.resize(imageBarriers.size());  pImg = imgHeap.data(); }
        if (bufferBarriers.size() > 16) { bufHeap.resize(bufferBarriers.size()); pBuf = bufHeap.data(); }

        for (size_t i = 0; i < imageBarriers.size(); i++)
        {
            const auto& b = imageBarriers[i];
            auto& vkb = pImg[i];
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
            vkb.subresourceRange = {
                toVk(b.Aspect), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS
            };
        }

        for (size_t i = 0; i < bufferBarriers.size(); i++)
        {
            const auto& b = bufferBarriers[i];
            auto& vkb = pBuf[i];
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

        VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dep.imageMemoryBarrierCount = uint32_t(imageBarriers.size());
        dep.pImageMemoryBarriers = pImg;
        dep.bufferMemoryBarrierCount = uint32_t(bufferBarriers.size());
        dep.pBufferMemoryBarriers = pBuf;

        vkCmdPipelineBarrier2(toVk(m_Handle), &dep);
    }

    void CommandBuffer::TransitionImage(ImageHandle image,
        ImageLayout oldLayout, ImageLayout newLayout,
        PipelineStage srcStage, PipelineStage dstStage,
        ImageAspect aspect)
    {
        ImageBarrier barrier{};
        barrier.Handle = image;
        barrier.OldLayout = oldLayout;
        barrier.NewLayout = newLayout;
        barrier.SrcStage = srcStage;
        barrier.DstStage = dstStage;
        barrier.SrcAccess = (oldLayout == ImageLayout::Undefined)
            ? AccessMask::None
            : AccessMask::MemoryWrite;
        barrier.DstAccess = (newLayout == ImageLayout::ShaderReadOnlyOptimal)
            ? AccessMask::ShaderRead
            : AccessMask::MemoryWrite | AccessMask::MemoryRead;
        barrier.Aspect = aspect;
        Barrier({ &barrier, 1 }, {});
    }

    void CommandBuffer::ClearColorImage(ImageHandle image, const float clearColor[4], ImageLayout layout)
    {
        VkClearColorValue clearValue;
        clearValue = { {clearColor[0], clearColor[1], clearColor[2], clearColor[3]} };

        VkImageSubresourceRange subImage{};
        subImage.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subImage.baseMipLevel = 0;
        subImage.levelCount = VK_REMAINING_MIP_LEVELS;
        subImage.baseArrayLayer = 0;
        subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

        vkCmdClearColorImage(toVk(m_Handle), toVk(image), toVk(layout), &clearValue, 1, &subImage);
    }

    void CommandBuffer::BlitImageToImage(ImageHandle src, const Extent3D srcExtent, ImageHandle dst, const Extent3D dstExtent)
    {
        VkImageBlit2 blitRegion = {};
        blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        blitRegion.pNext = nullptr;

        blitRegion.srcOffsets[1].x = srcExtent.Width;
        blitRegion.srcOffsets[1].y = srcExtent.Height;
        blitRegion.srcOffsets[1].z = srcExtent.Depth;

        blitRegion.dstOffsets[1].x = dstExtent.Width;
        blitRegion.dstOffsets[1].y = dstExtent.Height;
        blitRegion.dstOffsets[1].z = dstExtent.Depth;

        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;

        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
        blitInfo.dstImage = toVk(dst);
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.srcImage = toVk(src);
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.filter = VK_FILTER_LINEAR;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;

        vkCmdBlitImage2(toVk(m_Handle), &blitInfo);
    }

}