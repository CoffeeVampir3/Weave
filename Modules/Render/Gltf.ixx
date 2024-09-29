module;
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"
#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/core.hpp"

export module Gltf;
import std;
import Mesh;
import Logging;
import Command;

export namespace Weave {
    std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(
        vkb::Device device, 
        VkQueue graphicsQueue, 
        VmaAllocator allocator,
        ImmediateCommander immediateCmd,
        std::filesystem::path filePath) {
        if (!std::filesystem::exists(filePath)) {
            Logging::failure("Failed to find mesh asset path: {}", std::source_location::current(), filePath.string());
            abort();
        }
		static constexpr auto supportedExtensions =
			fastgltf::Extensions::KHR_mesh_quantization |
			fastgltf::Extensions::KHR_texture_transform |
			fastgltf::Extensions::KHR_materials_variants;

        fastgltf::Parser parser(supportedExtensions);

        constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;

        auto gltfFile = fastgltf::MappedGltfFile::FromPath(filePath);
        if (!static_cast<bool>(gltfFile)) {
            Logging::failure("Failed to open mesh asset with gltf error: {}", std::source_location::current(), fastgltf::getErrorMessage(gltfFile.error()));
            abort();
        }

        auto maybeAsset = parser.loadGltf(gltfFile.get(), filePath.parent_path(), gltfOptions);
        if (maybeAsset.error() != fastgltf::Error::None) {
            Logging::failure("Failed to load mesh asset with gltf error: {}", std::source_location::current(), fastgltf::getErrorMessage(maybeAsset.error()));
            abort();
        }
        auto gltf = std::move(maybeAsset.get());
        std::vector<std::shared_ptr<MeshAsset>> meshes;
        std::vector<uint32_t> indices;
        std::vector<Weave::Vertex> vertices;
        for (fastgltf::Mesh& mesh : gltf.meshes) {
            MeshAsset newmesh;

            newmesh.name = mesh.name;

            // clear the mesh arrays each mesh, we dont want to merge them by error
            indices.clear();
            vertices.clear();

            for (auto&& p : mesh.primitives) {
                GeoSurface newSurface;
                newSurface.startIndex = (uint32_t)indices.size();
                newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

                size_t initial_vtx = vertices.size();

                // load indexes
                {
                    fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                    indices.reserve(indices.size() + indexaccessor.count);

                    fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                        [&](std::uint32_t idx) {
                            indices.push_back(idx + initial_vtx);
                        });
                }

                // load vertex positions
                {
                    fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                    vertices.resize(vertices.size() + posAccessor.count);

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                        [&](glm::vec3 v, size_t index) {
                            Weave::Vertex newvtx{
                                .position = v,
                                .uv_x = 0,
                                .normal = { 1, 0, 0 },
                                .uv_y = 0,
                                .color = glm::vec4 { 1.0f }

                            };
                            vertices[initial_vtx + index] = newvtx;
                        });
                }

                // load vertex normals
                auto normals = p.findAttribute("NORMAL");
                if (normals != p.attributes.end()) {

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex],
                        [&](glm::vec3 v, size_t index) {
                            vertices[initial_vtx + index].normal = v;
                        });
                }

                // load UVs
                auto uv = p.findAttribute("TEXCOORD_0");
                if (uv != p.attributes.end()) {

                    fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).accessorIndex],
                        [&](glm::vec2 v, size_t index) {
                            vertices[initial_vtx + index].uv_x = v.x;
                            vertices[initial_vtx + index].uv_y = v.y;
                        });
                }

                // load vertex colors
                auto colors = p.findAttribute("COLOR_0");
                if (colors != p.attributes.end()) {

                    fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex],
                        [&](glm::vec4 v, size_t index) {
                            vertices[initial_vtx + index].color = v;
                        });
                }
                newmesh.surfaces.push_back(newSurface);
            }

            // display the vertex normals
            constexpr bool OverrideColors = true;
            if (OverrideColors) {
                for (Vertex& vtx : vertices) {
                    vtx.color = glm::vec4(vtx.normal / 4.0f, 1.0f);
                }
            }
            newmesh.meshBuffers = Weave::createMeshBuffer(device, graphicsQueue, allocator, immediateCmd, indices, vertices);

            meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
        }

        return meshes;

    }
}