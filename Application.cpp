#include "Application.hpp"

void    Application::run() {
    this->initWindow();
    this->initVulkan();
    this->mainLoop();
    this->cleanup();
}

/*
* Creation of the GLFW window
*/

void    Application::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    this->_window = glfwCreateWindow(WIDTH, HEIGHT, "vulkan", nullptr, nullptr);
}

/* 
* Creation of vulkan instance.
* The instance is a connection between your application and the vulkan library.
*/
void    Application::createInstance() {
    // Information about the application
    constexpr vk::ApplicationInfo   appInfo{
        .pApplicationName = "hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = vk::ApiVersion14
    };
    
    // Get the required layers
    std::vector<char const*> requiredLayers;
    if (enableValidationLayers)
    {
        requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // Check if the required layers are supported by the Vulkan implementation.
    auto layerProperties = this->_context.enumerateInstanceLayerProperties();
	auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
		                                            [&layerProperties](auto const &requiredLayer) {
			                                            return std::ranges::none_of(layerProperties,
			                                                                        [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
		                                            });

	if (unsupportedLayerIt != requiredLayers.end()) {
		throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
	}

    // Get the required extension of glfw
    auto    requiredExtensions = this->getRequiredInstanceExtensions();

    // Check if the required extensions are supported by the vulkan implementation
    auto    extensionProperties = this->_context.enumerateInstanceExtensionProperties();
    auto    unsupportedPropertyIt = std::ranges::find_if(requiredExtensions,
                                                         [&extensionProperties](auto const& requiredExtension) {
                                                            return std::ranges::none_of(extensionProperties,
                                                                                        [requiredExtension](auto const& extensionProperty)
                                                                                    {return strcmp(extensionProperty.extensionName, requiredExtension) == 0;});
                                                         });

    if (unsupportedPropertyIt != requiredExtensions.end()) {
		throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
    }

    // Vulkan is an platform-agnostic API, so we need to be able to interact with GLFW
    uint32_t    glfwExtensionCount = 0;
    auto        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Check if the required GLFW extensions are supported by the Vulkan implementation.
    //auto extensionProperties = this->_context.enumerateInstanceExtensionProperties();
    for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
        if (std::ranges::none_of(extensionProperties,
                                [glfwExtension = glfwExtensions[i]](auto const& extensionProperty)
                                { return strcmp(extensionProperty.extensionName, glfwExtension) == 0; }))
        {
            throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtensions[i]));
        }
    }

    // Which global extensions and validation layer we want tou use
    vk::InstanceCreateInfo createInfo{.pApplicationInfo        = &appInfo,
                                      .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
                                      .ppEnabledLayerNames     = requiredLayers.data(),
                                      .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
                                      .ppEnabledExtensionNames = requiredExtensions.data()};

    this->_instance = vk::raii::Instance(this->_context, createInfo);
}

/*
* Creation of an abstract surface for rendering
*/
void    Application::createSurface() {
    VkSurfaceKHR    surface;

    if (glfwCreateWindowSurface(*this->_instance, this->_window, nullptr, &surface) != 0) {
        throw std::runtime_error("Failed to create window surface");
    }

    this->_surface = vk::raii::SurfaceKHR(this->_instance, surface);
}

/*
* Setup the Debug Messenger
*/
void    Application::setupDebugMessenger() {
    if (!enableValidationLayers) {
        return ;
    }

    vk::DebugUtilsMessageSeverityFlagsEXT   severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

    vk::DebugUtilsMessageTypeFlagsEXT       messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    
    vk::DebugUtilsMessengerCreateInfoEXT    debugUtilsMessegerCreateInfoEXT{.messageSeverity = severityFlags,
                                                                            .messageType = messageTypeFlags,
                                                                            .pfnUserCallback = &debugCallback};

    this->_debugMessenger = this->_instance.createDebugUtilsMessengerEXT(debugUtilsMessegerCreateInfoEXT);
}

