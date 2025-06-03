#include <iostream>
#include <string>

#include "core/window.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    Window window(1080, 600, "Raytracer");
   
    while (!window.shouldClose()) {
      window.pollEvents();
    }

    return 0;
}