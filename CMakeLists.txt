# This requires a recent nightly build.
# This will be part of CMake 3.30.0.
cmake_minimum_required(VERSION 3.29.0 FATAL_ERROR)

# Enables the Standard module support. This needs to be done
# before selecting the languages.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
set(CMAKE_CXX_MODULE_STD ON)

project("VulkanApp" LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED NO_MODULE)

# Set language version used
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS ON)

# Create the executable target
add_executable(VulkanApp main.cpp)

add_subdirectory(vk-bootstrap)
add_subdirectory(fastgltf)
# Set fastgltf options
set(FASTGLTF_USE_STD_MODULE ON CACHE BOOL "" FORCE)
set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "" FORCE)

# Compile definitions for GLM, note the ZERO_TO_ONE here for depth, which swaps the depth direction.
add_compile_definitions(
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

file(GLOB_RECURSE MODULE_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/3P/*.cc"
)

file(GLOB THIRDPARTY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/3P/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/3P/*.c"
)

# Add modules using FILE_SET
target_sources(VulkanApp
    PUBLIC
    FILE_SET CXX_MODULES TYPE CXX_MODULES 
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${MODULE_FILES}
)

target_sources(VulkanApp
    PRIVATE
    ${THIRDPARTY_SOURCES}
)

# Add the Modules directory to include directories
target_include_directories(VulkanApp
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Modules
    ${CMAKE_CURRENT_SOURCE_DIR}/3P
)

target_link_libraries(VulkanApp
    PRIVATE
    dl
    pthread
    X11
    Xxf86vm
    Xrandr
    Xi
    SDL2::SDL2
    vk-bootstrap::vk-bootstrap
    vulkan
    fastgltf::fastgltf
)

# Add compiler options to each compilation unit.
get_property(TARGETS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
foreach(TARGET ${TARGETS})
    target_compile_options(${TARGET} PRIVATE -fno-exceptions)
endforeach()