module;
#include "VkBootstrap.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "glm/gtx/transform.hpp"
#include "vk_mem_alloc.h"

export module Draw;
import std;
import Logging;
import Image;
import Synchro;
import Command;
import Pipeline;
import Mesh;
import Check;
import Memory;
import Buffer;
import Descriptor;
import Scene;

export namespace Weave {
    struct FrameData {
        FrameSynchronizer synchronizer;

        VkCommandPool commandPool;
        VkCommandBuffer mainCommandBuffer;

        DeletionStack deleteStack;
        DescriptorAllocatorGrowable frameDescriptors;

        void destroy(const vkb::Device& device) {
            deleteStack.free();
            synchronizer.destroy(device);
            frameDescriptors.destroyPools(device);
            vkFreeCommandBuffers(device.device, commandPool, 1, &mainCommandBuffer);
            vkDestroyCommandPool(device.device, commandPool, nullptr);
        }
    };

    VkRenderingAttachmentInfo renderingAttachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout) {
        return {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = view,
            .imageLayout = layout,
            .loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clear ? *clear : VkClearValue{}
        };
    }

    VkRenderingInfo renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment,
        VkRenderingAttachmentInfo* depthAttachment)
    {
        return {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .renderArea = VkRect2D { VkOffset2D { 0, 0 }, renderExtent },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = colorAttachment,
            .pDepthAttachment = depthAttachment,
            .pStencilAttachment = nullptr
        };
    }

    VkRenderingAttachmentInfo depthAttachmentInfo(
        VkImageView view, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/)
    {
        return {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = view,
            .imageLayout = layout,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {
                .depthStencil = {
                    .depth = 0.f
                }
            },
        };
    }

    void drawImgui(vkb::Swapchain swapchain, VkCommandBuffer cmd, VkImageView view) {
        auto colorAttachment = renderingAttachmentInfo(view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto renderInfo = renderingInfo(swapchain.extent, &colorAttachment, nullptr);
        vkCmdBeginRendering(cmd, &renderInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        vkCmdEndRendering(cmd);
    }

    void drawBackground(vkb::Swapchain swapchain, VkCommandBuffer commandBuffer, Weave::ComputeEffect computeEffect, VkDescriptorSet descriptorSet) {
    	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeEffect.pipeline);

        // bind the descriptor set containing the draw image for the compute pipeline
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeEffect.layout, 0, 1, &descriptorSet, 0, nullptr);

        vkCmdPushConstants(commandBuffer, computeEffect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &computeEffect.data);
        // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
        vkCmdDispatch(commandBuffer, std::ceil(swapchain.extent.width / 16.0), std::ceil(swapchain.extent.height / 16.0), 1);
    }

    void drawGeometry(
        vkb::Device device,
        vkb::Swapchain swapchain,
        VmaAllocator allocator,
        FrameData& currentFrame,
        Weave::GPUSceneData& sceneData,
        VkCommandBuffer commandBuffer, 
        VkPipeline trianglePipeline,
        VkPipelineLayout trianglePipelineLayout,
        AllocatedImage& drawImage,
        AllocatedImage& depthImage,
        std::vector<std::shared_ptr<MeshAsset>> testMeshes) {
        //begin a render pass  connected to our draw image
        VkRenderingAttachmentInfo colorAttachment = renderingAttachmentInfo(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo depthAttachment = depthAttachmentInfo(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        
        uint32_t extentWidth = std::min(swapchain.extent.width, drawImage.imageExtent.width);
        uint32_t extentHeight = std::min(swapchain.extent.height, drawImage.imageExtent.height);
        VkExtent2D drawExtent {extentWidth, extentHeight};

        // Weave::DescriptorLayoutBuilder lb;
        // lb.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        // auto gpuSceneDescriptorLayout = lb.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        //
        // Weave::AllocatedBuffer gpuSceneDataBuffer = Weave::createBuffer(
        //     allocator,
        //     sizeof(GPUSceneData),
        //     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        //     VMA_MEMORY_USAGE_CPU_TO_GPU);
        //
        // currentFrame.deleteStack.push([=]()
        // {
        //     gpuSceneDataBuffer.destroy(allocator);
        // });
        // auto sceneUniformData = static_cast<GPUSceneData*>(gpuSceneDataBuffer.allocation->GetMappedData());
        // *sceneUniformData = sceneData;
        // VkDescriptorSet globalDescriptor = currentFrame.frameDescriptors.allocate(device, gpuSceneDescriptorLayout, nullptr);
        // DescriptorWriter writer;
        // writer.writeBuffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, BufferDescriptorType::UniformBuffer);
        // writer.updateSet(device, globalDescriptor);

        VkRenderingInfo renderInfo = renderingInfo(drawExtent, &colorAttachment, &depthAttachment);
        vkCmdBeginRendering(commandBuffer, &renderInfo);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

        //set dynamic viewport and scissor
        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(extentWidth),
            .height = static_cast<float>(extentHeight),
            .minDepth = 0.f,
            .maxDepth = 1.f,
        };

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {
            .offset = {
                .x = 0,
                .y = 0,
            },
            .extent = {
                .width = extentWidth,
                .height = extentHeight,
            },
        };

        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        glm::mat4 view = glm::translate(glm::vec3(0, 0, -5));
        glm::mat4 projection = glm::perspectiveFov(glm::radians(75.f),
            (float)extentWidth,
            (float)extentHeight,
            10000.0f, 0.1f);
        projection[1][1] *= -1;

        Weave::GPUDrawPushConstants pushConstants;
        pushConstants.world = projection*view;
        pushConstants.vertexBufferAddress = testMeshes[2]->meshBuffers.vertexBufferAddress;
        vkCmdPushConstants(commandBuffer, trianglePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Weave::GPUDrawPushConstants), &pushConstants);
        vkCmdBindIndexBuffer(commandBuffer, testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, testMeshes[2]->surfaces[0].count, 1, testMeshes[2]->surfaces[0].startIndex, 0, 0);

        vkCmdEndRendering(commandBuffer);        
    }

    void draw(
        vkb::Device vkbDevice, 
        vkb::Swapchain swapchain,
        VmaAllocator allocator,
        std::vector<VkImageView> const& imageViews,
        VkQueue graphicsQueue,
        VkPipeline trianglePipeline,
        VkPipelineLayout trianglePipelineLayout,
        AllocatedImage& drawImage,
        AllocatedImage& depthImage,
        Weave::FrameData& frameData,
        Weave::GPUSceneData& sceneData,
        std::vector<std::shared_ptr<MeshAsset>> testMeshes,
        bool& resizeWindow
        ) {
        VkDevice device = vkbDevice.device;
        auto commandBuffer = frameData.mainCommandBuffer;
        auto synchronizer = frameData.synchronizer;
        constexpr uint32_t oneSecondInNanoseconds = 1000000000;
        vkWaitForFences(device, 1, &synchronizer.renderFence, true, oneSecondInNanoseconds);

        frameData.deleteStack.free();
        frameData.frameDescriptors.clearPools(vkbDevice);

        vkResetFences(device, 1, &synchronizer.renderFence);

        uint32_t swapchainImageIndex;
        VkResult nextImgResult = vkAcquireNextImageKHR(device, swapchain.swapchain, oneSecondInNanoseconds, synchronizer.swapchainSemaphore, nullptr, &swapchainImageIndex);

        if(nextImgResult == VK_ERROR_OUT_OF_DATE_KHR) {
            resizeWindow = true;
            return;
        }

        vkResetCommandBuffer(commandBuffer, 0);
        auto cmdBeginInfo = Weave::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo);

        auto swapchainImage = swapchain.get_images().value()[swapchainImageIndex];

        Weave::transitionImage(
            commandBuffer, 
            drawImage.image, 
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        Weave::transitionImage(
            commandBuffer, 
            depthImage.image, 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        drawGeometry(
            vkbDevice,
            swapchain,
            allocator,
            frameData,
            sceneData,
            commandBuffer,
            trianglePipeline, 
            trianglePipelineLayout, 
            drawImage, 
            depthImage,
            testMeshes);

        Weave::transitionImage(
            commandBuffer, 
            drawImage.image, 
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        Weave::transitionImage(
            commandBuffer, 
            swapchainImage, 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        Weave::copyImageToImage(
            commandBuffer,
            drawImage.image,
            swapchainImage,
            drawImage.imageExtent,
            VkExtent3D{swapchain.extent.width, swapchain.extent.height, 1}
        );

        Weave::transitionImage(
            commandBuffer, 
            swapchainImage, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        Weave::transitionImage(
            commandBuffer, 
            swapchainImage, 
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        //finalize the command buffer (we can no longer add commands, but it can now be executed)
        Check::vkResult(
            vkEndCommandBuffer(commandBuffer)
        );

        auto cmdInfo = Weave::commandBufferSubmitInfo(commandBuffer);
        auto waitInfo = Weave::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
            synchronizer.swapchainSemaphore);

        auto signalInfo = Weave::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            synchronizer.renderSemaphore);

        VkSubmitInfo2 submit = Weave::queueSubmitInfo(&cmdInfo, &signalInfo, &waitInfo);

        Check::vkResult(
            vkQueueSubmit2(graphicsQueue, 1, &submit, synchronizer.renderFence)
        );

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &synchronizer.renderSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain,
            .pImageIndices = &swapchainImageIndex,
        };
        
        VkResult queuePresentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);

        if(queuePresentResult == VK_ERROR_OUT_OF_DATE_KHR || queuePresentResult == VK_SUBOPTIMAL_KHR) {
            resizeWindow = true;
        }
    }
}