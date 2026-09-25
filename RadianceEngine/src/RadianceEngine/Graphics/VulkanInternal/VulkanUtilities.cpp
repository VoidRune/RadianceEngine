#include "VulkanUtilities.h"
#include <RadianceEngine/Core/Log.h>
#include <vulkan/vk_enum_string_helper.h>
#include <algorithm>
#include <set>

/* Needed for surface creation */
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Rdn 
{
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

        switch (messageSeverity)
        {
        // The message must be an argument, never the format string: validation messages
        // routinely contain '{' / '}' (struct dumps), which makes std::vformat throw.
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            RDN_LOG_ERROR("{}", pCallbackData->pMessage);

            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            RDN_LOG_WARNING("{}", pCallbackData->pMessage);

            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:

            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:

            break;
        }

        return VK_FALSE;
    }

    const char* GetVulkanResultString(VkResult vkResult)
    {
        return string_VkResult(vkResult);
    }

	InstanceHandle CreateInstanceHandle(InstanceCreateInfo& info)
	{
        if (volkInitialize() != VK_SUCCESS)
        {
            RDN_LOG_ERROR("Failed to initialize volk!");
            abort();
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext = nullptr;
        appInfo.pApplicationName = info.applicationName;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = info.engineName;
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_MAKE_API_VERSION(0, info.apiVersion.Major, info.apiVersion.Minor, 0);

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.pNext = nullptr;

        uint32_t extensionCount = 0;
        std::vector<const char*> extensions;
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        for (const char* ext : info.instanceExtensions)
        {
            extensions.push_back(ext);
        }

        std::vector<const char*> validationLayers;
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};

        if (info.enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            validationLayers.push_back("VK_LAYER_KHRONOS_validation");

            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = debugCallback;

            createInfo.pNext = &debugCreateInfo;
        }

        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkInstance instance;
        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
        volkLoadInstance(instance);
        return fromVk(instance);
	}

    DebugUtilsMessengerHandle CreateDebugUtilsMessengerHandle(DebugUtilsMessengerCreateInfo& info)
    {
        VkDebugUtilsMessengerEXT debugUtilsMessenger = {};

        if (info.enableDebugUtilsMessenger)
        {
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = debugCallback;

            vkCreateDebugUtilsMessengerEXT(toVk(info.instance), &debugCreateInfo, nullptr, &debugUtilsMessenger);
        }

        return fromVk(debugUtilsMessenger);
    }

    PhysicalDeviceHandle SelectPhysicalDeviceHandle(PhysicalDeviceSelectInfo& info)
    {
        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(toVk(info.instance), &deviceCount, nullptr));

        if (deviceCount == 0)
        {
            RDN_LOG("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(toVk(info.instance), &deviceCount, devices.data()));

        int32_t bestScore = 0;
        VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

        for (VkPhysicalDevice& device : devices)
        {
            int32_t score = 0;

            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            switch (deviceProperties.deviceType)
            {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:      score += 100000; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:    score += 1000; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:       score += 10; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:               score += 1; break;
            }

            if (score > bestScore)
            {
                bestDevice = device;
                bestScore = score;
            }
        }

        if (bestDevice == VK_NULL_HANDLE)
        {
            RDN_LOG("Failed to find suitable GPU!");
        }

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(bestDevice, &properties);
        RDN_LOG(std::string("GPU used: ") + properties.deviceName);

        return fromVk(bestDevice);
    }

    SurfaceHandle CreateSurfaceHandle(SurfaceCreateInfo& info)
    {
        VkSurfaceKHR surface;
        VK_CHECK(glfwCreateWindowSurface(toVk(info.instance), (GLFWwindow*)info.windowHandle, nullptr, &surface));
        return fromVk(surface);
    }

    QueueFamilyIndices SelectQueueFamilies(QueueFamilySelectInfo& info)
    {
        QueueFamilyIndices queueIndices;

        queueIndices.GraphicsIndex = uint32_t(-1);
        queueIndices.PresentIndex = uint32_t(-1);
        //queueIndices.TransferIndex = uint32_t(-1);

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(toVk(info.physicalDevice), &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(toVk(info.physicalDevice), &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (VkQueueFamilyProperties& queueFamily : queueFamilies)
        {
            if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && 
                (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                queueIndices.GraphicsIndex = i;
            }

            VkBool32 presentSupport = false;
            VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(toVk(info.physicalDevice), i, toVk(info.surface), &presentSupport));
            if (presentSupport) {
                queueIndices.PresentIndex = i;
            }

            if (queueIndices.GraphicsIndex != uint32_t(-1) &&
                queueIndices.PresentIndex != uint32_t(-1))
            {
                break;
            }

            i++;
        }
        /*
        i = 0;
        for (VkQueueFamilyProperties& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                queueIndices.TransferIndex = i;

                if (queueFamily.queueFlags - VK_QUEUE_TRANSFER_BIT == 0)
                {
                    // Dedicated transfer queue
                    break;
                }
            }
            i++;
        }*/

        if (queueIndices.GraphicsIndex == uint32_t(-1))
        {
            RDN_LOG("Failed to find queue family indices with VK_QUEUE_GRAPHICS_BIT!");
        }

        return queueIndices;
    }

    DeviceHandle CreateLogicalDeviceHandle(DeviceCreateInfo& info)
    {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { info.queueFamilyIndices.GraphicsIndex, info.queueFamilyIndices.PresentIndex };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceVulkan11Features features_1_1 = {};
        features_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        features_1_1.pNext = nullptr;

        VkPhysicalDeviceVulkan12Features features_1_2 = {};
        features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features_1_2.pNext = &features_1_1;
        features_1_2.bufferDeviceAddress = VK_TRUE;
        features_1_2.descriptorIndexing = VK_TRUE;
        features_1_2.hostQueryReset = VK_TRUE;
        features_1_2.scalarBlockLayout = VK_TRUE;
        features_1_2.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
        features_1_2.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        features_1_2.descriptorBindingVariableDescriptorCount = VK_TRUE;
        features_1_2.descriptorBindingPartiallyBound = VK_TRUE;
        features_1_2.runtimeDescriptorArray = VK_TRUE;

        VkPhysicalDeviceVulkan13Features features_1_3 = {};
        features_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features_1_3.pNext = &features_1_2;
        features_1_3.dynamicRendering = VK_TRUE;
        features_1_3.synchronization2 = VK_TRUE;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures{};
        accelStructFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accelStructFeatures.pNext = &features_1_3;
        accelStructFeatures.accelerationStructure = VK_TRUE;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{};
        rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        rayTracingFeatures.pNext = &accelStructFeatures;
        rayTracingFeatures.rayTracingPipeline = VK_TRUE;
        rayTracingFeatures.rayTraversalPrimitiveCulling = VK_TRUE;

        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &rayTracingFeatures;
        features2.features = {};

        std::vector<const char*> deviceExtensions =
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
        };

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = &features2;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device;
        VK_CHECK(vkCreateDevice(toVk(info.physicalDevice), &createInfo, nullptr, &device));
        volkLoadDevice(device);
        return fromVk(device);
    }

    VkSurfaceFormatKHR FindSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkFormat preferredFormat);
    VkPresentModeKHR FindPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkPresentModeKHR preferredMode);
    VkExtent2D FindSurfaceExtent(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkExtent2D extent);

    SwapchainOutput CreateSwapchainHandle(SwapchainCreateInfo& info)
    {
        VkSurfaceCapabilitiesKHR capabilities;
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(toVk(info.physicalDevice), toVk(info.surface), &capabilities));

        // A swapchain can't have a zero extent. Bail before vkCreateSwapchainKHR, which would retire oldSwapchain.
        VkExtent2D extent = FindSurfaceExtent(toVk(info.physicalDevice), toVk(info.surface), VkExtent2D{ 128, 128 });
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
        output.extent[0] = extent.width;
        output.extent[1] = extent.height;
        output.extent[2] = 1;
        return output;
    }

    VkSurfaceFormatKHR FindSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkFormat preferredFormat)
    {
        uint32_t formatCount;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr));

        std::vector<VkSurfaceFormatKHR> formats;
        if (formatCount != 0)
        {
            formats.resize(formatCount);
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()));
        }

        if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
        {
            VkSurfaceFormatKHR format;
            format.format = VK_FORMAT_B8G8R8A8_UNORM;
            format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            return format;
        }

        for (VkSurfaceFormatKHR& availableFormat : formats) {
            if (availableFormat.format == preferredFormat &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return formats[0];
    }

    VkPresentModeKHR FindPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkPresentModeKHR preferredMode)
    {
        uint32_t presentModeCount;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));

        std::vector<VkPresentModeKHR> presentModes;
        if (presentModeCount != 0)
        {
            presentModes.resize(presentModeCount);
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));
        }

        for (const auto& availablePresentMode : presentModes) {
            if (availablePresentMode == preferredMode) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D FindSurfaceExtent(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkExtent2D extent)
    {
        VkExtent2D result;

        VkSurfaceCapabilitiesKHR capabilities;
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities));

        if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
        {
            result = capabilities.currentExtent;
        }
        else {

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(extent.width),
                static_cast<uint32_t>(extent.height)
            };

            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            result = actualExtent;
        }

        return result;
    }

    std::vector<ImageHandle> RetreiveSwapchainImages(SwapchainImagesRetreiveInfo& info)
    {
        uint32_t imageCount;
        VK_CHECK(vkGetSwapchainImagesKHR(toVk(info.logicalDevice), toVk(info.swapchain), &imageCount, nullptr));
        std::vector<VkImage> images(imageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(toVk(info.logicalDevice), toVk(info.swapchain), &imageCount, images.data()));

        std::vector<ImageHandle> imageHandles(imageCount);
        for (size_t i = 0; i < imageCount; i++)
        {
            imageHandles[i] = fromVk(images[i]);
        }

        return imageHandles;
    }

    FenceHandle CreateFenceHandle(FenceCreateInfo& info)
    {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (info.createSignaled)
        {
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }

        VkFence fence;
        VK_CHECK(vkCreateFence(toVk(info.logicalDevice), &fenceInfo, nullptr, &fence));
        return fromVk(fence);
    }

    SemaphoreHandle CreateSemaphoreHandle(SemaphoreCreateInfo& info)
    {
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore;
        VK_CHECK(vkCreateSemaphore(toVk(info.logicalDevice), &semaphoreInfo, nullptr, &semaphore));
        return fromVk(semaphore);
    }


}