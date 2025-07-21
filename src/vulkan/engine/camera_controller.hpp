#pragma once

#include <GLFW/glfw3.h>
#include "core/camera.hpp"

class CameraController {
    public:
        CameraController(Camera& camera) : camera(camera) {}

        ~CameraController() = default;

        bool onKey(const int key, const int scancode, const int action, const int mods) {
            switch (key) {
                case GLFW_KEY_W:
                    isMovingForward = action != GLFW_RELEASE;
                    return true;
                case GLFW_KEY_A:
                    isMovingLeft = action != GLFW_RELEASE;
                    return true;
                case GLFW_KEY_S:
                    isMovingBackward = action != GLFW_RELEASE;
                    return true;
                case GLFW_KEY_D:
                    isMovingRight = action != GLFW_RELEASE;
                    return true;
                case GLFW_KEY_LEFT_CONTROL:
                    isMovingUpward = action != GLFW_RELEASE;
                    return true;
                case GLFW_KEY_LEFT_SHIFT:
                    isMovingDownward = action != GLFW_RELEASE;
                    return true;
                default:
                    return false;
            }
        }

        bool onCursorPosition(double xpos, double ypos) {
            const auto deltaX = static_cast<float>(xpos - mouse.x);
            const auto deltaY = static_cast<float>(ypos - mouse.y);

            if (leftMouseClicked) {
                camera.updateCamPostionData(deltaX, deltaY);
            }

            if (rightMouseClicked) {
                camera.updateModelPostionData(deltaX, deltaY);
            }

            mouse.x = xpos;
            mouse.y = ypos;

            return leftMouseClicked || rightMouseClicked;

        }

        bool onMouseButton(int button, int action, int mods) {
            if (button == GLFW_MOUSE_BUTTON_LEFT)
            {
                leftMouseClicked = action == GLFW_PRESS;
            }

            if (button == GLFW_MOUSE_BUTTON_LEFT)
            {
                leftMouseClicked = action == GLFW_PRESS;
            }
        }

        bool updateCamera(double speed, double deltaTime) {
            const float delta = static_cast<float>(speed * deltaTime);

            if (isMovingForward)
                camera.moveForward(delta);
            if (isMovingBackward)
                camera.moveForward(-delta);
            if (isMovingLeft)
                camera.moveRight(-delta);
            if (isMovingRight)
                camera.moveRight(delta);
            if (isMovingUpward)
                camera.moveUp(delta);
            if (isMovingDownward)
                camera.moveUp(-delta);

            camera.rotate();

            camera.setCamPostionData(0.0f, 0.0f);

            return isMovingForward || isMovingBackward || isMovingLeft || isMovingRight || isMovingUpward || isMovingDownward || camera.getCamPositonData().x != 0.0f || camera.getCamPositonData().y != 0.0f;
        }

    private:
        Camera camera;

        PositonData mouse;

        bool isMovingForward {}, isMovingBackward {}, isMovingLeft {}, isMovingRight {};
        bool isMovingUpward {}, isMovingDownward {};

        bool leftMouseClicked {}, rightMouseClicked {};
};
