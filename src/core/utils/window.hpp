#pragma once

#include "core/window.hpp"
#include <iostream>
#include <stdexcept>

namespace utils {
    void glfwErrorCallback(const int error, const char* const description)
    {
        std::cerr << "ERROR: GLFW: " << description << " (code: " << error << ")" << std::endl;
    }

	void glfwKeyCallback(GLFWwindow* window, const int key, const int scancode, const int action, const int mods)
	{
		auto* const pWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
		if (pWindow->onKey)
		{
			pWindow->onKey(key, scancode, action, mods);
		}
	}

	void glfwCursorPositionCallback(GLFWwindow* window, const double xpos, const double ypos)
	{
		auto* const pWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
		if (pWindow->onCursorPosition)
		{
			pWindow->onCursorPosition(xpos, ypos);
		}
	}

    void glfwMouseButtonCallback(GLFWwindow* window, const int button, const int action, const int mods)
	{
		auto* const pWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
		if (pWindow->onMouseButton)
		{
			pWindow->onMouseButton(button, action, mods);
		}
	}

	void GlfwScrollCallback(GLFWwindow* window, const double xoffset, const double yoffset)
	{
		auto* const pWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
		if (pWindow->onScroll)
		{
			pWindow->onScroll(xoffset, yoffset);
		}
	}
}