#ifndef WINDOW_HPP
#define WINDOW_HPP

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <functional>

#include "core/utils/window.hpp"
#include "vulkan/engine/engine_config.hpp"

class Window {
    public:
        Window(const EngineConfig config): config(config) {
            glfwSetErrorCallback(utils::glfwErrorCallback);

            if (!glfwInit()) {
                throw std::runtime_error("Failed to initialize GLFW");
            }

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

            // if (!glfwVulkanSupported()) {
            //     throw std::runtime_error("Vulkan not support with this version of GLFW");
            // }

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, config.isResizable ? GLFW_TRUE : GLFW_FALSE);

            auto* const primaryMonitor = config.isFullscreen ? glfwGetPrimaryMonitor() : nullptr;

            window = glfwCreateWindow(config.width, config.height, config.appName.c_str(), primaryMonitor, nullptr);

            if (!window) {
                throw std::runtime_error("Failed to create GLFW window");
            }
            
            glfwSetWindowUserPointer(window, this);
            glfwSetKeyCallback(window, utils::glfwKeyCallback);
            glfwSetCursorPosCallback(window, utils::glfwCursorPositionCallback);
            glfwSetMouseButtonCallback(window, utils::glfwMouseButtonCallback);
            glfwSetScrollCallback(window, utils::GlfwScrollCallback);
        };

        ~Window(){
            if (window) {
                glfwDestroyWindow(window);
            }
            
            glfwTerminate();
            glfwSetErrorCallback(nullptr);
        };

        void run() {
            // why did he do this?
            glfwSetTime(0.0);

            while(!glfwWindowShouldClose(window)) {
                pollEvents();

                if (drawFrame) {
                    drawFrame();
                }
            }
        }

        std::function<void()> drawFrame;
		std::function<void(int key, int scancode, int action, int mods)> onKey;
		std::function<void(double xpos, double ypos)> onCursorPosition;
		std::function<void(int button, int action, int mods)> onMouseButton;
		std::function<void(double xoffset, double yoffset)> onScroll;

        GLFWwindow* getWindow() const {
            return window;
        }

        VkExtent2D getFramebufferSize() const
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            return VkExtent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        }

        VkExtent2D getWindowSize() const
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            return VkExtent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        }

        const char* getKeyName(const int key, const int scancode) const
        {
            return glfwGetKeyName(key, scancode);
        }

        void pollEvents() const {
            glfwPollEvents();
        }

        void wait() const {
            glfwWaitEvents();
        }

        bool close() const {
            return glfwWindowShouldClose(window);
        }
        
        bool isMinimized() const {
            const auto size = getFramebufferSize();
            return size.height == 0 && size.width == 0;
        }

        int getWidth() const {
            return config.width;
        }

        int getHeight() const {
            return config.height;
        }

        double getTime() const
        {
            return glfwGetTime();
        }
        
    private:
        const EngineConfig config;
        GLFWwindow* window;
};

#endif