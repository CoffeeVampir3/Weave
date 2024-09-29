#include <thread>
#include <SDL2/SDL.h>
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

import std;

import Logging;
import Image;
import Instance;
import Surface;
import Swapchain;
import Synchro;
import Memory;
import Command;
import Device;
import Descriptor;
import Pipeline;
import Gui;
import Mesh;
import Gltf;
import Draw;
import Scene;

int main() {
    Weave::DeletionStack pendingDelete;
    #define DEFER(func) pendingDelete.push(std::move([&]() { func; }))

    constexpr uint32_t width = 1024;
    constexpr uint32_t height = 1024;

    auto window = Weave::createWindow(width, height);
    DEFER(
        SDL_DestroyWindow(window);
    );

    auto vkbInstance = Weave::createInstance(true);
    DEFER(
        vkb::destroy_instance(vkbInstance);
    );

    auto surface = Weave::createSDLSurface(window, vkbInstance);
    DEFER( 
        vkDestroySurfaceKHR(vkbInstance.instance, surface, nullptr);
    );

    auto vkbDevice = Weave::createDevice(vkbInstance, surface);
    DEFER(
        vkDestroyDevice(vkbDevice.device, nullptr);
    );

    auto allocator = Weave::createAllocator(vkbDevice, vkbInstance.instance);
    DEFER(
        vmaDestroyAllocator(allocator);
    );

    auto vkbSwapchain = Weave::createSwapchain(width, height, vkbDevice, surface);
    auto swapchainImageViews = vkbSwapchain.get_image_views().value();
    DEFER(
        vkbSwapchain.destroy_image_views(swapchainImageViews);
        vkb::destroy_swapchain(vkbSwapchain);
    );

    auto graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    auto graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    constexpr uint32_t numberFrameBuffers = 2;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    auto drawImg = Weave::createSwapchainImage(
        vkbDevice, 
        vkbSwapchain, 
        allocator, 
        VK_FORMAT_R16G16B16A16_SFLOAT,
        drawImageUsages,
        VK_IMAGE_ASPECT_COLOR_BIT);
    DEFER(
        drawImg.destroy(vkbDevice, allocator);
    );
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    auto depthImg = Weave::createSwapchainImage(
        vkbDevice, 
        vkbSwapchain, 
        allocator, 
        VK_FORMAT_D32_SFLOAT,
        depthImageUsages,
        VK_IMAGE_ASPECT_DEPTH_BIT);
    DEFER(
        depthImg.destroy(vkbDevice, allocator);
    );

    std::array<Weave::FrameData, numberFrameBuffers> frames;
    for (auto& frame : frames) {
        std::vector<Weave::DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
        };
        auto [pool, buffer] = Weave::createPrimaryCommandBuffer(vkbDevice, graphicsQueueFamily);;
        frame.synchronizer = Weave::createFrameSynchronizer(vkbDevice);
        frame.mainCommandBuffer = buffer;
        frame.commandPool = pool;
        frame.frameDescriptors = Weave::DescriptorAllocatorGrowable{};
        frame.frameDescriptors.init(vkbDevice, 1000, frameSizes);
    }
    DEFER(
        for (auto frame : frames) {
            frame.destroy(vkbDevice);
        }
    );

    auto immediateCommander = Weave::ImmediateCommander{};
    immediateCommander.allocate(vkbDevice, graphicsQueueFamily);
    DEFER(
        immediateCommander.destroy(vkbDevice);
    );

    auto imguiPool = Weave::createImgui(vkbDevice, vkbSwapchain, vkbInstance, window, graphicsQueue);
    DEFER(
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(vkbDevice.device, imguiPool, nullptr);
    );

    int currentBackgroundEffect{0};

    auto [trianglePipelineLayout, trianglePipeline] = Weave::createTriangleMeshPipeline(vkbDevice, drawImg, depthImg);
    DEFER(
        vkDestroyPipeline(vkbDevice.device, trianglePipeline, nullptr);
        vkDestroyPipelineLayout(vkbDevice.device, trianglePipelineLayout, nullptr);
    );

    auto testMeshes = Weave::loadGltfMeshes(
        vkbDevice,
        graphicsQueue,
        allocator,
        immediateCommander,
        "Assets/basicmesh.glb").value();
    DEFER(
        for(auto& mesh : testMeshes) {
            mesh->meshBuffers.destroy(allocator);
        }
    );

    vkDeviceWaitIdle(vkbDevice.device);

    Weave::GPUSceneData sceneData;

    SDL_Event e;
    bool bQuit = false;
    bool stopRendering = false;
    bool resizeWindow = false;

    uint32_t currentRenderFrame = 0;
    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stopRendering = false;
                }
            }
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        // do not draw if we are minimized
        if (stopRendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Weave::draw(
            vkbDevice,
            vkbSwapchain,
            allocator,
            swapchainImageViews,
            graphicsQueue,
            trianglePipeline,
            trianglePipelineLayout,
            drawImg,
            depthImg,
            frames[currentRenderFrame],
            sceneData,
            testMeshes,
            resizeWindow
        );

        if(resizeWindow) {
            Weave::resizeSwapchain(vkbDevice, vkbSwapchain, swapchainImageViews, surface, window);
            resizeWindow = false;
        }

        currentRenderFrame = (currentRenderFrame + 1) % numberFrameBuffers;
    }

    vkDeviceWaitIdle(vkbDevice.device);
    pendingDelete.free();
    return -1;
}