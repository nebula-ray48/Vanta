//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <expected>
#include <cstdint>
#include <cstddef>
#include <glm/glm.hpp>

namespace vanta::scene {

    enum class GltfLoadError {
        FileNotFound,
        ParseFailed,
        BufferLoadFailed,
        UnsupportedFormat
    };

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 tangent;
    };

    struct MeshPrimitive {
        uint32_t first_index;
        uint32_t index_count;
        uint32_t vertex_offset;
        int32_t  material_index;
    };

    struct Material {
        int32_t base_color_texture_index{ -1 };
        int32_t normal_texture_index{ -1 };
        int32_t metallic_roughness_texture_index{ -1 };
        int32_t emissive_texture_index{ -1 };
        glm::vec4 base_color_factor{ 1.0f };
        float normal_scale{ 1.0f };
        float metallic_factor{ 1.0f };
        float roughness_factor{ 1.0f };
    };

    struct TextureData {
        std::vector<std::byte> raw_data;
        std::string name;
        std::string mime_type;
        std::string uri;
        bool is_srgb{ false };
    };

    struct Mesh {
        uint32_t first_primitive;
        uint32_t primitive_count;
    };

    struct Node {
        std::string name;
        int32_t mesh_index{ -1 };
        glm::mat4 local_transform{ 1.0f };
        std::vector<uint32_t> children;
    };

    struct GltfScene {
        std::vector<Vertex>        vertices;
        std::vector<uint32_t>      indices;
        std::vector<MeshPrimitive> primitives;
        std::vector<Mesh>          meshes;
        std::vector<Material>      materials;
        std::vector<TextureData>   images;
        std::vector<Node>          nodes;
        std::vector<uint32_t>      root_nodes;
    };

    [[nodiscard]] std::expected<GltfScene, GltfLoadError> load_gltf(const std::filesystem::path& file_path);

}  // namespace vanta::scene
