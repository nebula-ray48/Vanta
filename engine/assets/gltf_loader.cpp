//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "gltf_loader.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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
    fastgltf::GltfDataBuffer data;
    if (!data.loadFromFile(file_path)) {
        return std::unexpected(GltfLoadError::BufferLoadFailed);
    }

    auto asset_res = parser.loadGltf(&data, file_path.parent_path(),
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages);

    if (asset_res.error() != fastgltf::Error::None) {
        return std::unexpected(GltfLoadError::ParseFailed);
    }

    fastgltf::Asset& asset = asset_res.get();
    GltfScene scene;

    for (auto& image : asset.images) {
        TextureData tex_data;
        tex_data.name = image.name.c_str();

        std::visit(fastgltf::visitor{
            [&](auto& arg) {
                std::cerr << "Unknown image data source for image " << tex_data.name << "\n";
            },
            [&](fastgltf::sources::URI& uri) {
                tex_data.uri = std::string(uri.uri.path().begin(), uri.uri.path().end());
                std::cout << "Image source is URI: " << tex_data.uri << "\n";
            },
            [&](fastgltf::sources::Vector& vec) {
                std::cout << "Image source is Vector\n";
                tex_data.raw_data.assign(
                    reinterpret_cast<const std::byte*>(vec.bytes.data()),
                    reinterpret_cast<const std::byte*>(vec.bytes.data() + vec.bytes.size())
                );
                tex_data.mime_type = get_mime_type_string(vec.mimeType);
            },
            [&](fastgltf::sources::Array& arr) {
                std::cout << "Image source is Array\n";
                tex_data.raw_data.assign(
                    reinterpret_cast<const std::byte*>(arr.bytes.data()),
                    reinterpret_cast<const std::byte*>(arr.bytes.data() + arr.bytes.size())
                );
                tex_data.mime_type = get_mime_type_string(arr.mimeType);
            },
            [&](fastgltf::sources::BufferView& view) {
                std::cout << "Image source is BufferView\n";
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
                tex_data.mime_type = get_mime_type_string(fastgltf::MimeType::None);
            }
        }, image.data);

        scene.images.push_back(std::move(tex_data));
    }

    auto get_image_index = [&](size_t texture_index) -> int32_t {
        if (texture_index < asset.textures.size() && asset.textures[texture_index].imageIndex.has_value()) {
            return static_cast<int32_t>(asset.textures[texture_index].imageIndex.value());
        }
        return -1;
    };

    for (auto& material : asset.materials) {
        Material mat;
        if (material.pbrData.baseColorTexture.has_value()) {
            int32_t img_idx = get_image_index(material.pbrData.baseColorTexture->textureIndex);
            mat.base_color_texture_index = img_idx;
            if (img_idx >= 0 && static_cast<size_t>(img_idx) < scene.images.size()) {
                scene.images[static_cast<size_t>(img_idx)].is_srgb = true;
            }
        }
        if (material.emissiveTexture.has_value()) {
            int32_t img_idx = get_image_index(material.emissiveTexture->textureIndex);
            mat.emissive_texture_index = img_idx;
            if (img_idx >= 0 && static_cast<size_t>(img_idx) < scene.images.size()) {
                scene.images[static_cast<size_t>(img_idx)].is_srgb = true;
            }
        }
        if (material.normalTexture.has_value()) {
            int32_t img_idx = get_image_index(material.normalTexture->textureIndex);
            mat.normal_texture_index = img_idx;
            mat.normal_scale = material.normalTexture->scale;
            if (img_idx >= 0 && static_cast<size_t>(img_idx) < scene.images.size()) {
                scene.images[static_cast<size_t>(img_idx)].is_srgb = false;
            }
        }
        if (material.pbrData.metallicRoughnessTexture.has_value()) {
            int32_t img_idx = get_image_index(material.pbrData.metallicRoughnessTexture->textureIndex);
            mat.metallic_roughness_texture_index = img_idx;
            if (img_idx >= 0 && static_cast<size_t>(img_idx) < scene.images.size()) {
                scene.images[static_cast<size_t>(img_idx)].is_srgb = false;
            }
        }
        if (material.occlusionTexture.has_value()) {
            int32_t img_idx = get_image_index(material.occlusionTexture->textureIndex);
            mat.occlusion_texture_index = img_idx;
            mat.occlusion_strength = material.occlusionTexture->strength;
            if (img_idx >= 0 && static_cast<size_t>(img_idx) < scene.images.size()) {
                scene.images[static_cast<size_t>(img_idx)].is_srgb = false;
            }
        }
        mat.base_color_factor = glm::vec4(
            material.pbrData.baseColorFactor[0],
            material.pbrData.baseColorFactor[1],
            material.pbrData.baseColorFactor[2],
            material.pbrData.baseColorFactor[3]
        );
        mat.metallic_factor = material.pbrData.metallicFactor;
        mat.roughness_factor = material.pbrData.roughnessFactor;
        scene.materials.push_back(mat);
    }

    for (auto& mesh : asset.meshes) {
        Mesh m;
        m.first_primitive = static_cast<uint32_t>(scene.primitives.size());
        m.primitive_count = static_cast<uint32_t>(mesh.primitives.size());
        scene.meshes.push_back(m);

        for (auto& primitive : mesh.primitives) {
            MeshPrimitive prim;
            prim.first_index = static_cast<uint32_t>(scene.indices.size());
            prim.vertex_offset = static_cast<uint32_t>(scene.vertices.size());
            prim.material_index = primitive.materialIndex.has_value() ? static_cast<int32_t>(primitive.materialIndex.value()) : -1;

            uint32_t initial_vertex_count = static_cast<uint32_t>(scene.vertices.size());

            auto* position_it = primitive.findAttribute("POSITION");
            if (position_it != primitive.attributes.end()) {
                auto& accessor = asset.accessors[position_it->second];
                scene.vertices.resize(initial_vertex_count + accessor.count);
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 pos, std::size_t idx) {
                    scene.vertices[initial_vertex_count + idx].position = pos;
                });
            }

            auto* normal_it = primitive.findAttribute("NORMAL");
            if (normal_it != primitive.attributes.end()) {
                auto& accessor = asset.accessors[normal_it->second];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 normal, std::size_t idx) {
                    scene.vertices[initial_vertex_count + idx].normal = normal;
                });
            }

            auto* uv_it = primitive.findAttribute("TEXCOORD_0");
            if (uv_it != primitive.attributes.end()) {
                auto& accessor = asset.accessors[uv_it->second];
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

    for (auto& node : asset.nodes) {
        Node n;
        n.name = node.name.c_str();
        n.mesh_index = node.meshIndex.has_value() ? static_cast<int32_t>(node.meshIndex.value()) : -1;

        if (auto* m = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform)) {
            n.local_transform = glm::mat4(
                (*m)[0], (*m)[1], (*m)[2], (*m)[3],
                (*m)[4], (*m)[5], (*m)[6], (*m)[7],
                (*m)[8], (*m)[9], (*m)[10], (*m)[11],
                (*m)[12], (*m)[13], (*m)[14], (*m)[15]
            );
        } else if (auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
            glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(trs->translation[0], trs->translation[1], trs->translation[2]));
            glm::quat q(trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]); 
            glm::mat4 r = glm::mat4_cast(q);
            glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(trs->scale[0], trs->scale[1], trs->scale[2]));
            n.local_transform = t * r * s;
        }

        for (auto& child : node.children) {
            n.children.push_back(static_cast<uint32_t>(child));
        }
        scene.nodes.push_back(std::move(n));
    }

    if (asset.defaultScene.has_value() && asset.defaultScene.value() < asset.scenes.size()) {
        auto& gltf_scene = asset.scenes[asset.defaultScene.value()];
        for (auto& root : gltf_scene.nodeIndices) {
            scene.root_nodes.push_back(static_cast<uint32_t>(root));
        }
    } else if (!asset.scenes.empty()) {
        auto& gltf_scene = asset.scenes[0];
        for (auto& root : gltf_scene.nodeIndices) {
            scene.root_nodes.push_back(static_cast<uint32_t>(root));
        }
    }

    return scene;
}

}  // namespace vanta::scene
