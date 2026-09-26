#include "ResourceAllocator.h"
#include "VulkanInternal/VulkanUtilities.h"
#include "RadianceEngine/Core/Log.h"
#include <volk/volk.h>
#include <spirv_cross/spirv_cross.hpp>
#include <vulkan/vk_enum_string_helper.h>
#include <algorithm>
#include <format>
#include <map>

namespace Rdn
{
    namespace
    {
        enum class HostAccess { None, SequentialWrite, Random };

        struct VmaBuffer
        {
            VkBuffer Buffer = VK_NULL_HANDLE;
            VmaAllocation Allocation = VK_NULL_HANDLE;
            void* MappedPtr = nullptr;
        };

        VmaBuffer CreateVmaBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, HostAccess hostAccess,
            VkDeviceSize alignment = 0, bool persistentlyMapped = false)
        {
            VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            bufferInfo.size = size;
            bufferInfo.usage = usage;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo{};
            switch (hostAccess)
            {
            case HostAccess::None:
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
                break;
            case HostAccess::SequentialWrite:
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                break;
            case HostAccess::Random:
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
                allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                break;
            }
            if (persistentlyMapped)
                allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VmaBuffer result;
            VmaAllocationInfo allocationInfo{};
            VK_CHECK(vmaCreateBufferWithAlignment(allocator, &bufferInfo, &allocInfo, alignment, &result.Buffer, &result.Allocation, &allocationInfo));
            result.MappedPtr = allocationInfo.pMappedData;
            return result;
        }

        struct VmaAccelerationStructure
        {
            VkAccelerationStructureKHR Handle = VK_NULL_HANDLE;
            VmaBuffer Storage;
        };

