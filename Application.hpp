#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <map>
#include <stdexcept>
#include <vector>

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

class Application {
    private:
        GLFWwindow*                         _window = nullptr;
        vk::raii::Context                   _context;
        vk::raii::Instance                  _instance = nullptr;
        vk::raii::DebugUtilsMessengerEXT    _debugMessenger = nullptr;
        vk::raii::PhysicalDevice            _physicalDevice = nullptr;
        vk::raii::Device                    _logicalDevice = nullptr;
        vk::raii::Queue                     _graphicsQueue = nullptr;
        vk::raii::SurfaceKHR                _surface = nullptr;

        std::vector<const char*>            _requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

        void    initWindow();
        void    createInstance();
        void    setupDebugMessenger();
        void    pickPhysicalDevice();
        void    createLogicalDevice();
        void    initVulkan();
        void    mainLoop();
        void    cleanup();

        std::vector<const char*>    getRequiredInstanceExtensions();

        // Not used yet
        bool                        isDeviceSuitable( vk::raii::PhysicalDevice const& physicalDevice);

        static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                              vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                              const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                              void*                                         pUserData);

    public:
        void    run();
};
