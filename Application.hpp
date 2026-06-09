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

#include <glm/glm.hpp>

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <map>
#include <limits>
#include <stdexcept>
#include <vector>
#include <fstream>

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr uint32_t  WIDTH = 800;
constexpr uint32_t  HEIGHT = 600;
constexpr int       MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex
{
    glm::vec2   pos;
    glm::vec3   color;

    static vk::VertexInputBindingDescription    getBindingDescription()
    {
        return {.binding {0},
                .stride {sizeof(Vertex)},
                .inputRate {vk::VertexInputRate::eVertex}};
    }

    static std::array<vk::VertexInputAttributeDescription, 2>   getAttributeDescriptor()
    {
        return {{
        	{
            	.location = 0,
            	.binding = 0,
            	.format = vk::Format::eR32G32Sfloat,
            	.offset = offsetof(Vertex, pos)
        	},
        	{
            	.location = 1,
            	.binding = 0,
            	.format = vk::Format::eR32G32B32Sfloat,
            	.offset = offsetof(Vertex, color)
        	}
    	}};
    }
};

const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};

class Application {
    private:
        GLFWwindow*                             _window = nullptr;
        vk::raii::Context                       _context;
        vk::raii::Instance                      _instance = nullptr;
        vk::raii::DebugUtilsMessengerEXT        _debugMessenger = nullptr;
        vk::raii::PhysicalDevice                _physicalDevice = nullptr;
        vk::raii::Device                        _logicalDevice = nullptr;
        uint32_t                                _queueIndex     = ~0;
        vk::raii::Queue                         _graphicsQueue = nullptr;
        vk::raii::SurfaceKHR                    _surface = nullptr;
        vk::raii::SwapchainKHR                  _swapChain = nullptr;
        std::vector<vk::Image>                  _swapChainImages;
        vk::SurfaceFormatKHR                    _swapChainSurfaceFormat;
        vk::Extent2D                            _swapChainExtent;
        std::vector<vk::raii::ImageView>        _swapChainImageViews;
        vk::raii::PipelineLayout                _pipelineLayout = nullptr;
        vk::raii::Pipeline                      _graphicsPipeline = nullptr;
        vk::raii::CommandPool                   _commandPool = nullptr;
        vk::raii::Buffer                        _vertexBuffer = nullptr;
        std::vector<vk::raii::CommandBuffer>    _commandBuffers;
        std::vector<vk::raii::Semaphore>        _presentCompleteSemaphore;
        std::vector<vk::raii::Semaphore>        _renderFinishedSemaphore;
        std::vector<vk::raii::Fence>            _inFlightFences;

        uint32_t                                _frameIndex {0};

        // All the extension needed
        std::vector<const char*>            _requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

        void    initWindow();
        void    createInstance();
        void    setupDebugMessenger();
        void    createSurface();
        void    pickPhysicalDevice();
        void    createLogicalDevice();
        void    createSwapChain();
        void    createImageViews();
        void    createGraphicsPipeline();
        void    createCommandPool();
        void    createVertexBuffer();
        void    createCommandBuffer();
        void    createSyncObjects();
        void    initVulkan();
        void    mainLoop();
        void    cleanup();

        void    drawFrame();
        void    cleanupwapChain();
        void    recreateSwapChain();

        std::vector<const char*>    getRequiredInstanceExtensions();

        // Swap chain helper
        vk::SurfaceFormatKHR    chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
        uint32_t                chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities);
        vk::PresentModeKHR      chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes);
        vk::Extent2D            chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities);

        // shader help
        std::vector<char>                       readFile(const std::string& filename);
        [[nodiscard]] vk::raii::ShaderModule    createShaderModule(const std::vector<char>& code) const;

        // commandPool helper
        void    transitionImageLayout(
            uint32_t                imageIndex,
            vk::ImageLayout         old_layout,
            vk::ImageLayout         new_layout,
            vk::AccessFlags2        src_access_mask,
            vk::AccessFlags2        dst_access_mask,
            vk::PipelineStageFlags2 src_stage_mask,
            vk::PipelineStageFlags2 dst_stage_mask
        );
        void    recordCommandBuffer(uint32_t imageIndex);

        // Memory
        uint32_t    findMemoryType(uint32_t typeFiler, vk::MemoryPropertyFlags properties);

        // Not used yet
        bool                        isDeviceSuitable( vk::raii::PhysicalDevice const& physicalDevice);

        static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                              vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                              const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                              void*                                         pUserData);

    public:
        void    run();
};
