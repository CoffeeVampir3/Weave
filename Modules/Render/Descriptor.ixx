module;
#include "VkBootstrap.h"

export module Descriptor;
import std;
import Logging;
import Check;

namespace Weave {
    export enum class ImageDescriptorType {
        Sampler = VK_DESCRIPTOR_TYPE_SAMPLER,
        SampledImage = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        CombinedImageSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        StorageImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
    };
    export enum class BufferDescriptorType {
        UniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        StorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        UniformBufferDynamic = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        StorageBufferDynamic = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
    };
    export struct DescriptorWriter {
        std::deque<VkDescriptorImageInfo> imageInfos;
        std::deque<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkWriteDescriptorSet> writes;

        // https://vkguide.dev/docs/new_chapter_4/descriptor_abstractions/
        void writeImage(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, ImageDescriptorType type) {
            VkDescriptorImageInfo &info = imageInfos.emplace_back(
                VkDescriptorImageInfo{
                    .sampler = sampler,
                    .imageView = imageView,
                    .imageLayout = layout
                }
            );
            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = VK_NULL_HANDLE,
                .dstBinding = binding,
                .descriptorCount = 1,
                .descriptorType = static_cast<VkDescriptorType>(type),
                .pImageInfo = &info,
            };
            writes.push_back(write);
        }

        void writeBuffer(uint32_t binding, VkBuffer buffer, size_t size, size_t offset, BufferDescriptorType type) {
            VkDescriptorBufferInfo &info = bufferInfos.emplace_back(
                VkDescriptorBufferInfo{
                    .buffer = buffer,
                    .offset = offset,
                    .range = size
                }
            );
            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = VK_NULL_HANDLE,
                .dstBinding = binding,
                .descriptorCount = 1,
                .descriptorType = static_cast<VkDescriptorType>(type),
                .pBufferInfo = &info,
            };
            writes.push_back(write);
        }

        void clear() {
            imageInfos.clear();
            writes.clear();
            bufferInfos.clear();
        }

        void updateSet(VkDevice device, VkDescriptorSet set) {
            for (VkWriteDescriptorSet &write : writes) {
                write.dstSet = set;
            }

            vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
        }
    };

    export struct DescriptorAllocatorGrowable {
        inline static constexpr uint32_t maxPoolSize = 4092u;

        struct PoolSizeRatio {
            VkDescriptorType type;
            float ratio;
        };

        std::vector<PoolSizeRatio> ratios;
        std::vector<VkDescriptorPool> fullPools;
        std::vector<VkDescriptorPool> readyPools;
        uint32_t setsPerPool;

        void init(const vkb::Device& device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) {
            ratios.clear();
            for (auto r : poolRatios) {
                ratios.push_back(r);
            }
            VkDescriptorPool newPool = allocatePool(device, maxSets, poolRatios);
            setsPerPool = maxSets * 1.5;
            readyPools.push_back(newPool);
        }

        void clearPools(const vkb::Device& device) {
            for (auto p : readyPools) {
                vkResetDescriptorPool(device.device, p, 0);
            }
            for (auto p : fullPools) {
                vkResetDescriptorPool(device.device, p, 0);
                readyPools.push_back(p);
            }
            fullPools.clear();
        }

        void destroyPools(const vkb::Device& device) {
            for (auto p : readyPools) {
                vkDestroyDescriptorPool(device.device, p, nullptr);
            }
            readyPools.clear();
            for (auto p : fullPools) {
                vkDestroyDescriptorPool(device.device, p, nullptr);
            }
            fullPools.clear();
        }

        VkDescriptorSet allocate(const vkb::Device& device, VkDescriptorSetLayout layout, void* pNext) {
            VkDescriptorPool poolToUse = getPool(device);
            VkDescriptorSetAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = pNext,
                .descriptorPool = poolToUse,
                .descriptorSetCount = 1,
                .pSetLayouts = &layout
            };
            VkDescriptorSet ds;
            VkResult result = vkAllocateDescriptorSets(device.device, &allocInfo, &ds);
            if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
                fullPools.push_back(poolToUse);
                poolToUse = getPool(device);
                allocInfo.descriptorPool = poolToUse;
                Check::vkResult(
                    vkAllocateDescriptorSets(device.device, &allocInfo, &ds)
                );
            }
            readyPools.push_back(poolToUse);
            return ds;
        }

        private:
        VkDescriptorPool getPool(const vkb::Device& device) {
            VkDescriptorPool newPool;
            if (!readyPools.empty()) {
                newPool = readyPools.back();
                readyPools.pop_back();
            } else {
                newPool = allocatePool(device, setsPerPool, ratios);

                setsPerPool = setsPerPool * 1.5;
                setsPerPool = std::min(setsPerPool, maxPoolSize);
            }

            return newPool;
        }

        VkDescriptorPool allocatePool(const vkb::Device& device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios) {
            std::vector<VkDescriptorPoolSize> poolSizes;
            for (PoolSizeRatio ratio : poolRatios) {
                poolSizes.push_back(VkDescriptorPoolSize{
                    .type = ratio.type,
                    .descriptorCount = uint32_t(ratio.ratio * setCount)});
            }

            VkDescriptorPoolCreateInfo poolInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = 0,
                .maxSets = setCount,
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data()};

            VkDescriptorPool newPool;
            vkCreateDescriptorPool(device.device, &poolInfo, nullptr, &newPool);
            return newPool;
        }
    };

    export struct DescriptorLayoutBuilder {

        std::vector<VkDescriptorSetLayoutBinding> bindings;

        DescriptorLayoutBuilder& addBinding(uint32_t binding, VkDescriptorType type) {
            VkDescriptorSetLayoutBinding newbind {};
            newbind.binding = binding;
            newbind.descriptorCount = 1;
            newbind.descriptorType = type;

            bindings.push_back(newbind);
            return *this;
        }

        void clear() {
            bindings.clear();
        }

        VkDescriptorSetLayout build(
            VkDevice device, 
            VkShaderStageFlags shaderStages, 
            void* pNext = nullptr, 
            VkDescriptorSetLayoutCreateFlags flags = 0
        ) {
            for (auto& b : bindings) {
                b.stageFlags |= shaderStages;
            }

            VkDescriptorSetLayoutCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = pNext,
                .flags = flags,
                .bindingCount = (uint32_t)bindings.size(),
                .pBindings = bindings.data(),
            };

            VkDescriptorSetLayout set;
            Check::vkResult(
                vkCreateDescriptorSetLayout(device, &info, nullptr, &set)
            );

            return set;
        }
    };
}
