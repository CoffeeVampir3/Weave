module;
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"
#include "glm/glm.hpp"

export module Pipeline;
import std;
import Logging;
import File;
import Image;
import Mesh;
import Check;

export namespace Weave {
    struct ComputePushConstants {
        glm::vec4 data1;
        glm::vec4 data2;
        glm::vec4 data3;
        glm::vec4 data4;
    };

    struct ComputeEffect {
        std::string name;

        VkPipeline pipeline;
        VkPipelineLayout layout;

        ComputePushConstants data;
    };

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo(
        VkShaderStageFlagBits stage, 
        VkShaderModule module) {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = stage,
            .module = module,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        };
    }

    auto createBackgroundPipeline(vkb::Device vkbDevice, VkDescriptorSetLayout drawDescriptorLayout) {
        VkPipelineLayout computePipelineLayout;

        VkPushConstantRange pushConstant {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(ComputePushConstants),
        };

        VkPipelineLayoutCreateInfo computeLayoutInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 1,
            .pSetLayouts = &drawDescriptorLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstant,
        };

        Check::vkResult(
            vkCreatePipelineLayout(vkbDevice.device, &computeLayoutInfo, nullptr, &computePipelineLayout)
        );
        VkShaderModule gradientShader = loadShaderModule("Shaders/gradient.comp.spv", vkbDevice);
        VkShaderModule skyShader = loadShaderModule("Shaders/sky.comp.spv", vkbDevice);

        VkPipelineShaderStageCreateInfo stageinfo = shaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, gradientShader);

        VkComputePipelineCreateInfo computePipelineCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = stageinfo,
            .layout = computePipelineLayout,
        };

        std::vector<ComputeEffect> effects {
            {
                .name = "Gradient",
                .layout = computePipelineLayout,
                .data {
                    .data1 = glm::vec4(1, 0, 0, 1),
                    .data2 = glm::vec4(0, 0, 1, 1)
                }
            },
            {
                .name = "Sky",
                .layout = computePipelineLayout,
                .data {
                    .data1 = glm::vec4(0.1, 0.2, 0.4 ,0.97),
                }
            }
        };

        Check::vkResult(
            vkCreateComputePipelines(vkbDevice.device,nullptr,1,&computePipelineCreateInfo, nullptr, &effects[0].pipeline)
        );
        computePipelineCreateInfo.stage.module = skyShader;
        Check::vkResult(
            vkCreateComputePipelines(vkbDevice.device,nullptr,1,&computePipelineCreateInfo, nullptr, &effects[1].pipeline)
        );

        vkDestroyShaderModule(vkbDevice.device, gradientShader, nullptr);
        vkDestroyShaderModule(vkbDevice.device, skyShader, nullptr);
        return std::tuple(computePipelineLayout, effects);
    }

    struct PipelineBuilder {
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        VkPipelineInputAssemblyStateCreateInfo inputAssembly;
        VkPipelineRasterizationStateCreateInfo rasterizer;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineMultisampleStateCreateInfo multisampling;
        VkPipelineLayout pipelineLayout;
        VkPipelineDepthStencilStateCreateInfo depthStencil;
        VkPipelineRenderingCreateInfo renderInfo;
        VkFormat colorAttachmentformat;

        void clear() {
            shaderStages.clear();
            inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
            rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
            colorBlendAttachment = {};
            multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
            pipelineLayout = {};
            depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
            renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        }

        PipelineBuilder& addShader(VkShaderModule module, VkShaderStageFlagBits flags) {
            shaderStages.push_back(shaderStageCreateInfo(flags, module));
            return *this;
        }

        PipelineBuilder& topology(VkPrimitiveTopology topo) {
            inputAssembly.topology = topo;
            inputAssembly.primitiveRestartEnable = VK_FALSE;
            return *this;
        }

        PipelineBuilder& polygonMode(VkPolygonMode mode) {
            rasterizer.polygonMode = mode;
            rasterizer.lineWidth = 1.f;
            return *this;
        }

        PipelineBuilder& cullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) {
            rasterizer.cullMode = cullMode;
            rasterizer.frontFace = frontFace;
            return *this;
        }

        PipelineBuilder& multisampleNone() {
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisampling.minSampleShading = 1.0f;
            multisampling.pSampleMask = nullptr;
            multisampling.alphaToCoverageEnable = VK_FALSE;
            multisampling.alphaToOneEnable = VK_FALSE;
            return *this;
        }

        PipelineBuilder& disableBlending() {
            colorBlendAttachment.colorWriteMask = 
                VK_COLOR_COMPONENT_R_BIT | 
                VK_COLOR_COMPONENT_G_BIT | 
                VK_COLOR_COMPONENT_B_BIT | 
                VK_COLOR_COMPONENT_A_BIT;

            colorBlendAttachment.blendEnable = VK_FALSE;
            return *this;
        }

        PipelineBuilder& enableBlendFactor(VkBlendFactor srcFactor, VkBlendFactor dstFactor) {
            colorBlendAttachment.colorWriteMask = 
                VK_COLOR_COMPONENT_R_BIT | 
                VK_COLOR_COMPONENT_G_BIT | 
                VK_COLOR_COMPONENT_B_BIT | 
                VK_COLOR_COMPONENT_A_BIT;

            colorBlendAttachment.blendEnable = VK_TRUE;
            colorBlendAttachment.srcColorBlendFactor = srcFactor;
            colorBlendAttachment.dstColorBlendFactor = dstFactor;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            return *this;
        }

        PipelineBuilder& enableBlendingAdditive() {
            return enableBlendFactor(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE);
        }

        PipelineBuilder& enableBlendingAlphablend() {
            return enableBlendFactor(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        }

        PipelineBuilder& colorAttachmentFormat(VkFormat format) {
            colorAttachmentformat = format;
            renderInfo.colorAttachmentCount = 1;
            renderInfo.pColorAttachmentFormats = &colorAttachmentformat;
            return *this;
        }

        PipelineBuilder& depthFormat(VkFormat format) {
            renderInfo.depthAttachmentFormat = format;
            return *this;
        }

        PipelineBuilder& disableDepthTest() {
            depthStencil.depthTestEnable = VK_FALSE;
            depthStencil.depthWriteEnable = VK_FALSE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;
            depthStencil.front = {};
            depthStencil.back = {};
            depthStencil.minDepthBounds = 0.f;
            depthStencil.maxDepthBounds = 1.f;
            return *this;
        }

        PipelineBuilder& enableDepthTest(bool depthWriteEnable, VkCompareOp op) {
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = depthWriteEnable;
            depthStencil.depthCompareOp = op;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;
            depthStencil.front = {};
            depthStencil.back = {};
            depthStencil.minDepthBounds = 0.f;
            depthStencil.maxDepthBounds = 1.f;
            return *this;
        }

        VkPipeline build(vkb::Device device) {
            VkPipelineViewportStateCreateInfo viewportState = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1,
            };

            VkPipelineColorBlendStateCreateInfo colorBlending = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .pNext = nullptr,
                .logicOpEnable = VK_FALSE,
                .logicOp = VK_LOGIC_OP_COPY,
                .attachmentCount = 1,
                .pAttachments = &colorBlendAttachment,
            };

            VkPipelineVertexInputStateCreateInfo vertexInputInfo = { 
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            };

            std::array<VkDynamicState, 2> state = { 
                VK_DYNAMIC_STATE_VIEWPORT, 
                VK_DYNAMIC_STATE_SCISSOR,
            };
            VkPipelineDynamicStateCreateInfo dynamicInfo = { 
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = 2,
                .pDynamicStates = state.data(),
            };

            VkGraphicsPipelineCreateInfo pipelineInfo = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = &renderInfo,
                .flags = 0,
                .stageCount = (uint32_t)shaderStages.size(),
                .pStages = shaderStages.data(),
                .pVertexInputState = &vertexInputInfo,
                .pInputAssemblyState = &inputAssembly,
                .pTessellationState = nullptr,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = &depthStencil,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicInfo,
                .layout = pipelineLayout,
                .renderPass = nullptr,
            };

            VkPipeline pipeline;

            Check::vkResult(
                vkCreateGraphicsPipelines(
                    device.device, 
                    nullptr, 
                    1, 
                    &pipelineInfo, 
                    nullptr, 
                    &pipeline)
            );

            return pipeline;
        }
    };

    auto createTriangleMeshPipeline(vkb::Device device, Weave::AllocatedImage drawImage, Weave::AllocatedImage depthImage) {
        VkPushConstantRange bufferRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(Weave::GPUDrawPushConstants),
        };
        VkPipelineLayout trainglePipelineLayout;
        VkPipelineLayoutCreateInfo triangleLayoutInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &bufferRange,
        };
        vkCreatePipelineLayout(device.device, &triangleLayoutInfo, nullptr, &trainglePipelineLayout);

        auto fragment = loadShaderModule("Shaders/tri_mesh.frag.spv", device);
        auto vert = loadShaderModule("Shaders/tri_mesh.vert.spv", device);

        PipelineBuilder pb;

        pb.clear();
        pb.pipelineLayout = trainglePipelineLayout;
        pb
            .addShader(vert, VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(fragment, VK_SHADER_STAGE_FRAGMENT_BIT)
            .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .polygonMode(VK_POLYGON_MODE_FILL)
            .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
            .multisampleNone()
            .enableBlendingAdditive()
            .enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
            .colorAttachmentFormat(drawImage.imageFormat)
            .depthFormat(depthImage.imageFormat);

        auto trianglePipeline = pb.build(device);

        vkDestroyShaderModule(device.device, fragment, nullptr);
        vkDestroyShaderModule(device.device, vert, nullptr);

        return std::tuple(trainglePipelineLayout, trianglePipeline);
    }
}