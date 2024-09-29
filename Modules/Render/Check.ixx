module;
#include <vulkan/vk_enum_string_helper.h>

export module Check;
import std;
import Logging;

export namespace Check {
    constexpr bool checkingDisabled = false;
    template<typename T>
    inline void optional(std::optional<T> opt, const std::source_location& location = std::source_location::current()) {
        if constexpr (checkingDisabled) {
            return;
        }
        if (!opt) {
            Logging::failure("Optional was nullopt", location);
            std::abort();
        }
    }
    inline void vkResult(VkResult result, const std::source_location& location = std::source_location::current()) {
        if constexpr (checkingDisabled) {
            return;
        }
        if (result != VK_SUCCESS) {
            Logging::failure("VKResult was not success", location);
            std::abort();
        }
    }
    ///Assigns the first parameter to the second if it's valid, otherwise fails the program with a logged error.
    template<typename T, typename VkType>
    inline void vkAssignCheckedPtr(T& assignTo, VkType&& result, const std::source_location& location = std::source_location::current()) {
        if constexpr (checkingDisabled) {
            assignTo = static_cast<T>(result);
            return;
        }
        if (result == VK_NULL_HANDLE) {
            Logging::failure("Assignment was null handle", location);
            std::abort();
        }
        assignTo = static_cast<T>(result);
    }
}