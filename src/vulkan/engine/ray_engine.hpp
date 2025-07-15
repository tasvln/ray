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
#include "vulkan/ray/sbt.hpp"

#include "vulkan/utils/ray_engine.hpp"
#include "vulkan/utils/buffer.hpp"
#include "vulkan/utils/sbt.hpp"

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
            std::cout << "Initializing -> VulkanRayEngine" << std::endl;
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

            blasBuffer.memory = std::make_unique<VulkanDeviceMemory>(
                blasBuffer.buffer->allocateMemory(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            );

            blasScratchBuffer.buffer = std::make_unique<VulkanBuffer>(
                rasterEngine->getDevice().getDevice(),
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                totalReqs.buildScratchSize
            );

            blasScratchBuffer.memory = std::make_unique<VulkanDeviceMemory>(
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
            const auto& resources = rasterEngine->getResources();

	        std::vector<VkAccelerationStructureInstanceKHR> instances;

            // Hit group 0 = triangles; Hit group 1 = procedurals
            uint32_t instanceId = 0;

            for (const auto& model : resources.getModels()) {
                instances.push_back(
                    createTLASInstance(
                        blas[instanceId],
                        glm::mat4(1),
                        instanceId,
                        model.getProcedural() ? 1 : 0
                    )
                );

                instanceId++;
            }

            tlasInstanceBuffer = utils::createDeviceBuffer(
                rasterEngine->getDevice(),
                rasterEngine->getCommandPool(),
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                instances
            );

            memoryBarrier(commandBuffer);

            tlas.emplace_back(
                rasterEngine->getDevice(),
                *dispatch,
                *rayDeviceProps,
                tlasInstanceBuffer.buffer->getDeviceAddress(),
                static_cast<uint32_t>(instances.size())
            );

            const auto totalReqs = utils::getTotalRequirements(tlas);

            tlasBuffer.buffer= std::make_unique<VulkanBuffer>(
                rasterEngine->getDevice().getDevice(),
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                totalReqs.accelerationStructureSize
            );

            tlasBuffer.memory = std::make_unique<VulkanDeviceMemory>(
                tlasBuffer.buffer->allocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            );

            tlasScratchBuffer.buffer = std::make_unique<VulkanBuffer>(
                rasterEngine->getDevice().getDevice(),
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                totalReqs.buildScratchSize
            );

            tlasScratchBuffer.memory = std::make_unique<VulkanDeviceMemory>(
                tlasScratchBuffer.buffer->allocateMemory(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            );

            tlas[0].generateTLAS(
                commandBuffer,
                *tlasScratchBuffer.buffer,
                0,
                *tlasBuffer.buffer,
                0
            );
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

            // clean up scratch
            tlasScratchBuffer.clear();
            blasScratchBuffer.clear();
        }

        void clearAS() {
            // tlas
            tlas.clear();
            tlasBuffer.clear();
            tlasScratchBuffer.clear();
            tlasInstanceBuffer.clear();

            // blas
            blas.clear();
            blasBuffer.clear();
            blasScratchBuffer.clear();
        }

        void createSwapChain() {
            rasterEngine->createSwapChain();

            createOutputImage();

            pipeline = std::make_unique<VulkanRayPipeline>(
                rasterEngine->getDevice(),
                rasterEngine->getSwapChain(),
                rasterEngine->getUniformBuffers(),
                rasterEngine->getResources(),
                rasterEngine->getDepthBuffer(),
                tlas[0],
                *accumulation.imageView,
                *output.imageView,
                *dispatch
            );

            // shader group index, inline data that gets appended after the shader group handle the SBT
            const std::vector<utils::ShaderRecord> rayGenRecords = {
                {
                    pipeline->getGenShaderIndex(),
                    {}
                }
            };

            const std::vector<utils::ShaderRecord> rayMissRecords = {
                {
                    pipeline->getMissShaderIndex(),
                    {}
                }
            };

            const std::vector<utils::ShaderRecord> rayHitRecords = {
                {
                    pipeline->getTriangleHitGroupIndex(),
                    {}
                },
                {
                    pipeline->getProceduralHitGroupIndex(),
                    {}
                }
            };

            sbt = std::make_unique<VulkanRaySBT>(
                rasterEngine->getDevice(),
                *dispatch,
                *pipeline,
                *rayDeviceProps,
                rayGenRecords,
                rayMissRecords,
                rayHitRecords
            );
        }

        void createOutputImage() {
            const auto tiling = VK_IMAGE_TILING_OPTIMAL;
            const auto extent = rasterEngine->getSwapChain().getSwapChainExtent();
            const auto format = rasterEngine->getSwapChain().getSwapChainFormat();

            accumulation.image = std::make_unique<VulkanImage>(
                rasterEngine->getDevice(),
                extent,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                tiling, 
                VK_IMAGE_USAGE_STORAGE_BIT
            );

            accumulation.memory = std::make_unique<VulkanDeviceMemory>(
                accumulation.image->allocateMemory(
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                )
            );

            accumulation.imageView = std::make_unique<VulkanImageView>(
                rasterEngine->getDevice(),
                accumulation.image->getImage(),
                VK_FORMAT_R32G32B32A32_SFLOAT, 
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            output.image = std::make_unique<VulkanImage>(
                rasterEngine->getDevice(),
                extent,
                format,
                tiling, 
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            );

            output.memory = std::make_unique<VulkanDeviceMemory>(
                output.image->allocateMemory(
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                )
            );

            output.imageView = std::make_unique<VulkanImageView>(
                rasterEngine->getDevice(),
                output.image->getImage(),
                format, 
                VK_IMAGE_ASPECT_COLOR_BIT
            );
        }

        void clearSwapChain() {
            sbt.reset();
            pipeline.reset();
            output.clear();
            accumulation.clear();

            rasterEngine->clearSwapChain();
        }

        void render(VkCommandBuffer commandBuffer, const size_t currentFrame, const uint32_t imageIndex) {
            const auto extent = rasterEngine->getSwapChain().getSwapChainExtent();

            VkDescriptorSet descriptorSets[] = {
                pipeline->getDescriptorSet(currentFrame)
            };

            VkImageSubresourceRange subresourceRange {};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = 1;
            subresourceRange.baseArrayLayer = 0;
            subresourceRange.layerCount = 1;

            addImageMemoryBarrier(
                commandBuffer,
                accumulation.image->getImage(),
                subresourceRange,
                0,
                VK_ACCESS_SHADER_WRITE_BIT, 
                VK_IMAGE_LAYOUT_UNDEFINED, 
                VK_IMAGE_LAYOUT_GENERAL
            );

            addImageMemoryBarrier(
                commandBuffer,
                output.image->getImage(),
                subresourceRange,
                0,
                VK_ACCESS_SHADER_WRITE_BIT, 
                VK_IMAGE_LAYOUT_UNDEFINED, 
                VK_IMAGE_LAYOUT_GENERAL
            );

            // binding the pipeline
            vkCmdBindPipeline(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                pipeline->getPipeline()
            );

            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                pipeline->getPipelineLayout().getPipelineLayout(),
                0,
                1,
                descriptorSets,
                0,
                nullptr
            );

            VkStridedDeviceAddressRegionKHR rayGenSBT {};
            rayGenSBT.deviceAddress = sbt->getRayGenDeviceRegion().deviceAddress;
            rayGenSBT.stride = sbt->getRayGenDeviceRegion().stride;
            rayGenSBT.size = sbt->getRayGenDeviceRegion().size;

            VkStridedDeviceAddressRegionKHR rayMissSBT {};
            rayMissSBT.deviceAddress = sbt->getRayMissDeviceRegion().deviceAddress;
            rayMissSBT.stride = sbt->getRayMissDeviceRegion().stride;
            rayMissSBT.size = sbt->getRayMissDeviceRegion().size;

            VkStridedDeviceAddressRegionKHR rayHitSBT {};
            rayHitSBT.deviceAddress = sbt->getRayHitDeviceRegion().deviceAddress;
            rayHitSBT.stride = sbt->getRayHitDeviceRegion().stride;
            rayHitSBT.size = sbt->getRayHitDeviceRegion().size;

            VkStridedDeviceAddressRegionKHR callableSBT = {};

            dispatch->vkCmdTraceRaysKHR(
                commandBuffer,
                &rayGenSBT,
                &rayMissSBT,
                &rayHitSBT,
                &callableSBT,
                extent.width,
                extent.height,
                1
            );

            addImageMemoryBarrier(
                commandBuffer,
                output.image->getImage(),
                subresourceRange,
                VK_ACCESS_SHADER_WRITE_BIT, 
                VK_ACCESS_TRANSFER_READ_BIT, 
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            );

            addImageMemoryBarrier(
                commandBuffer,
                rasterEngine->getSwapChain().getSwapChainImages()[imageIndex],
                subresourceRange,
                VK_ACCESS_SHADER_WRITE_BIT, 
                VK_ACCESS_TRANSFER_READ_BIT, 
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            );

            // image -> swapchain image
            VkImageCopy copyRegion {};
            copyRegion.srcSubresource = {
                VK_IMAGE_ASPECT_COLOR_BIT, 
                0, 
                0, 
                1 
            };
            copyRegion.srcOffset = { 
                0, 
                0, 
                0 
            };

            copyRegion.dstSubresource = { 
                VK_IMAGE_ASPECT_COLOR_BIT, 
                0, 
                0, 
                1 
            };
            copyRegion.dstOffset = { 
                0, 
                0, 
                0 
            };

            copyRegion.extent = { 
                extent.width, 
                extent.height, 
                1 
            };

            vkCmdCopyImage(
                commandBuffer,
                output.image->getImage(),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                rasterEngine->getSwapChain().getSwapChainImages()[imageIndex],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion
            );

            addImageMemoryBarrier(
                commandBuffer,
                rasterEngine->getSwapChain().getSwapChainImages()[imageIndex],
                subresourceRange,
                VK_ACCESS_TRANSFER_WRITE_BIT, 
                0, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            );
        }
        

    private:
        std::unique_ptr<VulkanRasterEngine> rasterEngine;

        std::unique_ptr<VulkanRayPipeline> pipeline;

        std::vector<VulkanRayBLAS> blas;

        utils::BufferResource blasBuffer;
        utils::BufferResource blasScratchBuffer;

        std::vector<VulkanRayTLAS> tlas;

        utils::BufferResource tlasBuffer;
        utils::BufferResource tlasScratchBuffer;
        utils::BufferResource tlasInstanceBuffer;

        std::unique_ptr<VulkanRayDispatchTable> dispatch;
        std::unique_ptr<VulkanRayDeviceProperties> rayDeviceProps;

        utils::ImageData accumulation;
        utils::ImageData output;

        std::unique_ptr<VulkanRaySBT> sbt;

        VkAccelerationStructureInstanceKHR createTLASInstance(
            const VulkanRayBLAS& blas,
            const glm::mat4& transform,
            const uint32_t instanceId,
            const uint32_t hitGroupId
        ) {
            VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            addressInfo.accelerationStructure = blas.getStructure();

            const VkDeviceAddress addr = blas.getDispatchTable().vkGetAccelerationStructureDeviceAddressKHR(
                rasterEngine->getDevice().getDevice(), 
                &addressInfo
            );
        
            VkAccelerationStructureInstanceKHR instance{};
            instance.instanceCustomIndex = instanceId;
            instance.mask = 0xFF;
            // Set the hit group index, that will be used to find the shader code to execute when hitting the geometry.
            instance.instanceShaderBindingTableRecordOffset = hitGroupId;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance.accelerationStructureReference = addr;

            if (sizeof(instance.transform) == sizeof(float) * 12) {
                std::cout << "transform size mismatch" << std::endl;
            }

            std::memcpy(&instance.transform, &transform, sizeof(instance.transform));

            return instance;
        }

        void memoryBarrier(VkCommandBuffer commandBuffer) {
            // hmm -> Wait for the builder to complete by setting a barrier on the resulting buffer
            // aparently ->  important as the construction of the top-level hierarchy may be called right afterwards, before executing the command list

            VkMemoryBarrier memoryBarrier = {};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.pNext = nullptr;
            memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                0, 
                1, 
                &memoryBarrier, 
                0, 
                nullptr, 
                0, 
                nullptr
            );
        }

        // this is basically the transition layout function
        static void addImageMemoryBarrier(
			const VkCommandBuffer commandBuffer, 
			const VkImage image, 
			const VkImageSubresourceRange subresourceRange, 
			const VkAccessFlags srcAccessMask,
			const VkAccessFlags dstAccessMask, 
			const VkImageLayout oldLayout, 
			const VkImageLayout newLayout)
		{
			VkImageMemoryBarrier barrier;
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.pNext = nullptr;
			barrier.srcAccessMask = srcAccessMask;
			barrier.dstAccessMask = dstAccessMask;
			barrier.oldLayout = oldLayout;
			barrier.newLayout = newLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange = subresourceRange;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
				&barrier);
		}
};