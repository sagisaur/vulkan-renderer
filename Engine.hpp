#pragma once
#include "config.hpp"

#define USE_MESH 1

class Engine {
public:
    Engine();
    ~Engine();
    void run();
private:
    void loadModel();
    void createMeshlets();
    void createWindow();
    void createInstance();
    void createDevice();
    void createSurface();
    void createSwapchain();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createRenderpass();
    void createFramebuffers();
    void recordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, uint32_t currFrame);
    void recreateSwapchain();
    void cleanupSwapchain();
    void createVertexBuffer();
    void createIndexBuffer();
    void createMeshletBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDesctiptorSets();
    void updateUniformBuffers(uint32_t index);
    void createTextureImage();
    void createTextureSampler();
    void createDepthResources();
    void createColorResources();

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
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSetLayout pushDescriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    VkPipeline gfxPipeline;
    VkPipelineLayout gfxPipelineLayout;
    VkRenderPass renderpass;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceSize vertexBufferSize;
    VkDeviceMemory indexBufferMemory;
    VkBuffer meshletBuffer;
    VkDeviceMemory meshletBufferMemory;
    VkDeviceSize meshletBufferSize;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBufferMemory;
    std::vector<void*> uniformBufferMapped;
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    VkFormat depthFormat;
    uint32_t mipLevels;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    Mesh mesh;

    bool isDeviceSuitable(VkPhysicalDevice dev);
    QueueFamilies getQueueFamilies(VkPhysicalDevice dev);
    SurfaceDetails getSurfaceDetails(VkPhysicalDevice dev);
    VkSurfaceFormatKHR chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> formats);
    VkPresentModeKHR choosePresentMode(std::vector<VkPresentModeKHR> presentModes);
    VkExtent2D chooseSurfaceExtent(VkSurfaceCapabilitiesKHR cap);
    void createImageView(VkImage image, VkImageView& imageView, VkFormat format, VkImageAspectFlags aspectMask, uint32_t mipLevels);
    VkShaderModule createShaderModule(std::vector<char> code);
    VkCommandPool createCommandPool(uint32_t queueFamily, VkCommandPoolCreateFlagBits flags);
    VkCommandBuffer createCommandBuffer(VkCommandPool cmdPool);
    VkFence createFence(VkFenceCreateFlags flags);
    VkSemaphore createSemaphore();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(VkBuffer& buffer, VkDeviceMemory& memory, VkBufferUsageFlags usage, 
        VkDeviceSize size, VkMemoryPropertyFlags properties);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createImage(VkImage& image, VkDeviceMemory& imageMemory, uint32_t width, uint32_t height, VkFormat format, 
        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperty, uint32_t mipLevels, 
        VkSampleCountFlagBits samples);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void generateMipmaps(VkImage image, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels, VkFormat format);
    VkSampleCountFlagBits getMaxSamples();
    void createQueryPool();
    std::vector<const char*> requiredInstanceLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    std::vector<const char*> requiredInstanceExtensions = {
        "VK_KHR_surface",
        "VK_KHR_xcb_surface"
    };
    std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME
    };

    const int MAX_FRAMES_IN_FLIGHT = 3;
    const std::string MODEL_PATH = "../viking_room.obj";
    const std::string TEXTURE_PATH = "../viking_room.png";
    VkQueryPool queryPool;
    std::vector<uint64_t> queryResults;
    std::vector<double> gpuTimes;
};