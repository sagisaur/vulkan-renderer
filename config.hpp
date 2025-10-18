#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <optional>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "stb_image.h"

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoords;
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        // index of binding in the binding array
        bindingDescription.binding = 0; // would be a different value if the data was packed in different arrays
        bindingDescription.stride = sizeof(Vertex); // number of bytes between vertices
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // input rate (instance vs vertex)
        return bindingDescription;
    }
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescription() {
        std::array<VkVertexInputAttributeDescription, 3> attributes{};
        attributes[0].binding = 0; // which binding this attribute belongs to
        attributes[0].location = 0; // location from vertex shader
        attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[0].offset = offsetof(Vertex, pos);
        attributes[1].binding = 0; 
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(Vertex, color);
        attributes[2].binding = 0; 
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = offsetof(Vertex, texCoords);

        return attributes;
    }
};

struct QueueFamilies {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> transferFamily;
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
    }
};
struct SurfaceDetails {
    VkSurfaceCapabilitiesKHR cap;
    std::vector<VkPresentModeKHR> presentModes;
    std::vector<VkSurfaceFormatKHR> formats;
};
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

#define VK_CHECK(x) vk_check_result((x), #x, __FILE__, __LINE__)
inline const char* vk_result_to_string(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
        default: return "ERROR NOT RECOGNIZED";
    }
}
inline void vk_check_result(VkResult result, const char* expr, const char* file, int line) {
    if (result != VK_SUCCESS) {
        std::ostringstream oss;
        oss << "Vulkan error: " << vk_result_to_string(result)
            << " (" << result << ")"
            << " at " << file << ":" << line
            << " in call: " << expr;
        throw std::runtime_error(oss.str());
    }
}
static std::vector<char> readFile(std::string filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary); // start reading at the end, read file as binary
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}