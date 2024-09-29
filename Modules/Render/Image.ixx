module;
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

export module Image;

import std;
import Check;

export namespace Weave {
    struct AllocatedImage {
        VkImage image;
        VkImageView imageView;
        VmaAllocation allocation;
        VkExtent3D imageExtent;
        VkFormat imageFormat;

        void destroy(const vkb::Device& vkbDevice, const VmaAllocator allocator) const
        {
            vkDestroyImageView(vkbDevice.device, imageView, nullptr);
            vmaDestroyImage(allocator, image, allocation);
        }
    };

    VkImageSubresourceRange imageSubresourceRange(const VkImageAspectFlags aspectMask)
    {
        return {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        };
    }

    VkImageCreateInfo createImageInfo(const VkFormat format, const VkImageUsageFlags usageFlags, const VkExtent3D extent) {
        return {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
    }

    VkImageViewCreateInfo createImageViewInfo(const VkFormat format, const VkImage image, const VkImageAspectFlags aspectFlags) {
        return {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = {
                .aspectMask = aspectFlags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
    }

    void transitionImage(const VkCommandBuffer cmd, const VkImage image, const VkImageLayout currentLayout, const VkImageLayout newLayout) {
        VkImageAspectFlags aspectMask = 
            (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) 
            ? VK_IMAGE_ASPECT_DEPTH_BIT 
            : VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageMemoryBarrier2 imageBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .oldLayout = currentLayout,
            .newLayout = newLayout,
            .image = image,
            .subresourceRange = imageSubresourceRange(aspectMask),
        };

        VkDependencyInfo depInfo = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imageBarrier,
        };

        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    void copyImageToImage(
        const VkCommandBuffer cmd,
        const VkImage source,
        const VkImage destination,
        const VkExtent3D srcSize,
        const VkExtent3D dstSize) {
        VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

        blitRegion.srcOffsets[1].x = srcSize.width;
        blitRegion.srcOffsets[1].y = srcSize.height;
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = dstSize.width;
        blitRegion.dstOffsets[1].y = dstSize.height;
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;

        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        VkBlitImageInfo2 blitInfo = {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext = nullptr,
            .srcImage = source,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = destination,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blitRegion,
            .filter = VK_FILTER_LINEAR,
        };

        vkCmdBlitImage2(cmd, &blitInfo);
    }

    auto createSwapchainImage(
        const vkb::Device& vkbDevice,
        const vkb::Swapchain& vkbSwapchain,
        const VmaAllocator allocator,
        const VkFormat format,
        const VkImageUsageFlags usageFlags,
        const VkImageAspectFlags aspectFlags) {
        VkExtent3D drawImageExtent = {
            vkbSwapchain.extent.width,
            vkbSwapchain.extent.height,
            1
        };

        AllocatedImage image;
        image.imageFormat = format;
        image.imageExtent = drawImageExtent;

        VkImageCreateInfo imgInfo = createImageInfo(image.imageFormat, usageFlags, drawImageExtent);

        //for the draw image, we want to allocate it from gpu local memory
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        //allocate and create the image
        vmaCreateImage(allocator, &imgInfo, &allocInfo, &image.image, &image.allocation, nullptr);

        //VK_IMAGE_ASPECT_COLOR_BIT
        //build an image-view for the draw image to use for rendering
        VkImageViewCreateInfo viewInfo = createImageViewInfo(image.imageFormat, image.image, aspectFlags);

        Check::vkResult(
            vkCreateImageView(vkbDevice.device, &viewInfo, nullptr, &image.imageView)
        );
        return image;
    }
}