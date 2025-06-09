#define GLFW_INCLUDE_VULKAN
#include <iostream>
#include <string>

#include "core/window.hpp"
#include "vulkan/vulkan_context.hpp"

using namespace std;

void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error [" << error << "]: " << description << std::endl;
}

int main(int argc, char* argv[]) {
    glfwSetErrorCallback(glfwErrorCallback);
    Window window(1080, 600, "Ray");
    VulkanContext vk(window);

    try{
      while (!window.shouldClose()) {
        window.pollEvents();
        vk.drawFrame(window.getWindow());
      }
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}