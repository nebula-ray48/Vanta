//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "gltf_loader.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

namespace vanta::scene {

constexpr const char* get_mime_type_string(fastgltf::MimeType mime) {
    switch (mime) {
        case fastgltf::MimeType::JPEG: return "image/jpeg";
        case fastgltf::MimeType::PNG:  return "image/png";
        case fastgltf::MimeType::KTX2: return "image/ktx2";
        default: return "unknown";
    }
}

[[nodiscard]] std::expected<GltfScene, GltfLoadError> load_gltf(const std::filesystem::path& file_path) {
    if (!std::filesystem::exists(file_path)) {
        return std::unexpected(GltfLoadError::FileNotFound);
    }

    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(file_path);

    if (data.error() != fastgltf::Error::None) {
        return std::unexpected(GltfLoadError::BufferLoadFailed);
    }

    auto asset_res = parser.loadGltf(data.get(), file_path.parent_path(),
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages);

    if (asset_res.error() != fastgltf::Error::None) {
        return std::unexpected(GltfLoadError::ParseFailed);
    }

    fastgltf::Asset& asset = asset_res.get();
    GltfScene scene;

    for (auto& material : asset.materials) {
        Material mat;
        if (material.pbrData.baseColorTexture.has_value()) {
            mat.base_color_texture_index = static_cast<int32_t>(material.pbrData.baseColorTexture->textureIndex);
        }
        mat.base_color_factor = glm::vec4(
            material.pbrData.baseColorFactor[0],
            material.pbrData.baseColorFactor[1],
            material.pbrData.baseColorFactor[2],
            material.pbrData.baseColorFactor[3]
        );
        scene.materials.push_back(mat);
    }

    for (auto& image : asset.images) {
        TextureData tex_data;
        tex_data.name = image.name.c_str();

        std::visit(fastgltf::visitor{
            [](auto& arg) {},
            [&](fastgltf::sources::Vector& vec) {
                tex_data.raw_data.assign(
                    reinterpret_cast<const std::byte*>(vec.bytes.data()),
                    reinterpret_cast<const std::byte*>(vec.bytes.data() + vec.bytes.size())
                );
                tex_data.mime_type = get_mime_type_string(vec.mimeType); // 修正
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];
                std::visit(fastgltf::visitor{
                    [](auto& arg) {},
                    [&](fastgltf::sources::Vector& vec) {
                        tex_data.raw_data.assign(
                            reinterpret_cast<const std::byte*>(vec.bytes.data() + bufferView.byteOffset),
                            reinterpret_cast<const std::byte*>(vec.bytes.data() + bufferView.byteOffset + bufferView.byteLength)
                        );
                    },
                    [&](fastgltf::sources::Array& arr) {
                         tex_data.raw_data.assign(
                            reinterpret_cast<const std::byte*>(arr.bytes.data() + bufferView.byteOffset),
                            reinterpret_cast<const std::byte*>(arr.bytes.data() + bufferView.byteOffset + bufferView.byteLength)
                        );
                    }
                }, buffer.data);
                // 画像データ自体にMimeTypeが設定されていない場合のフォールバック
                tex_data.mime_type = get_mime_type_string(fastgltf::MimeType::None);
            }
        }, image.data);

        scene.images.push_back(std::move(tex_data));
    }

    for (auto& mesh : asset.meshes) {
        for (auto& primitive : mesh.primitives) {
            MeshPrimitive prim;
            prim.first_index = static_cast<uint32_t>(scene.indices.size());
            prim.vertex_offset = static_cast<uint32_t>(scene.vertices.size());
            prim.material_index = primitive.materialIndex.has_value() ? static_cast<int32_t>(primitive.materialIndex.value()) : -1;

            uint32_t initial_vertex_count = scene.vertices.size();

            auto* position_it = primitive.findAttribute("POSITION");
            if (position_it != primitive.attributes.end()) {
                auto& accessor = asset.accessors[position_it->accessorIndex];
                scene.vertices.resize(initial_vertex_count + accessor.count);
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 pos, std::size_t idx) {
                    scene.vertices[initial_vertex_count + idx].position = pos;
                });
            }

            auto* normal_it = primitive.findAttribute("NORMAL");
            if (normal_it != primitive.attributes.end()) {
                auto& accessor = asset.accessors[normal_it->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 normal, std::size_t idx) {
                    scene.vertices[initial_vertex_count + idx].normal = normal;
                });
            }

            auto* uv_it = primitive.findAttribute("TEXCOORD_0");
            if (uv_it != primitive.attributes.end()) {
                auto& accessor = asset.accessors[uv_it->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, accessor, [&](glm::vec2 uv, std::size_t idx) {
                    scene.vertices[initial_vertex_count + idx].uv = uv;
                });
            }

            if (primitive.indicesAccessor.has_value()) {
                auto& accessor = asset.accessors[primitive.indicesAccessor.value()];
                prim.index_count = static_cast<uint32_t>(accessor.count);
                scene.indices.reserve(scene.indices.size() + accessor.count);
                fastgltf::iterateAccessor<uint32_t>(asset, accessor, [&](uint32_t idx) {
                    scene.indices.push_back(idx);
                });
            } else {
                prim.index_count = 0;
            }

            scene.primitives.push_back(prim);
        }
    }

    return scene;
}

}  // namespace vanta::scene
