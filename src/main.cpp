#define GLFW_INCLUDE_VULKAN
#include <iostream>
#include <string>

#include "core/window.hpp"
#include "vulkan/vulkan_context.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    Window window(1080, 600, "Ray");
    cout << "Initializing Ray..." << endl;
    VulkanContext vk(window);

    cout << "Ray initialized successfully." << endl;

    try{
      while (!window.shouldClose()) {
        window.pollEvents();
        // vk.drawFrame(window.getWindow());
      }
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}