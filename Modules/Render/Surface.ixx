module;
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

export module Surface;
import std;
import Logging;
import Image;

export namespace Weave {
    auto createWindow(const uint32_t width, const uint32_t height) {
        SDL_Init(SDL_INIT_VIDEO);

        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        auto window = SDL_CreateWindow(
            "Vulkan Engine",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            width,
            height,
            window_flags);

        if(!window) {
            Logging::failure("Failed to create an SDL window.");
            abort();
        }
        return window;
    }

    VkSurfaceKHR createSDLSurface(auto window, vkb::Instance vkbInstance) {
        VkSurfaceKHR surface;
        SDL_Vulkan_CreateSurface(window, vkbInstance.instance, &surface);
        if(!surface) {
            Logging::failure("Failed to acquire an SDL surface.");
            abort();
        }
        return surface;
    }
}