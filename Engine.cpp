#include "Engine.hpp"
Engine::Engine() {
    createWindow();
    createInstance();
    createSurface();
    createDevice();
    createSwapchain();
}
Engine::~Engine() {
    for (auto imageView: swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
}
void Engine::run() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}
void Engine::createWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(800, 600, "Vulkan Window", nullptr, nullptr);
}
void Engine::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_4;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pApplicationName = "Vulkan Application";
    appInfo.pEngineName = "Vulkan engine";

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> availableLayers(count);
    vkEnumerateInstanceLayerProperties(&count, availableLayers.data());
    std::vector<const char*> requiredLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    std::set<std::string> requestedLayers(requiredLayers.begin(), requiredLayers.end());
    for (const auto layer: availableLayers) requestedLayers.erase(layer.layerName);
    if (!requestedLayers.empty()) throw std::runtime_error("Error: requested layers not supported");
    instanceInfo.enabledLayerCount = requiredLayers.size();
    instanceInfo.ppEnabledLayerNames = requiredLayers.data();

    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, availableExtensions.data());
    std::vector<const char*> requiredExtensions = {
        "VK_KHR_surface",
        "VK_KHR_xcb_surface"
    };
    std::set<std::string> requestedExtensions(requiredExtensions.begin(), requiredExtensions.end());
    for (const auto ext: availableExtensions) requestedExtensions.erase(ext.extensionName);
    if (!requestedExtensions.empty()) throw std::runtime_error("Error: requested instance extensions not supported");
    instanceInfo.enabledExtensionCount = requiredExtensions.size();
    instanceInfo.ppEnabledExtensionNames = requiredExtensions.data();

    VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance));
}
void Engine::createDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    for (const auto dev: devices) {
        if (isDeviceSuitable(dev)) {
            pDevice = dev;
            break;
        }
    }
    if (pDevice == VK_NULL_HANDLE) throw std::runtime_error("Error: no suitable physical device");
    queueFamilies = getQueueFamilies(pDevice);
    surfaceDetails = getSurfaceDetails(pDevice);

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = nullptr;
    
    vkEnumerateDeviceExtensionProperties(pDevice, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(count);
    vkEnumerateDeviceExtensionProperties(pDevice, nullptr, &count, availableExtensions.data());
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    std::set<std::string> requestedExtensions(requiredExtensions.begin(), requiredExtensions.end());
    for (const auto ext: availableExtensions) requestedExtensions.erase(ext.extensionName);
    if (!requestedExtensions.empty()) throw std::runtime_error("Error: requested device extensions not supported");
    deviceInfo.enabledExtensionCount = requiredExtensions.size();
    deviceInfo.ppEnabledExtensionNames = requiredExtensions.data();

    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(pDevice, &features);
    deviceInfo.pEnabledFeatures = &features;

    std::set<uint32_t> uniqueFamilies = {
        queueFamilies.graphicsFamily.value(),
    };
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    float priorioty = 1.0f;
    for (const auto family: uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.pQueuePriorities = &priorioty;
        queueInfo.queueCount = 1;
        queueInfo.queueFamilyIndex = family;
        queueInfos.push_back(queueInfo);
    }
    deviceInfo.queueCreateInfoCount = queueInfos.size();
    deviceInfo.pQueueCreateInfos = queueInfos.data();

    VK_CHECK(vkCreateDevice(pDevice, &deviceInfo, nullptr, &device));

    vkGetDeviceQueue(device, queueFamilies.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, queueFamilies.presentFamily.value(), 0, &presentQueue);
}
bool Engine::isDeviceSuitable(VkPhysicalDevice dev) {
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(dev, &features);
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(dev, &props);

    QueueFamilies _queueFamilies = getQueueFamilies(dev);
    SurfaceDetails _surfaceDetails = getSurfaceDetails(dev);

    return features.geometryShader == VK_TRUE &&
        props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        _queueFamilies.isComplete() &&
        !_surfaceDetails.formats.empty() &&
        !_surfaceDetails.presentModes.empty();
}
QueueFamilies Engine::getQueueFamilies(VkPhysicalDevice dev) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, queues.data());
    int i = 0;
    QueueFamilies _queueFamilies{};
    for (const auto family: queues) {
        if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            _queueFamilies.graphicsFamily = i;
        }
        VkBool32 presentSupported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupported);
        if (presentSupported == VK_TRUE) {
            _queueFamilies.presentFamily = i;
        }
        if (_queueFamilies.isComplete()) {
            break;
        }
    }
    return _queueFamilies;
}
void Engine::createSurface() {
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));
}
void Engine::createSwapchain() {
    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(surfaceDetails.formats);
    swapchainExtent = chooseSurfaceExtent(surfaceDetails.cap);
    swapchainFormat = surfaceFormat.format;

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.clipped = VK_TRUE; // whether to discard obscured pixels, i.e. obscured by another window
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // whether to use alpha channel for blending with other windows
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = swapchainExtent;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.minImageCount = surfaceDetails.cap.minImageCount+1;
    if (swapchainInfo.minImageCount > surfaceDetails.cap.maxImageCount && surfaceDetails.cap.maxImageCount>0) {
        swapchainInfo.minImageCount = surfaceDetails.cap.maxImageCount; 
    }
    swapchainInfo.presentMode = choosePresentMode(surfaceDetails.presentModes);
    swapchainInfo.preTransform = surfaceDetails.cap.currentTransform;
    
    uint32_t families[] = {queueFamilies.graphicsFamily.value(), queueFamilies.presentFamily.value()};
    if (queueFamilies.graphicsFamily.value() == queueFamilies.presentFamily.value()) {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.queueFamilyIndexCount = 1;
    } else {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
    }
    swapchainInfo.pQueueFamilyIndices = families;

    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain));
    uint32_t count = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
    swapchainImages.resize(count);
    swapchainImageViews.resize(count);
    vkGetSwapchainImagesKHR(device, swapchain, &count, swapchainImages.data());
    for (int i=0; i<count; i++) {
        createImageView(swapchainImages[i], swapchainImageViews[i], swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}
SurfaceDetails Engine::getSurfaceDetails(VkPhysicalDevice dev) {
    SurfaceDetails _surfaceDetails;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &_surfaceDetails.cap);
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &count, nullptr);
    _surfaceDetails.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &count, _surfaceDetails.formats.data());
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &count, nullptr);
    _surfaceDetails.presentModes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &count, _surfaceDetails.presentModes.data());
    return _surfaceDetails;
}
VkSurfaceFormatKHR Engine::chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> formats) {
    for (const auto format: formats) {
        if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats[0];
}
VkPresentModeKHR Engine::choosePresentMode(std::vector<VkPresentModeKHR> presentModes) {
    for (const auto mode: presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return presentModes[0];
}
VkExtent2D Engine::chooseSurfaceExtent(VkSurfaceCapabilitiesKHR cap) {
    if (cap.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return cap.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        actualExtent.width = std::clamp(actualExtent.width, cap.minImageExtent.width, cap.minImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, cap.minImageExtent.height, cap.minImageExtent.height);
        return actualExtent;
    }
}
void Engine::createImageView(VkImage image, VkImageView& imageView, VkFormat format, VkImageAspectFlags aspectMask) {
    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.format = format;
    imageViewInfo.image = image;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    // subresourceRange describes what the image's purpose is and which part of the image must be accessed
    imageViewInfo.subresourceRange.aspectMask = aspectMask;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.layerCount = 1;
    imageViewInfo.subresourceRange.levelCount = 1;
    VK_CHECK(vkCreateImageView(device, &imageViewInfo, nullptr, &imageView));
}