module;
#include "VkBootstrap.h"

export module Instance;
import std;
import Logging;

export namespace Weave {
    auto createInstance(bool useValidationLayers) {
        vkb::InstanceBuilder builder;

        //make the vulkan instance, with basic debug features
        auto maybeInstanceBuilder = builder.set_app_name("Weave")
            .request_validation_layers(useValidationLayers)
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0)
            .build();

        auto vkbInstance = maybeInstanceBuilder;

        if(!vkbInstance) {
            Logging::failure("Failed to create a vulkan instance.");
            abort();
        }

        return vkbInstance.value();
    }
}