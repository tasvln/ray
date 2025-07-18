#pragma once

#include "sampler.hpp"

#include <stb_image.h>
#include <chrono>
#include <memory>
#include <string>

class VulkanTexture {
    public:
        VulkanTexture(
            const std::string& filename
        ) :
            pixels(nullptr, stbi_image_free) 
        {
            const auto timer = std::chrono::high_resolution_clock::now();

            unsigned char* rawPixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            channels = 4; // Because of STBI_rgb_alpha

            if (!rawPixels) {
                throw std::runtime_error("failed to load texture image: " + filename);
            }

            pixels.reset(rawPixels);

            const auto elapsed = std::chrono::duration<float>(
                std::chrono::high_resolution_clock::now() - timer).count();
        }

        // VulkanTexture(
        //     int width, 
        //     int height, 
        //     int channels, 
        //     VulkanSampler samplerConfig,
        //     unsigned char* pixels
        // ) : width(width), 
        //     height(height), 
        //     channels(channels), 
        //     samplerConfig(samplerConfig), 
        //     pixels(pixels, stbi_image_free) 
        // {}

        // ~VulkanTexture() = default;

        // VulkanTexture loadTexture(const std::string& filename, const VulkanSampler& samplerConfig) {
        //     const auto timer = std::chrono::high_resolution_clock::now();

        //     int width, height, channels;

        //     unsigned char* pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        //     if (!pixels) {
        //         std::runtime_error("failed to load texture image '" + filename + "'");
        //     }

        //     const auto elapsed = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - timer).count();

        //     return VulkanTexture(width, height, channels, samplerConfig, pixels);
        // }

        int getWidth() const {
            return width;
        }

        int getHeight() const {
            return height;
        }

        int getChannels() const { 
            return channels; 
        }

        const unsigned char* getPixels() const {
            return pixels.get();
        }

    private:
        int width;
        int height;
        int channels;

        // for pixels
        std::unique_ptr<unsigned char, void(*) (void*)> pixels;
};