/*
* Check all GPU on the machine
*/
void    Application::pickPhysicalDevice() {
    auto                                            physicalDevices = this->_instance.enumeratePhysicalDevices();
    std::multimap<int, vk::raii::PhysicalDevice>    candidates;

    if (physicalDevices.empty()) {
        std::runtime_error("Failed to find GPU with vulkan support!");
    }

    for (const auto& device: physicalDevices) {
        auto        deviceProperties = device.getProperties();
        auto        deviceFeatures = device.getFeatures();
        uint32_t    score = 0;

        // Discrete GPU have significant performance advantage
        if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            score += 1000;
        }

        // Maximum possible size of texture affect graphics quality
        score += deviceProperties.limits.maxImageDimension2D;

        // Application can't function without geometry shadder
        if (deviceFeatures.geometryShader == false) {
            continue ;
        }

        candidates.insert(std::make_pair(score, device));
    }

    if (candidates.empty()) {
        throw std::runtime_error("Failed to find a correct GPU");
    }

    for (const auto& candidate: candidates) {
        std::cout << "candidate: " << candidate.second.getProperties().deviceName << " score: " << candidate.first << std::endl;
    }

    this->_physicalDevice = candidates.rbegin()->second;

    std::cout << "GPU chosen: " << this->_physicalDevice.getProperties().deviceName << std::endl;
}

/**
* Select GPU and create a queue for it
*/
void    Application::createLogicalDevice() {
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = this->_physicalDevice.getQueueFamilyProperties();

	// get the first index into queueFamilyProperties which supports both graphics and present
	for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
    {
		if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
		    this->_physicalDevice.getSurfaceSupportKHR(qfpIndex, *this->_surface))
		{
			// found a queue family that supports both graphics and present
			this->_queueIndex = qfpIndex;
			break;
		}
	}

	if (static_cast<int>(this->_queueIndex) == ~0) {
		throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
	}

	// query for Vulkan 1.3 features
	vk::StructureChain<vk::PhysicalDeviceFeatures2,
                       vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
                       vk::PhysicalDeviceVulkan11Features> featureChain = {
	    {},                                                             // vk::PhysicalDeviceFeatures2
	    {.synchronization2 = true, .dynamicRendering = true},           // vk::PhysicalDeviceVulkan13Features
	    {.extendedDynamicState = true},                                 // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        {.shaderDrawParameters = true}                                  // vk::PhysicalDeviceVulkan11Features
	};

	// create a Device
	float                     queuePriority = 0.5f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = this->_queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
	vk::DeviceCreateInfo      deviceCreateInfo{.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
	                                           .queueCreateInfoCount    = 1,
	                                           .pQueueCreateInfos       = &deviceQueueCreateInfo,
	                                           .enabledExtensionCount   = static_cast<uint32_t>(this->_requiredDeviceExtension.size()), // To enable extension for SwapChain 
	                                           .ppEnabledExtensionNames = this->_requiredDeviceExtension.data()}; // To enable extension for SwapChain

	this->_logicalDevice = vk::raii::Device(this->_physicalDevice, deviceCreateInfo);
	this->_graphicsQueue  = vk::raii::Queue(this->_logicalDevice, this->_queueIndex, 0);
}

/*
* SwapChain creation
*/
void    Application::createSwapChain()
{
    vk::SurfaceCapabilitiesKHR surfaceCapabilities          = this->_physicalDevice.getSurfaceCapabilitiesKHR(*this->_surface); // To check if GLFW surface capacity is compatible with the SwapChain
	this->_swapChainExtent                                  = chooseSwapExtent(surfaceCapabilities);
	uint32_t minImageCount                                  = chooseSwapMinImageCount(surfaceCapabilities);
	std::vector<vk::SurfaceFormatKHR> availableFormats      = this->_physicalDevice.getSurfaceFormatsKHR(*this->_surface); // To check if GLFW surface format is compatible with the SwapChain
	this->_swapChainSurfaceFormat                           = chooseSwapSurfaceFormat(availableFormats);
	std::vector<vk::PresentModeKHR> availablePresentModes   = this->_physicalDevice.getSurfacePresentModesKHR(*this->_surface); // Tocheck if GLFW surface Present mode is compatible with the SwapChain
	vk::PresentModeKHR              presentMode             = chooseSwapPresentMode(availablePresentModes);

	vk::SwapchainCreateInfoKHR swapChainCreateInfo{.surface          = *this->_surface,
	                                               .minImageCount    = minImageCount,
	                                               .imageFormat      = this->_swapChainSurfaceFormat.format,
	                                               .imageColorSpace  = this->_swapChainSurfaceFormat.colorSpace,
	                                               .imageExtent      = this->_swapChainExtent,
	                                               .imageArrayLayers = 1,
	                                               .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
	                                               .imageSharingMode = vk::SharingMode::eExclusive,
	                                               .preTransform     = surfaceCapabilities.currentTransform,
	                                               .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
	                                               .presentMode      = presentMode,
	                                               .clipped          = true};

	this->_swapChain       = vk::raii::SwapchainKHR(this->_logicalDevice, swapChainCreateInfo);
	this->_swapChainImages = this->_swapChain.getImages();
}

