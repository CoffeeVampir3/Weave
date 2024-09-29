module;
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"
#include "glm/glm.hpp"

export module Mesh;
import std;
import Logging;
import Buffer;
import Command;
import Memory;

export namespace Weave {
    struct Vertex {
        glm::vec3 position;
        float uv_x;
        glm::vec3 normal;
        float uv_y;
        glm::vec4 color;
    };

    struct GPUMeshBuffer {
        AllocatedBuffer indexBuffer;
        AllocatedBuffer vertexBuffer;
        VkDeviceAddress vertexBufferAddress;

        void destroy(VmaAllocator allocator) {
            indexBuffer.destroy(allocator);
            vertexBuffer.destroy(allocator);
        }
    };

    struct GPUDrawPushConstants {
        glm::mat4 world;
        VkDeviceAddress vertexBufferAddress;
    };

    struct GeoSurface {
        uint32_t startIndex;
        uint32_t count;
    };

    struct MeshAsset {
        std::string name;

        std::vector<GeoSurface> surfaces;
        GPUMeshBuffer meshBuffers;
    };

    GPUMeshBuffer createMeshBuffer(
        vkb::Device device, 
        VkQueue graphicsQueue, 
        VmaAllocator allocator, 
        ImmediateCommander immediateCmd, 
        std::span<uint32_t> indices, 
        std::span<Vertex> vertices) {
        const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
        const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

        GPUMeshBuffer meshSurface;
        meshSurface.vertexBuffer = Weave::createBuffer(
            allocator,
            vertexBufferSize, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        VkBufferDeviceAddressInfo deviceAdressInfo{ 
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = meshSurface.vertexBuffer.buffer 
        };

        meshSurface.vertexBufferAddress = vkGetBufferDeviceAddress(device.device, &deviceAdressInfo);
        meshSurface.indexBuffer = Weave::createBuffer(
            allocator,
            indexBufferSize, 
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | 
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        AllocatedBuffer staging = Weave::createBuffer(
            allocator,
            vertexBufferSize + indexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY);
        
        void* data = staging.allocation->GetMappedData();
        memcpy(data, vertices.data(), vertexBufferSize);
        memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

        immediateCmd.submit(device, graphicsQueue, [&](VkCommandBuffer cmd) {
            VkBufferCopy vertexCopy{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertexBufferSize
            };
            vkCmdCopyBuffer(cmd, staging.buffer, meshSurface.vertexBuffer.buffer, 1, &vertexCopy);
            VkBufferCopy indexCopy{
                .srcOffset = vertexBufferSize,
                .dstOffset = 0,
                .size = indexBufferSize
            };
            vkCmdCopyBuffer(cmd, staging.buffer, meshSurface.indexBuffer.buffer, 1, &indexCopy);
        });

        staging.destroy(allocator);

        return meshSurface;
    }
}
