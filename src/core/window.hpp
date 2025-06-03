#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <string>

class Window {
    public:
        Window(int width, int height, const std::string& title){
            if (!glfwInit()) {
                throw std::runtime_error("Failed to initialize GLFW");
            }

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
            if (!window) {
                glfwTerminate();
                throw std::runtime_error("Failed to create GLFW window");
            }

            glfwMakeContextCurrent(window);
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

        GLFWwindow* getWindow() const {
            return window;
        }
    private:
        GLFWwindow* window;
};

#endif