void    Application::createImageViews()
{
    assert(this->_swapChainImageViews.empty());

    vk::ImageViewCreateInfo imageViewsCreateInfo {  .viewType           = vk::ImageViewType::e2D,
                                                    .format             = this->_swapChainSurfaceFormat.format,
                                                    .subresourceRange   = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

    for (auto& image: this->_swapChainImages)
    {
        imageViewsCreateInfo.image = image;
        this->_swapChainImageViews.emplace_back(this->_logicalDevice, imageViewsCreateInfo);
    }
}

void    Application::createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding  uboLayoutBinding{
        .binding            = 0,
        .descriptorType     = vk::DescriptorType::eUniformBuffer,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eVertex 
    };

    vk::DescriptorSetLayoutCreateInfo   layoutInfo{
        .bindingCount   = 1,
        .pBindings      = &uboLayoutBinding
    };

    this->_descriptorSetLayout = vk::raii::DescriptorSetLayout(this->_logicalDevice, layoutInfo);
}

void    Application::createGraphicsPipeline()
{
    vk::raii::ShaderModule shaderModule = createShaderModule(readFile("../shaders/compiled/slang.spv"));

    // Shader step usage
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eVertex,  // Execute it as a vertex shader
        .module = shaderModule,                     // module that regroup this shader
        .pName = "vertMain"                         // Take the function "vertMain"
    };

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eFragment,    // Execute it as a fragment shader
        .module = shaderModule,                         // module that regroup this shader
        .pName = "fragMain"                             // Take the function "fragMain"
    };

    // Tell how to use the shader
    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Describe the format of the vertex data that will be passed to the vertex shader  
    auto                                    bindingDescriptor = Vertex::getBindingDescription();
    auto                                    attributeDescriptor = Vertex::getAttributeDescriptor();
    vk::PipelineVertexInputStateCreateInfo  vertexInputInfo{.vertexBindingDescriptionCount      = 1,
                                                            .pVertexBindingDescriptions         = &bindingDescriptor,
                                                            .vertexAttributeDescriptionCount    = static_cast<uint32_t>(attributeDescriptor.size()),
                                                            .pVertexAttributeDescriptions       = attributeDescriptor.data()};
    
    // Describe what kind of geometry will be drawn from the vertices and if primitive should be enable
    vk::PipelineInputAssemblyStateCreateInfo    inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};

    /* 
    * The viewport describe the region of the framebuffer that the output will be rendered to almost always (0, 0) to (width, heigh)
    * The scissor define which map pixel have the right to be shown
    */ 
    vk::PipelineViewportStateCreateInfo         viewportState{.viewportCount = 1, .scissorCount = 1};

    // The rasterizer take the geometry and transform it into fragment that will be after colored by the frag shader
    vk::PipelineRasterizationStateCreateInfo    rasterizer{ .depthClampEnable        = VK_FALSE,                     
		                                                    .rasterizerDiscardEnable = VK_FALSE,
		                                                    .polygonMode             = vk::PolygonMode::eFill,
		                                                    .cullMode                = vk::CullModeFlagBits::eBack, 
		                                                    .frontFace               = vk::FrontFace::eClockwise,
		                                                    .depthBiasEnable         = VK_FALSE,
		                                                    .lineWidth               = 1.0f};
    
    // Multisampling to perform antialiasing
    vk::PipelineMultisampleStateCreateInfo      multisampling{  .rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                                .sampleShadingEnable = VK_FALSE};

    // color blending from the color of the framebuffer and the color of the fragment shader
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable    = VK_FALSE,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

    vk::PipelineColorBlendStateCreateInfo colorBlending{    .logicOpEnable = VK_FALSE,
                                                            .logicOp = vk::LogicOp::eCopy,
                                                            .attachmentCount = 1,
                                                            .pAttachments = &colorBlendAttachment};

    // This tell the pipeline that the viewport and scissor are dynamic 
    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount         = 0,
        .pSetLayouts            = &*this->_descriptorSetLayout,
        .pushConstantRangeCount = 0
    };

	this->_pipelineLayout = vk::raii::PipelineLayout(this->_logicalDevice, pipelineLayoutInfo);

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
		{.stageCount          = 2,
		 .pStages             = shaderStages,
		 .pVertexInputState   = &vertexInputInfo,
		 .pInputAssemblyState = &inputAssembly,
		 .pViewportState      = &viewportState,
		 .pRasterizationState = &rasterizer,
		 .pMultisampleState   = &multisampling,
		 .pColorBlendState    = &colorBlending,
		 .pDynamicState       = &dynamicState,
		 .layout              = this->_pipelineLayout,
		 .renderPass          = nullptr},
		{.colorAttachmentCount = 1, .pColorAttachmentFormats = &this->_swapChainSurfaceFormat.format}};

	this->_graphicsPipeline = vk::raii::Pipeline(this->_logicalDevice, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

void    Application::createCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo{.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                       .queueFamilyIndex = this->_queueIndex};

    this->_commandPool = vk::raii::CommandPool(this->_logicalDevice, poolInfo);
}

