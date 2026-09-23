//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <cstddef>

struct EntityId { uint32_t value; };
struct MeshId { uint32_t value; };

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;
    uint32_t texture_id = 0;

    static vk::VertexInputBindingDescription get_binding_description() {
        return vk::VertexInputBindingDescription()
            .setBinding(0)
            .setStride(sizeof(Vertex))
            .setInputRate(vk::VertexInputRate::eVertex);
    }

    static std::array<vk::VertexInputAttributeDescription, 5> get_attribute_descriptions() {
        return {
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)),
            vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32Sfloat,     offsetof(Vertex, uv)),
            vk::VertexInputAttributeDescription(4, 0, vk::Format::eR32Uint,          offsetof(Vertex, texture_id))
        };
    }
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // TODO: キューブや平面の生成ロジックは、MeshDataのstaticメソッドとして実装
    static MeshData new_cube(float size, glm::vec3 color);
    static MeshData new_plane(float width, float depth, glm::vec3 color);
};

struct RenderInstance {
    EntityId entity_id;
    MeshId mesh_id;
    glm::mat4 model_matrix;
};

struct RenderSnapshot {
    uint64_t frame_number;
    std::vector<RenderInstance> instances;
    glm::mat4 view_matrix;
};

struct alignas(16) GpuTransform {
    glm::vec3 position;
    float _pad0;
    glm::vec4 rotation;
    glm::vec3 scale;
    float _pad1;
};

struct alignas(16) GpuEntity {
    uint32_t id;
    uint32_t mesh_id;
    uint32_t _pad0[2];
    GpuTransform transform;
};

struct PushConstants {
    glm::mat4 mvp;
    PushConstants(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj) {
        mvp = proj * view * model;
    }
};
