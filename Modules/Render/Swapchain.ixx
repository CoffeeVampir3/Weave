module;
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

export module Swapchain;
import std;
import Logging;
import Check;

export namespace Weave {
    auto createSwapchain(uint32_t width, uint32_t height, vkb::Device vkbDevice, VkSurfaceKHR surface) {
        vkb::SwapchainBuilder swapchainBuilder{ 
            vkbDevice,
            surface 
        };
        auto vkbSwapchain = swapchainBuilder
            .use_default_format_selection()
            //use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build();

        if(!vkbSwapchain) {
            Logging::failure("Failed to create a valid swapchain.");
            abort();
        }

        return vkbSwapchain.value();
    }

    void resizeSwapchain(vkb::Device vkbDevice, vkb::Swapchain& swapchain, std::vector<VkImageView>& imageViews, VkSurfaceKHR surface, auto window) {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        swapchain.destroy_image_views(imageViews);
        vkb::destroy_swapchain(swapchain);
        swapchain = createSwapchain(w, h, vkbDevice, surface);
        imageViews = swapchain.get_image_views().value();
    }
}