void    Application::createVertexBuffer()
{
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);

	memcpy(dataStaging, vertices.data(), bufferSize);

	stagingBufferMemory.unmapMemory();

	std::tie(this->_vertexBuffer, this->_vertexBufferMemory) = createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

	copyBuffer(stagingBuffer, this->_vertexBuffer, bufferSize);
}

void    Application::createIndexBuffer()
{
    vk::DeviceSize  bufferSize = sizeof(indices[0]) * indices.size();

    auto [stagingBuffer, stagingBufferMemory] = this->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    
    void    *data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, indices.data(), static_cast<std::size_t>(bufferSize));
    stagingBufferMemory.unmapMemory();

    std::tie(this->_indexBuffer, this->_indexBufferMemory) = this->createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

    this->copyBuffer(stagingBuffer, this->_indexBuffer, bufferSize);
}

void    Application::createUniformBuffers()
{
    for (std::size_t i {0}; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vk::DeviceSize  bufferSize = sizeof(UniformBufferObject);

        auto [buffer, bufferMem] = this->createBuffer(
            bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        this->_uniformBuffers.emplace_back(std::move(buffer));
        this->_uniformBuffersMemory.emplace_back(std::move(bufferMem));
        this->_uniformBuffersMapped.emplace_back(this->_uniformBuffersMemory.back().mapMemory(0, bufferSize));
    }
}

void    Application::createCommandBuffer()
{
    this->_commandBuffers.clear();

    vk::CommandBufferAllocateInfo allocInfo{ .commandPool = this->_commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = MAX_FRAMES_IN_FLIGHT };

    this->_commandBuffers = vk::raii::CommandBuffers(this->_logicalDevice, allocInfo);
}

void    Application::createSyncObjects()
{
    assert(this->_presentCompleteSemaphore.empty() && this->_renderFinishedSemaphore.empty() && this->_inFlightFences.empty());

    for (std::size_t i {0}; i < this->_swapChainImages.size(); ++i)
    {
        this->_renderFinishedSemaphore.emplace_back(this->_logicalDevice, vk::SemaphoreCreateInfo());
    }

    for (std::size_t i {0}; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        this->_presentCompleteSemaphore.emplace_back(this->_logicalDevice, vk::SemaphoreCreateInfo());
        this->_inFlightFences.emplace_back(this->_logicalDevice, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
    }
}

void    Application::initVulkan() {
    this->createInstance();
    this->setupDebugMessenger();
    this->createSurface();
    this->pickPhysicalDevice();
    this->createLogicalDevice();
    this->createSwapChain();
    this->createImageViews();
    this->createDescriptorSetLayout();
    this->createGraphicsPipeline();
    this->createCommandPool();
    this->createVertexBuffer();
    this->createIndexBuffer();
    this->createUniformBuffers();
    this->createCommandBuffer();
    this->createSyncObjects();
}

void    Application::mainLoop() {
    while(!glfwWindowShouldClose(this->_window)) {
        glfwPollEvents();
        this->drawFrame();
    }
    this->_logicalDevice.waitIdle();
}

void    Application::cleanup() {
    this->cleanupwapChain();

    this->_inFlightFences.clear();
    this->_renderFinishedSemaphore.clear();
    this->_presentCompleteSemaphore.clear();
    this->_commandBuffers.clear();
    this->_vertexBuffer = nullptr;
    this->_vertexBufferMemory = nullptr;
    this->_indexBuffer = nullptr;
    this->_indexBufferMemory = nullptr;
    this->_commandPool = nullptr;
    this->_descriptorSetLayout = nullptr;
    this->_pipelineLayout      = nullptr;
    this->_graphicsPipeline = nullptr;
    this->_pipelineLayout = nullptr;
    this->_swapChainImageViews.clear();
    this->_swapChain = nullptr;
    this->_logicalDevice = nullptr;
    this->_surface = nullptr;
    this->_debugMessenger = nullptr;
    this->_instance = nullptr;

    glfwDestroyWindow(this->_window);
    glfwTerminate();
}

void    Application::drawFrame()
{   
    auto fenceResult = this->_logicalDevice.waitForFences(*this->_inFlightFences[this->_frameIndex], VK_TRUE, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to wait for fence!");
    }

    this->_logicalDevice.resetFences(*this->_inFlightFences[this->_frameIndex]);

    auto [result, imageIndex] = this->_swapChain.acquireNextImage(UINT64_MAX, *this->_presentCompleteSemaphore[this->_frameIndex], nullptr);

    this->_commandBuffers[this->_frameIndex].reset();
    this->recordCommandBuffer(imageIndex);

    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = 1,
		                                  .pWaitSemaphores      = &*this->_presentCompleteSemaphore[this->_frameIndex],
		                                  .pWaitDstStageMask    = &waitDestinationStageMask,
		                                  .commandBufferCount   = 1,
		                                  .pCommandBuffers      = &*this->_commandBuffers[this->_frameIndex],
		                                  .signalSemaphoreCount = 1,
		                                  .pSignalSemaphores    = &*this->_renderFinishedSemaphore[imageIndex]};

	this->_graphicsQueue.submit(submitInfo, *this->_inFlightFences[this->_frameIndex]);

	const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1, .pWaitSemaphores = &*this->_renderFinishedSemaphore[imageIndex], .swapchainCount = 1, .pSwapchains = &*this->_swapChain, .pImageIndices = &imageIndex};
	result = this->_graphicsQueue.presentKHR(presentInfoKHR);
	switch (result)
	{
		case vk::Result::eSuccess:
			break;
		case vk::Result::eSuboptimalKHR:
			std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
			break;
		default:
			break;        // an unexpected result is returned!
	}

    this->_frameIndex = (this->_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void    Application::updateUniformBuffer(uint32_t currentImage)
{
    
}

void    Application::cleanupwapChain()
{
    this->_swapChainImageViews.clear();
    this->_swapChain = nullptr;
}

void    Application::recreateSwapChain()
{
    int width = 0, height = 0;
	glfwGetFramebufferSize(this->_window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(this->_window, &width, &height);
		glfwWaitEvents();
	}

    this->_logicalDevice.waitIdle();

    this->cleanupwapChain();

    this->createSwapChain();
    this->createImageViews();
}

std::vector<const char*>    Application::getRequiredInstanceExtensions() {
    uint32_t    glfwExtensionCount = 0;
    auto        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*>    extension(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extension.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return (extension);
}

/*
* The first parameter specifies the severity of the message, which is one of the following flags:
*
* vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose: Diagnostic message from Vulkan components like the loader, layers and drivers
* vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo : Informational message like the creation of a resource
* vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning: Message about behavior that is not necessarily an error, but very likely a bug in your application
* vk::DebugUtilsMessageSeverityFlagBitsEXT::eError : Message about behavior that is invalid and may cause crashes
*
* The messageType parameter can have the following values:
*
* vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral : Some event has happened that is unrelated to the specification or performan
* vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation : Something has happened that violates the specification or indicates a possible mista
* vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance: Potential non-optimal use of Vulkan
*
* The pCallbackData parameter refers to a vk::DebugUtilsMessengerCallbackDataEXT struct containing the details of the message itself, with the most important members being:
*
* pMessage : The debug message as a null-terminated strin
* pObjects : Array of Vulkan object handles related to the messag
* objectCount: Number of objects in the array
*
* pUserData parameter contains a pointer specified during the setup of the callback and allows you to pass your own data to it.
*/

VKAPI_ATTR vk::Bool32 VKAPI_CALL Application::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                              vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                              const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                              void*                                         pUserData) {

    (void)severity;
    (void)pUserData;

    std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

    return (VK_FALSE);
}

bool Application::isDeviceSuitable( vk::raii::PhysicalDevice const & physicalDevice ) {
  // Check if the physicalDevice supports the Vulkan 1.3 API version
  bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

  // Check if any of the queue families support graphics operations
  auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
  bool supportsGraphics = std::ranges::any_of( queueFamilies, []( auto const & qfp ) { return !!( qfp.queueFlags & vk::QueueFlagBits::eGraphics ); } );

  // Check if all required physicalDevice extensions are available
  auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
  bool supportsAllRequiredExtensions =
    std::ranges::all_of( this->_requiredDeviceExtension,
                         [&availableDeviceExtensions]( auto const & requiredDeviceExtension )
                         {
                           return std::ranges::any_of( availableDeviceExtensions,
                                                       [requiredDeviceExtension]( auto const & availableDeviceExtension )
                                                       { return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0; } );
                         } );

  // Check if the physicalDevice supports the required features (dynamic rendering and extended dynamic state)
  auto features =
    physicalDevice
      .template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
  bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                  features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

  // Return true if the physicalDevice meets all the criteria
  return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
}

vk::SurfaceFormatKHR Application::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    const auto formatIt = std::ranges::find_if(
        availableFormats,
        [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });

    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

uint32_t            Application::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
{
    auto    minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    
    if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) 
    {
        minImageCount = surfaceCapabilities.maxImageCount;
    }

    return minImageCount;
}

vk::PresentModeKHR  Application::chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
{
	assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));

	return std::ranges::any_of(availablePresentModes,
	                           [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
	           vk::PresentModeKHR::eMailbox :
	           vk::PresentModeKHR::eFifo;
}

vk::Extent2D        Application::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}

	int width, height;
	glfwGetFramebufferSize(this->_window, &width, &height);

	return {
	    std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
	    std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}

