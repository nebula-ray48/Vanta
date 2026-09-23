//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once
#include <expected>
#include <filesystem>
#include <memory>

extern "C" void stbi_image_free(void* retval_from_stbi_load);

enum class TextureError {
    FileNotFound,
    LoadFailed
};

using StbImagePtr = std::unique_ptr<unsigned char, decltype(&stbi_image_free)>;

struct RawImage {
    int width = 0;
    int height = 0;
    int channels = 0;
    StbImagePtr data{nullptr, stbi_image_free};
};

// 副作用のない純粋な関数として定義
[[nodiscard]] std::expected<RawImage, TextureError> load_image(const std::filesystem::path& filepath);
