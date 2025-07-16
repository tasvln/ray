#pragma once

#include "engine_config.hpp"
#include "vulkan/helpers/scene_resources.hpp"
#include "vulkan/raster/uniform_buffer.hpp"

class EngineInterface {
    public:
        virtual ~EngineInterface() = default;


        virtual void createSwapChain() = 0;
        virtual void clearSwapChain() = 0;

        virtual const VulkanSceneResources& getResources() const = 0;
        // virtual UniformData getUBO(VkExtent2D extent) const = 0;
        // virtual void render();

        virtual void run() = 0;
        virtual void drawFrame() = 0;
        virtual void updateUniformBuffer() = 0;

        virtual void run() = 0;

        virtual void onKey(int key, int scancode, int action, int mods) {}
		virtual void onCursorPosition(double xpos, double ypos) {}
		virtual void onMouseButton(int button, int action, int mods) {}
		virtual void onScroll(double xoffset, double yoffset) {}

    private:
};