#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <iostream>

#include <string>

struct EngineConfig
{
    VkPresentModeKHR presentMode;
    std::string appName;
    uint32_t width;
    uint32_t height;
    uint32_t numOfSamples;
    uint32_t numOfBounces;
    uint32_t maxNumberOfSamples;
    float heatMapScale;
    bool isResizable;
    bool isFullscreen;
    bool enableValidationLayers;
    bool enableWireframeMode;
    bool enableRayTracing;
    bool enableHeatMap;
};

struct CameraConfig {
    glm::mat4 modelView;
    float pov; // field of view
    float aperture;
    float focusDistance;
    float controlSpeed;
    bool isGammaCorrected;
    bool hasSky;
    inline const static float minPov = 10.0f;
    inline const static float maxPov = 10.0f;
};
