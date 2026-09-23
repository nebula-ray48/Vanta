// Copyright (c) 2026 Nebula-Ray42.
//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "mesh.h"

#include "render_types.h"

namespace vanta::scene {
    MeshData create_ground_grid(float size, float uv_scale, uint32_t tex_id) {
        MeshData mesh;

        mesh.vertices = {
                {{-size, 0.0f, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f},         tex_id},
                {{ size, 0.0f, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {uv_scale, 0.0f},     tex_id},
                {{ size, 0.0f,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {uv_scale, uv_scale}, tex_id},
                {{-size, 0.0f,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, uv_scale},     tex_id}
        };

        mesh.indices = {
            0, 1, 2,
            2, 3, 0
        };

        return mesh;
    }

    MeshData create_cube(float size, const glm::vec3& color, uint32_t tex_id) {
        MeshData mesh;
        float h = size * 0.5f;

        // Front, Back, Top, Bottom, Right, Left
        mesh.vertices = {
            // Front
            {{-h, -h,  h}, color, { 0,  0,  1}, {0, 0}, tex_id},
            {{ h, -h,  h}, color, { 0,  0,  1}, {1, 0}, tex_id},
            {{ h,  h,  h}, color, { 0,  0,  1}, {1, 1}, tex_id},
            {{-h,  h,  h}, color, { 0,  0,  1}, {0, 1}, tex_id},
            // Back
            {{ h, -h, -h}, color, { 0,  0, -1}, {0, 0}, tex_id},
            {{-h, -h, -h}, color, { 0,  0, -1}, {1, 0}, tex_id},
            {{-h,  h, -h}, color, { 0,  0, -1}, {1, 1}, tex_id},
            {{ h,  h, -h}, color, { 0,  0, -1}, {0, 1}, tex_id},
            // Top
            {{-h,  h,  h}, color, { 0,  1,  0}, {0, 0}, tex_id},
            {{ h,  h,  h}, color, { 0,  1,  0}, {1, 0}, tex_id},
            {{ h,  h, -h}, color, { 0,  1,  0}, {1, 1}, tex_id},
            {{-h,  h, -h}, color, { 0,  1,  0}, {0, 1}, tex_id},
            // Bottom
            {{-h, -h, -h}, color, { 0, -1,  0}, {0, 0}, tex_id},
            {{ h, -h, -h}, color, { 0, -1,  0}, {1, 0}, tex_id},
            {{ h, -h,  h}, color, { 0, -1,  0}, {1, 1}, tex_id},
            {{-h, -h,  h}, color, { 0, -1,  0}, {0, 1}, tex_id},
            // Right
            {{ h, -h,  h}, color, { 1,  0,  0}, {0, 0}, tex_id},
            {{ h, -h, -h}, color, { 1,  0,  0}, {1, 0}, tex_id},
            {{ h,  h, -h}, color, { 1,  0,  0}, {1, 1}, tex_id},
            {{ h,  h,  h}, color, { 1,  0,  0}, {0, 1}, tex_id},
            // Left
            {{-h, -h, -h}, color, {-1,  0,  0}, {0, 0}, tex_id},
            {{-h, -h,  h}, color, {-1,  0,  0}, {1, 0}, tex_id},
            {{-h,  h,  h}, color, {-1,  0,  0}, {1, 1}, tex_id},
            {{-h,  h, -h}, color, {-1,  0,  0}, {0, 1}, tex_id}
        };

        for (uint32_t i = 0; i < 6; ++i) {
            uint32_t offset = i * 4;
            mesh.indices.push_back(offset + 0);
            mesh.indices.push_back(offset + 1);
            mesh.indices.push_back(offset + 2);
            mesh.indices.push_back(offset + 2);
            mesh.indices.push_back(offset + 3);
            mesh.indices.push_back(offset + 0);
        }

        return mesh;
    }
}  // namespace vanta::scene


