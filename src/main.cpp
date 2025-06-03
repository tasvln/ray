#include <iostream>
#include <string>

#include "core/window.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    Window window(800, 600, "Ray");

    while (!window.shouldClose()) {
        window.pollEvents();
    }

    return 0;
}