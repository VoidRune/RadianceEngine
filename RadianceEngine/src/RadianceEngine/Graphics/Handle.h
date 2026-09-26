#pragma once
#include <cstdint>
#include <mutex>

namespace Rdn
{
    template<typename Tag>
    struct Handle {
        uint64_t Id = 0;
		//Handle() = default;
		//Handle(uint64_t id) : Id(id) {}
		//Handle(struct NullHandleType) : Id(0) {}
        bool Valid() const { return Id != 0; }
        explicit operator bool() const { return Valid(); }
        bool operator==(Handle o) const { return Id == o.Id; }
        bool operator!=(Handle o) const { return Id != o.Id; }
    };

	//inline constexpr struct NullHandleType {} NullHandle{};

	struct BufferTag {};
	struct ImageTag {};
	struct InstanceTag {};
	struct PhysicalDeviceTag {};
	struct DebugUtilsMessengerTag {};
	struct SurfaceTag {};
	struct DeviceTag {};
	struct QueueTag {};
	struct AllocatorTag {};
	struct AllocationTag {};
	struct SemaphoreTag {};
	struct CommandBufferTag {};
	struct FenceTag {};
	struct DeviceMemoryTag {};
	struct EventTag {};
	struct QueryPoolTag {};
	struct BufferViewTag {};
	struct ImageViewTag {};
	struct ShaderModuleTag {};
	struct PipelineCacheTag {};
	struct PipelineLayoutTag {};
	struct PipelineTag {};
	struct RenderPassTag {};
	struct DescriptorSetLayoutTag {};
	struct SamplerTag {};
	struct DescriptorSetTag {};
	struct DescriptorPoolTag {};
	struct FramebufferTag {};
	struct CommandPoolTag {};
	struct SwapchainTag {};
	struct AccelerationStructureTag {};

	using BufferHandle = Handle<BufferTag>;
	using ImageHandle = Handle<ImageTag>;
	using InstanceHandle = Handle<InstanceTag>;
	using PhysicalDeviceHandle = Handle<PhysicalDeviceTag>;
	using DebugUtilsMessengerHandle = Handle<DebugUtilsMessengerTag>;
	using SurfaceHandle = Handle<SurfaceTag>;
	using DeviceHandle = Handle<DeviceTag>;
	using QueueHandle = Handle<QueueTag>;
	using AllocatorHandle = Handle<AllocatorTag>;
	using AllocationHandle = Handle<AllocationTag>;
	using SemaphoreHandle = Handle<SemaphoreTag>;
	using CommandBufferHandle = Handle<CommandBufferTag>;
	using FenceHandle = Handle<FenceTag>;
	using DeviceMemoryHandle = Handle<DeviceMemoryTag>;
	using EventHandle = Handle<EventTag>;
	using QueryPoolHandle = Handle<QueryPoolTag>;
	using BufferViewHandle = Handle<BufferViewTag>;
	using ImageViewHandle = Handle<ImageViewTag>;
	using ShaderModuleHandle = Handle<ShaderModuleTag>;
	using PipelineCacheHandle = Handle<PipelineCacheTag>;
	using PipelineLayoutHandle = Handle<PipelineLayoutTag>;
	using PipelineHandle = Handle<PipelineTag>;
	using RenderPassHandle = Handle<RenderPassTag>;
	using DescriptorSetLayoutHandle = Handle<DescriptorSetLayoutTag>;
	using SamplerHandle = Handle<SamplerTag>;
	using DescriptorSetHandle = Handle<DescriptorSetTag>;
	using DescriptorPoolHandle = Handle<DescriptorPoolTag>;
	using FramebufferHandle = Handle<FramebufferTag>;
	using CommandPoolHandle = Handle<CommandPoolTag>;
	using SwapchainHandle = Handle<SwapchainTag>;
	using AccelerationStructureHandle = Handle<AccelerationStructureTag>;

	class NonCopyable
	{
	public:
		NonCopyable() = default;
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;
	};

	struct QueueContext {
		QueueContext() {}
		QueueHandle Handle{ };
		uint32_t FamilyIndex{ 0 };
		std::mutex Mutex;
	};
}