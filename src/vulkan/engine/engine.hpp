#pragma once

#include "camera_controller.hpp"
#include "ray_engine.hpp"

class Engine {
    public:
        Engine() {
            
            // set configurations
            {
                config.appName = "Ray";
                config.width = 1400;
                config.height = 800;

                // number of ray samples per pixel
                config.numOfSamples = 8;
                // number of maxium bouces per day
                config.numOfBounces = 16;
                // the total number of accumulated ray samples per pixel
                config. maxNumberOfSamples = (64 * 1024);

                config.heatMapScale = 1.5f;

                config.enableRayTracing = true;
                config.enableValidationLayers = true;
                config.enableWireframeMode = false;
                config.enableHeatMap = false;

                config.isFullscreen = false;
                config.isResizable = false;
                config.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            }

            const auto validationLayers = config.enableValidationLayers ? 
            
            std::vector<const char*> {
                "VK_LAYER_KHRONOS_validation"
            } : std::vector<const char*>();

            // create window
            window = std::make_unique<Window>(config);
            // create instance
            instance = std::make_unique<VulkanInstance>(validationLayers);
            // create surface
            surface = std::make_unique<VulkanSurface>(*instance, *window);

            // run ray engine
            rayEngine = std::make_unique<VulkanRayEngine>(
                config,
                resources,
                *window,
                *instance,
                *surface,
                currentFrame
            );

            // createDevice
            createDevice();

            createSwapChain();
        }

        ~Engine() {}

        void createSwapChain() {
            rayEngine->createSwapChain();

            resetAccumulatedImage = true;
        }

        void reBuildEngine() {
            rayEngine->getRasterEngine().getDevice().wait();
            rayEngine->clearSwapChain();
            rayEngine->clearAS();
            setOnDevice();
            createSwapChain();
        }

        void createDevice() {
            rayEngine->createDevice();
            rayEngine->setRayOnDevice();
            setOnDevice();
        }

        void setOnDevice() {
            createSceneResources();
            rayEngine->createAS();
        }

        void createSceneResources() {
            std::vector<VulkanModel> models;
            std::vector<VulkanTexture> textures;

            camConfig.modelView = glm::mat4(1.0f);
            camConfig.pov = 90;
            camConfig.aperture = .05f;
            camConfig.focusDistance = 2.0f;
            camConfig.controlSpeed = 2.0f;
            camConfig.isGammaCorrected = true;
            camConfig.hasSky = true;

            camera = make_unique<Camera>(
                glm::vec3(0.0f, 0.0f, 5.0f)
            );
            cameraController = make_unique<CameraController>(camera);

            VulkanModel model("../assets/models/cottage/cottage_obj.obj");
            models.emplace_back(model);

            VulkanTexture texture("../assets/textures/cottage/cottage_diffuse.png");
            textures.emplace_back(textures);

            resources = std::make_unique<VulkanSceneResources>(
                rayEngine->getRasterEngine().getDevice(),
                rayEngine->getRasterEngine().getCommandPool(),
                models,
                textures
            );

            camera->reset(camConfig.modelView);
            resetAccumulatedImage = true;
        }

        void run() {
            // if (device) 
            //     throw std::runtime_error("Physical device has not been created");
            
            currentFrame = 0;
            
            window->drawFrame = [this]() {
                drawFrame();
            };

            window->onKey = [this](const int key, const int scancode, const int action, const int mods) {
                onKey(key, scancode, action, mods);
            };

            window->onCursorPosition = [this](const double xpos, const double ypos) {
                onCursorPosition(xpos, ypos);
            };

            window->onMouseButton = [this](const int button, const int action, const int mods) {
                onMouseButton(button, action, mods);
            };

            window->onScroll = [this](const double xoffset, const double yoffset) {
                onScroll(xoffset, yoffset);
            };

            window->run();

            // wait for device
            
            // device->wait();
            rayEngine->getRasterEngine().getDevice().wait();
        }

        void onKey(int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS) {
                switch(key) {
                    case GLFW_KEY_ESCAPE:
                        window->close();
                        break;
                    // Add any custom key toggles here if needed
                    default:
                        break;
                }
            }

