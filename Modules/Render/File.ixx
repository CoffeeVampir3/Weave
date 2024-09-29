module;
#include "VkBootstrap.h"

export module File;
import std;
import Logging;
import Check;

export namespace Weave {
    VkShaderModule loadShaderModule(const std::filesystem::path& path, const vkb::Device& vkbDevice) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            return nullptr;
        }

        // find what the size of the file is by looking up the location of the cursor
        // because the cursor is at the end, it gives the size directly in bytes
        size_t fileSize = (size_t)file.tellg();

        // spirv expects the buffer to be on uint32, so make sure to reserve a int
        // vector big enough for the entire file
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        // put file cursor at beginning
        file.seekg(0);

        // load the entire file into the buffer
        file.read((char*)buffer.data(), fileSize);

        // now that the file is loaded into the buffer, we can close it
        file.close();

        // create a new shader module, using the buffer we loaded
        VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .codeSize = buffer.size() * sizeof(uint32_t),
            .pCode = buffer.data(),
        };

        // check that the creation goes well.
        VkShaderModule shaderModule;
        Check::vkResult(
            vkCreateShaderModule(vkbDevice.device, &createInfo, nullptr, &shaderModule)
        );
        return shaderModule;
    }
}
