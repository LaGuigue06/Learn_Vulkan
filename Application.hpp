#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <map>
#include <limits>
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
        vk::raii::SwapchainKHR              _swapChain = nullptr;
        std::vector<vk::Image>              _swapChainImages;
        vk::SurfaceFormatKHR                _swapChainSurfaceFormat;
        vk::Extent2D                        _swapChainExtent;
        std::vector<vk::raii::ImageView>    _swapChainImageViews;

        std::vector<const char*>            _requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

        void    initWindow();
        void    createInstance();
        void    setupDebugMessenger();
        void    createSurface();
        void    pickPhysicalDevice();
        void    createLogicalDevice();
        void    createSwapChain();
        void    initVulkan();
        void    mainLoop();
        void    cleanup();

        std::vector<const char*>    getRequiredInstanceExtensions();

        // Swap chain
        vk::SurfaceFormatKHR    chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
        uint32_t                chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities);
        vk::PresentModeKHR      chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes);
        vk::Extent2D            chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities);

        // Not used yet
        bool                        isDeviceSuitable( vk::raii::PhysicalDevice const& physicalDevice);

        static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                              vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                              const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                              void*                                         pUserData);

    public:
        void    run();
};
