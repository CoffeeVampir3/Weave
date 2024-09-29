module;
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

export module Buffer;
import std;
import Logging;
import Check;

export namespace Weave {
    struct AllocatedBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo info;

        void destroy(VmaAllocator allocator) const
        {
            vmaDestroyBuffer(allocator, buffer, allocation);
        }
    };

    AllocatedBuffer createBuffer(VmaAllocator allocator, const size_t allocSize, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage) {
        const VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = allocSize,
            .usage = usage,
        };

        const VmaAllocationCreateInfo allocInfo = {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = memoryUsage,
        };

        AllocatedBuffer newBuffer;

        Check::vkResult(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

        return newBuffer;
    }
}
