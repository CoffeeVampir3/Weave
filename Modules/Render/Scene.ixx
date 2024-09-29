module;
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include "VkBootstrap.h"
#include "glm/glm.hpp"

export module Scene;
import std;
import Logging;

export namespace Weave {
    struct GPUSceneData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewproj;
        glm::vec4 ambientColor;
        glm::vec4 sunlightDirection; // w for sun power
        glm::vec4 sunlightColor;
    };
}
