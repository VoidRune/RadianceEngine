#include "ResourceAllocator.h"
#include "VulkanInternal/VulkanUtilities.h"
#include "RadianceEngine/Core/Log.h"
#include <volk/volk.h>
#include <spirv_cross/spirv_cross.hpp>
#include <map>

namespace Rdn
{
	ResourceAllocator::ResourceAllocator(Device* device)
	{
        m_Device = device;
		m_LogicalDevice = device->GetLogicalDevice();
		m_PhysicalDevice = device->GetPhysicalDevice();

        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = toVk(m_PhysicalDevice);
        allocatorInfo.device = toVk(m_LogicalDevice);
        allocatorInfo.instance = toVk(device->GetInstance());
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;

        VmaAllocator allocator;
        VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator));
        m_Allocator = fromVk(allocator);

        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4096 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, 256 },
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 64 },
        };

        VkDescriptorPoolCreateInfo descriptorPoolCI{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        descriptorPoolCI.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descriptorPoolCI.maxSets = 32;
        descriptorPoolCI.poolSizeCount = uint32_t(sizeof(poolSizes) / sizeof(poolSizes[0]));
        descriptorPoolCI.pPoolSizes = poolSizes;

        VkDescriptorPool descriptorPool;
        VK_CHECK(vkCreateDescriptorPool(toVk(m_LogicalDevice), &descriptorPoolCI, nullptr, &descriptorPool));
        m_DescriptorPool = fromVk(descriptorPool);
	}

	ResourceAllocator::~ResourceAllocator()
	{
		FreeResources();
		vkDestroyDescriptorPool(toVk(m_LogicalDevice), toVk(m_DescriptorPool), nullptr);
		vmaDestroyAllocator(toVk(m_Allocator));
	}

    void ResourceAllocator::FreeResources()
    {
        for (auto& [key, layout] : m_DescriptorSetLayouts)
        {
            vkDestroyDescriptorSetLayout(toVk(m_LogicalDevice), toVk(layout), nullptr);
        }
        m_DescriptorSetLayouts.clear();

        for (auto& res : m_Buffers)
            DestroyGpuBuffer(res);
        m_Buffers.clear();
        for (auto& res : m_RingBuffers)
            DestroyGpuRingBuffer(res);
        m_RingBuffers.clear();
        for (auto& res : m_Images)
            DestroyGpuImage(res);
        m_Images.clear();
        for (auto& res : m_Samplers)
            DestroySampler(res);
        m_Samplers.clear();
        for (auto& res : m_Shaders)
            DestroyShader(res);
        m_Shaders.clear();
        for (auto& res : m_BottomLevelASs)
            DestroyBottomLevelAS(res);
        m_BottomLevelASs.clear();
        for (auto& res : m_TopLevelASs)
            DestroyTopLevelAS(res);
        m_TopLevelASs.clear();
        for (auto& res : m_Pipelines)
            DestroyPipeline(res);
        m_Pipelines.clear();
        for (auto& res : m_RaytracingPipelines)
            DestroyRaytracingPipeline(res);
        m_RaytracingPipelines.clear();
        for (auto& res : m_DescriptorSets)
            DestroyDescriptorSet(res);
        m_DescriptorSets.clear();
    }

    void* ResourceAllocator::MapMemory(GpuBuffer* buffer)
    {
        void* data;
        VK_CHECK(vmaMapMemory(toVk(m_Allocator), toVk(buffer->m_Allocation), &data));
        buffer->m_MappedPtr = data;
        return data;
    }

    void ResourceAllocator::UnmapMemory(GpuBuffer* buffer)
    {
        vmaUnmapMemory(toVk(m_Allocator), toVk(buffer->m_Allocation));
        buffer->m_MappedPtr = nullptr;
    }

    void* ResourceAllocator::MapMemory(GpuRingBuffer* ringBuffer, uint32_t elementIndex)
    {
        void* data;
        VK_CHECK(vmaMapMemory(toVk(m_Allocator), toVk(ringBuffer->m_Allocation), &data));
        ringBuffer->m_MappedPtr = data;
        return static_cast<uint8_t*>(data) + elementIndex * ringBuffer->GetElementSize();
    }

    void ResourceAllocator::UnmapMemory(GpuRingBuffer* ringBuffer)
    {
        vmaUnmapMemory(toVk(m_Allocator), toVk(ringBuffer->m_Allocation));
        ringBuffer->m_MappedPtr = nullptr;
    }

    uint64_t ResourceAllocator::GetBufferDeviceAddress(GpuBuffer* buffer)
    {
        VkBufferDeviceAddressInfo addressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        addressInfo.buffer = toVk(buffer->m_Buffer);
        VkDeviceAddress address = vkGetBufferDeviceAddress(toVk(m_LogicalDevice), &addressInfo);
        return address;
    }

    void ResourceAllocator::SetDeviceLocalBufferData(GpuBuffer* buffer, const void* data, uint32_t size)
    {
        GpuBuffer stagingBuffer;
        CreateGpuBuffer(&stagingBuffer, GpuBufferDesc{
            .Size = size,
            .UsageFlags = BufferUsage::TransferSrc,
            .MemoryProperty = MemoryProperty::HostVisible,
        });

        void* dataPtr = MapMemory(&stagingBuffer);
        memcpy(dataPtr, data, size);
        UnmapMemory(&stagingBuffer);

        m_Device->ImmediateSubmit([&](CommandBuffer& cmd) {
            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = size;

            vkCmdCopyBuffer(
                toVk(cmd.GetHandle()),
                toVk(stagingBuffer.GetHandle()),
                toVk(buffer->GetHandle()),
                1,
                &copyRegion
            );
        });

        ReleaseResource(&stagingBuffer);
    }

    void ResourceAllocator::SetImageData(GpuImage* image, const void* data, uint32_t size, ImageLayout newLayout)
    {
        GpuBuffer stagingBuffer;
        CreateGpuBuffer(&stagingBuffer, GpuBufferDesc{
            .Size = size,
            .UsageFlags = BufferUsage::TransferSrc,
            .MemoryProperty = MemoryProperty::HostVisible,
        });

        void* dataPtr = MapMemory(&stagingBuffer);
        memcpy(dataPtr, data, size);
        UnmapMemory(&stagingBuffer);

        m_Device->ImmediateSubmit([&](CommandBuffer& cmd) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = toVk(image->GetHandle());
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = image->GetMipLevels();
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;

            vkCmdPipelineBarrier(toVk(cmd.GetHandle()), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = image->GetImageSize().Width;
            region.imageExtent.height = image->GetImageSize().Height;
            region.imageExtent.depth = image->GetImageSize().Depth;
            vkCmdCopyBufferToImage(toVk(cmd.GetHandle()), toVk(stagingBuffer.GetHandle()), toVk(image->GetHandle()), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            VkImageMemoryBarrier use_barrier = {};
            use_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            use_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            use_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            use_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            use_barrier.newLayout = toVk(newLayout);
            if (image->GetMipLevels() > 1)
                use_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            use_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            use_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            use_barrier.image = toVk(image->GetHandle());
            use_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            use_barrier.subresourceRange.levelCount = 1;
            use_barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(toVk(cmd.GetHandle()), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &use_barrier);
        });

        ReleaseResource(&stagingBuffer);
    }

    static uint32_t GetFormatByteSize(Format format)
    {
        switch (format)
        {
        case Format::R8_Unorm: case Format::R8_Snorm: case Format::R8_Uint: case Format::R8_Sint: case Format::R8_Srgb:
            return 1;
        case Format::R8G8_Unorm: case Format::R8G8_Snorm: case Format::R8G8_Uint: case Format::R8G8_Sint: case Format::R8G8_Srgb:
            return 2;
        case Format::R8G8B8_Unorm: case Format::R8G8B8_Snorm: case Format::R8G8B8_Uint: case Format::R8G8B8_Sint: case Format::R8G8B8_Srgb:
        case Format::B8G8R8_Unorm: case Format::B8G8R8_Snorm: case Format::B8G8R8_Uint: case Format::B8G8R8_Sint: case Format::B8G8R8_Srgb:
            return 3;
        case Format::R8G8B8A8_Unorm: case Format::R8G8B8A8_Snorm: case Format::R8G8B8A8_Uint: case Format::R8G8B8A8_Sint: case Format::R8G8B8A8_Srgb:
        case Format::B8G8R8A8_Unorm: case Format::B8G8R8A8_Snorm: case Format::B8G8R8A8_Uint: case Format::B8G8R8A8_Sint: case Format::B8G8R8A8_Srgb:
            return 4;
        case Format::R16_Unorm: case Format::R16_Snorm: case Format::R16_Uint: case Format::R16_Sint: case Format::R16_Sfloat:
            return 2;
        case Format::R16G16_Unorm: case Format::R16G16_Snorm: case Format::R16G16_Uint: case Format::R16G16_Sint: case Format::R16G16_Sfloat:
            return 4;
        case Format::R16G16B16_Unorm: case Format::R16G16B16_Snorm: case Format::R16G16B16_Uint: case Format::R16G16B16_Sint: case Format::R16G16B16_Sfloat:
            return 6;
        case Format::R16G16B16A16_Unorm: case Format::R16G16B16A16_Snorm: case Format::R16G16B16A16_Uint: case Format::R16G16B16A16_Sint: case Format::R16G16B16A16_Sfloat:
            return 8;
        case Format::R32_Uint: case Format::R32_Sint: case Format::R32_Sfloat:
            return 4;
        case Format::R32G32_Uint: case Format::R32G32_Sint: case Format::R32G32_Sfloat:
            return 8;
        case Format::R32G32B32_Uint: case Format::R32G32B32_Sint: case Format::R32G32B32_Sfloat:
            return 12;
        case Format::R32G32B32A32_Uint: case Format::R32G32B32A32_Sint: case Format::R32G32B32A32_Sfloat:
            return 16;
        case Format::R64_Uint: case Format::R64_Sint: case Format::R64_Sfloat:
            return 8;
        case Format::R64G64_Uint: case Format::R64G64_Sint: case Format::R64G64_Sfloat:
            return 16;
        case Format::R64G64B64_Uint: case Format::R64G64B64_Sint: case Format::R64G64B64_Sfloat:
            return 24;
        case Format::R64G64B64A64_Uint: case Format::R64G64B64A64_Sint: case Format::R64G64B64A64_Sfloat:
            return 32;
        default:
            return 0;
        }
    }

    std::vector<uint8_t> ResourceAllocator::GetImageData(GpuImage* image, ImageLayout currentLayout)
    {
        Extent3D extent = image->GetImageSize();
        uint32_t texelSize = GetFormatByteSize(image->GetFormat());
        if (texelSize == 0)
        {
            RDN_LOG_FATAL("GetImageData: image format is not a supported uncompressed color format (or is unhandled) -- cannot size the readback buffer");
            return {};
        }

        uint32_t size = extent.Width * extent.Height * extent.Depth * texelSize;

        GpuBuffer stagingBuffer;
        CreateGpuBuffer(&stagingBuffer, GpuBufferDesc{
            .Size = size,
            .UsageFlags = BufferUsage::TransferDst,
            .MemoryProperty = MemoryProperty::HostVisible,
        });

        m_Device->ImmediateSubmit([&](CommandBuffer& cmd) {
            cmd.TransitionImage(image->GetHandle(), currentLayout, ImageLayout::TransferSrcOptimal, PipelineStage::AllCommands, PipelineStage::Transfer, ImageAspect::Color);

            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = extent.Width;
            region.imageExtent.height = extent.Height;
            region.imageExtent.depth = extent.Depth;
            vkCmdCopyImageToBuffer(toVk(cmd.GetHandle()), toVk(image->GetHandle()), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, toVk(stagingBuffer.GetHandle()), 1, &region);

            cmd.TransitionImage(image->GetHandle(), ImageLayout::TransferSrcOptimal, currentLayout, PipelineStage::Transfer, PipelineStage::AllCommands, ImageAspect::Color);
        });

        std::vector<uint8_t> result(size);
        void* mappedPtr = MapMemory(&stagingBuffer);
        memcpy(result.data(), mappedPtr, size);
        UnmapMemory(&stagingBuffer);

        ReleaseResource(&stagingBuffer);

        return result;
    }


    void ResourceAllocator::CreateGpuBuffer(GpuBuffer* gpuBuffer, const GpuBufferDesc& desc)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.Size;
        bufferInfo.usage = toVk(desc.UsageFlags);
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.requiredFlags = (VkMemoryPropertyFlags)desc.MemoryProperty;
        if (desc.Mapped)
        {
            allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        if (allocInfo.requiredFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ||
            allocInfo.requiredFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ||
            allocInfo.requiredFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
        {
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }

        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        VK_CHECK(vmaCreateBuffer(toVk(m_Allocator), &bufferInfo, &allocInfo,
            &buffer,
            &allocation,
            &allocationInfo));

        gpuBuffer->m_Buffer = fromVk(buffer);
        gpuBuffer->m_Allocation = fromVk(allocation);
        gpuBuffer->m_Size = desc.Size;
        gpuBuffer->m_MappedPtr = allocationInfo.pMappedData;

        m_Buffers.insert(gpuBuffer);
    }

    void ResourceAllocator::DestroyGpuBuffer(GpuBuffer* gpuBuffer)
    {
        vmaDestroyBuffer(toVk(m_Allocator), toVk(gpuBuffer->m_Buffer), toVk(gpuBuffer->m_Allocation));
    }

    void ResourceAllocator::ReleaseResource(GpuBuffer* gpuBuffer)
    {
        if (!m_Buffers.contains(gpuBuffer))
        {
            RDN_LOG_FATAL("Cannot find GpuBuffer resource to release!");
            return;
        }
        DestroyGpuBuffer(gpuBuffer);
        m_Buffers.erase(gpuBuffer);
    }

    void ResourceAllocator::CreateGpuRingBuffer(GpuRingBuffer* gpuRingBuffer, const GpuRingBufferDesc& desc)
    {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(toVk(m_PhysicalDevice), &physicalDeviceProperties);

        uint32_t alignment = 1;
        if ((uint32_t(desc.UsageFlags) & uint32_t(BufferUsage::UniformBuffer)) != 0)
            alignment = std::max<uint32_t>(alignment, uint32_t(physicalDeviceProperties.limits.minUniformBufferOffsetAlignment));
        if ((uint32_t(desc.UsageFlags) & uint32_t(BufferUsage::StorageBuffer)) != 0)
            alignment = std::max<uint32_t>(alignment, uint32_t(physicalDeviceProperties.limits.minStorageBufferOffsetAlignment));

        uint32_t elementStride = (desc.ElementSize + alignment - 1) & ~(alignment - 1);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.ElementCount * elementStride;
        bufferInfo.usage = toVk(desc.UsageFlags);
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.requiredFlags = (VkMemoryPropertyFlags)desc.MemoryProperty;
        if (desc.Mapped)
        {
            allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        if (allocInfo.requiredFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ||
            allocInfo.requiredFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ||
            allocInfo.requiredFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
        {
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }

        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        VK_CHECK(vmaCreateBuffer(toVk(m_Allocator), &bufferInfo, &allocInfo,
            &buffer,
            &allocation,
            &allocationInfo));

        gpuRingBuffer->m_Buffer = fromVk(buffer);
        gpuRingBuffer->m_Allocation = fromVk(allocation);
        gpuRingBuffer->m_WholeSize = desc.ElementCount * elementStride;
        gpuRingBuffer->m_ElementSize = elementStride;
        gpuRingBuffer->m_ElementCount = desc.ElementCount;
        gpuRingBuffer->m_MappedPtr = allocationInfo.pMappedData;

        m_RingBuffers.insert(gpuRingBuffer);
    }

    void ResourceAllocator::DestroyGpuRingBuffer(GpuRingBuffer* gpuRingBuffer)
    {
        vmaDestroyBuffer(toVk(m_Allocator), toVk(gpuRingBuffer->m_Buffer), toVk(gpuRingBuffer->m_Allocation));
    }

    void ResourceAllocator::ReleaseResource(GpuRingBuffer* gpuRingBuffer)
    {
        if (!m_RingBuffers.contains(gpuRingBuffer))
        {
            RDN_LOG_FATAL("Cannot find GpuRingBuffer resource to release!");
            return;
        }
        DestroyGpuRingBuffer(gpuRingBuffer);
        m_RingBuffers.erase(gpuRingBuffer);
    }

	void ResourceAllocator::CreateGpuImage(GpuImage* gpuImage, const GpuImageDesc& desc)
	{
        VkImageType imageType = VK_IMAGE_TYPE_2D;
        VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D;
        if (desc.ImageSize.Depth > 1)
        {
            imageType = VK_IMAGE_TYPE_3D;
            imageViewType = VK_IMAGE_VIEW_TYPE_3D;
        }

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.pNext = nullptr;
        imageInfo.imageType = imageType;
        imageInfo.format = toVk(desc.Format);
        imageInfo.extent.width = desc.ImageSize.Width;
        imageInfo.extent.height = desc.ImageSize.Height;
        imageInfo.extent.depth = desc.ImageSize.Depth;
        imageInfo.mipLevels = desc.MipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = toVk(desc.UsageFlags);
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.flags = 0; // Optional

        if (desc.MipLevels > 1)
            imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkImage image;
        VmaAllocation allocation;
        VK_CHECK(vmaCreateImage(toVk(m_Allocator), &imageInfo, &allocInfo, &image, &allocation, nullptr));
        gpuImage->m_Image = fromVk(image);
        gpuImage->m_Allocation = fromVk(allocation);

        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = image;
        imageViewInfo.viewType = imageViewType;
        imageViewInfo.format = toVk(desc.Format);
        imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.subresourceRange.aspectMask = toVk(desc.AspectFlags);
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = desc.MipLevels;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        VK_CHECK(vkCreateImageView(toVk(m_LogicalDevice), &imageViewInfo, nullptr, &imageView));
        gpuImage->m_ImageView = fromVk(imageView);
        gpuImage->m_Format = desc.Format;
        gpuImage->m_ImageSize = desc.ImageSize;
        gpuImage->m_MipLevels = desc.MipLevels;

        m_Images.insert(gpuImage);
	}

    void ResourceAllocator::DestroyGpuImage(GpuImage* gpuImage)
    {
        vmaDestroyImage(toVk(m_Allocator), toVk(gpuImage->m_Image), toVk(gpuImage->m_Allocation));
        vkDestroyImageView(toVk(m_LogicalDevice), toVk(gpuImage->m_ImageView), nullptr);
    }

    void ResourceAllocator::ReleaseResource(GpuImage* gpuImage)
    {
        if (!m_Images.contains(gpuImage))
        {
            RDN_LOG_FATAL("Cannot find GpuImage resource to release!");
            return;
        }
        DestroyGpuImage(gpuImage);
        m_Images.erase(gpuImage);
    }

    void ResourceAllocator::CreateSampler(Sampler* sampler, const SamplerDesc& desc)
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = toVk(desc.MagFilter);
        samplerInfo.minFilter = toVk(desc.MinFilter);
        samplerInfo.addressModeU = toVk(desc.AddressMode);
        samplerInfo.addressModeV = samplerInfo.addressModeU;
        samplerInfo.addressModeW = samplerInfo.addressModeU;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;

        VkSampler samplerEx;
        VK_CHECK(vkCreateSampler(toVk(m_LogicalDevice), &samplerInfo, nullptr, &samplerEx));
        sampler->m_Sampler = fromVk(samplerEx);

        m_Samplers.insert(sampler);
    }

    void ResourceAllocator::DestroySampler(Sampler* sampler)
    {
        vkDestroySampler(toVk(m_LogicalDevice), toVk(sampler->m_Sampler), nullptr);
    }

    void ResourceAllocator::ReleaseResource(Sampler* sampler)
    {
        if (!m_Samplers.contains(sampler))
        {
            RDN_LOG_FATAL("Cannot find Sampler resource to release!");
            return;
        }
        DestroySampler(sampler);
        m_Samplers.erase(sampler);
    }

    void ResourceAllocator::CreateShader(Shader* shader, const ShaderDesc& desc)
    {
        if (desc.SpirV.empty())
        {
            RDN_LOG_ERROR("CreateShader: SpirV is empty");
            return;
        }

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = desc.SpirV.size() * sizeof(uint32_t);
        createInfo.pCode = reinterpret_cast<const uint32_t*>(desc.SpirV.data());

        VkShaderModule vkModule;
        VK_CHECK(vkCreateShaderModule(toVk(m_LogicalDevice), &createInfo, nullptr, &vkModule));
        shader->m_Module = fromVk(vkModule);
        shader->m_EntryPoint = desc.EntryPoint;
        shader->m_ShaderStage = desc.ShaderStage;

        spirv_cross::Compiler comp(desc.SpirV);
        spirv_cross::ShaderResources res = comp.get_shader_resources();


        static constexpr uint32_t kBindlessThreshold = 16;
        static constexpr uint32_t kRuntimeArrayCount = 0;


        auto reflectList = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& list,
            DescriptorType type)
            {
                for (const auto& resource : list)
                {
                    uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
                    uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);

                    const spirv_cross::SPIRType& arrayType = comp.get_type(resource.type_id);

                    uint32_t count = 1;
                    bool isRuntimeArray = false;

                    if (!arrayType.array.empty())
                    {
                        count = arrayType.array[0]; // 0 = runtime array in spirv-cross
                        isRuntimeArray = (count == 0);
                    }

                    bool isBindless = isRuntimeArray || count >= kBindlessThreshold;

                    Shader::DescriptorLayoutBinding b;
                    b.SetIndex = set;
                    b.Binding = binding;
                    b.DescriptorCount = isRuntimeArray ? kRuntimeArrayCount : count;
                    b.Type = type;
                    b.IsBindless = isBindless;
                    b.Stage = desc.ShaderStage;
                    //b.Name = resource.name;
                    shader->m_LayoutBindings.push_back(b);
                }
            };

        reflectList(res.sampled_images, DescriptorType::CombinedImageSampler);
        reflectList(res.separate_images, DescriptorType::SampledImage);
        reflectList(res.storage_images, DescriptorType::StorageImage);
        reflectList(res.uniform_buffers, DescriptorType::UniformBuffer);
        reflectList(res.storage_buffers, DescriptorType::StorageBuffer);
        reflectList(res.separate_samplers, DescriptorType::Sampler);
        reflectList(res.acceleration_structures, DescriptorType::AccelerationStructure);

        shader->m_PushConstantSize = 0;
        for (const auto& pc : res.push_constant_buffers)
        {
            const auto& bufferType = comp.get_type(pc.base_type_id);
            uint32_t size = uint32_t(comp.get_declared_struct_size(bufferType));
            shader->m_PushConstantSize = std::max(shader->m_PushConstantSize, size);
        }

        shader->m_StageOutputs = uint32_t(res.stage_outputs.size());

        m_Shaders.insert(shader);
    }

    void ResourceAllocator::DestroyShader(Shader* shader)
    {
        vkDestroyShaderModule(toVk(m_LogicalDevice), toVk(shader->m_Module), nullptr);
    }

    void ResourceAllocator::ReleaseResource(Shader* shader)
    {
        if (!m_Shaders.contains(shader))
        {
            RDN_LOG_FATAL("Cannot find Shader resource to release!");
            return;
        }
        DestroyShader(shader);
        m_Shaders.erase(shader);
    }

    void ResourceAllocator::CreateBottomLevelAS(BottomLevelAS* bottomLevelAS, const BottomLevelASDesc& desc)
    {
        VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
        VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};

        VkBufferDeviceAddressInfo addrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        addrInfo.buffer = toVk(desc.VertexBuffer);
        vertexBufferDeviceAddress.deviceAddress = vkGetBufferDeviceAddress(toVk(m_LogicalDevice), &addrInfo);
        addrInfo.buffer = toVk(desc.IndexBuffer);
        indexBufferDeviceAddress.deviceAddress = vkGetBufferDeviceAddress(toVk(m_LogicalDevice), &addrInfo);

        //Build
        VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        accelerationStructureGeometry.geometry.triangles.vertexFormat = toVk(desc.VertexFormat);
        accelerationStructureGeometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.maxVertex = desc.VertexCount > 0 ? desc.VertexCount - 1 : 0;
        accelerationStructureGeometry.geometry.triangles.vertexStride = desc.VertexStride;
        accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        accelerationStructureGeometry.geometry.triangles.indexData = indexBufferDeviceAddress;

        //Get size info
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
        accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

        const uint32_t numTriangles = desc.NumTriangles;
        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            toVk(m_LogicalDevice),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &numTriangles,
            &accelerationStructureBuildSizesInfo);


        VkBuffer blasBuffer;
        VmaAllocation blasAllocation;
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
            bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.requiredFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

            VK_CHECK(vmaCreateBuffer(toVk(m_Allocator), &bufferInfo, &allocInfo,
                &blasBuffer,
                &blasAllocation,
                nullptr));
        }

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
        accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = blasBuffer;
        accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        VkAccelerationStructureKHR blasHandle;
        VK_CHECK(vkCreateAccelerationStructureKHR(toVk(m_LogicalDevice), &accelerationStructureCreateInfo, nullptr, &blasHandle));

        VkBuffer scratchBuffer;
        VmaAllocation scratchBufferAllocation;
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = accelerationStructureBuildSizesInfo.buildScratchSize;
            bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            VK_CHECK(vmaCreateBuffer(toVk(m_Allocator), &bufferInfo, &allocInfo,
                &scratchBuffer,
                &scratchBufferAllocation,
                nullptr));
        }
        addrInfo.buffer = scratchBuffer;
        VkDeviceAddress scratchBufferDeviceAddress = vkGetBufferDeviceAddress(toVk(m_LogicalDevice), &addrInfo);

        VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
        accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationBuildGeometryInfo.dstAccelerationStructure = blasHandle;
        accelerationBuildGeometryInfo.geometryCount = 1;
        accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
        accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBufferDeviceAddress;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
        accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

        m_Device->ImmediateSubmit([=](CommandBuffer& cmd) {
            vkCmdBuildAccelerationStructuresKHR(
                toVk(cmd.GetHandle()),
                1,
                &accelerationBuildGeometryInfo,
                accelerationBuildStructureRangeInfos.data());
            });

        vmaDestroyBuffer(toVk(m_Allocator), scratchBuffer, scratchBufferAllocation);

        VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
        accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationDeviceAddressInfo.accelerationStructure = blasHandle;
        VkDeviceAddress blasDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(toVk(m_LogicalDevice), &accelerationDeviceAddressInfo);
        bottomLevelAS->m_Handle = fromVk(blasHandle);
        bottomLevelAS->m_Buffer = fromVk(blasBuffer);
        bottomLevelAS->m_Allocation = fromVk(blasAllocation);
        bottomLevelAS->m_DeviceAddress = blasDeviceAddress;

        m_BottomLevelASs.insert(bottomLevelAS);
    }

    void ResourceAllocator::DestroyBottomLevelAS(BottomLevelAS* bottomLevelAS)
    {
        vkDestroyAccelerationStructureKHR(toVk(m_LogicalDevice), toVk(bottomLevelAS->m_Handle), nullptr);
        vmaDestroyBuffer(toVk(m_Allocator), toVk(bottomLevelAS->m_Buffer), toVk(bottomLevelAS->m_Allocation));
    }

    void ResourceAllocator::ReleaseResource(BottomLevelAS* bottomLevelAS)
    {
        if (!m_BottomLevelASs.contains(bottomLevelAS))
        {
            RDN_LOG_FATAL("Cannot find BottomLevelAS resource to release!");
            return;
        }

        DestroyBottomLevelAS(bottomLevelAS);
        m_BottomLevelASs.erase(bottomLevelAS);
    }

    void ResourceAllocator::BuildTopLevelAS(TopLevelAS* topLevelAS)
    {
        DestroyTopLevelAS(topLevelAS);

        auto instances = topLevelAS->m_Instances;

        std::vector<VkAccelerationStructureInstanceKHR> vkInstances;
        std::vector<VkTransformMatrixKHR> matrices;
        vkInstances.resize(instances.size());
        matrices.resize(instances.size());
        for (size_t i = 0; i < instances.size(); i++)
        {
            VkTransformMatrixKHR& transformMatrix = matrices[i];
            memcpy(transformMatrix.matrix, instances[i].TransformMatrix, sizeof(float) * 12);

            VkAccelerationStructureInstanceKHR inst{};
            inst.transform = transformMatrix;
            inst.instanceCustomIndex = instances[i].InstanceCustomIndex;
            inst.mask = 0xFF;
            inst.instanceShaderBindingTableRecordOffset = 0;
            inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            inst.accelerationStructureReference = instances[i].BottomLevelASHandle;
            vkInstances[i] = inst;
        }

        VkBuffer instanceBuffer;
        VmaAllocation instanceBufferAllocation;
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(VkAccelerationStructureInstanceKHR) * vkInstances.size();
            bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

            VK_CHECK(vmaCreateBuffer(toVk(m_Allocator), &bufferInfo, &allocInfo,
                &instanceBuffer,
                &instanceBufferAllocation,
                nullptr));

            void* data;
            VK_CHECK(vmaMapMemory(toVk(m_Allocator), instanceBufferAllocation, &data));
            memcpy(data, vkInstances.data(), vkInstances.size() * sizeof(VkAccelerationStructureInstanceKHR));
            vmaUnmapMemory(toVk(m_Allocator), instanceBufferAllocation);
        }

        topLevelAS->m_InstanceBuffer = fromVk(instanceBuffer);
        topLevelAS->m_InstanceAllocation = fromVk(instanceBufferAllocation);

        VkBufferDeviceAddressInfo addrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        addrInfo.buffer = instanceBuffer;
        VkDeviceAddress instanceBufferDeviceAddress = vkGetBufferDeviceAddress(toVk(m_LogicalDevice), &addrInfo);

        VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
        instanceDataDeviceAddress.deviceAddress = instanceBufferDeviceAddress;

        VkAccelerationStructureGeometryKHR accelerationStructure2Geometry{};
        accelerationStructure2Geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructure2Geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        accelerationStructure2Geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructure2Geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        accelerationStructure2Geometry.geometry.instances.arrayOfPointers = VK_FALSE;
        accelerationStructure2Geometry.geometry.instances.data = instanceDataDeviceAddress;

        // Get size info
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuild2GeometryInfo{};
        accelerationStructureBuild2GeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuild2GeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationStructureBuild2GeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuild2GeometryInfo.geometryCount = 1;
        accelerationStructureBuild2GeometryInfo.pGeometries = &accelerationStructure2Geometry;

        uint32_t primitive_count = static_cast<uint32_t>(vkInstances.size());

        VkAccelerationStructureBuildSizesInfoKHR tlasaccelerationStructureBuildSizesInfo{};
        tlasaccelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            toVk(m_LogicalDevice),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuild2GeometryInfo,
            &primitive_count,
            &tlasaccelerationStructureBuildSizesInfo);

        VkBuffer tlasBuffer;
        VmaAllocation tlasAllocation;
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = tlasaccelerationStructureBuildSizesInfo.accelerationStructureSize;
            bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.requiredFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

            VK_CHECK(vmaCreateBuffer(toVk(m_Allocator), &bufferInfo, &allocInfo,
                &tlasBuffer,
                &tlasAllocation,
                nullptr));
        }


        VkAccelerationStructureCreateInfoKHR tlasaccelerationStructureCreateInfo{};
        tlasaccelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        tlasaccelerationStructureCreateInfo.buffer = tlasBuffer;
        tlasaccelerationStructureCreateInfo.size = tlasaccelerationStructureBuildSizesInfo.accelerationStructureSize;
        tlasaccelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        VkAccelerationStructureKHR tlas;
        VK_CHECK(vkCreateAccelerationStructureKHR(toVk(m_LogicalDevice), &tlasaccelerationStructureCreateInfo, nullptr, &tlas));


        VkBuffer scratchBuffer2;
        VmaAllocation scratchBuffer2Allocation;
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = tlasaccelerationStructureBuildSizesInfo.buildScratchSize;
            bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            VK_CHECK(vmaCreateBuffer(toVk(m_Allocator), &bufferInfo, &allocInfo,
                &scratchBuffer2,
                &scratchBuffer2Allocation,
                nullptr));
        }
        addrInfo.buffer = scratchBuffer2;
        VkDeviceAddress scratchBuffer2DeviceAddress = vkGetBufferDeviceAddress(toVk(m_LogicalDevice), &addrInfo);


        VkAccelerationStructureBuildGeometryInfoKHR accelerationBuild2GeometryInfo{};
        accelerationBuild2GeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationBuild2GeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationBuild2GeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationBuild2GeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationBuild2GeometryInfo.dstAccelerationStructure = tlas;
        accelerationBuild2GeometryInfo.geometryCount = 1;
        accelerationBuild2GeometryInfo.pGeometries = &accelerationStructure2Geometry;
        accelerationBuild2GeometryInfo.scratchData.deviceAddress = scratchBuffer2DeviceAddress;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuild2RangeInfo{};
        accelerationStructureBuild2RangeInfo.primitiveCount = primitive_count;
        accelerationStructureBuild2RangeInfo.primitiveOffset = 0;
        accelerationStructureBuild2RangeInfo.firstVertex = 0;
        accelerationStructureBuild2RangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuild2StructureRangeInfos = { &accelerationStructureBuild2RangeInfo };

        m_Device->ImmediateSubmit([=](CommandBuffer& cmd) {
            vkCmdBuildAccelerationStructuresKHR(
                toVk(cmd.GetHandle()),
                1,
                &accelerationBuild2GeometryInfo,
                accelerationBuild2StructureRangeInfos.data());
            });

        vmaDestroyBuffer(toVk(m_Allocator), scratchBuffer2, scratchBuffer2Allocation);

        topLevelAS->m_Handle = fromVk(tlas);
        topLevelAS->m_Buffer = fromVk(tlasBuffer);
        topLevelAS->m_Allocation = fromVk(tlasAllocation);
    }

    void ResourceAllocator::CreateTopLevelAS(TopLevelAS* topLevelAS)
    {
        topLevelAS->m_Device = m_Device;
        topLevelAS->m_LogicalDevice = m_LogicalDevice;
        topLevelAS->m_Allocator = m_Allocator;

        m_TopLevelASs.insert(topLevelAS);
    }

    void ResourceAllocator::DestroyTopLevelAS(TopLevelAS* topLevelAS)
    {
        if (topLevelAS->m_Handle.Valid()) vkDestroyAccelerationStructureKHR(toVk(m_LogicalDevice), toVk(topLevelAS->m_Handle), nullptr);
        if (topLevelAS->m_Buffer.Valid()) vmaDestroyBuffer(toVk(m_Allocator), toVk(topLevelAS->m_Buffer), toVk(topLevelAS->m_Allocation));
        if (topLevelAS->m_InstanceBuffer.Valid()) vmaDestroyBuffer(toVk(m_Allocator), toVk(topLevelAS->m_InstanceBuffer), toVk(topLevelAS->m_InstanceAllocation));
    }

    void ResourceAllocator::ReleaseResource(TopLevelAS* topLevelAS)
    {
        if (!m_TopLevelASs.contains(topLevelAS))
        {
            RDN_LOG_FATAL("Cannot find TopLevelAS resource to release!");
            return;
        }
        DestroyTopLevelAS(topLevelAS);
        m_TopLevelASs.erase(topLevelAS);
    }

    DescriptorSetLayoutHandle ResourceAllocator::GetOrCreateDescriptorSetLayout(const DescriptorSetLayoutKey& key)
    {
        auto it = m_DescriptorSetLayouts.find(key);
        if (it != m_DescriptorSetLayouts.end())
            return it->second;

        std::vector<VkDescriptorSetLayoutBinding> vkBindings;
        std::vector<VkDescriptorBindingFlags> bindingFlags;
        vkBindings.reserve(key.Bindings.size());
        bindingFlags.reserve(key.Bindings.size());

        for (const auto& b : key.Bindings)
        {
            VkDescriptorSetLayoutBinding vkb{};
            vkb.binding = b.Binding;
            vkb.descriptorType = toVk(b.Type);
            vkb.descriptorCount = (b.Count == 0) ? MaxBindlessDescriptors : b.Count;
            vkb.stageFlags = toVk(b.Stages);
            vkBindings.push_back(vkb);

            // Per-binding flags
            VkDescriptorBindingFlags flags = 0;
            if (b.IsBindless) {
                flags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                    | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

                if (&b == &key.Bindings.back())
                    flags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            }
            else {
                flags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            }
            bindingFlags.push_back(flags);
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsCI{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO
        };
        flagsCI.bindingCount = uint32_t(bindingFlags.size());
        flagsCI.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo layoutCI{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
        };
        layoutCI.pNext = &flagsCI;
        layoutCI.bindingCount = uint32_t(vkBindings.size());
        layoutCI.pBindings = vkBindings.data();

        if (key.UsePushDescriptors)
            layoutCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        else if (key.HasBindless)
            layoutCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

        VkDescriptorSetLayout vkLayout;
        VK_CHECK(vkCreateDescriptorSetLayout(toVk(m_LogicalDevice), &layoutCI, nullptr, &vkLayout));
        auto layout = fromVk(vkLayout);
        m_DescriptorSetLayouts[key] = layout;
        return layout;
    }

    void ResourceAllocator::CreatePipeline(Pipeline* pipeline, const PipelineDesc& desc)
    {
        std::map<uint32_t, std::map<uint32_t, Shader::DescriptorLayoutBinding>> bindings;

        uint32_t pushConstantSize = 0;
        uint32_t colorAttachCount = 0;

        for (const Shader* shader : desc.ShaderStages)
        {
            for (const auto& b : shader->m_LayoutBindings)
            {
                auto& set = bindings[b.SetIndex];
                if (!set.contains(b.Binding))
                {
                    set[b.Binding] = b;
                }
                else
                {
                    set[b.Binding].Stage |= shader->m_ShaderStage;
                }
            }

            pushConstantSize = std::max(pushConstantSize, shader->m_PushConstantSize);

            if (shader->m_ShaderStage == ShaderStage::Fragment)
                colorAttachCount = shader->m_StageOutputs;
        }

        uint32_t maxSetIndex = 0;
        for (const auto& [setIdx, _] : bindings)
            maxSetIndex = std::max(maxSetIndex, setIdx);

        std::vector<VkDescriptorSetLayout> layouts;
        layouts.reserve(maxSetIndex + 1);

        for (uint32_t setIdx = 0; setIdx <= maxSetIndex; setIdx++)
        {
            auto setIt = bindings.find(setIdx);
            if (setIt == bindings.end())
            {
                DescriptorSetLayoutKey emptyKey{};
                layouts.push_back(toVk(GetOrCreateDescriptorSetLayout(emptyKey)));
                continue;
            }

            DescriptorSetLayoutKey key;

            key.UsePushDescriptors = false;
            for (const uint32_t& pd : desc.PushDescriptorSets)
            {
                if (pd == setIdx)
                {
                    key.UsePushDescriptors = true;
                    break;
                }
            }

            for (const auto& [bindIdx, b] : setIt->second)
            {
                DescriptorSetLayoutKey::BindingKey bk;
                bk.Binding = b.Binding;
                bk.Count = b.DescriptorCount;
                bk.Type = b.Type;
                bk.Stages = b.Stage;
                bk.IsBindless = b.IsBindless;
                key.Bindings.push_back(bk);

                if (b.IsBindless)
                    key.HasBindless = true;
            }

            std::sort(key.Bindings.begin(), key.Bindings.end(), [](const auto& a, const auto& b) { return a.Binding < b.Binding; });

            layouts.push_back(toVk(GetOrCreateDescriptorSetLayout(key)));
        }

        // PUSH CONSTANT RANGE

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = pushConstantSize;

        // PIPELINE LAYOUT

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipelineLayoutInfo.pSetLayouts = layouts.data();
        if (pushConstantSize > 0)
        {
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        }

        VkPipelineLayout pipelineLayout;
        VK_CHECK(vkCreatePipelineLayout(toVk(m_LogicalDevice), &pipelineLayoutInfo, nullptr, &pipelineLayout));
        pipeline->m_PipelineLayout = fromVk(pipelineLayout);

        pipeline->m_SetLayouts.resize(layouts.size());
        for (size_t i = 0; i < layouts.size(); i++)
            pipeline->m_SetLayouts[i] = fromVk(layouts[i]);

        // SHADER STAGES

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        shaderStages.reserve(desc.ShaderStages.size());
        for (const Shader* shader : desc.ShaderStages)
        {
            VkPipelineShaderStageCreateInfo stageCI{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            stageCI.stage = toVk(shader->m_ShaderStage);
            stageCI.module = toVk(shader->m_Module);
            stageCI.pName = shader->m_EntryPoint.c_str();
            shaderStages.push_back(stageCI);
        }

        // VERTEX INPUT

        std::vector<VkVertexInputAttributeDescription> vertexAttribs;
        std::vector<VkVertexInputBindingDescription>   vertexBindings;

        for (uint32_t i = 0; i < uint32_t(desc.VertexAttributes.m_Attributes.size()); i++)
        {
            const auto& attr = desc.VertexAttributes.m_Attributes[i];
            vertexAttribs.push_back({ i, 0, toVk(attr.format), attr.offset });
        }
        if (desc.VertexAttributes.m_VertexSize > 0)
            vertexBindings.push_back({ 0, desc.VertexAttributes.m_VertexSize, VK_VERTEX_INPUT_RATE_VERTEX });

        VkPipelineVertexInputStateCreateInfo vertexInputCI{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInputCI.vertexAttributeDescriptionCount = uint32_t(vertexAttribs.size());
        vertexInputCI.pVertexAttributeDescriptions = vertexAttribs.data();
        vertexInputCI.vertexBindingDescriptionCount = uint32_t(vertexBindings.size());
        vertexInputCI.pVertexBindingDescriptions = vertexBindings.data();

        // FIXED FUNCTION STATE

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssemblyCI.topology = toVk(desc.Topology);
        inputAssemblyCI.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportCI{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportCI.viewportCount = 1;
        viewportCI.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterCI{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterCI.polygonMode = VK_POLYGON_MODE_FILL;
        rasterCI.lineWidth = 1.0f;
        rasterCI.cullMode = toVk(desc.CullMode);
        rasterCI.frontFace = VK_FRONT_FACE_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo msCI{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        msCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        msCI.minSampleShading = 1.0f;

        VkPipelineDepthStencilStateCreateInfo depthCI{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthCI.depthTestEnable = desc.DepthTest ? VK_TRUE : VK_FALSE;
        depthCI.depthWriteEnable = desc.DepthWrite ? VK_TRUE : VK_FALSE;
        depthCI.depthCompareOp = toVk(desc.DepthCompareOp);
        depthCI.maxDepthBounds = 1.0f;

        // COLOR BLEND

        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
        blendAttachments.reserve(colorAttachCount);

        for (uint32_t i = 0; i < colorAttachCount; i++)
        {
            VkPipelineColorBlendAttachmentState att{};
            att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            att.blendEnable = VK_FALSE;
            att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            att.colorBlendOp = VK_BLEND_OP_ADD;
            att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            att.alphaBlendOp = VK_BLEND_OP_ADD;
            blendAttachments.push_back(att);
        }

        VkPipelineColorBlendStateCreateInfo blendCI{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        blendCI.attachmentCount = uint32_t(blendAttachments.size());
        blendCI.pAttachments = blendAttachments.data();

        // DYNAMIC STATE

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicCI{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicCI.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicCI.pDynamicStates = dynamicStates.data();

        // DYNAMIC RENDERING

        std::vector<VkFormat> colorFormats;
        colorFormats.reserve(desc.ColorAttachmentFormats.size());
        for (Format f : desc.ColorAttachmentFormats)
            colorFormats.push_back(toVk(f));

        VkPipelineRenderingCreateInfo renderingCI{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        renderingCI.colorAttachmentCount = uint32_t(colorFormats.size());
        renderingCI.pColorAttachmentFormats = colorFormats.data();
        renderingCI.depthAttachmentFormat = toVk(desc.DepthAttachmentFormat);

        // ASSEMBLE

        VkGraphicsPipelineCreateInfo pipelineCI{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineCI.pNext = &renderingCI;
        pipelineCI.stageCount = uint32_t(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();
        pipelineCI.pVertexInputState = &vertexInputCI;
        pipelineCI.pInputAssemblyState = &inputAssemblyCI;
        pipelineCI.pViewportState = &viewportCI;
        pipelineCI.pRasterizationState = &rasterCI;
        pipelineCI.pMultisampleState = &msCI;
        pipelineCI.pDepthStencilState = &depthCI;
        pipelineCI.pColorBlendState = &blendCI;
        pipelineCI.pDynamicState = &dynamicCI;
        pipelineCI.layout = toVk(pipeline->m_PipelineLayout);
        pipelineCI.basePipelineIndex = -1;

        VkPipeline vkPipeline;
        VK_CHECK(vkCreateGraphicsPipelines(toVk(m_LogicalDevice), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &vkPipeline));
        pipeline->m_Pipeline = fromVk(vkPipeline);

        m_Pipelines.insert(pipeline);
    }

    void ResourceAllocator::DestroyPipeline(Pipeline* pipeline)
    {
        vkDestroyPipelineLayout(toVk(m_LogicalDevice), toVk(pipeline->m_PipelineLayout), nullptr);
        vkDestroyPipeline(toVk(m_LogicalDevice), toVk(pipeline->m_Pipeline), nullptr);
    }

    void ResourceAllocator::ReleaseResource(Pipeline* pipeline)
    {
        if (!m_Pipelines.contains(pipeline))
        {
            RDN_LOG_FATAL("Cannot find Pipeline resource to release!");
            return;
        }
        DestroyPipeline(pipeline);
        m_Pipelines.erase(pipeline);
    }

    static Shader* GetShaderWithStage(ShaderStage stage, const std::vector<Shader*>& shaders)
    {
        for (auto& s : shaders)
        {
            if (s->GetShaderStage() == stage)
            {
                return s;
            }
        }
        return nullptr;
    }

    static inline uint32_t align_u32(uint32_t v, uint32_t a) { return a == 0 ? v : (v + a - 1) & ~(a - 1); }
    static inline uint64_t align_u64(uint64_t v, uint64_t a) { return a == 0 ? v : (v + a - 1) & ~(a - 1); }

    void ResourceAllocator::CreateRaytracingPipeline(RayTracingPipeline* raytracingPipeline, const RayTracingPipelineDesc& desc)
    {
        std::map<uint32_t, std::map<uint32_t, Shader::DescriptorLayoutBinding>> bindings;

        uint32_t pushConstantSize = 0;

        for (const Shader* shader : desc.ShaderStages)
        {
            for (const auto& b : shader->m_LayoutBindings)
            {
                auto& set = bindings[b.SetIndex];
                if (!set.contains(b.Binding))
                {
                    set[b.Binding] = b;
                }
                else
                {
                    set[b.Binding].Stage |= shader->m_ShaderStage;
                }
            }

            pushConstantSize = std::max(pushConstantSize, shader->m_PushConstantSize);
        }

        uint32_t maxSetIndex = 0;
        for (const auto& [setIdx, _] : bindings)
            maxSetIndex = std::max(maxSetIndex, setIdx);

        std::vector<VkDescriptorSetLayout> layouts;
        layouts.reserve(maxSetIndex + 1);

        for (uint32_t setIdx = 0; setIdx <= maxSetIndex; setIdx++)
        {
            auto setIt = bindings.find(setIdx);
            if (setIt == bindings.end())
            {
                DescriptorSetLayoutKey emptyKey{};
                layouts.push_back(toVk(GetOrCreateDescriptorSetLayout(emptyKey)));
                continue;
            }

            DescriptorSetLayoutKey key;

            key.UsePushDescriptors = false;
            for (const uint32_t& pd : desc.PushDescriptorSets)
            {
                if (pd == setIdx)
                {
                    key.UsePushDescriptors = true;
                    break;
                }
            }

            for (const auto& [bindIdx, b] : setIt->second)
            {
                DescriptorSetLayoutKey::BindingKey bk;
                bk.Binding = b.Binding;
                bk.Count = b.DescriptorCount;
                bk.Type = b.Type;
                bk.Stages = b.Stage;
                bk.IsBindless = b.IsBindless;
                key.Bindings.push_back(bk);

                if (b.IsBindless)
                    key.HasBindless = true;
            }

            std::sort(key.Bindings.begin(), key.Bindings.end(), [](const auto& a, const auto& b) { return a.Binding < b.Binding; });

            layouts.push_back(toVk(GetOrCreateDescriptorSetLayout(key)));
        }

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
        pushConstantRange.offset = 0;
        pushConstantRange.size = pushConstantSize;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipelineLayoutInfo.pSetLayouts = layouts.data();
        if (pushConstantSize > 0)
        {
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        }

        VkPipelineLayout pipelineLayout;
        VK_CHECK(vkCreatePipelineLayout(toVk(m_LogicalDevice), &pipelineLayoutInfo, nullptr, &pipelineLayout));

        raytracingPipeline->m_SetLayouts.resize(layouts.size());
        for (size_t i = 0; i < layouts.size(); i++)
            raytracingPipeline->m_SetLayouts[i] = fromVk(layouts[i]);

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

        // Ray generation group
        {
            Shader* shader = GetShaderWithStage(ShaderStage::RayGen, desc.ShaderStages);
            if (shader == nullptr)
            {
                RDN_LOG_ERROR("RayTracingPipelineDesc does not contain shader with RayGen shaders stage!");
                vkDestroyPipelineLayout(toVk(m_LogicalDevice), pipelineLayout, nullptr);
                return;
            }
            VkPipelineShaderStageCreateInfo stage{};
            stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage.flags = 0;
            stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            stage.module = toVk(shader->m_Module);
            stage.pName = shader->m_EntryPoint.c_str();
            stage.pSpecializationInfo = nullptr;
            shaderStages.push_back(stage);

            VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
            shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
            shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
            shaderGroups.push_back(shaderGroup);
        }

        // Miss group
        {
            Shader* shader = GetShaderWithStage(ShaderStage::RayMiss, desc.ShaderStages);
            if (shader == nullptr)
            {
                RDN_LOG_ERROR("RayTracingPipelineDesc does not contain shader with RayMiss shaders stage!");
                vkDestroyPipelineLayout(toVk(m_LogicalDevice), pipelineLayout, nullptr);
                return;
            }
            VkPipelineShaderStageCreateInfo stage{};
            stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage.flags = 0;
            stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
            stage.module = toVk(shader->m_Module);
            stage.pName = shader->m_EntryPoint.c_str();
            stage.pSpecializationInfo = nullptr;
            shaderStages.push_back(stage);

            VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
            shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
            shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
            shaderGroups.push_back(shaderGroup);
        }

        // Closest hit group
        {
            Shader* shader = GetShaderWithStage(ShaderStage::RayClosestHit, desc.ShaderStages);
            if (shader == nullptr)
            {
                RDN_LOG_ERROR("RayTracingPipelineDesc does not contain shader with RayClosestHit shaders stage!");
                vkDestroyPipelineLayout(toVk(m_LogicalDevice), pipelineLayout, nullptr);
                return;
            }
            VkPipelineShaderStageCreateInfo stage{};
            stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage.flags = 0;
            stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            stage.module = toVk(shader->m_Module);
            stage.pName = shader->m_EntryPoint.c_str();
            stage.pSpecializationInfo = nullptr;
            shaderStages.push_back(stage);

            VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
            shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
            shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
            shaderGroups.push_back(shaderGroup);
        }

        VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
        rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        rayTracingPipelineCI.pStages = shaderStages.data();
        rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
        rayTracingPipelineCI.pGroups = shaderGroups.data();
        rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
        rayTracingPipelineCI.layout = pipelineLayout;

        VkPipeline rtPipeline;
        VK_CHECK(vkCreateRayTracingPipelinesKHR(toVk(m_LogicalDevice), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &rtPipeline));


        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
        VkPhysicalDeviceProperties2 pdprops{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        pdprops.pNext = &rayProps;
        vkGetPhysicalDeviceProperties2(toVk(m_PhysicalDevice), &pdprops);
        uint32_t handleSize = rayProps.shaderGroupHandleSize;
        uint32_t handleAlign = rayProps.shaderGroupHandleAlignment;
        uint32_t baseAlign = rayProps.shaderGroupBaseAlignment;

        uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
        std::vector<uint8_t> handles(groupCount * handleSize);
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(toVk(m_LogicalDevice), rtPipeline, 0, groupCount, handles.size(), handles.data()));

        uint64_t sbtRecordSize = align_u64(handleSize, handleAlign);

        uint64_t raygenRegionSize = align_u64(sbtRecordSize * 1, baseAlign);
        uint64_t missRegionOffset = raygenRegionSize;
        uint64_t missRegionSize = align_u64(sbtRecordSize * 1, baseAlign);
        uint64_t hitRegionOffset = missRegionOffset + missRegionSize;
        uint64_t hitRegionSize = align_u64(sbtRecordSize * 1, baseAlign);

        uint64_t sbtSize = hitRegionOffset + hitRegionSize;

        if (handleSize == 0 || sbtSize == 0)
        {
            RDN_LOG_FATAL("CreateRaytracingPipeline: shaderGroupHandleSize/alignment queried as 0 — physical device did not report valid ray tracing pipeline properties, cannot size the shader binding table!");
            vkDestroyPipeline(toVk(m_LogicalDevice), rtPipeline, nullptr);
            vkDestroyPipelineLayout(toVk(m_LogicalDevice), pipelineLayout, nullptr);
            return;
        }

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sbtSize;
        bufferInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VkBuffer buffer;
        VmaAllocation allocation;
        VK_CHECK(vmaCreateBuffer(toVk(m_Allocator), &bufferInfo, &allocInfo,
            &buffer,
            &allocation,
            nullptr));


        void* data;
        VK_CHECK(vmaMapMemory(toVk(m_Allocator), allocation, &data));
        memcpy((uint8_t*)data + 0 * sbtRecordSize, handles.data() + 0 * handleSize, handleSize);
        memcpy((uint8_t*)data + raygenRegionSize + 0 * sbtRecordSize, handles.data() + 1 * handleSize, handleSize);
        memcpy((uint8_t*)data + raygenRegionSize + missRegionSize + 0 * sbtRecordSize, handles.data() + 2 * handleSize, handleSize);
        vmaUnmapMemory(toVk(m_Allocator), allocation);

        VkBufferDeviceAddressInfo addrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        addrInfo.buffer = buffer;
        VkDeviceAddress address = vkGetBufferDeviceAddress(toVk(m_LogicalDevice), &addrInfo);

        raytracingPipeline->m_RayGenShaderBindingTable.DeviceAddress = address + 0;
        raytracingPipeline->m_RayGenShaderBindingTable.Stride = sbtRecordSize;
        raytracingPipeline->m_RayGenShaderBindingTable.Size = sbtRecordSize;
        raytracingPipeline->m_RayMissShaderBindingTable.DeviceAddress = address + raygenRegionSize;
        raytracingPipeline->m_RayMissShaderBindingTable.Stride = sbtRecordSize;
        raytracingPipeline->m_RayMissShaderBindingTable.Size = missRegionSize;
        raytracingPipeline->m_RayClosestHitShaderBindingTable.DeviceAddress = address + raygenRegionSize + missRegionSize;
        raytracingPipeline->m_RayClosestHitShaderBindingTable.Stride = sbtRecordSize;
        raytracingPipeline->m_RayClosestHitShaderBindingTable.Size = hitRegionSize;

        raytracingPipeline->m_Pipeline = fromVk(rtPipeline);
        raytracingPipeline->m_PipelineLayout = fromVk(pipelineLayout);
        raytracingPipeline->m_ShaderBindingTableBuffer = fromVk(buffer);
        raytracingPipeline->m_ShaderBindingTableAllocation = fromVk(allocation);

        m_RaytracingPipelines.insert(raytracingPipeline);
    }

    void ResourceAllocator::ReleaseResource(RayTracingPipeline* raytracingPipeline)
    {
        if (!m_RaytracingPipelines.contains(raytracingPipeline))
        {
            RDN_LOG_FATAL("Cannot find RaytracingPipeline resource to release!");
            return;
        }
        DestroyRaytracingPipeline(raytracingPipeline);
        m_RaytracingPipelines.erase(raytracingPipeline);
    }

    void ResourceAllocator::DestroyRaytracingPipeline(RayTracingPipeline* raytracingPipeline)
    {
        vmaDestroyBuffer(toVk(m_Allocator), toVk(raytracingPipeline->m_ShaderBindingTableBuffer), toVk(raytracingPipeline->m_ShaderBindingTableAllocation));
        vkDestroyPipelineLayout(toVk(m_LogicalDevice), toVk(raytracingPipeline->m_PipelineLayout), nullptr);
        vkDestroyPipeline(toVk(m_LogicalDevice), toVk(raytracingPipeline->m_Pipeline), nullptr);
    }

    void ResourceAllocator::AllocateDescriptorSet(DescriptorSet* descriptorSet, Pipeline* pipeline, uint32_t setIndex, uint32_t variableDescriptorCount)
    {
        AllocateDescriptorSetFromLayout(descriptorSet, pipeline->GetSetLayout(setIndex), variableDescriptorCount);
    }

    void ResourceAllocator::AllocateDescriptorSet(DescriptorSet* descriptorSet, RayTracingPipeline* pipeline, uint32_t setIndex, uint32_t variableDescriptorCount)
    {
        AllocateDescriptorSetFromLayout(descriptorSet, pipeline->GetSetLayout(setIndex), variableDescriptorCount);
    }

    void ResourceAllocator::AllocateDescriptorSetFromLayout(DescriptorSet* descriptorSet, DescriptorSetLayoutHandle layout, uint32_t variableDescriptorCount)
    {
        VkDescriptorSetLayout vkLayout = toVk(layout);

        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
        variableCountInfo.descriptorSetCount = 1;
        variableCountInfo.pDescriptorCounts = &variableDescriptorCount;

        VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.pNext = &variableCountInfo;
        allocInfo.descriptorPool = toVk(m_DescriptorPool);
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &vkLayout;

        VkDescriptorSet vkSet;
        VK_CHECK(vkAllocateDescriptorSets(toVk(m_LogicalDevice), &allocInfo, &vkSet));
        descriptorSet->m_DescriptorSet = fromVk(vkSet);

        m_DescriptorSets.insert(descriptorSet);
    }

    void ResourceAllocator::UpdateDescriptorSet(DescriptorSet* descriptorSet, const DescriptorWrite& write)
    {
        VkDescriptorSet dstSet = toVk(descriptorSet->GetHandle());

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(write.m_BufferWrites.size() + write.m_ImageWrites.size() + write.m_AccelerationStructureWrites.size());

        std::vector<VkDescriptorBufferInfo> bufferInfos;
        bufferInfos.reserve(write.m_BufferWrites.size());

        for (auto& bw : write.m_BufferWrites)
        {
            VkDescriptorBufferInfo bufferInfo = {};
            bufferInfo.buffer = toVk(bw.Buffer);
            bufferInfo.offset = bw.Offset;
            bufferInfo.range = bw.Size == 0 ? VK_WHOLE_SIZE : bw.Size;
            bufferInfos.push_back(bufferInfo);

            VkWriteDescriptorSet writeInfo = {};
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.dstSet = dstSet;
            writeInfo.dstBinding = bw.Binding;
            writeInfo.dstArrayElement = bw.ArrayElement;
            writeInfo.descriptorType = toVk(bw.Type);
            writeInfo.descriptorCount = 1;
            writeInfo.pBufferInfo = &bufferInfos[bufferInfos.size() - 1];
            writes.push_back(writeInfo);
        }

        std::vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.reserve(write.m_ImageWrites.size());

        for (auto& iw : write.m_ImageWrites)
        {
            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageView = toVk(iw.ImageView);
            imageInfo.imageLayout = toVk(iw.ImageLayout);
            imageInfo.sampler = toVk(iw.Sampler);
            imageInfos.push_back(imageInfo);

            VkWriteDescriptorSet writeInfo = {};
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.dstSet = dstSet;
            writeInfo.dstBinding = iw.Binding;
            writeInfo.dstArrayElement = iw.ArrayElement;
            writeInfo.descriptorType = toVk(iw.Type);
            writeInfo.descriptorCount = 1;
            writeInfo.pImageInfo = &imageInfos[imageInfos.size() - 1];
            writes.push_back(writeInfo);
        }

        std::vector<VkAccelerationStructureKHR> accelerationHandles;
        accelerationHandles.reserve(write.m_AccelerationStructureWrites.size());

        std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accelerationInfos;
        accelerationInfos.reserve(write.m_AccelerationStructureWrites.size());

        for (auto& sw : write.m_AccelerationStructureWrites)
        {
            accelerationHandles.push_back(toVk(sw.AccelerationStructure));
            VkWriteDescriptorSetAccelerationStructureKHR accelInfo{};
            accelInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            accelInfo.accelerationStructureCount = 1;
            accelInfo.pAccelerationStructures = &accelerationHandles[accelerationHandles.size() - 1];
            accelerationInfos.push_back(accelInfo);

            VkWriteDescriptorSet writeInfo = {};
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.pNext = &accelerationInfos[accelerationInfos.size() - 1];
            writeInfo.dstSet = dstSet;
            writeInfo.dstBinding = sw.Binding;
            writeInfo.dstArrayElement = sw.ArrayElement;
            writeInfo.descriptorType = toVk(sw.Type);
            writeInfo.descriptorCount = 1;
            writes.push_back(writeInfo);
        }

        vkUpdateDescriptorSets(toVk(m_LogicalDevice), uint32_t(writes.size()), writes.data(), 0, nullptr);
    }

    void ResourceAllocator::DestroyDescriptorSet(DescriptorSet* descriptorSet)
    {
        VkDescriptorSet vkSet = toVk(descriptorSet->GetHandle());
        vkFreeDescriptorSets(toVk(m_LogicalDevice), toVk(m_DescriptorPool), 1, &vkSet);
    }

    void ResourceAllocator::ReleaseResource(DescriptorSet* descriptorSet)
    {
        if (!m_DescriptorSets.contains(descriptorSet))
        {
            RDN_LOG_FATAL("Cannot find DescriptorSet resource to release!");
            return;
        }
        DestroyDescriptorSet(descriptorSet);
        m_DescriptorSets.erase(descriptorSet);
    }
}