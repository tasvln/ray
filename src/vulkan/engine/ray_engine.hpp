#pragma once

#include "raster_engine.hpp"
#include "engine_interface.hpp"

#include "vulkan/ray/acceleration_structure.hpp"
#include "vulkan/ray/device_properties.hpp"
#include "vulkan/ray/ray_pipeline.hpp"
#include "vulkan/ray/dispatch_table.hpp"
#include "vulkan/ray/blas_geometry.hpp"
#include "vulkan/ray/blas.hpp"
#include "vulkan/ray/tlas.hpp"
#include "vulkan/ray/stb.hpp"

#include "vulkan/utils/ray_engine.hpp"

struct RayEngineImageData {

};

class VulkanRayEngine: public EngineInterface {
    public:
        VulkanRayEngine(
            const EngineConfig& config, 
            const VkPresentModeKHR presentMode,
            const VulkanSceneResources& resources
        ) :
            rasterEngine(std::make_unique<VulkanRasterEngine>(config, resources, VK_PRESENT_MODE_FIFO_KHR))
        {
            // hmm?
        }

        ~VulkanRayEngine() {
            rasterEngine->clearSwapChain();
            clearAS();
        }

        void setDevice(
            std::vector<const char*>& requiredExtensions,
            VkPhysicalDeviceFeatures& deviceFeatures,
            void* nextDeviceFeatures
        ) {
            requiredExtensions.insert(requiredExtensions.end(),
            {	
                VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME
            });

            // Buffer device address features
            VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {};
            bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            bufferDeviceAddressFeatures.pNext = nextDeviceFeatures;
            bufferDeviceAddressFeatures.bufferDeviceAddress = true;

            // Descriptor indexing features
            VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
            indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            indexingFeatures.pNext = &bufferDeviceAddressFeatures;
            indexingFeatures.runtimeDescriptorArray = true;
            indexingFeatures.shaderSampledImageArrayNonUniformIndexing = true;

            // Acceleration structure features
            VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
            accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            accelerationStructureFeatures.pNext = &indexingFeatures;
            accelerationStructureFeatures.accelerationStructure = true;
            
            // Ray tracing pipeline features
            VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {};
            rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            rayTracingFeatures.pNext = &accelerationStructureFeatures;
            rayTracingFeatures.rayTracingPipeline = true;

            rasterEngine->createDevice(
                requiredExtensions,
                deviceFeatures,
                &rayTracingFeatures
            );
        }

        void setRayOnDevice() {
            dispatch = std::make_unique<VulkanRayDispatchTable>(rasterEngine->getDevice().getDevice());
            rayDeviceProps = std::make_unique<VulkanRayDeviceProperties>(rasterEngine->getDevice().getDevice());
        }

        void createBLAS(VkCommandBuffer commandBuffer) {
            const auto& resources = rasterEngine->getResources();
            
            // Triangles via vertex buffers. Procedurals via AABBs.
            uint32_t vertexOffset = 0;
            uint32_t indexOffset = 0;
            uint32_t aabbOffset = 0;

            for (const auto& model : resources.getModels()) {
                const auto numOfVertex = static_cast<uint32_t>(model.getNumOfVertices());
                const auto numOfIndex = static_cast<uint32_t>(model.getNumOfIndices());

                VulkanRayBLASGeometry blasGeometries;

                model.getProcedural() ? blasGeometries.addAaBb(
                    resources,
                    aabbOffset,
                    1,
                    true
                ) : blasGeometries.addTriangles(
                    resources,
                    vertexOffset,
                    numOfVertex,
                    indexOffset,
                    numOfIndex,
                    true
                );

                blas.emplace_back(
                    rasterEngine->getDevice(),
                    *dispatch,
                    *rayDeviceProps,
                    blasGeometries
                );

                vertexOffset += numOfVertex * sizeof(VulkanVertex);
                indexOffset += numOfIndex * sizeof(uint32_t);
                aabbOffset += sizeof(VkAabbPositionsKHR);
            }

            // allocate memory

            const auto totalReqs = utils::getTotalRequirements(blas);

            blasBuffer.buffer = std::make_unique<VulkanBuffer>(
                rasterEngine->getDevice().getDevice(),
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                totalReqs.accelerationStructureSize
            );

            blasBuffer.deviceMemory = std::make_unique<VulkanDeviceMemory>(
                blasBuffer.buffer->allocateMemory(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            );

            blasScratchBuffer.buffer = std::make_unique<VulkanBuffer>(
                rasterEngine->getDevice().getDevice(),
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                totalReqs.buildScratchSize
            );

            blasScratchBuffer.deviceMemory = std::make_unique<VulkanDeviceMemory>(
                blasScratchBuffer.buffer->allocateMemory(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            );

            // generate structures
            VkDeviceSize offset = 0;
            VkDeviceSize scratchOffset = 0;

            for (size_t i = 0; i != blas.size(); i++) {
                blas[i].generateBLAS(
                    commandBuffer,
                    *blasScratchBuffer.buffer,
                    scratchOffset,
                    *blasBuffer.buffer,
                    offset
                );

                offset += blas[i].getBuildSizeInfo().accelerationStructureSize;
                scratchOffset += blas[i].getBuildSizeInfo().buildScratchSize;

                std::cout << "BLAS #" << i << " : " << blas[i].getStructure() << std::endl; 
            }
        }

        void createTLAS(VkCommandBuffer commandBuffer) {

        }

        void createAS() {
            VulkanCommandBuffers commandBuffers(
                rasterEngine->getDevice().getDevice(),
                rasterEngine->getCommandPool(),
                1
            );

            VkCommandBuffer commandBuffer = commandBuffers.getCommandBuffers()[0];

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            createBLAS(commandBuffer);
            createTLAS(commandBuffer);

			vkEndCommandBuffer(commandBuffer);

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;

			const auto graphicsQueue = rasterEngine->getDevice().getGraphicsQueue();

			vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr);
			vkQueueWaitIdle(graphicsQueue);
        }

        void clearAS() {

        }

        void createSwapChain() {

        }

        void clearSwapChain() {

        }

        void createOutputImage() {

        }

        void render() {

        }
        

    private:
        std::unique_ptr<VulkanRasterEngine> rasterEngine;

        std::vector<VulkanRayBLAS> blas;
        utils::StructureData blasBuffer;
        utils::StructureData blasScratchBuffer;

        std::vector<VulkanRayTLAS> tlas;
        utils::StructureData tlasBuffer;
        utils::StructureData tlasScratchBuffer;
        utils::StructureData tlasInstancehBuffer;

        std::unique_ptr<VulkanRayDispatchTable> dispatch;
        std::unique_ptr<VulkanRayDeviceProperties> rayDeviceProps;
};