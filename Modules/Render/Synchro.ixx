module;
#include "VkBootstrap.h"

export module Synchro;
import std;
import Logging;
import Check;

export namespace Weave {
    struct FrameSynchronizer {
        VkSemaphore swapchainSemaphore, renderSemaphore;
        VkFence renderFence;

        void destroy(const vkb::Device& vkbDevice) const
        {
            vkDestroyFence(vkbDevice.device, renderFence, nullptr);
            vkDestroySemaphore(vkbDevice.device, renderSemaphore, nullptr);
            vkDestroySemaphore(vkbDevice.device, swapchainSemaphore, nullptr);
        }
    };

    VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) {
        return {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = semaphore,
            .value = 1,
            .stageMask = stageMask,
            .deviceIndex = 0,
        };
    }

    VkFenceCreateInfo fenceCreateInfo() {
        return {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
    }

    FrameSynchronizer createFrameSynchronizer(const vkb::Device& vkbDevice) {
        VkFenceCreateInfo fenceInfo = fenceCreateInfo();

        VkSemaphoreCreateInfo semaphoreInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
        };
        FrameSynchronizer synchronizer;
        Check::vkResult(
            vkCreateFence(vkbDevice.device, &fenceInfo, nullptr, &synchronizer.renderFence)
        );
        Check::vkResult(
            vkCreateSemaphore(vkbDevice.device, &semaphoreInfo, nullptr, &synchronizer.swapchainSemaphore)
        );
        Check::vkResult(
            vkCreateSemaphore(vkbDevice.device, &semaphoreInfo, nullptr, &synchronizer.renderSemaphore)
        );
        return synchronizer;
    }
}
