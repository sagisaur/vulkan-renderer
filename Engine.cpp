#include "Engine.hpp"
Engine::Engine() {
    createWindow();
    createInstance();
    createSurface();
    createDevice();
    createSwapchain();
    createRenderpass();
    createGraphicsPipeline();
    createFramebuffers();
}
Engine::~Engine() {
    for (auto framebuffer: swapchainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    vkDestroyPipeline(device, gfxPipeline, nullptr);
    vkDestroyPipelineLayout(device, gfxPipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderpass, nullptr);
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
    VkCommandPool gfxCommandPool = createCommandPool(queueFamilies.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBuffer gfxCommandBuffer = createCommandBuffer(gfxCommandPool);
    VkSemaphore imageAvailable = createSemaphore();
    VkSemaphore renderDone = createSemaphore();
    VkFence cmdBufferReady = createFence();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // wait until this command buffer is ready to be rerecorded
        vkWaitForFences(device, 1, &cmdBufferReady, VK_TRUE, ~0ull);
        vkResetFences(device, 1, &cmdBufferReady);
        
        // acquire free image from swapchain
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapchain, ~0ull, imageAvailable, VK_NULL_HANDLE, &imageIndex);
        
        vkResetCommandBuffer(gfxCommandBuffer, 0);
        recordCommandBuffer(gfxCommandBuffer, imageIndex);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &gfxCommandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderDone;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailable;
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.pWaitDstStageMask = waitStages;
        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, cmdBufferReady));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderDone;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;
        vkQueuePresentKHR(graphicsQueue, &presentInfo);
    }
    vkQueueWaitIdle(graphicsQueue);
    vkDestroyCommandPool(device, gfxCommandPool, nullptr);
    vkDestroyFence(device, cmdBufferReady, nullptr);
    vkDestroySemaphore(device, imageAvailable, nullptr);
    vkDestroySemaphore(device, renderDone, nullptr);
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
    
    deviceInfo.enabledExtensionCount = requiredDeviceExtensions.size();
    deviceInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

    VkPhysicalDeviceFeatures features{};
    features.geometryShader = VK_TRUE;
    deviceInfo.pEnabledFeatures = &features;

    std::set<uint32_t> uniqueFamilies = {
        queueFamilies.graphicsFamily.value(), queueFamilies.presentFamily.value()
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

    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, availableExtensions.data());
    std::set<std::string> requestedExtensions(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());
    for (const auto ext: availableExtensions) requestedExtensions.erase(ext.extensionName);

    QueueFamilies _queueFamilies = getQueueFamilies(dev); // all queue families that are available on this device
    SurfaceDetails _surfaceDetails = getSurfaceDetails(dev); // surface details that this device supports

    return features.geometryShader == VK_TRUE &&
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
        createImageView(swapchainImages[i], swapchainImageViews[i], swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
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
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;

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
    rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterInfo.depthBiasEnable = VK_FALSE;

    // multisampling
    VkPipelineMultisampleStateCreateInfo msaaInfo{};
    msaaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaaInfo.sampleShadingEnable = VK_FALSE;
    msaaInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &gfxPipelineLayout));

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
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    VkAttachmentDescription attachments[] = {colorAttachment};

    // each subpass references >=1 attachments from the array above
    VkAttachmentReference colorAttachmentRef{};
    // which attachment to reference by its index in the array above
    // this index is directly referenced by fragment shader output, i.e layout (location=0) out vec4 outColor
    colorAttachmentRef.attachment = 0;
    // what layout we want this attachment to have once subpass starts
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // used as color buffer

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL; // implicit subpass before or after the rendering pass
    dep.dstSubpass = 0; // current subpass, must be always bigger than srcSubpass
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

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
        VkImageView attachments[] = {swapchainImageViews[i]};
        
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
void Engine::recordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pInheritanceInfo = nullptr; // which state to inherit from primary buffer, only relevant for seconday buffers
    // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: command buffer will be rerecorded right after executing it once
    // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: command buffer is secondary and will be entirely within a single renderpass
    // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: command buffer can be resubmitted while it is also already pending execution
    beginInfo.flags = 0;
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
    {
        VkRenderPassBeginInfo renderpassBeginInfo{};
        renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpassBeginInfo.renderPass = renderpass;
        renderpassBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
        renderpassBeginInfo.renderArea.offset = {0, 0};
        renderpassBeginInfo.renderArea.extent = swapchainExtent;
        std::array<VkClearValue, 1> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        renderpassBeginInfo.clearValueCount = clearValues.size();
        renderpassBeginInfo.pClearValues = clearValues.data();
        // VK_SUBPASS_CONTENTS_INLINE: render pass commands will be embedded in the primary command buffer itself and no secondary
        // command buffer will be executed
        // VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: render pass commands will be executed from secondary command buffer
        vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        {
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = swapchainExtent.width;
            viewport.height = swapchainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 0.0f;
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
            VkRect2D scissor{};
            scissor.extent = swapchainExtent;
            scissor.offset = {0, 0};
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

            vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
        }
        vkCmdEndRenderPass(cmdBuffer);
    }
    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

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
    } else { // means the device allows us to specify any extent
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
VkFence Engine::createFence() {
    VkFenceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
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