            resetAccumulatedImage |= cameraController->onKey(key, scancode, action, mods);
        }

        void onCursorPosition(const double xpos, const double ypos) {
            resetAccumulatedImage |= cameraController->onCursorPosition(xpos, ypos);
        }

        void onMouseButton(const int button, const int action, const int mods) {
            resetAccumulatedImage |= cameraController->onMouseButton(button, action, mods);
        }

        void onScroll(const double xoffset, const double yoffset) {
            const auto prevPov = camConfig.pov;

            camConfig.pov = std::clamp(
                static_cast<float>(prevPov - yoffset),
                camConfig.minPov,
                camConfig.maxPov
            );

            resetAccumulatedImage = prevPov != camConfig.pov;
        }

        bool checkConfig(EngineConfig prevEngineConfig, CameraConfig prevCamConfig) {
            return config.enableRayTracing != prevEngineConfig.enableRayTracing || config.numOfBounces != prevEngineConfig.numOfBounces ||
            camConfig.pov != prevCamConfig.pov || camConfig.aperture != prevCamConfig.aperture || camConfig.focusDistance != prevCamConfig.focusDistance;
        }

        void drawFrame() {
            if (
                resetAccumulatedImage || checkConfig(prevConfig, prevCamConfig) // | !config.acculateRays
            ) {
                totalNumberOfSamples = 0;
                resetAccumulatedImage = false;
            }

            prevConfig = config;
            prevCamConfig = camConfig;

            // sample count
            numberOfSamples = glm::clamp(
                config.maxNumberOfSamples - totalNumberOfSamples,
                0u,
                config.numOfSamples
            );

            totalNumberOfSamples += numberOfSamples;

            constexpr auto noTimeout = std::numeric_limits<uint64_t>::max();

            auto& inFlightFence = rayEngine->getRasterEngine().getInFlightFences()[currentFrame];
            const auto imageAvailableSemaphore = rayEngine->getRasterEngine().getImageAvailableSemaphores()[currentFrame].getSemaphore();
            const auto renderFinishSemaphore = rayEngine->getRasterEngine().getRenderFinishedSemaphores()[currentFrame].getSemaphore();

            inFlightFence.wait(noTimeout);

            uint32_t imageIndex;
            if(!rayEngine->getRasterEngine().getAcquiredNextImage(imageIndex)) return;

            auto commandBuffer = rayEngine->getRasterEngine().getCommandBuffers().begin(currentFrame);
            render(commandBuffer, imageIndex);
            rayEngine->getRasterEngine().getCommandBuffers().end(currentFrame);

            updateUniformBuffer();

            rayEngine->getRasterEngine().submitRender(commandBuffer, imageAvailableSemaphore, renderFinishSemaphore);

            if (!rayEngine->getRasterEngine().presentImage(imageIndex)) return;

            currentFrame = (currentFrame + 1) % rayEngine->getRasterEngine().getInFlightFences().size();
            rayEngine->setCurrentFrame(currentFrame);
        }

        void updateUniformBuffer() {
            rayEngine->getRasterEngine().getUniformBuffers()[currentFrame].setUniformBufferInMemory(
                getUniformBufferObject(
                    rayEngine->getRasterEngine().getSwapChain().getSwapChainExtent()
                )
            );
        }

        void render(VkCommandBuffer commandBuffer, const uint32_t imageIndex) {

            // camera
            const auto deltaTime = tick();
            resetAccumulatedImage = cameraController->updateCamera(camConfig.controlSpeed, deltaTime);

            // render scene
            config.enableRayTracing ? 
                    rayEngine->render(commandBuffer, imageIndex)
                : 
                    rayEngine->getRasterEngine().render(commandBuffer, imageIndex);
        }

        // camera matrix gets passed into the UBO
        UniformBufferObject getUniformBufferObject(const VkExtent2D extent) const {
            UniformBufferObject ubo {};

            ubo.modelView = camera->getViewMatrix();
            ubo.projection = glm::perspective(
                glm::radians(camConfig.pov),
                extent.width / static_cast<float>(extent.height),
                0.1f,
                10000.0f
            );
            // inverting the Y coordinate in vulkan cause it ppoints down by default
            ubo.projection[1][1] *= -1;
            ubo.modelViewInverse = glm::inverse(ubo.modelView);
            ubo.projectionInverse = glm::inverse(ubo.projectionInverse);
            ubo.aperture = camConfig.aperture;
            ubo.focusDistance = camConfig.focusDistance;

            // why?
            ubo.totalNumberOfSamples = totalNumberOfSamples;
            ubo.numberOfSamples = config.numOfSamples;
            ubo.numberOfBounces = config.numOfBounces;

            ubo.randomSeed = 1;
            ubo.hasSky = camConfig.hasSky;
            ubo.showHeatmap = config.enableHeatMap;
            ubo.heatMapScale = config.heatMapScale;

            return ubo;
        }

        void getStats(double deltaTime) {

        }
        
    private:
        size_t currentFrame;
        double engineTime;

        EngineConfig config;   
        CameraConfig camConfig;

        EngineConfig prevConfig;
        CameraConfig prevCamConfig;

        uint32_t totalNumberOfSamples;
        uint32_t numberOfSamples;

        /* 
            In a progressive ray tracer, the image is rendered over multiple frames, and each frame contributes to a 
            better/less noisy result. This is called accumulation. If the camera moves , the accumulated image becomes 
            invalid because it's based on the old camera position. So, i need to reset the accumulation to start 
            rendering from scratch with the new camera view.
        */
        bool resetAccumulatedImage; 

        // i could just call both here and connect them together
        // that would be easier than the on-top-of-extenter scenario i was going for
        std::unique_ptr<VulkanRayEngine> rayEngine;

        // base level instances
        std::unique_ptr<Window> window;
        std::unique_ptr<VulkanInstance> instance;
        std::unique_ptr<VulkanSurface> surface;

        std::unique_ptr<VulkanSceneResources> resources;

        std::unique_ptr<Camera> camera;
        std::unique_ptr<CameraController> cameraController;

        double tick() {
            const auto newTime = window->getTime();
            const auto delta = newTime - engineTime;
            engineTime = newTime;
            return delta;
        }
};