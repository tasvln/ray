#pragma once

#include "ray_engine.hpp"

class Engine {
    public:
        Engine() {
            
            // set configurations
            {
                config.appName = "Ray";
                config.width = 1400;
                config.height = 800;
                config.enableRayTracing = true;
                config.enableValidationLayers = true;
                config.enableWireframeMode = false;
                config.isFullscreen = false;
                config.isResizable = false;
                config.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            }

            const auto validationLayers = config.enableValidationLayers ? 
            
            std::vector<const char*> {
                "VK_LAYER_KHRONOS_validation"
            } : std::vector<const char*>();

            // create window
            window = std::make_unique<Window>(config);
            // create instance
            instance = std::make_unique<VulkanInstance>(validationLayers);
            // create surface
            surface = std::make_unique<VulkanSurface>(*instance, *window);
            // create device


            // run ray engine
            // rayEngine = std::make_unique<VulkanRayEngine>(
            //     config,
            //     resources,
            //     currentFrame
            // )



        }

        ~Engine() {}

        void run() {
            // if (device) 
            //     throw std::runtime_error("Physical device has not been created");
            
            currentFrame = 0;
            
            window->drawFrame = [this]() {
                drawFrame();
            };

            window->onKey = [this](const int key, const int scancode, const int action, const int mods) {
                onKey(key, scancode, action, mods);
            };

            window->onCursorPosition = [this](const double xpos, const double ypos) {
                onCursorPosition(xpos, ypos);
            };

            window->onMouseButton = [this](const int button, const int action, const int mods) {
                onMouseButton(button, action, mods);
            };

            window->onScroll = [this](const double xoffset, const double yoffset) {
                onScroll(xoffset, yoffset);
            };

            window->run();

            // wait for device
            
            // device->wait();
        }

    private:
        size_t currentFrame;
        // i could just call both here and connect them together
        // that would be easier than the on-top-of-extenter scenario i was going for
        std::unique_ptr<VulkanRayEngine> rayEngine;

        std::unique_ptr<VulkanSceneResources> resources;
        EngineConfig config;

        // base level instances
        std::unique_ptr<Window> window;
        std::unique_ptr<VulkanInstance> instance;
        std::unique_ptr<VulkanSurface> surface;


};