std::vector<char>   Application::readFile(const std::string& filename)
{
    /*
    * ate start the reading at the end of the file
    * binary read the file as a binary file
    */ 
    std::ifstream   file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file");
    }

    // reseve enough space for the buffer
    std::vector<char>   buffer(file.tellg());

    // place the pointer to the begining of the file
    file.seekg(0, std::ios::beg);

    // give the data of the file inside the buffer
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();

    return(buffer);
}

[[nodiscard]] vk::raii::ShaderModule    Application::createShaderModule(const std::vector<char>& code) const
{
    vk::ShaderModuleCreateInfo createInfo{  .codeSize = code.size() * sizeof(char),
                                            .pCode = reinterpret_cast<const uint32_t*>(code.data())};

    vk::raii::ShaderModule      shaderModule{this->_logicalDevice, createInfo};

    return (shaderModule);
}

// for the transition of the image layout before and after the rendering
void    Application::transitionImageLayout(
            uint32_t                imageIndex,
            vk::ImageLayout         old_layout,
            vk::ImageLayout         new_layout,
            vk::AccessFlags2        src_access_mask,
            vk::AccessFlags2        dst_access_mask,
            vk::PipelineStageFlags2 src_stage_mask,
            vk::PipelineStageFlags2 dst_stage_mask
        )
{
    vk::ImageMemoryBarrier2 barrier = {
		.srcStageMask        = src_stage_mask,
		.srcAccessMask       = src_access_mask,
		.dstStageMask        = dst_stage_mask,
		.dstAccessMask       = dst_access_mask,
		.oldLayout           = old_layout,
		.newLayout           = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image               = this->_swapChainImages[imageIndex],
		.subresourceRange    = {
		       .aspectMask     = vk::ImageAspectFlagBits::eColor,
		       .baseMipLevel   = 0,
		       .levelCount     = 1,
		       .baseArrayLayer = 0,
		       .layerCount     = 1}};
        
	vk::DependencyInfo dependencyInfo = {
		.dependencyFlags         = {},
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers    = &barrier};

    this->_commandBuffers[this->_frameIndex].pipelineBarrier2(dependencyInfo);
}

