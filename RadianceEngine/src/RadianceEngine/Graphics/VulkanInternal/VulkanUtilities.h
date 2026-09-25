#pragma once
#include <RadianceEngine/Graphics/Handle.h>
#include <RadianceEngine/Graphics/Common.h>
#define VK_NO_PROTOTYPE
#include <volk/volk.h>
#include <VulkanMemoryAllocator/vk_mem_alloc.h>
#include <vector>
#include <iostream>

namespace Rdn
{
	const char* GetVulkanResultString(VkResult result);

	#if defined(DEBUG) || defined(RELEASE)
	#define VK_CHECK(x)															\
		{																		\
			VkResult err_temp = x;												\
			if (err_temp != VK_SUCCESS)														\
			{																	\
				std::cout <<"Detected Vulkan error: " << Rdn::GetVulkanResultString(err_temp) << std::endl; \
				abort();														\
			}																	\
		}
	#else
	#define VK_CHECK(x) x
	#endif

	inline VkInstance					toVk(InstanceHandle h) { return reinterpret_cast<VkInstance>(static_cast<uintptr_t>(h.Id)); }
	inline VkPhysicalDevice				toVk(PhysicalDeviceHandle h) { return reinterpret_cast<VkPhysicalDevice>(static_cast<uintptr_t>(h.Id)); }
	inline VkDevice						toVk(DeviceHandle h) { return reinterpret_cast<VkDevice>(static_cast<uintptr_t>(h.Id)); }
	inline VkQueue						toVk(QueueHandle h) { return reinterpret_cast<VkQueue>(static_cast<uintptr_t>(h.Id)); }
	inline VkCommandBuffer				toVk(CommandBufferHandle h) { return reinterpret_cast<VkCommandBuffer>(static_cast<uintptr_t>(h.Id)); }

