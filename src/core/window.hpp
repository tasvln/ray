#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <string>

class Window {
    public:
        Window(int width, int height, const std::string& title) : width(width), height(height), framebufferResized(false) {
            if (!glfwInit()) {
                throw std::runtime_error("Failed to initialize GLFW");
            }

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

            window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
            if (!window) {
                glfwTerminate();
                throw std::runtime_error("Failed to create GLFW window");
            }

            glfwMakeContextCurrent(window);
            glfwSetWindowUserPointer(window, this);
            glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        };

        ~Window(){
            if (window) {
                glfwDestroyWindow(window);
            }
            glfwTerminate();
        };

        bool shouldClose() const {
            return glfwWindowShouldClose(window);
        }

        void pollEvents() const {
            glfwPollEvents();
        }

        bool isFramebufferResized() const {
            return framebufferResized;
        }

        void resetFramebufferResized() {
            framebufferResized = false;
        }

        int getWidth() const {
            return width;
        }

        int getHeight() const {
            return height;
        }

        GLFWwindow* getWindow() const {
            return window;
        }
        
    private:
        GLFWwindow* window;
        int width;
        int height;
        bool framebufferResized;

        static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
            Window* win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
            win->framebufferResized = true;
            win->width = width;
            win->height = height;
        }
};

#endif