void    Application::recordCommandBuffer(uint32_t imageIndex)
{
    auto &commandBuffer = this->_commandBuffers[this->_frameIndex];
    commandBuffer.begin({});

    // Transition the image layout for rendering
    this->transitionImageLayout(
        imageIndex,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput
    );

    // Set up the color attachment
    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::RenderingAttachmentInfo attachmentInfo = {
		    .imageView   = this->_swapChainImageViews[imageIndex],
		    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		    .loadOp      = vk::AttachmentLoadOp::eClear,
		    .storeOp     = vk::AttachmentStoreOp::eStore,
		    .clearValue  = clearColor};

    // Set up the rendering info
    vk::RenderingInfo renderingInfo = {
		    .renderArea           = {.offset = {0, 0}, .extent = this->_swapChainExtent},
		    .layerCount           = 1,
		    .colorAttachmentCount = 1,
		    .pColorAttachments    = &attachmentInfo};

    // Begin rendering
    commandBuffer.beginRendering(renderingInfo);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *this->_graphicsPipeline);
    commandBuffer.bindVertexBuffers(0, *this->_vertexBuffer, {0});
    commandBuffer.bindIndexBuffer(*this->_indexBuffer, 0, vk::IndexType::eUint16);
    commandBuffer.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(this->_swapChainExtent.width), static_cast<float>(this->_swapChainExtent.height), 0.0f, 1.0f});
    commandBuffer.setScissor(0, vk::Rect2D{vk::Offset2D{0, 0}, this->_swapChainExtent});
    commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    // End rendering
    commandBuffer.endRendering();

    // Transition the image layout for presentation
    this->transitionImageLayout(
        imageIndex,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe
    );

    commandBuffer.end();
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Application::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
    vk::BufferCreateInfo   bufferInfo{.size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
    vk::raii::Buffer       buffer          = vk::raii::Buffer(this->_logicalDevice, bufferInfo);
    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{.allocationSize = memRequirements.size, .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
    vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(this->_logicalDevice, allocInfo);
    buffer.bindMemory(*bufferMemory, 0);
    return {std::move(buffer), std::move(bufferMemory)};
}

void    Application::copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size)
{
    vk::CommandBufferAllocateInfo allocInfo{ .commandPool = this->_commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
    vk::raii::CommandBuffer commandCopyBuffer = std::move(this->_logicalDevice.allocateCommandBuffers(allocInfo).front());
    commandCopyBuffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{0, 0, size});
    commandCopyBuffer.end();
    this->_graphicsQueue.submit(vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer}, nullptr);
    this->_graphicsQueue.waitIdle();
}

uint32_t    Application::findMemoryType(uint32_t typeFiler, vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties  memProperties = this->_physicalDevice.getMemoryProperties();

    for (uint32_t i {0}; i < memProperties.memoryTypeCount; ++i)
    {
        if ((typeFiler & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}