        VmaAccelerationStructure CreateVmaAccelerationStructure(VkDevice device, VmaAllocator allocator, VkAccelerationStructureTypeKHR type, VkDeviceSize size)
        {
            VmaAccelerationStructure result;
            result.Storage = CreateVmaBuffer(allocator, size,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, HostAccess::None);

            VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
            createInfo.buffer = result.Storage.Buffer;
            createInfo.size = size;
            createInfo.type = type;
            VK_CHECK(vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &result.Handle));
            return result;
        }

        void DestroyVmaAccelerationStructure(VkDevice device, VmaAllocator allocator, VmaAccelerationStructure& accelerationStructure)
        {
            vkDestroyAccelerationStructureKHR(device, accelerationStructure.Handle, nullptr);
            vmaDestroyBuffer(allocator, accelerationStructure.Storage.Buffer, accelerationStructure.Storage.Allocation);
            accelerationStructure = {};
        }

        HostAccess HostAccessFor(MemoryProperty memoryProperty, bool mapped)
        {
            if (HasAny(memoryProperty & MemoryProperty::HostCached))
                return HostAccess::Random;
            if (mapped || HasAny(memoryProperty & (MemoryProperty::HostVisible | MemoryProperty::HostCoherent)))
                return HostAccess::SequentialWrite;
            return HostAccess::None;
        }

        VkDeviceAddress GetDeviceAddress(VkDevice device, VkBuffer buffer)
        {
            VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
            addressInfo.buffer = buffer;
            return vkGetBufferDeviceAddress(device, &addressInfo);
        }

        Extent3D MipExtent(Extent3D extent, uint32_t level)
        {
            return { std::max(1u, extent.Width >> level), std::max(1u, extent.Height >> level), std::max(1u, extent.Depth >> level) };
        }

        uint32_t GetFormatByteSize(Format format)
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
    }

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
        allocatorInfo.vulkanApiVersion = std::min(device->GetProperties().ApiVersion, VK_API_VERSION_1_4);
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        if (device->IsExtensionEnabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME))
            allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
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
        for (auto& res : m_ComputePipelines)
            DestroyComputePipeline(res);
        m_ComputePipelines.clear();
        for (auto& res : m_DescriptorSets)
            DestroyDescriptorSet(res);
        m_DescriptorSets.clear();
    }

    void* ResourceAllocator::MapMemory(GpuBuffer* buffer)
    {
        void* data;
        VK_CHECK(vmaMapMemory(toVk(m_Allocator), toVk(buffer->m_Allocation), &data));
        VK_CHECK(vmaInvalidateAllocation(toVk(m_Allocator), toVk(buffer->m_Allocation), 0, VK_WHOLE_SIZE));
        buffer->m_MappedPtr = data;
        return data;
    }

    void ResourceAllocator::UnmapMemory(GpuBuffer* buffer)
    {
        VK_CHECK(vmaFlushAllocation(toVk(m_Allocator), toVk(buffer->m_Allocation), 0, VK_WHOLE_SIZE));
        vmaUnmapMemory(toVk(m_Allocator), toVk(buffer->m_Allocation));

        VmaAllocationInfo allocationInfo;
        vmaGetAllocationInfo(toVk(m_Allocator), toVk(buffer->m_Allocation), &allocationInfo);
        buffer->m_MappedPtr = allocationInfo.pMappedData;
    }

    void* ResourceAllocator::MapMemory(GpuRingBuffer* ringBuffer, uint32_t elementIndex)
    {
        void* data;
        VK_CHECK(vmaMapMemory(toVk(m_Allocator), toVk(ringBuffer->m_Allocation), &data));
        VK_CHECK(vmaInvalidateAllocation(toVk(m_Allocator), toVk(ringBuffer->m_Allocation), 0, VK_WHOLE_SIZE));
        ringBuffer->m_MappedPtr = data;
        return static_cast<uint8_t*>(data) + ringBuffer->GetOffset(elementIndex);
    }

    void ResourceAllocator::UnmapMemory(GpuRingBuffer* ringBuffer)
    {
        VK_CHECK(vmaFlushAllocation(toVk(m_Allocator), toVk(ringBuffer->m_Allocation), 0, VK_WHOLE_SIZE));
        vmaUnmapMemory(toVk(m_Allocator), toVk(ringBuffer->m_Allocation));

        VmaAllocationInfo allocationInfo;
        vmaGetAllocationInfo(toVk(m_Allocator), toVk(ringBuffer->m_Allocation), &allocationInfo);
        ringBuffer->m_MappedPtr = allocationInfo.pMappedData;
    }

    uint64_t ResourceAllocator::GetBufferDeviceAddress(GpuBuffer* buffer)
    {
        return GetDeviceAddress(toVk(m_LogicalDevice), toVk(buffer->m_Buffer));
    }

    void ResourceAllocator::SetDeviceLocalBufferData(GpuBuffer* buffer, const void* data, uint64_t size)
    {
        if (size > buffer->GetSize())
        {
            RDN_LOG_ERROR("SetDeviceLocalBufferData: {} bytes don't fit into a buffer of {} bytes", size, buffer->GetSize());
            return;
        }

        VmaBuffer staging = CreateVmaBuffer(toVk(m_Allocator), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, HostAccess::SequentialWrite);
        VK_CHECK(vmaCopyMemoryToAllocation(toVk(m_Allocator), data, staging.Allocation, 0, size));

        m_Device->ImmediateSubmit([&](CommandBuffer& cmd) {
            cmd.CopyBuffer(fromVk(staging.Buffer), buffer->GetHandle(), size);
            cmd.Barrier(GlobalBarrier{
                .SrcStage = PipelineStage::AllTransfer, .DstStage = PipelineStage::AllCommands,
                .SrcAccess = AccessMask::TransferWrite, .DstAccess = AccessMask::MemoryRead | AccessMask::MemoryWrite });
        });
        buffer->m_SyncState = {};

        vmaDestroyBuffer(toVk(m_Allocator), staging.Buffer, staging.Allocation);
    }

    void ResourceAllocator::SetImageData(GpuImage* image, const void* data, uint32_t size, ImageLayout newLayout)
    {
        const Extent3D extent = image->GetImageSize();
        const uint32_t mipLevels = image->GetMipLevels();
        const uint64_t baseLevelSize = uint64_t(extent.Width) * extent.Height * extent.Depth * GetFormatByteSize(image->GetFormat());
        if (size < baseLevelSize)
        {
            RDN_LOG_ERROR("SetImageData: {} bytes given, but the base level needs {}", size, baseLevelSize);
            return;
        }

        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(toVk(m_PhysicalDevice), toVk(image->GetFormat()), &formatProperties);
        const VkFormatFeatureFlags blitFeatures = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
        const bool canBlit = (formatProperties.optimalTilingFeatures & blitFeatures) == blitFeatures;
        const Filter mipFilter = (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ? Filter::Linear : Filter::Nearest;
        if (mipLevels > 1 && !canBlit)
        {
            RDN_LOG_ERROR("SetImageData: {} can't be blitted, so mip levels 1..{} stay undefined", string_VkFormat(toVk(image->GetFormat())), mipLevels - 1);
        }

        VmaBuffer staging = CreateVmaBuffer(toVk(m_Allocator), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, HostAccess::SequentialWrite);
        VK_CHECK(vmaCopyMemoryToAllocation(toVk(m_Allocator), data, staging.Allocation, 0, size));

        m_Device->ImmediateSubmit([&](CommandBuffer& cmd) {
            const ImageHandle handle = image->GetHandle();

            cmd.Barrier(ImageBarrier{
                .Handle = handle, .OldLayout = ImageLayout::Undefined, .NewLayout = ImageLayout::TransferDstOptimal,
                .SrcStage = PipelineStage::None, .DstStage = PipelineStage::AllTransfer,
                .SrcAccess = AccessMask::None, .DstAccess = AccessMask::TransferWrite });
            cmd.CopyBufferToImage(fromVk(staging.Buffer), handle, extent);

            uint32_t lastWrittenLevel = 0;
            if (canBlit)
            {
                for (uint32_t level = 1; level < mipLevels; level++)
                {
                    cmd.Barrier(ImageBarrier{
                        .Handle = handle, .OldLayout = ImageLayout::TransferDstOptimal, .NewLayout = ImageLayout::TransferSrcOptimal,
                        .SrcStage = PipelineStage::AllTransfer, .DstStage = PipelineStage::AllTransfer,
                        .SrcAccess = AccessMask::TransferWrite, .DstAccess = AccessMask::TransferRead,
                        .BaseMip = level - 1, .MipCount = 1 });
                    cmd.BlitImage(handle, MipExtent(extent, level - 1), handle, MipExtent(extent, level), mipFilter, level - 1, level);
                }
                lastWrittenLevel = mipLevels - 1;
            }

            if (lastWrittenLevel > 0)
            {
                cmd.Barrier(ImageBarrier{
                    .Handle = handle, .OldLayout = ImageLayout::TransferSrcOptimal, .NewLayout = newLayout,
                    .SrcStage = PipelineStage::AllTransfer, .DstStage = PipelineStage::AllCommands,
                    .SrcAccess = AccessMask::None, .DstAccess = AccessMask::MemoryRead,
                    .MipCount = lastWrittenLevel });
            }
            cmd.Barrier(ImageBarrier{
                .Handle = handle, .OldLayout = ImageLayout::TransferDstOptimal, .NewLayout = newLayout,
                .SrcStage = PipelineStage::AllTransfer, .DstStage = PipelineStage::AllCommands,
                .SrcAccess = AccessMask::TransferWrite, .DstAccess = AccessMask::MemoryRead,
                .BaseMip = lastWrittenLevel, .MipCount = mipLevels - lastWrittenLevel });
        });
        image->m_SyncState = {};
        image->m_SyncState.Layout = newLayout;

        vmaDestroyBuffer(toVk(m_Allocator), staging.Buffer, staging.Allocation);
    }

    std::vector<uint8_t> ResourceAllocator::GetImageData(GpuImage* image)
    {
        const ImageLayout currentLayout = image->m_SyncState.Layout;
        const ImageLayout restoreLayout = (currentLayout == ImageLayout::Undefined) ? ImageLayout::TransferSrcOptimal : currentLayout;

        const Extent3D extent = image->GetImageSize();
        const uint32_t texelSize = GetFormatByteSize(image->GetFormat());
        if (texelSize == 0)
        {
            RDN_LOG_FATAL("GetImageData: image format is not a supported uncompressed color format (or is unhandled) -- cannot size the readback buffer");
            return {};
        }
        const uint32_t size = extent.Width * extent.Height * extent.Depth * texelSize;

        VmaBuffer readback = CreateVmaBuffer(toVk(m_Allocator), size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, HostAccess::Random);

        m_Device->ImmediateSubmit([&](CommandBuffer& cmd) {
            const ImageHandle handle = image->GetHandle();

            cmd.Barrier(ImageBarrier{
                .Handle = handle, .OldLayout = currentLayout, .NewLayout = ImageLayout::TransferSrcOptimal,
                .SrcStage = PipelineStage::AllCommands, .DstStage = PipelineStage::AllTransfer,
                .SrcAccess = AccessMask::MemoryWrite, .DstAccess = AccessMask::TransferRead });
            cmd.CopyImageToBuffer(handle, fromVk(readback.Buffer), extent);
            cmd.Barrier(GlobalBarrier{
                .SrcStage = PipelineStage::AllTransfer, .DstStage = PipelineStage::Host,
                .SrcAccess = AccessMask::TransferWrite, .DstAccess = AccessMask::HostRead });
            if (restoreLayout != ImageLayout::TransferSrcOptimal)
            {
                cmd.Barrier(ImageBarrier{
                    .Handle = handle, .OldLayout = ImageLayout::TransferSrcOptimal, .NewLayout = restoreLayout,
                    .SrcStage = PipelineStage::AllTransfer, .DstStage = PipelineStage::AllCommands,
                    .SrcAccess = AccessMask::None, .DstAccess = AccessMask::MemoryRead | AccessMask::MemoryWrite });
            }
        });
        image->m_SyncState = {};
        image->m_SyncState.Layout = restoreLayout;

        std::vector<uint8_t> result(size);
        VK_CHECK(vmaCopyAllocationToMemory(toVk(m_Allocator), readback.Allocation, 0, result.data(), size));
        vmaDestroyBuffer(toVk(m_Allocator), readback.Buffer, readback.Allocation);
        return result;
    }

    void ResourceAllocator::CreateGpuBuffer(GpuBuffer* gpuBuffer, const GpuBufferDesc& desc)
    {
        VmaBuffer buffer = CreateVmaBuffer(toVk(m_Allocator), desc.Size, toVk(desc.UsageFlags),
            HostAccessFor(desc.MemoryProperty, desc.Mapped), 0, desc.Mapped);

        gpuBuffer->m_Buffer = fromVk(buffer.Buffer);
        gpuBuffer->m_Allocation = fromVk(buffer.Allocation);
        gpuBuffer->m_Size = desc.Size;
        gpuBuffer->m_MappedPtr = buffer.MappedPtr;
        gpuBuffer->m_SyncState = {};

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
        uint64_t alignment = 1;
        if (HasAny(desc.UsageFlags & BufferUsage::UniformBuffer))
            alignment = std::max(alignment, m_Device->GetProperties().MinUniformBufferOffsetAlignment);
        if (HasAny(desc.UsageFlags & BufferUsage::StorageBuffer))
            alignment = std::max(alignment, m_Device->GetProperties().MinStorageBufferOffsetAlignment);
        const uint32_t elementStride = uint32_t((desc.ElementSize + alignment - 1) & ~(alignment - 1));

        VmaBuffer buffer = CreateVmaBuffer(toVk(m_Allocator), uint64_t(desc.ElementCount) * elementStride, toVk(desc.UsageFlags),
            HostAccessFor(desc.MemoryProperty, desc.Mapped), 0, desc.Mapped);

        gpuRingBuffer->m_Buffer = fromVk(buffer.Buffer);
        gpuRingBuffer->m_Allocation = fromVk(buffer.Allocation);
        gpuRingBuffer->m_WholeSize = desc.ElementCount * elementStride;
        gpuRingBuffer->m_ElementSize = elementStride;
        gpuRingBuffer->m_ElementCount = desc.ElementCount;
        gpuRingBuffer->m_MappedPtr = buffer.MappedPtr;

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
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

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

        constexpr ImageUsage viewableUsages = ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::ColorAttachment
            | ImageUsage::DepthStencilAttachment | ImageUsage::TransientAttachment | ImageUsage::InputAttachment;
        VkImageView imageView = VK_NULL_HANDLE;
        if (HasAny(desc.UsageFlags & viewableUsages))
            VK_CHECK(vkCreateImageView(toVk(m_LogicalDevice), &imageViewInfo, nullptr, &imageView));
        gpuImage->m_ImageView = fromVk(imageView);
        gpuImage->m_Format = desc.Format;
        gpuImage->m_ImageSize = desc.ImageSize;
        gpuImage->m_MipLevels = desc.MipLevels;
        gpuImage->m_SyncState = {};

        m_Images.insert(gpuImage);
	}

    void ResourceAllocator::DestroyGpuImage(GpuImage* gpuImage)
    {
        vkDestroyImageView(toVk(m_LogicalDevice), toVk(gpuImage->m_ImageView), nullptr);
        vmaDestroyImage(toVk(m_Allocator), toVk(gpuImage->m_Image), toVk(gpuImage->m_Allocation));
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
        if (desc.ShaderStage == ShaderStage::Compute)
        {
            shader->m_WorkgroupSize = {
                comp.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0),
                comp.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1),
                comp.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2),
            };
        }

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
        const VkDevice device = toVk(m_LogicalDevice);
        const VmaAllocator allocator = toVk(m_Allocator);
        if (desc.Geometries.empty())
        {
            RDN_LOG_ERROR("CreateBottomLevelAS: no geometries");
            return;
        }

        const VkDeviceAddress vertexAddress = GetDeviceAddress(device, toVk(desc.VertexBuffer));
        const VkDeviceAddress indexAddress = GetDeviceAddress(device, toVk(desc.IndexBuffer));
        std::vector<VkAccelerationStructureGeometryKHR> geometries;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
        std::vector<uint32_t> primitiveCounts;
        for (const BottomLevelASGeometry& part : desc.Geometries)
        {
            VkAccelerationStructureGeometryKHR& geometry = geometries.emplace_back(VkAccelerationStructureGeometryKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR });
            geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            geometry.geometry.triangles.vertexFormat = toVk(desc.VertexFormat);
            geometry.geometry.triangles.vertexData.deviceAddress = vertexAddress;
            geometry.geometry.triangles.maxVertex = desc.VertexCount > 0 ? desc.VertexCount - 1 : 0;
            geometry.geometry.triangles.vertexStride = desc.VertexStride;
            geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
            geometry.geometry.triangles.indexData.deviceAddress = indexAddress;

            ranges.push_back({ part.TriangleCount, part.FirstIndex * uint32_t(sizeof(uint32_t)), 0, 0 });
            primitiveCounts.push_back(part.TriangleCount);
        }

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.geometryCount = uint32_t(geometries.size());
        buildInfo.pGeometries = geometries.data();

        VkAccelerationStructureBuildSizesInfoKHR sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, primitiveCounts.data(), &sizes);

        VmaAccelerationStructure built = CreateVmaAccelerationStructure(device, allocator, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, sizes.accelerationStructureSize);
        VmaBuffer scratch = CreateVmaBuffer(allocator, sizes.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            HostAccess::None, m_Device->GetProperties().MinAccelerationStructureScratchOffsetAlignment);
        buildInfo.dstAccelerationStructure = built.Handle;
        buildInfo.scratchData.deviceAddress = GetDeviceAddress(device, scratch.Buffer);

        VkQueryPoolCreateInfo queryPoolInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        queryPoolInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
        queryPoolInfo.queryCount = 1;
        VkQueryPool queryPool;
        VK_CHECK(vkCreateQueryPool(device, &queryPoolInfo, nullptr, &queryPool));

        const VkAccelerationStructureBuildRangeInfoKHR* buildRanges = ranges.data();
        m_Device->ImmediateSubmit([&](CommandBuffer& cmd) {
            const VkCommandBuffer vkCmd = toVk(cmd.GetHandle());
            vkCmdResetQueryPool(vkCmd, queryPool, 0, 1);
            cmd.Barrier(GlobalBarrier{
                .SrcStage = PipelineStage::AllCommands, .DstStage = PipelineStage::AccelerationStructureBuild,
                .SrcAccess = AccessMask::MemoryWrite, .DstAccess = AccessMask::MemoryRead });
            vkCmdBuildAccelerationStructuresKHR(vkCmd, 1, &buildInfo, &buildRanges);
            cmd.Barrier(GlobalBarrier{
                .SrcStage = PipelineStage::AccelerationStructureBuild, .DstStage = PipelineStage::AllCommands,
                .SrcAccess = AccessMask::AccelerationStructureWrite, .DstAccess = AccessMask::MemoryRead });
            vkCmdWriteAccelerationStructuresPropertiesKHR(vkCmd, 1, &built.Handle, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);
        });

        VkDeviceSize compactedSize = 0;
        VK_CHECK(vkGetQueryPoolResults(device, queryPool, 0, 1, sizeof(compactedSize), &compactedSize, sizeof(compactedSize),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
        vkDestroyQueryPool(device, queryPool, nullptr);
        vmaDestroyBuffer(allocator, scratch.Buffer, scratch.Allocation);

        VmaAccelerationStructure result = built;
        if (compactedSize > 0 && compactedSize < sizes.accelerationStructureSize)
        {
            result = CreateVmaAccelerationStructure(device, allocator, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, compactedSize);
            m_Device->ImmediateSubmit([&](CommandBuffer& cmd) {
                VkCopyAccelerationStructureInfoKHR copyInfo{ VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
                copyInfo.src = built.Handle;
                copyInfo.dst = result.Handle;
                copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
                vkCmdCopyAccelerationStructureKHR(toVk(cmd.GetHandle()), &copyInfo);
                cmd.Barrier(GlobalBarrier{
                    .SrcStage = PipelineStage::AllCommands, .DstStage = PipelineStage::AllCommands,
                    .SrcAccess = AccessMask::AccelerationStructureWrite, .DstAccess = AccessMask::MemoryRead });
            });
            DestroyVmaAccelerationStructure(device, allocator, built);
        }

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
        addressInfo.accelerationStructure = result.Handle;

        bottomLevelAS->m_Handle = fromVk(result.Handle);
        bottomLevelAS->m_Buffer = fromVk(result.Storage.Buffer);
        bottomLevelAS->m_Allocation = fromVk(result.Storage.Allocation);
        bottomLevelAS->m_DeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);

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
        const VkDevice device = toVk(m_LogicalDevice);
        const VmaAllocator allocator = toVk(m_Allocator);

        if (topLevelAS->m_Handle.Valid())
        {
            m_Device->WaitIdle();
            DestroyTopLevelAS(topLevelAS);
        }

        std::vector<VkAccelerationStructureInstanceKHR> instances(topLevelAS->m_Instances.size());
        for (size_t i = 0; i < instances.size(); i++)
        {
            const TopLevelASInstance& source = topLevelAS->m_Instances[i];
            VkAccelerationStructureInstanceKHR& instance = instances[i];
            memcpy(instance.transform.matrix, source.TransformMatrix, sizeof(instance.transform.matrix));
            instance.instanceCustomIndex = source.InstanceCustomIndex;
            instance.mask = 0xFF;
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance.accelerationStructureReference = source.BottomLevelASAddress;
        }

        const VkDeviceSize instanceBytes = std::max<size_t>(instances.size(), 1) * sizeof(VkAccelerationStructureInstanceKHR);
        VmaBuffer instanceBuffer = CreateVmaBuffer(allocator, instanceBytes,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, HostAccess::SequentialWrite, 16);
        if (!instances.empty())
            VK_CHECK(vmaCopyMemoryToAllocation(allocator, instances.data(), instanceBuffer.Allocation, 0, instances.size() * sizeof(VkAccelerationStructureInstanceKHR)));

        VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.arrayOfPointers = VK_FALSE;
        geometry.geometry.instances.data.deviceAddress = GetDeviceAddress(device, instanceBuffer.Buffer);

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geometry;

        const uint32_t primitiveCount = uint32_t(instances.size());
        VkAccelerationStructureBuildSizesInfoKHR sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizes);

        VmaAccelerationStructure tlas = CreateVmaAccelerationStructure(device, allocator, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, sizes.accelerationStructureSize);
        VmaBuffer scratch = CreateVmaBuffer(allocator, sizes.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            HostAccess::None, m_Device->GetProperties().MinAccelerationStructureScratchOffsetAlignment);
        buildInfo.dstAccelerationStructure = tlas.Handle;
        buildInfo.scratchData.deviceAddress = GetDeviceAddress(device, scratch.Buffer);

        const VkAccelerationStructureBuildRangeInfoKHR buildRange{ primitiveCount, 0, 0, 0 };
        const VkAccelerationStructureBuildRangeInfoKHR* buildRanges = &buildRange;
        m_Device->ImmediateSubmit([&](CommandBuffer& cmd) {
            cmd.Barrier(GlobalBarrier{
                .SrcStage = PipelineStage::AllCommands, .DstStage = PipelineStage::AccelerationStructureBuild,
                .SrcAccess = AccessMask::MemoryWrite, .DstAccess = AccessMask::MemoryRead });
            vkCmdBuildAccelerationStructuresKHR(toVk(cmd.GetHandle()), 1, &buildInfo, &buildRanges);
            cmd.Barrier(GlobalBarrier{
                .SrcStage = PipelineStage::AccelerationStructureBuild, .DstStage = PipelineStage::AllCommands,
                .SrcAccess = AccessMask::AccelerationStructureWrite, .DstAccess = AccessMask::MemoryRead });
        });

        vmaDestroyBuffer(allocator, scratch.Buffer, scratch.Allocation);
        vmaDestroyBuffer(allocator, instanceBuffer.Buffer, instanceBuffer.Allocation);

        topLevelAS->m_Handle = fromVk(tlas.Handle);
        topLevelAS->m_Buffer = fromVk(tlas.Storage.Buffer);
        topLevelAS->m_Allocation = fromVk(tlas.Storage.Allocation);
    }

    void ResourceAllocator::CreateTopLevelAS(TopLevelAS* topLevelAS)
    {
        m_TopLevelASs.insert(topLevelAS);
    }

    void ResourceAllocator::DestroyTopLevelAS(TopLevelAS* topLevelAS)
    {
        if (topLevelAS->m_Handle.Valid()) vkDestroyAccelerationStructureKHR(toVk(m_LogicalDevice), toVk(topLevelAS->m_Handle), nullptr);
        if (topLevelAS->m_Buffer.Valid()) vmaDestroyBuffer(toVk(m_Allocator), toVk(topLevelAS->m_Buffer), toVk(topLevelAS->m_Allocation));
        topLevelAS->m_Handle = {};
        topLevelAS->m_Buffer = {};
        topLevelAS->m_Allocation = {};
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

    PipelineLayoutHandle ResourceAllocator::CreatePipelineLayout(std::span<Shader* const> shaders, std::span<const uint32_t> pushDescriptorSets, std::vector<DescriptorSetLayoutHandle>& setLayouts)
    {
        std::map<uint32_t, std::map<uint32_t, Shader::DescriptorLayoutBinding>> bindings;
        ShaderStage stages = {};
        uint32_t pushConstantSize = 0;
        for (const Shader* shader : shaders)
        {
            for (const Shader::DescriptorLayoutBinding& binding : shader->m_LayoutBindings)
            {
                const auto [it, inserted] = bindings[binding.SetIndex].try_emplace(binding.Binding, binding);
                if (!inserted)
                    it->second.Stage |= shader->m_ShaderStage;
            }
            stages |= shader->m_ShaderStage;
            pushConstantSize = std::max(pushConstantSize, shader->m_PushConstantSize);
        }

        const uint32_t setCount = bindings.empty() ? 0 : bindings.rbegin()->first + 1;
        std::vector<VkDescriptorSetLayout> layouts;
        layouts.reserve(setCount);
        setLayouts.clear();
        for (uint32_t setIndex = 0; setIndex < setCount; setIndex++)
        {
            DescriptorSetLayoutKey key;
            if (const auto set = bindings.find(setIndex); set != bindings.end())
            {
                key.UsePushDescriptors = std::ranges::find(pushDescriptorSets, setIndex) != pushDescriptorSets.end();
                for (const auto& [bindingIndex, binding] : set->second)
                {
                    key.Bindings.push_back({ binding.Binding, binding.DescriptorCount, binding.Type, binding.Stage, binding.IsBindless });
                    key.HasBindless |= binding.IsBindless;
                }
            }
            setLayouts.push_back(GetOrCreateDescriptorSetLayout(key));
            layouts.push_back(toVk(setLayouts.back()));
        }

        const VkPushConstantRange pushConstantRange{ VkShaderStageFlags(toVk(stages)), 0, pushConstantSize };
        VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        layoutInfo.setLayoutCount = uint32_t(layouts.size());
        layoutInfo.pSetLayouts = layouts.data();
        if (pushConstantSize > 0)
        {
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &pushConstantRange;
        }

        VkPipelineLayout pipelineLayout;
        VK_CHECK(vkCreatePipelineLayout(toVk(m_LogicalDevice), &layoutInfo, nullptr, &pipelineLayout));
        return fromVk(pipelineLayout);
    }
    void ResourceAllocator::CreatePipeline(Pipeline* pipeline, const PipelineDesc& desc)
    {
        pipeline->m_PipelineLayout = CreatePipelineLayout(desc.ShaderStages, desc.PushDescriptorSets, pipeline->m_SetLayouts);

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
        blendAttachments.reserve(desc.ColorAttachmentFormats.size());

        for (size_t i = 0; i < desc.ColorAttachmentFormats.size(); i++)
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
        const VkPipelineLayout pipelineLayout = toVk(CreatePipelineLayout(desc.ShaderStages, desc.PushDescriptorSets, raytracingPipeline->m_SetLayouts));

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


        uint32_t handleSize = m_Device->GetProperties().ShaderGroupHandleSize;
        uint32_t handleAlign = m_Device->GetProperties().ShaderGroupHandleAlignment;
        uint32_t baseAlign = m_Device->GetProperties().ShaderGroupBaseAlignment;

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

        VmaBuffer sbt = CreateVmaBuffer(toVk(m_Allocator), sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            HostAccess::SequentialWrite, baseAlign);
        VkBuffer buffer = sbt.Buffer;
        VmaAllocation allocation = sbt.Allocation;

        void* data;
        VK_CHECK(vmaMapMemory(toVk(m_Allocator), allocation, &data));
        memcpy((uint8_t*)data + 0 * sbtRecordSize, handles.data() + 0 * handleSize, handleSize);
        memcpy((uint8_t*)data + raygenRegionSize + 0 * sbtRecordSize, handles.data() + 1 * handleSize, handleSize);
        memcpy((uint8_t*)data + raygenRegionSize + missRegionSize + 0 * sbtRecordSize, handles.data() + 2 * handleSize, handleSize);
        vmaUnmapMemory(toVk(m_Allocator), allocation);

        VkDeviceAddress address = GetDeviceAddress(toVk(m_LogicalDevice), buffer);

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

    void ResourceAllocator::CreateComputePipeline(ComputePipeline* computePipeline, const ComputePipelineDesc& desc)
    {
        Shader* shader = desc.ComputeShader;
        if (!shader || shader->m_ShaderStage != ShaderStage::Compute)
        {
            RDN_LOG_ERROR("CreateComputePipeline: ComputeShader must be a compute shader");
            return;
        }

        computePipeline->m_PipelineLayout = CreatePipelineLayout({ &shader, 1 }, desc.PushDescriptorSets, computePipeline->m_SetLayouts);

        VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        pipelineInfo.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = toVk(shader->m_Module);
        pipelineInfo.stage.pName = shader->m_EntryPoint.c_str();
        pipelineInfo.layout = toVk(computePipeline->m_PipelineLayout);
        pipelineInfo.basePipelineIndex = -1;

        VkPipeline pipeline;
        VK_CHECK(vkCreateComputePipelines(toVk(m_LogicalDevice), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
        computePipeline->m_Pipeline = fromVk(pipeline);
        computePipeline->m_WorkgroupSize = shader->m_WorkgroupSize;

        m_ComputePipelines.insert(computePipeline);
    }

    void ResourceAllocator::DestroyComputePipeline(ComputePipeline* computePipeline)
    {
        vkDestroyPipelineLayout(toVk(m_LogicalDevice), toVk(computePipeline->m_PipelineLayout), nullptr);
        vkDestroyPipeline(toVk(m_LogicalDevice), toVk(computePipeline->m_Pipeline), nullptr);
    }

    void ResourceAllocator::ReleaseResource(ComputePipeline* computePipeline)
    {
        if (!m_ComputePipelines.contains(computePipeline))
        {
            RDN_LOG_FATAL("Cannot find ComputePipeline resource to release!");
            return;
        }
        DestroyComputePipeline(computePipeline);
        m_ComputePipelines.erase(computePipeline);
    }

    void ResourceAllocator::AllocateDescriptorSet(DescriptorSet* descriptorSet, ComputePipeline* pipeline, uint32_t setIndex, uint32_t variableDescriptorCount)
    {
        AllocateDescriptorSetFromLayout(descriptorSet, pipeline->GetSetLayout(setIndex), variableDescriptorCount);
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
        const VulkanDescriptorWrites writes(write, toVk(descriptorSet->GetHandle()));
        vkUpdateDescriptorSets(toVk(m_LogicalDevice), uint32_t(writes.Writes.size()), writes.Writes.data(), 0, nullptr);
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

    std::string ResourceAllocator::DescribeMemoryUsage() const
    {
        const VkPhysicalDeviceMemoryProperties* memoryProperties = nullptr;
        vmaGetMemoryProperties(toVk(m_Allocator), &memoryProperties);

        std::vector<VmaBudget> budgets(memoryProperties->memoryHeapCount);
        vmaGetHeapBudgets(toVk(m_Allocator), budgets.data());

        const bool driverBudget = m_Device->IsExtensionEnabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);

        constexpr double MiB = 1024.0 * 1024.0;
        std::string out = "GPU memory:\n";
        for (uint32_t heap = 0; heap < memoryProperties->memoryHeapCount; heap++)
        {
            const VkMemoryHeap& heapInfo = memoryProperties->memoryHeaps[heap];
            const VmaStatistics& stats = budgets[heap].statistics;
            out += std::format("    heap {} ({}, {:.0f} MiB): {:.2f} MiB in {} allocations, {:.1f} MiB reserved in {} blocks",
                heap, (heapInfo.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "device local" : "host", heapInfo.size / MiB,
                stats.allocationBytes / MiB, stats.allocationCount, stats.blockBytes / MiB, stats.blockCount);
            if (driverBudget)
                out += std::format(", process usage {:.1f} MiB of a {:.1f} MiB budget", budgets[heap].usage / MiB, budgets[heap].budget / MiB);
            out += "\n";
        }
        return out;
    }
}