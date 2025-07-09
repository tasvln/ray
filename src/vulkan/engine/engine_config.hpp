#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>

#include <string>

struct EngineConfig
{
    VkPresentModeKHR presentMode;
    std::string appName;
    uint32_t width;
    uint32_t height;
    bool isResizable;
    bool isFullscreen;
    bool enableValidationLayers;
    bool enableWireframeMode;
    bool enableRayTracing;
};
