#include "Application.hpp"

namespace {
}

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
* The instance is a connection between your application and the vulkan library
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

    // Get the required extension
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

    this->_physicalDevice = candidates.rbegin()->second;

    std::cout << "GPU chosen: " << this->_physicalDevice.getProperties().deviceName << std::endl;
}

void    Application::createLogicalDevice() {
    std::vector<vk::QueueFamilyProperties>  queueFamilyProperties = this->_physicalDevice.getQueueFamilyProperties();
    auto                                    graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) { return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0); });
    auto                                    graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
    float                                   queuePriority = 0.5f;

    vk::DeviceQueueCreateInfo               deviceQueueCreateInfo { .queueFamilyIndex = graphicsIndex,
                                                                    .queueCount = 1,
                                                                    .pQueuePriorities = &queuePriority};

    // Create a chain of feature structures
    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
        {},                               // vk::PhysicalDeviceFeatures2 (empty for now)
        {.dynamicRendering = true },      // Enable dynamic rendering from Vulkan 1.3
        {.extendedDynamicState = true }   // Enable extended dynamic state from the extension
    };

    vk::DeviceCreateInfo deviceCreateInfo{
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(this->_requiredDeviceExtension.size()),
        .ppEnabledExtensionNames = this->_requiredDeviceExtension.data()
    };

    this->_logicalDevice = vk::raii::Device(this->_physicalDevice, deviceCreateInfo);

    this->_graphicsQueue = vk::raii::Queue(this->_logicalDevice, graphicsIndex, 0);
}

void    Application::initVulkan() {
    this->createInstance();
    this->setupDebugMessenger();
    this->pickPhysicalDevice();
    this->createLogicalDevice();
}

void    Application::mainLoop() {
    while(!glfwWindowShouldClose(this->_window)) {
        glfwPollEvents();
    }
}

void    Application::cleanup() {
    glfwDestroyWindow(this->_window);
    glfwTerminate();
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

    return (vk::False);
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