	inline VkBuffer						toVk(BufferHandle h) { return reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(h.Id)); }
	inline VkImage						toVk(ImageHandle h) { return reinterpret_cast<VkImage>(static_cast<uintptr_t>(h.Id)); }
	inline VkDebugUtilsMessengerEXT		toVk(DebugUtilsMessengerHandle h) { return reinterpret_cast<VkDebugUtilsMessengerEXT>(static_cast<uintptr_t>(h.Id)); }
	inline VkSurfaceKHR					toVk(SurfaceHandle h) { return reinterpret_cast<VkSurfaceKHR>(static_cast<uintptr_t>(h.Id)); }
	inline VkSemaphore					toVk(SemaphoreHandle h) { return reinterpret_cast<VkSemaphore>(static_cast<uintptr_t>(h.Id)); }
	inline VkFence						toVk(FenceHandle h) { return reinterpret_cast<VkFence>(static_cast<uintptr_t>(h.Id)); }
	inline VkDeviceMemory				toVk(DeviceMemoryHandle h) { return reinterpret_cast<VkDeviceMemory>(static_cast<uintptr_t>(h.Id)); }
	inline VkEvent						toVk(EventHandle h) { return reinterpret_cast<VkEvent>(static_cast<uintptr_t>(h.Id)); }
	inline VkQueryPool					toVk(QueryPoolHandle h) { return reinterpret_cast<VkQueryPool>(static_cast<uintptr_t>(h.Id)); }
	inline VkBufferView					toVk(BufferViewHandle h) { return reinterpret_cast<VkBufferView>(static_cast<uintptr_t>(h.Id)); }
	inline VkImageView					toVk(ImageViewHandle h) { return reinterpret_cast<VkImageView>(static_cast<uintptr_t>(h.Id)); }
	inline VkShaderModule				toVk(ShaderModuleHandle h) { return reinterpret_cast<VkShaderModule>(static_cast<uintptr_t>(h.Id)); }
	inline VkPipelineCache				toVk(PipelineCacheHandle h) { return reinterpret_cast<VkPipelineCache>(static_cast<uintptr_t>(h.Id)); }
	inline VkPipelineLayout				toVk(PipelineLayoutHandle h) { return reinterpret_cast<VkPipelineLayout>(static_cast<uintptr_t>(h.Id)); }
	inline VkPipeline					toVk(PipelineHandle h) { return reinterpret_cast<VkPipeline>(static_cast<uintptr_t>(h.Id)); }
	inline VkRenderPass					toVk(RenderPassHandle h) { return reinterpret_cast<VkRenderPass>(static_cast<uintptr_t>(h.Id)); }
	inline VkDescriptorSetLayout		toVk(DescriptorSetLayoutHandle h) { return reinterpret_cast<VkDescriptorSetLayout>(static_cast<uintptr_t>(h.Id)); }
	inline VkSampler					toVk(SamplerHandle h) { return reinterpret_cast<VkSampler>(static_cast<uintptr_t>(h.Id)); }
	inline VkDescriptorSet				toVk(DescriptorSetHandle h) { return reinterpret_cast<VkDescriptorSet>(static_cast<uintptr_t>(h.Id)); }
	inline VkDescriptorPool				toVk(DescriptorPoolHandle h) { return reinterpret_cast<VkDescriptorPool>(static_cast<uintptr_t>(h.Id)); }
	inline VkFramebuffer				toVk(FramebufferHandle h) { return reinterpret_cast<VkFramebuffer>(static_cast<uintptr_t>(h.Id)); }
	inline VkCommandPool				toVk(CommandPoolHandle h) { return reinterpret_cast<VkCommandPool>(static_cast<uintptr_t>(h.Id)); }
	inline VkSwapchainKHR				toVk(SwapchainHandle h) { return reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(h.Id)); }
	inline VkAccelerationStructureKHR	toVk(AccelerationStructureHandle h) { return reinterpret_cast<VkAccelerationStructureKHR>(static_cast<uintptr_t>(h.Id)); }
	inline VmaAllocator					toVk(AllocatorHandle h) { return reinterpret_cast<VmaAllocator>(static_cast<uintptr_t>(h.Id)); }
	inline VmaAllocation				toVk(AllocationHandle h) { return reinterpret_cast<VmaAllocation>(static_cast<uintptr_t>(h.Id)); }

	inline InstanceHandle               fromVk(VkInstance vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline PhysicalDeviceHandle         fromVk(VkPhysicalDevice vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline DeviceHandle                 fromVk(VkDevice vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline QueueHandle                  fromVk(VkQueue vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline CommandBufferHandle          fromVk(VkCommandBuffer vk) { return { reinterpret_cast<uint64_t>(vk) }; }

	inline BufferHandle                 fromVk(VkBuffer vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline ImageHandle                  fromVk(VkImage vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline DebugUtilsMessengerHandle    fromVk(VkDebugUtilsMessengerEXT vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline SurfaceHandle                fromVk(VkSurfaceKHR vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline SemaphoreHandle              fromVk(VkSemaphore vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline FenceHandle                  fromVk(VkFence vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline DeviceMemoryHandle           fromVk(VkDeviceMemory vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline EventHandle                  fromVk(VkEvent vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline QueryPoolHandle              fromVk(VkQueryPool vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline BufferViewHandle             fromVk(VkBufferView vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline ImageViewHandle              fromVk(VkImageView vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline ShaderModuleHandle           fromVk(VkShaderModule vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline PipelineCacheHandle          fromVk(VkPipelineCache vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline PipelineLayoutHandle         fromVk(VkPipelineLayout vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline PipelineHandle               fromVk(VkPipeline vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline RenderPassHandle             fromVk(VkRenderPass vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline DescriptorSetLayoutHandle    fromVk(VkDescriptorSetLayout vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline SamplerHandle                fromVk(VkSampler vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline DescriptorSetHandle          fromVk(VkDescriptorSet vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline DescriptorPoolHandle         fromVk(VkDescriptorPool vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline FramebufferHandle            fromVk(VkFramebuffer vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline CommandPoolHandle            fromVk(VkCommandPool vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline SwapchainHandle              fromVk(VkSwapchainKHR vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline AccelerationStructureHandle  fromVk(VkAccelerationStructureKHR vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline AllocatorHandle				fromVk(VmaAllocator vk) { return { reinterpret_cast<uint64_t>(vk) }; }
	inline AllocationHandle				fromVk(VmaAllocation vk) { return { reinterpret_cast<uint64_t>(vk) }; }

	// Currently enum values must match Vulkan integer values precisely
	// Another approach is array lookup
	inline VkPresentModeKHR				toVk(PresentMode e) { return static_cast<VkPresentModeKHR>(e); }
	inline VkFormat						toVk(Format e) { return static_cast<VkFormat>(e); }
	// PipelineStage/AccessMask include sync2-only values above bit 31 (Copy, Resolve, Blit, Clear,
	// IndexInput, VertexAttributeInput, PreRasterizationShaders, ShaderSampledRead, ShaderStorageRead,
	// ShaderStorageWrite, ...). These are consumed exclusively by the sync2 barrier path
	// (VkImageMemoryBarrier2/VkBufferMemoryBarrier2, both *Flags2 fields), so return the 64-bit
	// *Flags2 types here -- returning the legacy 32-bit VkPipelineStageFlags/VkAccessFlags silently
	// truncated any of those high-bit values to 0 before they ever reached the barrier.
	inline VkPipelineStageFlags2		toVk(PipelineStage e) { return static_cast<VkPipelineStageFlags2>(e); }
	inline VkAccessFlags2				toVk(AccessMask e) { return static_cast<VkAccessFlags2>(e); }
	inline VkImageAspectFlags			toVk(ImageAspect e) { return static_cast<VkImageAspectFlags>(e); }
	inline VkImageLayout				toVk(ImageLayout e) { return static_cast<VkImageLayout>(e); }
	inline VkImageUsageFlags			toVk(ImageUsage e) { return static_cast<VkImageUsageFlags>(e); }
	inline VkBufferUsageFlags			toVk(BufferUsage e) { return static_cast<VkBufferUsageFlags>(e); }
	inline VkMemoryPropertyFlags		toVk(MemoryProperty e) { return static_cast<VkMemoryPropertyFlags>(e); }
	inline VkShaderStageFlagBits		toVk(ShaderStage e) { return static_cast<VkShaderStageFlagBits>(e); }
	inline VkDescriptorType				toVk(DescriptorType e) { return static_cast<VkDescriptorType>(e); }
	inline VkDescriptorBindingFlags		toVk(DescriptorFlag e) { return static_cast<VkDescriptorBindingFlags>(e); }
	inline VkPrimitiveTopology			toVk(PrimitiveTopology e) { return static_cast<VkPrimitiveTopology>(e); }
	inline VkCullModeFlags				toVk(CullMode e) { return static_cast<VkCullModeFlags>(e); }
	inline VkCompareOp					toVk(CompareOperation e) { return static_cast<VkCompareOp>(e); }
	inline VkPipelineBindPoint			toVk(PipelineBindPoint e) { return static_cast<VkPipelineBindPoint>(e); }
	inline VkAttachmentLoadOp			toVk(AttachmentLoadOp e) { return static_cast<VkAttachmentLoadOp>(e); }
	inline VkAttachmentStoreOp			toVk(AttachmentStoreOp e) { return static_cast<VkAttachmentStoreOp>(e); }
	inline VkFilter						toVk(Filter e) { return static_cast<VkFilter>(e); }
	inline VkSamplerAddressMode			toVk(SamplerAddressMode e) { return static_cast<VkSamplerAddressMode>(e); }

	inline Format						fromVk(VkFormat e) { return static_cast<Format>(e); }


	struct QueueFamilyIndices
	{
		uint32_t GraphicsIndex = uint32_t(-1);
		uint32_t PresentIndex = uint32_t(-1);
		//uint32_t TransferIndex = uint32_t(-1);
	};

	struct ApiVersion
	{
		uint32_t Major = 0;
		uint32_t Minor = 0;
	};

	/* INSTANCE */
	struct InstanceCreateInfo
	{
		const char* applicationName = "";
		const char* engineName = "";
		ApiVersion apiVersion = ApiVersion(1, 3);
		const std::vector<const char*>& instanceExtensions = {};
		bool enableValidationLayers = false;
	};
	InstanceHandle CreateInstanceHandle(InstanceCreateInfo& info);

	/* DEBUG UTILS MESSENGER */
	struct DebugUtilsMessengerCreateInfo
	{
		InstanceHandle instance = {};
		bool enableDebugUtilsMessenger = false;
	};
	DebugUtilsMessengerHandle CreateDebugUtilsMessengerHandle(DebugUtilsMessengerCreateInfo& info);

	/* PHYSICAL DEVICE */
	struct PhysicalDeviceSelectInfo
	{
		InstanceHandle instance = {};
	};
	PhysicalDeviceHandle SelectPhysicalDeviceHandle(PhysicalDeviceSelectInfo& info);

	/* SURFACE */
	struct SurfaceCreateInfo
	{
		InstanceHandle instance = {};
		void* windowHandle = {};
	};
	SurfaceHandle CreateSurfaceHandle(SurfaceCreateInfo& info);

	/* QUEUE FAMILIES */
	struct QueueFamilySelectInfo
	{
		PhysicalDeviceHandle physicalDevice = {};
		SurfaceHandle surface = {};
	};
	QueueFamilyIndices SelectQueueFamilies(QueueFamilySelectInfo& info);

	/* LOGICAL DEVICE */
	struct DeviceCreateInfo
	{
		PhysicalDeviceHandle physicalDevice = {};
		QueueFamilyIndices queueFamilyIndices = {};
	};
	DeviceHandle CreateLogicalDeviceHandle(DeviceCreateInfo& info);

	/* SWAPCHAIN */
	struct SwapchainCreateInfo
	{
		InstanceHandle instance = {};
		PhysicalDeviceHandle physicalDevice = {};
		DeviceHandle logicalDevice = {};
		SurfaceHandle surface = {};
		uint32_t graphicsFamilyIndex = {};
		uint32_t presentFamilyIndex = {};
		uint32_t desiredImageCount = {}; // 0: one more than the surface minimum
		PresentMode presentMode = {};
		SwapchainHandle oldSwapchain = {};
	};

	struct SwapchainOutput
	{
		SwapchainHandle swapchain = {}; // null while the surface has no area (minimized)
		Format surfaceFormat = {};
		PresentMode presentMode = {};
		uint32_t extent[3] = {};
	};
	SwapchainOutput CreateSwapchainHandle(SwapchainCreateInfo& info);

	struct SwapchainImagesRetreiveInfo
	{
		DeviceHandle logicalDevice = {};
		SwapchainHandle swapchain = {};
	};
	std::vector<ImageHandle> RetreiveSwapchainImages(SwapchainImagesRetreiveInfo& info);

	struct FenceCreateInfo
	{
		DeviceHandle logicalDevice = {};
		bool createSignaled = {};
	};
	FenceHandle CreateFenceHandle(FenceCreateInfo& info);

	struct SemaphoreCreateInfo
	{
		DeviceHandle logicalDevice = {};
	};
	SemaphoreHandle CreateSemaphoreHandle(SemaphoreCreateInfo& info);

}