#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct PositonData {
    float x {};
    float y {};
};

// camera using a 4x4 matrix -> way better than eular tranformations
class Camera {
public:
    Camera(
        const glm::vec3& position = glm::vec3(0.0f, 0.0f, 5.0f)
    ) :
        position(position),
        orientation(1.0f),
        right(1.0f, 0.0f, 0.0f),
        up(0.0f, 1.0f, 0.0f),
        front(0.0f, 0.0f, -1.0f)
    {}

    ~Camera() = default;

    glm::mat4 getViewMatrix() const {
        // The view matrix transforms world coordinates to camera space.
        // Optionally multiplied by model rotation (if you want model rotation separate from camera).
        // This matrix is what you pass to Vulkan’s uniform buffer or shader as the camera view.

        // Apply incremental rotation from mouse input
        // note swap (rotY affects X axis)
        // values are divided by 300.0 to scale them down (i.e., reduce sensitivity).
        const auto cameraRotX = static_cast<float>(model.y / 300.0);
        const auto cameraRotY = static_cast<float>(model.x / 300.0);

        // Model rotation matrix ("camera local rotation"???)
        glm::mat4 modelMatrix = 
                glm::rotate(
                    glm::mat4(1.0f), cameraRotY * glm::radians(90.0f), glm::vec3(0,1,0)
                ) * 
                glm::rotate(
                    glm::mat4(1.0f), cameraRotX * glm::radians(90.0f), glm::vec3(1,0,0)
                );

        // Camera view matrix = orientation * translate(-position)
        glm::mat4 viewMatrix = orientation * glm::translate(glm::mat4(1.0f), -position);

        return viewMatrix * modelMatrix;
    }

    void rotate() {
        // Yaw (turning left/right) rotate in world space x.
        // Pitch (looking up/down) rotate in camera's local space y.
        // basically a "head turning" behavior that feels natural.

        glm::mat4 yawX = glm::rotate(glm::mat4(1.0f), cam.x / 300.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 pitchY = glm::rotate(glm::mat4(1.0f), cam.y / 300.0f, glm::vec3(1.0f, 0.0f, 0.0f));

        orientation = yawX * orientation * pitchY;

        update();
    }

    void reset(const glm::mat4& modelView) {
        // Given a modelView matrix (usually from your scene or previous state), invert it to get the world transform.
        // Extract position and orientation from it.
        // Reset rotation increments (rotX_, rotY_) which store small changes from mouse or input.
        auto inverseModelView = glm::inverse(modelView);

        position = glm::vec3(inverseModelView * glm::vec4(0, 0, 0, 1));

        // Extract rotation only (3x3 upper-left), discard scale/translation
        orientation = glm::mat4(glm::mat3(modelView));

        // Rotation increments (e.g. from mouse delta)
        cam.x = 0.0f;
        cam.y = 0.0f;

        model.x = 0.0f;
        model.y = 0.0f;

        update();
    }

    void moveForward(float delta) {
        position += front * delta;
    }

    void moveRight(float delta) {
        position += right * delta;
    }

    void moveUp(float delta) {
        position += up * delta;
    }

    const glm::vec3& getPosition() const { 
        return position; 
    }

    const PositonData getCamPositonData() const { 
        return cam; 
    }

    const PositonData getModelPositonData() const { 
        return model; 
    }

    void updateCamPostionData(const float x, const float y) {
        cam.x += x;
        cam.y += y;
    }

    void updateModelPostionData(const float x, const float y) {
        model.x += x;
        model.y += y;
    }

    void setCamPostionData(const float x, const float y) {
        cam.x = x;
        cam.y = y;
    }

    void setModelPostionData(const float x, const float y) {
        model.x = x;
        model.y = y;
    }

private:
    glm::vec3 position;
    // a 4x4 matrix representing camera rotation (no translation)
    // stores camera rotation, so we can easily rotate the camera by multiplying rotation matrices.
    glm::mat4 orientation;

    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;

    PositonData cam {};
    PositonData model {};

    // update camera vectors
    void update() { 
        // Inverse orientation rotates from camera space to world space
        glm::mat4 invertedOrientation = glm::inverse(orientation);

        right = glm::normalize(
            glm::vec3(
                invertedOrientation * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)
            )
        );

        up = glm::normalize(
            glm::vec3(
                invertedOrientation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)
            )
        );

        front = glm::normalize(
            glm::vec3(
                invertedOrientation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)
            )
        );
    }
};
