module;
#include "VkBootstrap.h"

export module Command;
import std;
import Logging;
import Synchro;
import Check;

export namespace Weave {
    VkCommandBufferBeginInfo commandBufferBeginInfo(const VkCommandBufferUsageFlags flags) {
        return {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = flags,
            .pInheritanceInfo = nullptr
        };
    }

    VkCommandBufferSubmitInfo commandBufferSubmitInfo(const VkCommandBuffer cmd){
        return {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = nullptr,
            .commandBuffer = cmd,
            .deviceMask = 0
        };
    }

    VkCommandBufferAllocateInfo commandBufferAllocateInfo(const VkCommandPool pool) {
        return {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
    }

    VkCommandPoolCreateInfo commandPoolCreateInfo(auto queueFamilyIndex) {
        return {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilyIndex
        };
    }

    VkSubmitInfo2 queueSubmitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo) {
        return {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0u : 1u,
            .pWaitSemaphoreInfos = waitSemaphoreInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = cmd,
            .signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0u : 1u,
            .pSignalSemaphoreInfos = signalSemaphoreInfo
        };
    }

    struct FrameCommandBuffer {
        VkCommandPool pool;
        VkCommandBuffer buffer;
    };

    FrameCommandBuffer createPrimaryCommandBuffer(const vkb::Device vkbDevice, auto graphicsQueueFamily) {
        VkCommandPoolCreateInfo commandPoolInfo = commandPoolCreateInfo(graphicsQueueFamily);
        FrameCommandBuffer cmdBuffer;

        Check::vkResult(
            vkCreateCommandPool(vkbDevice.device, &commandPoolInfo, nullptr, &cmdBuffer.pool)
        );

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = commandBufferAllocateInfo(cmdBuffer.pool);
        Check::vkResult(
            vkAllocateCommandBuffers(vkbDevice.device, &cmdAllocInfo, &cmdBuffer.buffer)
        );

        return cmdBuffer;
    }

    struct ImmediateCommander {
        VkFence fence;
        VkCommandPool pool;
        VkCommandBuffer buffer;

        void allocate(vkb::Device vkbDevice, auto graphicsQueueFamily) {
            VkCommandPoolCreateInfo commandPoolInfo = commandPoolCreateInfo(graphicsQueueFamily);
            Check::vkResult(
                vkCreateCommandPool(vkbDevice.device, &commandPoolInfo, nullptr, &pool)
            );

            VkCommandBufferAllocateInfo cmdAllocInfo = commandBufferAllocateInfo(pool);
            Check::vkResult(
                vkAllocateCommandBuffers(vkbDevice.device, &cmdAllocInfo, &buffer)
            );
            
            auto fenceCreateInfo = Weave::fenceCreateInfo();
            Check::vkResult(
                vkCreateFence(vkbDevice.device, &fenceCreateInfo, nullptr, &fence)
            );
        }

        void destroy(const vkb::Device& vkbDevice) const
        {
            vkDestroyCommandPool(vkbDevice.device, pool, nullptr);
            vkDestroyFence(vkbDevice.device, fence, nullptr);
        }

        void submit(const vkb::Device vkbDevice, auto graphicsQueue, std::function<void(VkCommandBuffer cmd)>&& function) {
            vkResetFences(vkbDevice.device, 1, &fence);
            vkResetCommandBuffer(buffer, 0);

            auto beginInfo = commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            Check::vkResult(
                vkBeginCommandBuffer(buffer, &beginInfo)
            );

            function(buffer);

            vkEndCommandBuffer(buffer);
            auto cmdSubmitInfo = commandBufferSubmitInfo(buffer);
            auto submitInfo = queueSubmitInfo(&cmdSubmitInfo, nullptr, nullptr);

            Check::vkResult(
                vkQueueSubmit2(graphicsQueue, 1, &submitInfo, fence)
            );
            Check::vkResult(
                vkWaitForFences(vkbDevice.device, 1, &fence, true, 9999999999)
            );
        }
    };
}
