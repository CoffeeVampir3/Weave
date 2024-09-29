module;
#include "VkBootstrap.h"

export module Device;
import std;
import Logging;

export namespace Weave {
    auto createDevice(vkb::Instance vkbInstance, auto surface) {
        //vulkan 1.3 features
        VkPhysicalDeviceVulkan13Features features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = true,
            .dynamicRendering = true,
        };
        //vulkan 1.2 features

        VkPhysicalDeviceVulkan12Features features12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .descriptorIndexing = true,
            .bufferDeviceAddress = true,
        };

        vkb::PhysicalDeviceSelector selector{ vkbInstance };
        vkb::PhysicalDevice physicalDevice = selector
            .set_minimum_version(1, 3)
            .set_required_features_13(features)
            .set_required_features_12(features12)
            .set_surface(surface)
            .select()
            .value();

        //CHECK
        vkb::DeviceBuilder deviceBuilder{ physicalDevice };
        auto vkbDevice = deviceBuilder.build();

        if(!vkbDevice) {
            Logging::failure("Failed to acquire valid devices.");
            abort();
        }

        return vkbDevice.value();
    }
}
