module;
#include "VkBootstrap.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

export module Gui;
import std;
import Logging;
import Check;

export namespace Weave {
    auto createImgui(vkb::Device vkbDevice, vkb::Swapchain swapchain, vkb::Instance instance, auto window, auto graphicsQueue) {
        VkDescriptorPoolSize poolSizes[] { 
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } 
        };

        VkDescriptorPoolCreateInfo poolInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 1000,
            .poolSizeCount = (uint32_t)std::size(poolSizes),
            .pPoolSizes = poolSizes
        };

        VkDescriptorPool imguiPool;
        Check::vkResult(
            vkCreateDescriptorPool(vkbDevice.device, &poolInfo, nullptr, &imguiPool)
        );

        ImGui::CreateContext();
        ImGui_ImplSDL2_InitForVulkan(window);

        ImGui_ImplVulkan_InitInfo initInfo{
            .Instance = instance.instance,
            .PhysicalDevice = vkbDevice.physical_device,
            .Device = vkbDevice.device,
            .Queue = graphicsQueue,
            .DescriptorPool = imguiPool,
            .MinImageCount = 3,
            .ImageCount = 3,
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
            .UseDynamicRendering = VK_TRUE
        };

        initInfo.PipelineRenderingCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapchain.image_format
        };

        ImGui_ImplVulkan_Init(&initInfo);

        ImGui_ImplVulkan_CreateFontsTexture();

        return imguiPool;
    }
}
