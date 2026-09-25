// bindings.slang
//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

module common_bindings;

import common_types;

// ==========================================
// Bindless Set (Set 0)
// ==========================================
// Binding 0: Global Uniform Buffer
[[vk::binding(0, 0)]]
public ConstantBuffer<GlobalUbo> globalData;

// Binding 1: Global Bindless Textures
[[vk::binding(1, 0)]]
public Texture2D bindlessTextures[];

// Binding 2: Global Sampler
[[vk::binding(2, 0)]]
public SamplerState defaultSampler;

// Binding 3: Object Data Buffer
[[vk::binding(3, 0)]]
public StructuredBuffer<ObjectData> objectBuffer;

// Binding 4: Environment Cubemap
[[vk::binding(4, 0)]]
public SamplerCube envCubemap;
