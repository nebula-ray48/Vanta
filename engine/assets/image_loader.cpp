//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "image_loader.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "../include/ext/stb_image.h"

std::expected<RawImage, TextureError> load_image(const std::filesystem::path& filepath) {
    if (!std::filesystem::exists(filepath)) {
        return std::unexpected(TextureError::FileNotFound);
    }

    int width, height, channels;
    unsigned char* raw_data = stbi_load(filepath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!raw_data) {
        return std::unexpected(TextureError::LoadFailed);
    }

    RawImage img;
    img.width = width;
    img.height = height;
    img.channels = 4;
    img.data.reset(raw_data);

    return img;
}

std::expected<RawImage, TextureError> load_image_from_memory(const std::byte* data, size_t size) {
    if (!data || size == 0) {
        return std::unexpected(TextureError::LoadFailed);
    }

    int width, height, channels;
    unsigned char* raw_data = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(data),
        static_cast<int>(size),
        &width, &height, &channels, STBI_rgb_alpha);

    if (!raw_data) {
        return std::unexpected(TextureError::LoadFailed);
    }

    RawImage img;
    img.width = width;
    img.height = height;
    img.channels = 4; // We requested STBI_rgb_alpha
    img.data.reset(raw_data);

    return img;
}
