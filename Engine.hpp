#pragma once
#include "config.hpp"

class Engine {
public:
    Engine();
    ~Engine();
    void run();
private:
    void createWindow();
    void createInstance();
    void createDevice();
    void createSurface();
    void createSwapchain();
    void createGraphicsPipeline();
    void createRenderpass();
    void createFramebuffers();
    void recordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex);

    GLFWwindow* window;
    VkInstance instance;
    VkPhysicalDevice pDevice;
    VkDevice device;
    QueueFamilies queueFamilies;
    VkSurfaceKHR surface;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapchain;
    VkFormat swapchainFormat;
    VkExtent2D swapchainExtent;
    SurfaceDetails surfaceDetails;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkPipeline gfxPipeline;
    VkPipelineLayout gfxPipelineLayout;
    VkRenderPass renderpass;
    std::vector<VkFramebuffer> swapchainFramebuffers;

    bool isDeviceSuitable(VkPhysicalDevice dev);
    QueueFamilies getQueueFamilies(VkPhysicalDevice dev);
    SurfaceDetails getSurfaceDetails(VkPhysicalDevice dev);
    VkSurfaceFormatKHR chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> formats);
    VkPresentModeKHR choosePresentMode(std::vector<VkPresentModeKHR> presentModes);
    VkExtent2D chooseSurfaceExtent(VkSurfaceCapabilitiesKHR cap);
    void createImageView(VkImage image, VkImageView& imageView, VkFormat format, VkImageAspectFlags aspectMask);
    VkShaderModule createShaderModule(std::vector<char> code);
    VkCommandPool createCommandPool(uint32_t queueFamily, VkCommandPoolCreateFlagBits flags);
    VkCommandBuffer createCommandBuffer(VkCommandPool cmdPool);
    VkFence createFence();
    VkSemaphore createSemaphore();
    std::vector<const char*> requiredInstanceLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    std::vector<const char*> requiredInstanceExtensions = {
        "VK_KHR_surface",
        "VK_KHR_xcb_surface"
    };
    std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
};