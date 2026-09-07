//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <expected>
#include <string>
#include <vector>

#include "slang.h"

namespace vanta::slang_generator {
    struct ResourceBindingsSoA;

    // ==========================================
    // 型の種類を区別するタグ
    // メモリ最適化: SoA配列に並べた際のフットプリントを最小化するため uint8_t を指定
    // ==========================================
    enum class TypeCategory : uint8_t {
        BASIC,
        USER_STRUCT,
        ARRAY
    };

    // ==========================================
    // 1. 構造体の中身（メンバー変数）のデータ (SoAレイアウト)
    // ==========================================
    struct StructMembersSoA {
        std::vector<std::string>  names;
        std::vector<std::string>  type_names;
        std::vector<TypeCategory> categories;
        std::vector<uint32_t>     offsets;
        std::vector<uint32_t>     sizes;
        std::vector<uint32_t>     array_sizes;
    };

    std::expected<ResourceBindingsSoA, std::string> extract_resource_bindings(slang::ShaderReflection* reflection);
    std::expected<std::string, std::string> generate_cpp_bindings(const ResourceBindingsSoA& soa_data);

    // ==========================================
    // 2. シェーダー内の構造体自体のデータ
    // ==========================================
    struct ShaderStruct {
        std::string name;
        uint32_t total_size;
        StructMembersSoA members;
    };

    // ==========================================
    // 3. バインディング（リソース）のデータ (SoAレイアウト)
    // Vulkanの DescriptorSetLayout 構築を見据え、一括処理しやすいSoAへ変更
    // ==========================================
    struct ResourceBindingsSoA {
        std::vector<std::string> names;
        std::vector<uint32_t>    sets;
        std::vector<uint32_t>    bindings;
    };

}  // namespace vanta::slang_generator
