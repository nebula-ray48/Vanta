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
