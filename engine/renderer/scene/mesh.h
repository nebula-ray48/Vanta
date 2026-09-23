//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include "render_types.h"

namespace vanta::scene {

    MeshData create_ground_grid(float size, float uv_scale, uint32_t tex_id);
    MeshData create_cube(float size, const glm::vec3& color, uint32_t tex_id);
}

