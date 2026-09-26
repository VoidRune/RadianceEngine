#include "VulkanUtilities.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/DescriptorWrite.h>
#include <vulkan/vk_enum_string_helper.h>
#include <algorithm>
#include <cstring>
#include <format>
#include <iterator>
#include <set>
#include <string>

/* Needed for surface creation */
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Rdn
{
    namespace
    {
        VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData)
        {
            switch (messageSeverity)
            {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                RDN_LOG_ERROR("{}", pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                RDN_LOG_WARNING("{}", pCallbackData->pMessage);
                break;
            default:
                break;
            }
            return VK_FALSE;
        }

        VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo()
        {
            VkDebugUtilsMessengerCreateInfoEXT createInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = DebugCallback;
            return createInfo;
        }

        bool IsInstanceLayerAvailable(const char* name)
        {
            uint32_t count = 0;
            vkEnumerateInstanceLayerProperties(&count, nullptr);
            std::vector<VkLayerProperties> layers(count);
            vkEnumerateInstanceLayerProperties(&count, layers.data());
            return std::ranges::any_of(layers, [&](const VkLayerProperties& layer) { return strcmp(layer.layerName, name) == 0; });
        }

        std::vector<VkExtensionProperties> EnumerateInstanceExtensions(const char* layer)
        {
            uint32_t count = 0;
            vkEnumerateInstanceExtensionProperties(layer, &count, nullptr);
            std::vector<VkExtensionProperties> extensions(count);
            vkEnumerateInstanceExtensionProperties(layer, &count, extensions.data());
            return extensions;
        }

        std::vector<VkExtensionProperties> EnumerateDeviceExtensions(VkPhysicalDevice device)
        {
            uint32_t count = 0;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
            std::vector<VkExtensionProperties> extensions(count);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
            return extensions;
        }

        bool HasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name)
        {
            return std::ranges::any_of(extensions, [&](const VkExtensionProperties& extension) { return strcmp(extension.extensionName, name) == 0; });
        }

        struct DeviceFeatureChain
        {
            VkPhysicalDeviceFeatures2 Core{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
            VkPhysicalDeviceVulkan12Features Vulkan12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
            VkPhysicalDeviceVulkan13Features Vulkan13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
            VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelerationStructure{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
            VkPhysicalDeviceRayTracingPipelineFeaturesKHR RayTracingPipeline{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };

            DeviceFeatureChain()
            {
                Core.pNext = &Vulkan12;
                Vulkan12.pNext = &Vulkan13;
                Vulkan13.pNext = &AccelerationStructure;
                AccelerationStructure.pNext = &RayTracingPipeline;
            }
            DeviceFeatureChain(const DeviceFeatureChain&) = delete;
            DeviceFeatureChain& operator=(const DeviceFeatureChain&) = delete;
        };

        struct DeviceFeature
        {
            const char* Name;
            VkBool32& (*Get)(DeviceFeatureChain&);
        };

#define RDN_DEVICE_FEATURE(member) DeviceFeature{ #member, [](DeviceFeatureChain& chain) -> VkBool32& { return chain.member; } }

        const DeviceFeature RequiredFeatures[] = {
            RDN_DEVICE_FEATURE(Vulkan12.bufferDeviceAddress),
            RDN_DEVICE_FEATURE(Vulkan12.descriptorIndexing),
            RDN_DEVICE_FEATURE(Vulkan12.scalarBlockLayout),
            RDN_DEVICE_FEATURE(Vulkan12.runtimeDescriptorArray),
            RDN_DEVICE_FEATURE(Vulkan12.descriptorBindingPartiallyBound),
            RDN_DEVICE_FEATURE(Vulkan12.descriptorBindingVariableDescriptorCount),
            RDN_DEVICE_FEATURE(Vulkan12.descriptorBindingSampledImageUpdateAfterBind),
            RDN_DEVICE_FEATURE(Vulkan12.descriptorBindingStorageBufferUpdateAfterBind),
            RDN_DEVICE_FEATURE(Vulkan12.shaderSampledImageArrayNonUniformIndexing),
            RDN_DEVICE_FEATURE(Vulkan13.dynamicRendering),
            RDN_DEVICE_FEATURE(Vulkan13.synchronization2),
            RDN_DEVICE_FEATURE(AccelerationStructure.accelerationStructure),
            RDN_DEVICE_FEATURE(RayTracingPipeline.rayTracingPipeline),
        };

        const DeviceFeature OptionalFeatures[] = {
            RDN_DEVICE_FEATURE(Vulkan12.hostQueryReset),
            RDN_DEVICE_FEATURE(RayTracingPipeline.rayTraversalPrimitiveCulling),
        };

#undef RDN_DEVICE_FEATURE

        const char* const RequiredDeviceExtensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        };

        const char* const OptionalDeviceExtensions[] = {
            VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        };

        QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            uint32_t count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

            constexpr VkQueueFlags graphicsAndCompute = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            QueueFamilyIndices indices;
            for (uint32_t i = 0; i < count; i++)
            {
                const bool graphics = (families[i].queueFlags & graphicsAndCompute) == graphicsAndCompute;
                VkBool32 present = VK_FALSE;
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present));

                if (graphics && present)
                    return { i, i };
                if (graphics && indices.GraphicsIndex == uint32_t(-1))
                    indices.GraphicsIndex = i;
                if (present && indices.PresentIndex == uint32_t(-1))
                    indices.PresentIndex = i;
            }
            return indices;
        }

        std::vector<std::string> FindMissingRequirements(VkPhysicalDevice device, VkSurfaceKHR surface, QueueFamilyIndices& queueFamilies)
        {
            std::vector<std::string> missing;

            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);
            if (properties.apiVersion < VK_API_VERSION_1_3)
            {
                missing.push_back(std::format("Vulkan 1.3 (it has {}.{})", VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion)));
                return missing;
            }

            const std::vector<VkExtensionProperties> extensions = EnumerateDeviceExtensions(device);
            for (const char* extension : RequiredDeviceExtensions)
            {
                if (!HasExtension(extensions, extension))
                    missing.push_back(extension);
            }

            if (missing.empty())
            {
                DeviceFeatureChain supported;
                vkGetPhysicalDeviceFeatures2(device, &supported.Core);
                for (const DeviceFeature& feature : RequiredFeatures)
                {
                    if (!feature.Get(supported))
                        missing.push_back(feature.Name);
                }
            }

            queueFamilies = FindQueueFamilies(device, surface);
            if (queueFamilies.GraphicsIndex == uint32_t(-1))
                missing.push_back("a graphics + compute queue");
            if (queueFamilies.PresentIndex == uint32_t(-1))
                missing.push_back("presentation to the window surface");
            return missing;
        }

        uint64_t ScoreDevice(VkPhysicalDevice device)
        {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);
            VkPhysicalDeviceMemoryProperties memory;
            vkGetPhysicalDeviceMemoryProperties(device, &memory);

            uint64_t deviceLocalBytes = 0;
            for (uint32_t i = 0; i < memory.memoryHeapCount; i++)
            {
                if (memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                    deviceLocalBytes = std::max(deviceLocalBytes, uint64_t(memory.memoryHeaps[i].size));
            }

            uint64_t typeRank = 0;
            switch (properties.deviceType)
            {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   typeRank = 4; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: typeRank = 3; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    typeRank = 2; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            typeRank = 1; break;
            default:                                     break;
            }
            return (typeRank << 48) + (deviceLocalBytes >> 20) + 1;
        }

        std::string Join(const std::vector<std::string>& items)
        {
            std::string joined;
            for (const std::string& item : items)
                joined += (joined.empty() ? "" : ", ") + item;
            return joined;
        }

        VkSurfaceFormatKHR FindSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkFormat preferredFormat)
        {
            uint32_t formatCount = 0;
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr));
            std::vector<VkSurfaceFormatKHR> formats(formatCount);
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()));

            for (const VkSurfaceFormatKHR& format : formats)
            {
                if (format.format == preferredFormat && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                    return format;
            }
            return formats.empty() ? VkSurfaceFormatKHR{ preferredFormat, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
        }

        VkPresentModeKHR FindPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkPresentModeKHR preferredMode)
        {
            uint32_t presentModeCount = 0;
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));
            std::vector<VkPresentModeKHR> presentModes(presentModeCount);
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));

            return std::ranges::find(presentModes, preferredMode) != presentModes.end() ? preferredMode : VK_PRESENT_MODE_FIFO_KHR;
        }

        VkExtent2D FindSurfaceExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D fallback)
        {
            if (capabilities.currentExtent.width != UINT32_MAX)
                return capabilities.currentExtent;

            return {
                std::clamp(fallback.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                std::clamp(fallback.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
            };
        }
    }

    const char* GetVulkanResultString(VkResult vkResult)
    {
        return string_VkResult(vkResult);
    }

	InstanceOutput CreateInstanceHandle(const InstanceCreateInfo& info)
	{
        if (volkInitialize() != VK_SUCCESS)
        {
            RDN_LOG_ERROR("Failed to initialize volk!");
            abort();
        }

        VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.pApplicationName = info.applicationName;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = info.engineName;
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_MAKE_API_VERSION(0, info.apiVersion.Major, info.apiVersion.Minor, 0);

        InstanceOutput output;
        std::vector<const char*> extensions(info.instanceExtensions.begin(), info.instanceExtensions.end());
        std::vector<const char*> layers;

        if (info.enableValidationLayers)
        {
            constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";
            output.validationEnabled = IsInstanceLayerAvailable(validationLayer);
            if (output.validationEnabled)
            {
                layers.push_back(validationLayer);
            }
            else
            {
                RDN_LOG_WARNING("{} is not installed, running without validation", validationLayer);
            }

            output.debugUtilsEnabled = HasExtension(EnumerateInstanceExtensions(nullptr), VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
                || (output.validationEnabled && HasExtension(EnumerateInstanceExtensions(validationLayer), VK_EXT_DEBUG_UTILS_EXTENSION_NAME));
            if (output.debugUtilsEnabled)
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = MakeDebugMessengerCreateInfo();

        VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        createInfo.pNext = output.debugUtilsEnabled ? &debugCreateInfo : nullptr;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkInstance instance;
        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
        volkLoadInstance(instance);
        output.instance = fromVk(instance);
        return output;
	}

    DebugUtilsMessengerHandle CreateDebugUtilsMessengerHandle(const DebugUtilsMessengerCreateInfo& info)
    {
        VkDebugUtilsMessengerEXT debugUtilsMessenger = VK_NULL_HANDLE;
        if (info.enableDebugUtilsMessenger)
        {
            VkDebugUtilsMessengerCreateInfoEXT createInfo = MakeDebugMessengerCreateInfo();
            VK_CHECK(vkCreateDebugUtilsMessengerEXT(toVk(info.instance), &createInfo, nullptr, &debugUtilsMessenger));
        }
        return fromVk(debugUtilsMessenger);
    }

    SurfaceHandle CreateSurfaceHandle(const SurfaceCreateInfo& info)
    {
        VkSurfaceKHR surface;
        VK_CHECK(glfwCreateWindowSurface(toVk(info.instance), (GLFWwindow*)info.windowHandle, nullptr, &surface));
        return fromVk(surface);
    }

    PhysicalDeviceSelection SelectPhysicalDevice(const PhysicalDeviceSelectInfo& info)
    {
        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(toVk(info.instance), &deviceCount, nullptr));
        std::vector<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(toVk(info.instance), &deviceCount, devices.data()));

        PhysicalDeviceSelection selection;
        uint64_t bestScore = 0;
        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);

            QueueFamilyIndices queueFamilies;
            const std::vector<std::string> missing = FindMissingRequirements(device, toVk(info.surface), queueFamilies);
            if (!missing.empty())
            {
                RDN_LOG_WARNING("Skipping {}: it lacks {}", properties.deviceName, Join(missing));
                continue;
            }

            const uint64_t score = ScoreDevice(device);
            if (score > bestScore)
            {
                bestScore = score;
                selection = { fromVk(device), queueFamilies };
            }
        }

        if (!selection.physicalDevice)
        {
            RDN_LOG_FATAL("None of the {} Vulkan devices supports what the renderer needs", deviceCount);
            abort();
        }
        return selection;
    }

    DeviceOutput CreateLogicalDeviceHandle(const DeviceCreateInfo& info)
    {
        const VkPhysicalDevice physicalDevice = toVk(info.physicalDevice);

        const float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        for (uint32_t queueFamily : std::set<uint32_t>{ info.queueFamilyIndices.GraphicsIndex, info.queueFamilyIndices.PresentIndex })
        {
            VkDeviceQueueCreateInfo queueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        DeviceFeatureChain supported;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &supported.Core);

        DeviceFeatureChain enabled;
        for (const DeviceFeature& feature : RequiredFeatures)
            feature.Get(enabled) = VK_TRUE;
        for (const DeviceFeature& feature : OptionalFeatures)
            feature.Get(enabled) = feature.Get(supported);

        DeviceOutput output;
        output.enabledExtensions.assign(std::begin(RequiredDeviceExtensions), std::end(RequiredDeviceExtensions));
        const std::vector<VkExtensionProperties> availableExtensions = EnumerateDeviceExtensions(physicalDevice);
        for (const char* extension : OptionalDeviceExtensions)
        {
            if (HasExtension(availableExtensions, extension))
                output.enabledExtensions.push_back(extension);
        }

        VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        createInfo.pNext = &enabled.Core;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(output.enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = output.enabledExtensions.data();

        VkDevice device;
        VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));
        volkLoadDevice(device);
        output.device = fromVk(device);
        return output;
    }

    SwapchainOutput CreateSwapchainHandle(const SwapchainCreateInfo& info)
    {
        VkSurfaceCapabilitiesKHR capabilities;
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(toVk(info.physicalDevice), toVk(info.surface), &capabilities));

        // A swapchain can't have a zero extent. Bail before vkCreateSwapchainKHR, which would retire oldSwapchain.
        const VkExtent2D extent = FindSurfaceExtent(capabilities, VkExtent2D{ 128, 128 });
        if (extent.width == 0 || extent.height == 0)
            return {};

        VkSurfaceFormatKHR surfaceFormat = FindSurfaceFormat(toVk(info.physicalDevice), toVk(info.surface), VK_FORMAT_B8G8R8A8_UNORM);
        VkPresentModeKHR presentMode = FindPresentMode(toVk(info.physicalDevice), toVk(info.surface), toVk(info.presentMode));

        // One more than the minimum, so acquiring rarely has to wait for the presentation engine
        uint32_t imageCount = info.desiredImageCount ? info.desiredImageCount : capabilities.minImageCount + 1;
        imageCount = std::max(imageCount, capabilities.minImageCount);
        if (capabilities.maxImageCount > 0)
            imageCount = std::min(imageCount, capabilities.maxImageCount);

        VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        for (VkCompositeAlphaFlagBitsKHR candidate : { VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR })
        {
            if (capabilities.supportedCompositeAlpha & candidate)
            {
                compositeAlpha = candidate;
                break;
            }
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = toVk(info.surface);
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        // COLOR_ATTACHMENT is always supported, the transfer usages almost always are
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | (capabilities.supportedUsageFlags & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));

        uint32_t indices[] = { info.graphicsFamilyIndex, info.presentFamilyIndex };
        if (indices[0] != indices[1])
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = indices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = compositeAlpha;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = toVk(info.oldSwapchain);

        VkSwapchainKHR swapchain;
        VK_CHECK(vkCreateSwapchainKHR(toVk(info.logicalDevice), &createInfo, nullptr, &swapchain));

        SwapchainOutput output;
        output.swapchain = fromVk(swapchain);
        output.surfaceFormat = fromVk(surfaceFormat.format);
        output.presentMode = static_cast<PresentMode>(presentMode);
        output.extent = { extent.width, extent.height };
        return output;
    }

    std::vector<ImageHandle> RetrieveSwapchainImages(const SwapchainImagesRetrieveInfo& info)
    {
        uint32_t imageCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(toVk(info.logicalDevice), toVk(info.swapchain), &imageCount, nullptr));
        std::vector<VkImage> images(imageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(toVk(info.logicalDevice), toVk(info.swapchain), &imageCount, images.data()));

        std::vector<ImageHandle> imageHandles;
        imageHandles.reserve(imageCount);
        for (VkImage image : images)
            imageHandles.push_back(fromVk(image));
        return imageHandles;
    }

    FenceHandle CreateFenceHandle(const FenceCreateInfo& info)
    {
        VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = info.createSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

        VkFence fence;
        VK_CHECK(vkCreateFence(toVk(info.logicalDevice), &fenceInfo, nullptr, &fence));
        return fromVk(fence);
    }

    SemaphoreHandle CreateSemaphoreHandle(const SemaphoreCreateInfo& info)
    {
        VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        VkSemaphore semaphore;
        VK_CHECK(vkCreateSemaphore(toVk(info.logicalDevice), &semaphoreInfo, nullptr, &semaphore));
        return fromVk(semaphore);
    }

    VulkanDescriptorWrites::VulkanDescriptorWrites(const DescriptorWrite& write, VkDescriptorSet dstSet)
    {
        const std::vector<BufferWrite>& bufferWrites = write.GetBufferWrites();
        const std::vector<ImageWrite>& imageWrites = write.GetImageWrites();
        const std::vector<AccelerationStructureWrite>& accelerationStructureWrites = write.GetAccelerationStructureWrites();

        Writes.reserve(bufferWrites.size() + imageWrites.size() + accelerationStructureWrites.size());
        BufferInfos.reserve(bufferWrites.size());
        ImageInfos.reserve(imageWrites.size());
        AccelerationStructures.reserve(accelerationStructureWrites.size());
        AccelerationStructureInfos.reserve(accelerationStructureWrites.size());

        auto addWrite = [&](uint32_t binding, uint32_t arrayElement, DescriptorType type) -> VkWriteDescriptorSet& {
            VkWriteDescriptorSet& writeInfo = Writes.emplace_back(VkWriteDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET });
            writeInfo.dstSet = dstSet;
            writeInfo.dstBinding = binding;
            writeInfo.dstArrayElement = arrayElement;
            writeInfo.descriptorCount = 1;
            writeInfo.descriptorType = toVk(type);
            return writeInfo;
        };

        for (const BufferWrite& bw : bufferWrites)
        {
            BufferInfos.push_back({ toVk(bw.Buffer), bw.Offset, bw.Size });
            addWrite(bw.Binding, bw.ArrayElement, bw.Type).pBufferInfo = &BufferInfos.back();
        }

        for (const ImageWrite& iw : imageWrites)
        {
            ImageInfos.push_back({ toVk(iw.Sampler), toVk(iw.ImageView), toVk(iw.ImageLayout) });
            addWrite(iw.Binding, iw.ArrayElement, iw.Type).pImageInfo = &ImageInfos.back();
        }

        for (const AccelerationStructureWrite& aw : accelerationStructureWrites)
        {
            AccelerationStructures.push_back(toVk(aw.AccelerationStructure));
            VkWriteDescriptorSetAccelerationStructureKHR& info = AccelerationStructureInfos.emplace_back(
                VkWriteDescriptorSetAccelerationStructureKHR{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR });
            info.accelerationStructureCount = 1;
            info.pAccelerationStructures = &AccelerationStructures.back();
            addWrite(aw.Binding, aw.ArrayElement, aw.Type).pNext = &info;
        }
    }


}