#pragma once
#include "config.hpp"

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};
const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

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
    void recreateSwapchain();
    void cleanupSwapchain();
    void createVertexBuffer();
    void createIndexBuffer();

    GLFWwindow* window;
    VkInstance instance;
    VkPhysicalDevice pDevice;
    VkDevice device;
    QueueFamilies queueFamilies;
    VkSurfaceKHR surface;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue transferQueue;
    VkSwapchainKHR swapchain;
    VkFormat swapchainFormat;
    VkExtent2D swapchainExtent;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkPipeline gfxPipeline;
    VkPipelineLayout gfxPipelineLayout;
    VkRenderPass renderpass;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

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
    VkFence createFence(VkFenceCreateFlags flags);
    VkSemaphore createSemaphore();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(VkBuffer& buffer, VkDeviceMemory& memory, VkBufferUsageFlags usage, 
        VkDeviceSize size, VkMemoryPropertyFlags properties);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
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

    const int MAX_FRAMES_IN_FLIGHT = 3;
};