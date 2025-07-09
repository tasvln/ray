#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>

#include "core/window.hpp"

class VulkanSurface {
    public:
        VulkanSurface(const VkInstance& instance, const Window& window): instance(instance) {
            createSurface(window);
        }

        ~VulkanSurface(){
            if (surface != nullptr) {
                vkDestroySurfaceKHR(instance, surface, nullptr);
                surface = nullptr;
            }
        }

    private:
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkInstance instance = VK_NULL_HANDLE;

        void createSurface(const Window& window) {
            if (glfwCreateWindowSurface(instance, window.getWindow(), nullptr, &surface) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create window surface");
            }
        }
};