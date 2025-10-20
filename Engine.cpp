#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "Engine.hpp"
Engine::Engine() {
    loadModel();
    createWindow();
    createInstance();
    createSurface();
    createDevice();
    createSwapchain();
    createColorResources();
    createDepthResources();
    createRenderpass();
    createFramebuffers();
    createUniformBuffers();
    createTextureImage();
    createTextureSampler();
    createDescriptorSetLayout();
    createDescriptorPool();
    createDesctiptorSets();
    createGraphicsPipeline();
    createVertexBuffer();
    createIndexBuffer();
    createQueryPool();
}
Engine::~Engine() {
    vkDestroyQueryPool(device, queryPool, nullptr);
    cleanupSwapchain();
    vkDestroySampler(device, textureSampler, nullptr);
    vkDestroyImageView(device, textureImageView, nullptr);
    vkFreeMemory(device, textureImageMemory, nullptr);
    vkDestroyImage(device, textureImage, nullptr);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        vkFreeMemory(device, uniformBufferMemory[i], nullptr);
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
    }
    vkFreeMemory(device, indexBufferMemory, nullptr);
    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkDestroyPipeline(device, gfxPipeline, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, pushDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipelineLayout(device, gfxPipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderpass, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
}
void Engine::run() {
    VkCommandPool gfxCommandPool = createCommandPool(queueFamilies.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    std::vector<VkCommandBuffer> gfxCommandBuffers(MAX_FRAMES_IN_FLIGHT);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) gfxCommandBuffers[i] = createCommandBuffer(gfxCommandPool);
    std::vector<VkSemaphore> imageAvailable(MAX_FRAMES_IN_FLIGHT);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) imageAvailable[i] = createSemaphore();
    std::vector<VkSemaphore> renderDone(MAX_FRAMES_IN_FLIGHT);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) renderDone[i] = createSemaphore();
    std::vector<VkFence> cmdBufferReady(MAX_FRAMES_IN_FLIGHT);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) cmdBufferReady[i] = createFence(VK_FENCE_CREATE_SIGNALED_BIT);
    
    double lastTime = glfwGetTime();
    uint32_t currFrame = 0;
    int framesPassed = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // wait until this command buffer is ready to be rerecorded
        vkWaitForFences(device, 1, &cmdBufferReady[currFrame], VK_TRUE, ~0ull);
        
        // acquire free image from swapchain
        uint32_t imageIndex;
        VkResult res = vkAcquireNextImageKHR(device, swapchain, ~0ull, imageAvailable[currFrame], VK_NULL_HANDLE, &imageIndex);
        if (res==VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            continue;
        } else if (res!=VK_SUCCESS && res!=VK_SUBOPTIMAL_KHR) {
            VK_CHECK(res);
        }
        
        vkResetFences(device, 1, &cmdBufferReady[currFrame]);
        vkResetCommandBuffer(gfxCommandBuffers[currFrame], 0);

        recordCommandBuffer(gfxCommandBuffers[currFrame], imageIndex, currFrame);
        
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &gfxCommandBuffers[currFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderDone[currFrame];
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailable[currFrame];
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.pWaitDstStageMask = waitStages;
        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, cmdBufferReady[currFrame]));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderDone[currFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;
        res = vkQueuePresentKHR(graphicsQueue, &presentInfo);
        if (res==VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            continue;
        } else if (res!=VK_SUCCESS && res!=VK_SUBOPTIMAL_KHR) {
            VK_CHECK(res);
        }

        currFrame = (currFrame+1)%MAX_FRAMES_IN_FLIGHT;

        framesPassed++;
        double currentTime = glfwGetTime();
        double elapsed = currentTime - lastTime; 
        uint32_t prevFrame = (currFrame + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
        VkResult result = vkGetQueryPoolResults(device, queryPool, prevFrame * 2, 2, 
            sizeof(uint64_t) * 2, &queryResults[prevFrame * 2], sizeof(uint64_t), 
            VK_QUERY_RESULT_64_BIT);
            
        if (result == VK_SUCCESS) {
            uint64_t startTime = queryResults[prevFrame * 2];
            uint64_t endTime = queryResults[prevFrame * 2 + 1];
            
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(pDevice, &props);
            float timestampPeriod = props.limits.timestampPeriod;
            
            double gpuTimeMs = (endTime - startTime) * timestampPeriod / 1000000.0;
            
            gpuTimes.push_back(gpuTimeMs);
            if (gpuTimes.size() > 30) {
                gpuTimes.erase(gpuTimes.begin());
            }
            
            double avgGpuTime = 0.0;
            for (double time : gpuTimes) {
                avgGpuTime += time;
            }
            avgGpuTime /= gpuTimes.size();
            
            if (elapsed >= 2.0) {
                std::ostringstream title;
                title << "CPU: " << std::fixed << std::setprecision(1) << framesPassed/elapsed 
                    << " FPS, GPU: " << std::setprecision(3) << avgGpuTime 
                    << "ms (avg " << gpuTimes.size() << " frames), "
                    << "Triangles: " << indices.size()/3;
                glfwSetWindowTitle(window, title.str().c_str());
                framesPassed = 0;
                lastTime = currentTime;
            }
        }
    }
    vkQueueWaitIdle(graphicsQueue);
    vkDestroyCommandPool(device, gfxCommandPool, nullptr);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(device, cmdBufferReady[i], nullptr);
        vkDestroySemaphore(device, imageAvailable[i], nullptr);
        vkDestroySemaphore(device, renderDone[i], nullptr);
    }
}
void Engine::loadModel() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    std::string warn;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str(), nullptr)) {
        throw std::runtime_error(err);
    }
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.x = floatToHalf(attrib.vertices[3 * index.vertex_index + 0]);
            vertex.y = floatToHalf(attrib.vertices[3 * index.vertex_index + 1]);
            vertex.z = floatToHalf(attrib.vertices[3 * index.vertex_index + 2]);
            vertex.tx = floatToHalf(attrib.texcoords[2 * index.texcoord_index + 0]);
            vertex.ty = floatToHalf(1.0f - attrib.texcoords[2 * index.texcoord_index + 1]);
            glm::vec3 normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2],
            };
            normal = glm::normalize(normal);
            // input float is [-1.0, 1.0], we need to convert it to [0, 255] to fit into uint8_t
            vertex.nx = uint8_t((normal.x*0.5f+0.5f)*255.0f);
            vertex.ny = uint8_t((normal.y*0.5f+0.5f)*255.0f);
            vertex.nz = uint8_t((normal.z*0.5f+0.5f)*255.0f);
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
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

    std::set<std::string> requestedLayers(requiredInstanceLayers.begin(), requiredInstanceLayers.end());
    for (const auto layer: availableLayers) requestedLayers.erase(layer.layerName);
    if (!requestedLayers.empty()) throw std::runtime_error("Error: requested layers not supported");
    instanceInfo.enabledLayerCount = requiredInstanceLayers.size();
    instanceInfo.ppEnabledLayerNames = requiredInstanceLayers.data();

    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, availableExtensions.data());

    std::set<std::string> requestedExtensions(requiredInstanceExtensions.begin(), requiredInstanceExtensions.end());
    for (const auto ext: availableExtensions) requestedExtensions.erase(ext.extensionName);
    if (!requestedExtensions.empty()) throw std::runtime_error("Error: requested instance extensions not supported");
    instanceInfo.enabledExtensionCount = requiredInstanceExtensions.size();
    instanceInfo.ppEnabledExtensionNames = requiredInstanceExtensions.data();

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
            msaaSamples = getMaxSamples();
            break;
        }
    }
    if (pDevice == VK_NULL_HANDLE) throw std::runtime_error("Error: no suitable physical device");
    queueFamilies = getQueueFamilies(pDevice);

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = nullptr;
    
    deviceInfo.enabledExtensionCount = requiredDeviceExtensions.size();
    deviceInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.features.geometryShader = VK_TRUE;
    features.features.samplerAnisotropy = VK_TRUE;
    features.features.sampleRateShading = VK_TRUE;
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.shaderInt8 = VK_TRUE;
    features12.shaderFloat16 = VK_TRUE;
    features12.storageBuffer8BitAccess = VK_TRUE;
    VkPhysicalDevice16BitStorageFeatures features16{};
    features16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
    features16.storageBuffer16BitAccess = VK_TRUE;
    features12.pNext = &features16;
    features.pNext = &features12;
    deviceInfo.pNext = &features;

    std::set<uint32_t> uniqueFamilies = {
        queueFamilies.graphicsFamily.value(), queueFamilies.presentFamily.value(), queueFamilies.transferFamily.value()
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
    vkGetDeviceQueue(device, queueFamilies.transferFamily.value(), 0, &transferQueue);
}
bool Engine::isDeviceSuitable(VkPhysicalDevice dev) {
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(dev, &features);
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(dev, &props);

    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, availableExtensions.data());
    std::set<std::string> requestedExtensions(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());
    for (const auto ext: availableExtensions) requestedExtensions.erase(ext.extensionName);

    QueueFamilies _queueFamilies = getQueueFamilies(dev); // all queue families that are available on this device
    SurfaceDetails _surfaceDetails = getSurfaceDetails(dev); // surface details that this device supports

    return features.geometryShader == VK_TRUE &&
        features.samplerAnisotropy == VK_TRUE && // anisotropic filtering is required to handle undersampling
        features.sampleRateShading == VK_TRUE && // enable sample shading 
        props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        requestedExtensions.empty() &&
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
        if (family.queueFlags & VK_QUEUE_TRANSFER_BIT && !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            _queueFamilies.transferFamily = i;
        }
        VkBool32 presentSupported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupported);
        if (presentSupported == VK_TRUE) {
            _queueFamilies.presentFamily = i;
        }
        if (_queueFamilies.isComplete()) {
            break;
        }
        i++;
    }
    return _queueFamilies;
}
void Engine::createSurface() {
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));
}
void Engine::createSwapchain() {
    // query surface details here since during recreation of the swapchain we need to know
    // the updated capabilities to figure out the new extent
    SurfaceDetails surfaceDetails = getSurfaceDetails(pDevice);
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
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // swapchain images are not shared among different queues
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
        createImageView(swapchainImages[i], swapchainImageViews[i], swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}
void Engine::createDescriptorSetLayout() {
    // describes the descriptor set to be bounded
    VkDescriptorSetLayoutBinding MVPLayoutBinding{};
    MVPLayoutBinding.binding = 0; // referenced in the shader
    MVPLayoutBinding.descriptorCount = 1; // it is possible for a shader variable to represent an array of UBOs
    MVPLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    MVPLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    MVPLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding bindings[] = {MVPLayoutBinding, samplerLayoutBinding};

    VkDescriptorSetLayoutBinding vertexLayoutBinding{};
    vertexLayoutBinding.binding = 0;
    vertexLayoutBinding.descriptorCount = 1;
    vertexLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    vertexLayoutBinding.pImmutableSamplers = nullptr;

    // this should be a separate set as we are supplying a flag for push descriptors
    VkDescriptorSetLayoutBinding pushBindings[] = {vertexLayoutBinding};

    // tells the pipeline what kind of descriptor sets to expect
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = 2;
    descriptorSetLayoutInfo.pBindings = bindings;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));

    descriptorSetLayoutInfo.bindingCount = 1;
    descriptorSetLayoutInfo.pBindings = pushBindings;
    descriptorSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr, &pushDescriptorSetLayout));
}
void Engine::createGraphicsPipeline() {
    auto vertCode = readFile("../shader.vert.spv");
    auto fragCode = readFile("../shader.frag.spv");
    VkShaderModule vertShaderModule = createShaderModule(vertCode);
    VkShaderModule fragShaderModule = createShaderModule(fragCode);
    VkPipelineShaderStageCreateInfo vertInfo{};
    vertInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertInfo.module = vertShaderModule;
    vertInfo.pName = "main";
    vertInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertInfo.pSpecializationInfo = nullptr; // allows us to specify shader constants
    VkPipelineShaderStageCreateInfo fragInfo{};
    fragInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragInfo.module = fragShaderModule;
    fragInfo.pName = "main";
    fragInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragInfo.pSpecializationInfo = nullptr;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos = {vertInfo, fragInfo};

    // how to treat incoming data
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescription = Vertex::getAttributeDescription();
    // vertexInputInfo.vertexBindingDescriptionCount = 1;
    // vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    // vertexInputInfo.vertexAttributeDescriptionCount = attributeDescription.size();
    // vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

    // how to assemble vertex shader output
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // triangle is formed every 3 vertices with no reuse
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;  // used to break up lines and triangles in _STRIP topology

    // the region of the attachment that the output will be rendered to
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchainExtent.width;
    viewport.height = (float)swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
    scissor.extent = swapchainExtent;
    scissor.offset = {0, 0};
    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.scissorCount = 1;
    viewportInfo.viewportCount = 1;

    // this will be set in the actual loop
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = dynamicStates.size();
    dynamicInfo.pDynamicStates = dynamicStates.data();

    VkPipelineRasterizationStateCreateInfo rasterInfo{};
    rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterInfo.depthClampEnable = VK_FALSE; // whether to clamp fragments beyond far and near planes to them (instead of discarding)
    rasterInfo.rasterizerDiscardEnable = VK_FALSE; // transform feedback
    rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterInfo.lineWidth = 1.0f;
    rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterInfo.depthBiasEnable = VK_FALSE;

    // multisampling
    VkPipelineMultisampleStateCreateInfo msaaInfo{};
    msaaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaaInfo.sampleShadingEnable = VK_TRUE;
    msaaInfo.minSampleShading = 0.2f;
    msaaInfo.rasterizationSamples = msaaSamples;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.logicOpEnable = VK_FALSE;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    pipelineLayoutInfo.setLayoutCount = 2;
    VkDescriptorSetLayout layouts[] = {descriptorSetLayout, pushDescriptorSetLayout};
    pipelineLayoutInfo.pSetLayouts = layouts;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &gfxPipelineLayout));

    VkPipelineDepthStencilStateCreateInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthInfo.depthTestEnable = VK_TRUE;
    depthInfo.depthWriteEnable = VK_TRUE;
    depthInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthInfo.depthBoundsTestEnable = VK_FALSE; // allows to keep fragments within a specific depth range
    depthInfo.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStageInfos.size();
    pipelineInfo.pStages = shaderStageInfos.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pDynamicState = &dynamicInfo;
    pipelineInfo.pRasterizationState = &rasterInfo;
    pipelineInfo.pMultisampleState = &msaaInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.pDepthStencilState = &depthInfo;
    pipelineInfo.layout = gfxPipelineLayout;
    pipelineInfo.renderPass = renderpass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = nullptr; // in case we want to derive this pipeline from an already existing one
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &gfxPipeline));

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}
void Engine::createRenderpass() {
    // attachments must be in the sam order they are provided in the framebuffer
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.samples = msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.samples = msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    VkAttachmentDescription colorResolveAttachment{};
    colorResolveAttachment.format = swapchainFormat;
    colorResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment, colorResolveAttachment};

    // each subpass references >=1 attachments from the array above
    VkAttachmentReference colorAttachmentRef{};
    // which attachment to reference by its index in the array above
    // this index is directly referenced by fragment shader output, i.e layout (location=0) out vec4 outColor
    colorAttachmentRef.attachment = 0;
    // what layout we want this attachment to have once subpass starts
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // used as color buffer

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorResolveAttachmentRef{};
    colorResolveAttachmentRef.attachment = 2;
    colorResolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorResolveAttachmentRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL; // implicit subpass before or after the rendering pass
    dep.dstSubpass = 0; // current subpass, must be always bigger than srcSubpass
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | 
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // wait until writes are done to depth buffer
    dep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | 
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // we clear the depth buffer first

    VkRenderPassCreateInfo renderpassInfo{};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassInfo.attachmentCount = sizeof(attachments)/sizeof(attachments[0]);
    renderpassInfo.pAttachments = attachments;
    renderpassInfo.subpassCount = 1;
    renderpassInfo.pSubpasses = &subpass;
    renderpassInfo.dependencyCount = 1;
    renderpassInfo.pDependencies = &dep;
    VK_CHECK(vkCreateRenderPass(device, &renderpassInfo, nullptr, &renderpass));
}
void Engine::createFramebuffers() {
    swapchainFramebuffers.resize(swapchainImages.size());
    for (int i=0; i<swapchainFramebuffers.size(); i++) {
        VkImageView attachments[] = {colorImageView, depthImageView, swapchainImageViews[i]};
        
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.attachmentCount = sizeof(attachments)/sizeof(attachments[0]);
        info.pAttachments = attachments;
        info.renderPass = renderpass;
        info.width = swapchainExtent.width;
        info.height = swapchainExtent.height;
        info.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &info, nullptr, &swapchainFramebuffers[i]));
    }
}
void Engine::recordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, uint32_t currFrame) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pInheritanceInfo = nullptr; // which state to inherit from primary buffer, only relevant for seconday buffers
    // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: command buffer will be rerecorded right after executing it once
    // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: command buffer is secondary and will be entirely within a single renderpass
    // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: command buffer can be resubmitted while it is also already pending execution
    beginInfo.flags = 0;
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
    {
        vkCmdResetQueryPool(cmdBuffer, queryPool, currFrame * 2, 2);
        vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, currFrame * 2);

        VkRenderPassBeginInfo renderpassBeginInfo{};
        renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpassBeginInfo.renderPass = renderpass;
        renderpassBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
        renderpassBeginInfo.renderArea.offset = {0, 0};
        renderpassBeginInfo.renderArea.extent = swapchainExtent;
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        renderpassBeginInfo.clearValueCount = clearValues.size();
        renderpassBeginInfo.pClearValues = clearValues.data();
        // VK_SUBPASS_CONTENTS_INLINE: render pass commands will be embedded in the primary command buffer itself and no secondary
        // command buffer will be executed
        // VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: render pass commands will be executed from secondary command buffer
        vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        {
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);

            // VkDeviceSize offsets[] = {0};
            // vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, offsets);
            vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = swapchainExtent.width;
            viewport.height = swapchainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
            VkRect2D scissor{};
            scissor.extent = swapchainExtent;
            scissor.offset = {0, 0};
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

            updateUniformBuffers(currFrame);

            PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR = 
                (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR");
            if (!vkCmdPushDescriptorSetKHR) {
                throw std::runtime_error("Failed to load vkCmdPushDescriptorSetKHR function");
            }
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = vertexBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = vertexBufferSize;
            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstBinding = 0;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeDescriptorSet.dstArrayElement = 0;
            writeDescriptorSet.pBufferInfo = &bufferInfo;
            vkCmdPushDescriptorSetKHR(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipelineLayout, 1, 1, &writeDescriptorSet);

            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipelineLayout, 0, 1, &descriptorSets[currFrame], 0, nullptr);

            // vkCmdDraw(cmdBuffer, vertices.size(), 1, 0, 0);
            vkCmdDrawIndexed(cmdBuffer, indices.size(), 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(cmdBuffer);
        vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, currFrame * 2 + 1);
    }
    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

}
void Engine::cleanupSwapchain() {
    vkDestroyImageView(device, colorImageView, nullptr);
    vkDestroyImage(device, colorImage, nullptr);
    vkFreeMemory(device, colorImageMemory, nullptr);
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);
    for (auto framebuffer: swapchainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    for (auto imageView: swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}
void Engine::createVertexBuffer() {
    vertexBufferSize = sizeof(vertices[0])*vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT here means GPU keeps track of writes to this buffer
    // if the writes are done to cache or they are not done yet, GPU will take it into account
    // without this flag we have to manually flush writes
    createBuffer(stagingBuffer, stagingBufferMemory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, vertexBufferSize, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(vertexBuffer, vertexBufferMemory, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vertexBufferSize, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    copyBuffer(stagingBuffer, vertexBuffer, vertexBufferSize);

    vkFreeMemory(device, stagingBufferMemory, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
}
void Engine::createIndexBuffer() {
    VkDeviceSize size = sizeof(indices[0])*indices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT here means GPU keeps track of writes to this buffer
    // if the writes are done to cache or they are not done yet, GPU will take it into account
    // without this flag we have to manually flush writes
    createBuffer(stagingBuffer, stagingBufferMemory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, size, 0, &data);
    memcpy(data, indices.data(), size);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(indexBuffer, indexBufferMemory, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, size, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    copyBuffer(stagingBuffer, indexBuffer, size);

    vkFreeMemory(device, stagingBufferMemory, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
}
void Engine::createUniformBuffers() {
    VkDeviceSize size = sizeof(UniformBufferObject);
    uniformBufferMapped.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBufferMemory.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(uniformBuffers[i], uniformBufferMemory[i], VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, size, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        // persistent mapping since values are updated every frame
        vkMapMemory(device, uniformBufferMemory[i], 0, size, 0, &uniformBufferMapped[i]);
    }
}
void Engine::createDescriptorPool() {
    // this describes only one descriptor type per pool
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT; // how many descriptors of this type to create
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    // descriptor sets must be allocated from a descriptor pool
    // so descriptor set here consists of two descriptors
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT; // max number of descriptor sets allocated from this pool

    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));
}
void Engine::createDesctiptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = descriptorPool;
    info.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    info.pSetLayouts = layouts.data(); // must be supplied for each descriptor set
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(device, &info, descriptorSets.data()));
    // configuration of the descriptor sets
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject); 

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = textureImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0; // in case the descriptor set is an array
        descriptorWrites[0].descriptorCount = 1; // in case we want to update multiple descriptor sets at once
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].pBufferInfo = &bufferInfo;
        descriptorWrites[0].pImageInfo = nullptr;
        descriptorWrites[0].pTexelBufferView = nullptr;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].pBufferInfo = nullptr;
        descriptorWrites[1].pImageInfo = &imageInfo;
        descriptorWrites[1].pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(device, 2, descriptorWrites.data(), 0, nullptr);
    }
}
void Engine::updateUniformBuffers(uint32_t index) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), swapchainExtent.width / (float) swapchainExtent.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;
    memcpy(uniformBufferMapped[index], &ubo, sizeof(UniformBufferObject));
}
void Engine::createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize size = texWidth*texHeight*4; // 4 bytes in total, 1 byte per channel
    if (!pixels) throw std::runtime_error("Error: cannot read the texture file");
    mipLevels = std::floor(std::log2(std::max(texWidth, texHeight)))+1;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(stagingBuffer, stagingBufferMemory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, size, 0, &data);
    memcpy(data, pixels, size);
    vkUnmapMemory(device, stagingBufferMemory);
    stbi_image_free(pixels);

    createImage(textureImage, textureImageMemory, texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mipLevels, VK_SAMPLE_COUNT_1_BIT);
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
    copyBufferToImage(stagingBuffer, textureImage, texWidth, texHeight);
    generateMipmaps(textureImage, texWidth, texHeight, mipLevels, VK_FORMAT_R8G8B8A8_UNORM);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    createImageView(textureImage, textureImageView, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
}
void Engine::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // how to interpolate texels that are magnified, helps with oversampling 
    samplerInfo.minFilter = VK_FILTER_LINEAR; // how to interpolate texels that are minified, helps with undersampling
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE; // enable to deal with undersampling
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(pDevice, &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // if VK_TRUE, then we can use coordinates [0, texWidth]
    // if VK_TRUE, then texels will first be compared to a value, and the result 
    // of that comparison is used in filtering operations
    samplerInfo.compareEnable = VK_FALSE; 
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler));
}
void Engine::createDepthResources() {
    std::vector<VkFormat> availableDepthFormats = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    for (const auto format: availableDepthFormats) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(pDevice, format, &props);
        if ((props.optimalTilingFeatures & 
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            depthFormat = format;
            break;
        }
    }
    createImage(depthImage, depthImageMemory, swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, 
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, msaaSamples);
    createImageView(depthImage, depthImageView, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}
void Engine::createColorResources() {
    createImage(colorImage, colorImageMemory, swapchainExtent.width, swapchainExtent.height, swapchainFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, msaaSamples);
    createImageView(colorImage, colorImageView, swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}
void Engine::recreateSwapchain() {
    vkDeviceWaitIdle(device);

    cleanupSwapchain();

    createSwapchain();
    createColorResources();
    createDepthResources();
    createFramebuffers();
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
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
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
    } else { // means the device allows us to specify any extent
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        actualExtent.width = std::clamp(actualExtent.width, cap.minImageExtent.width, cap.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, cap.minImageExtent.height, cap.maxImageExtent.height);
        return actualExtent;
    }
}
void Engine::createImageView(VkImage image, VkImageView& imageView, VkFormat format, VkImageAspectFlags aspectMask, uint32_t mipLevels) {
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
    imageViewInfo.subresourceRange.levelCount = mipLevels;
    VK_CHECK(vkCreateImageView(device, &imageViewInfo, nullptr, &imageView));
}
VkShaderModule Engine::createShaderModule(std::vector<char> code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size(); // size in bytes
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));
    return shaderModule;
}
VkCommandPool Engine::createCommandPool(uint32_t queueFamily, VkCommandPoolCreateFlagBits flags) {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: command buffers allocated from this pool will be short-lived,
    // meaning they will be reset or freed in a short timeframe
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: any command buffer allocated from this pool can be
    // individually reset to its initial state either by calling vkResetCommandBuffer or via
    // implicit reset when calling vkBeginCommandBuffer
    // if this flag is not set, then vkResetCommandBuffer must not be called for any command buffers from this pool
    info.flags = flags;
    info.queueFamilyIndex = queueFamily;
    VkCommandPool cmdPool;
    VK_CHECK(vkCreateCommandPool(device, &info, nullptr, &cmdPool));
    return cmdPool;
}
VkCommandBuffer Engine::createCommandBuffer(VkCommandPool cmdPool) {
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandBufferCount = 1;
    info.commandPool = cmdPool;
    // VK_COMMAND_BUFFER_LEVEL_SECONDARY: cannot be submitted to queue directly, but can be called from PRIMARY buffer
    // VK_COMMAND_BUFFER_LEVEL_PRIMARY: can be submitted to queue directly, but cannot be called from another buffer
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &info, &cmdBuffer));
    return cmdBuffer;
}
VkFence Engine::createFence(VkFenceCreateFlags flags) {
    VkFenceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = flags;
    VkFence fence;
    VK_CHECK(vkCreateFence(device, &info, nullptr, &fence));
    return fence;
}
VkSemaphore Engine::createSemaphore() {
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.flags = 0;
    VkSemaphore sem;
    VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &sem));
    return sem;
}
uint32_t Engine::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    // memory heaps are distinct memory resources like VRAM or swap space in RAM in case memory spills from VRAM,
    // different types of memory exist within those heaps
    /*
    typedef struct VkPhysicalDeviceMemoryProperties {
        uint32_t        memoryTypeCount;
        VkMemoryType    memoryTypes[VK_MAX_MEMORY_TYPES];
        uint32_t        memoryHeapCount;
        VkMemoryHeap    memoryHeaps[VK_MAX_MEMORY_HEAPS];
    } VkPhysicalDeviceMemoryProperties;
    */
    // here we only care about type of memory, not what heap it comes from
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(pDevice, &memProperties); // get memory types and heaps
    for (uint32_t i = 0; i<memProperties.memoryTypeCount; i++) { // iterate over all available memory types
        if ((typeFilter & (1<<i)) && // typeFilter specifies the bit field of memory types that are suitable
            ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)) {
            return i; // index of the suitable memory type
        }
    }
    throw std::runtime_error("Error: no suitable memory type");
}
void Engine::createBuffer(VkBuffer& buffer, VkDeviceMemory& memory, VkBufferUsageFlags usage, 
        VkDeviceSize size, VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));

    VkMemoryRequirements memReq; // buffer's memory requirements, i.e size, alignment, memory type
    vkGetBufferMemoryRequirements(device, buffer, &memReq);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory));

    vkBindBufferMemory(device, buffer, memory, 0);
}
void Engine::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandPool cmdPool = createCommandPool(queueFamilies.transferFamily.value(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    VkCommandBuffer cmdBuffer = createCommandBuffer(cmdPool);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    {
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    }
    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &cmdBuffer;

    VkFence fence = createFence(0);
    vkQueueSubmit(transferQueue, 1, &info, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, ~0ull);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyFence(device, fence, nullptr);
}
void Engine::createImage(VkImage& image, VkDeviceMemory& imageMemory, uint32_t width, uint32_t height, VkFormat format, 
    VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperty, uint32_t mipLevels, VkSampleCountFlagBits samples) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.format = format;
    // the way texels (pixels within texture) are laid out physically, cannot be changed in the future
    // VK_IMAGE_TILING_LINEAR: texels are laid out in a row-major order, useful if we want to access image texels in the memory
    // VK_IMAGE_TILING_OPTIMAL: texels are laid out according to implementation
    imageInfo.tiling = tiling; 
    // VK_IMAGE_LAYOUT_UNDEFINED: not usable by the GPU, first transition will discard the texels
    // VK_IMAGE_LAYOUT_PREINITIALIZED: not usable by the GPU, first transition will preserve texels though
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // only used by one queue - graphics queue
    imageInfo.samples = samples;
    VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image));

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(device, image, &memReq);
    VkMemoryAllocateInfo memInfo{};
    memInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memInfo.allocationSize = memReq.size;
    memInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, memoryProperty);
    VK_CHECK(vkAllocateMemory(device, &memInfo, nullptr, &imageMemory));
    VK_CHECK(vkBindImageMemory(device, image, imageMemory, 0));
}
void Engine::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
    // must be graphics queue since operations specified in the barrier reside in the graphics queue
    VkCommandPool cmdPool = createCommandPool(queueFamilies.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    VkCommandBuffer cmdBuffer = createCommandBuffer(cmdPool);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        // in case we use the barrier to transfer queue family ownership
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage;
        VkPipelineStageFlags dstStage;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0; // practically not waiting for anything at all
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // start writing as soon as possible
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // wait for transfer to finish
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }

        vkCmdPipelineBarrier(cmdBuffer, 
            srcStage, dstStage,
            // either 0 or VK_DEPENDENCY_BY_REGION_BIT: turns the barrier into a per-region condition, i.e
            // implementation is allowed reading from the parts of a resource that were written so far
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }
    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &cmdBuffer;

    VkFence fence = createFence(0);
    vkQueueSubmit(graphicsQueue, 1, &info, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, ~0ull);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyFence(device, fence, nullptr);
}
void Engine::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandPool cmdPool = createCommandPool(queueFamilies.transferFamily.value(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    VkCommandBuffer cmdBuffer = createCommandBuffer(cmdPool);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    {
        VkBufferImageCopy copyRegion{};
        // bufferImageHeight and bufferRowLength specify how pixels are laid out, i.e there may be padding
        copyRegion.bufferImageHeight = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferOffset = 0;

        copyRegion.imageExtent = {width, height, 1};
        copyRegion.imageOffset = {0, 0, 0};

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageSubresource.mipLevel = 0;
        // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL indicates what layout the image is currently using
        vkCmdCopyBufferToImage(cmdBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    }
    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &cmdBuffer;

    VkFence fence = createFence(0);
    vkQueueSubmit(transferQueue, 1, &info, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, ~0ull);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyFence(device, fence, nullptr);
}
void Engine::generateMipmaps(VkImage image, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels, VkFormat format) {
    // check if the format of the texture supports linear filtering
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(pDevice, format, &props);
    if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("Error: cannot blit the image");
    }

    VkCommandPool cmdPool = createCommandPool(queueFamilies.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    VkCommandBuffer cmdBuffer = createCommandBuffer(cmdPool);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    {
        // at the beginning all levels are set to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        // we need to set the very first level to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        // blit level to the level below, and then transition image from VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        // to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;
        for (int i=1; i<mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i-1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // wait until writing to this level is done 
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmdBuffer, 
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
            
            VkImageBlit blit{};
            // 3D region that data is copied from
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i-1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth/2 : 1, mipHeight > 1 ? mipHeight/2 : 1, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;
            vkCmdBlitImage(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_LINEAR);

            // now transition the parent level from TRANSFER_SRC to SHADER_READ
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmdBuffer, 
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);
            if (mipWidth > 1) mipWidth/=2;
            if (mipHeight > 1) mipHeight/=2;
        }
        // very last image is left in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer, 
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }
    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &cmdBuffer;

    VkFence fence = createFence(0);
    vkQueueSubmit(graphicsQueue, 1, &info, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, ~0ull);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyFence(device, fence, nullptr);
}
VkSampleCountFlagBits Engine::getMaxSamples() {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(pDevice, &props);
    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
    return VK_SAMPLE_COUNT_1_BIT;
}
void Engine::createQueryPool() {
    VkQueryPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    poolInfo.queryCount = MAX_FRAMES_IN_FLIGHT * 2;
    VK_CHECK(vkCreateQueryPool(device, &poolInfo, nullptr, &queryPool));
    
    queryResults.resize(MAX_FRAMES_IN_FLIGHT * 2);
}