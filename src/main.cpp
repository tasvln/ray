#define GLFW_INCLUDE_VULKAN
#include <iostream>
#include <string>

#include "core/window.hpp"
#include "vulkan/vulkan_context.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    Window window(1440, 800, "Ray");
    cout << "Initializing Ray..." << endl;
    VulkanContext vk(window);

    cout << "Ray initialized successfully." << endl;

    try{
      while (!window.shouldClose()) {
        window.pollEvents();
        // vk.drawFrame(window.getWindow());
        vk.renderFrame();
      }
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    }
    
    vk.waitDeviceIdle();
    return EXIT_SUCCESS;
}