#define GLFW_INCLUDE_VULKAN
#include <iostream>
#include <string>

#include "core/window.hpp"
#include "vulkan/vulkan_context.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    try{
      Window window(1080, 600, "Ray");
      VulkanContext vk(window);
    
      while (!window.shouldClose()) {
        window.pollEvents();
        vk.drawFrame();
      }
      
    } catch (const std::exception& e) {
      fprintf(stderr, "Error: %s\n", e.what());
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}