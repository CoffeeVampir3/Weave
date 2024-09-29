module;
#include "VkBootstrap.h"
#include "vma.h"

export module Memory;
import std;
import Logging;

export namespace Weave {
    struct DeletionStack {
        void push(std::function<void()>&& function) {
            deletors.push(std::move(function));
        }

        void free() {
            while (!deletors.empty()) {
                deletors.top()();
                deletors.pop();
            }
        }

        private:
        std::stack<std::function<void()>> deletors;
    };

    VmaAllocator createAllocator(vkb::Device vkbDevice, auto instance) {
        VmaAllocator allocator;
        VmaAllocatorCreateInfo allocatorInfo = {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = vkbDevice.physical_device,
            .device = vkbDevice.device,
            .instance = instance,
        };
        vmaCreateAllocator(&allocatorInfo, &allocator);
        return allocator;
    }
}
