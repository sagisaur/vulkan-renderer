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
#include <unordered_map>
#include <iomanip>
#include <unordered_set>

#include "tiny_obj_loader.h"
#include "stb_image.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Vertex {
    // instead of using floats (32 bits) we will use uint16_t, which are
    // actually float16_t in the shader
    uint16_t x, y, z, w; // w is only for alignment
    uint8_t nx, ny, nz, nw;
    uint16_t tx, ty;
    bool operator==(const Vertex& other) const {
        return x == other.x && y == other.y && z == other.z && 
            nx == other.nx && ny == other.ny && nz == other.nz &&
            tx == other.tx && ty == other.ty;
    }
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
        attributes[0].format = VK_FORMAT_R16G16B16_SFLOAT;
        attributes[0].offset = offsetof(Vertex, x);
        attributes[1].binding = 0; 
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R8G8B8_UINT;
        attributes[1].offset = offsetof(Vertex, nx);
        attributes[2].binding = 0; 
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R16G16_SFLOAT;
        attributes[2].offset = offsetof(Vertex, tx);

        return attributes;
    }
};
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(glm::vec3(vertex.x, vertex.y, vertex.z)) ^
                   (hash<glm::vec3>()(glm::vec3(vertex.nx, vertex.ny, vertex.nz)) << 1)) >> 1) ^
                   (hash<glm::vec2>()(glm::vec2(vertex.tx, vertex.ty)) << 1);
        }
    };
}

// this double indexing is more memory efficient compared to
// simply storing 126*3 indices of size uint32_t
struct Meshlet {
    uint32_t vertices[64];
    uint8_t indices[126*3];
    uint8_t triangleCount;
    uint8_t vertexCount; 
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Meshlet> meshlets;
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
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
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

// this is important as simply using reinterpret_cast<uint16_t> doesn't work
// it would only read 16 bits of the float
// this function allows us to rebias exponent, shrink mantissa and store the resulting
// 16 bits inside uint16_t, which later are going to be interpreted as float16_t in the shader bitwise
inline uint16_t floatToHalf(float f) {
    union { float f; uint32_t i; } u = { f };
    
    uint32_t sign = (u.i >> 16) & 0x8000;
    int32_t exp = ((u.i >> 23) & 0xFF) - 127 + 15;
    uint32_t mantissa = u.i & 0x7FFFFF;
    
    if (exp <= 0) return sign; // Underflow
    if (exp >= 31) return sign | 0x7C00; // Overflow/infinity
    
    return sign | (exp << 10) | (mantissa >> 13);
}