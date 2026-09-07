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
}  // namespace vanta::scene


