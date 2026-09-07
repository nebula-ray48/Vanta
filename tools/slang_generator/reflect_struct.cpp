//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include <slang.h>

#include <expected>
#include <string>
#include <string_view>

#include "generator_core.h"

namespace vanta::slang_generator {

    // TODO 将来的には std::string ではなく EngineError 構造体への移行
    using ExtractResult = std::expected<ShaderStruct, std::string>;

    static ExtractResult extract_struct_info_from_variable(
        slang::ShaderReflection* reflection,
        const std::string_view variable_name
    ) {
        if (reflection == nullptr) {
            return std::unexpected("Reflection data is null.");
        }

        slang::VariableLayoutReflection* target_var_layout = nullptr;
        uint32_t const param_count = reflection->getParameterCount();

        for (uint32_t i = 0; i < param_count; ++i) {
            slang::VariableLayoutReflection* param = reflection->getParameterByIndex(i);
            slang::VariableReflection* var = param->getVariable();

            if ((var != nullptr) && variable_name == var->getName()) {
                target_var_layout = param;
                break;
            }
        }

        if (target_var_layout == nullptr) {
            return std::unexpected("Variable not found: " + std::string(variable_name));
        }

        // 2. 変数の「型のレイアウト情報」を取得
        slang::TypeLayoutReflection* type_layout = target_var_layout->getTypeLayout();

        if (type_layout->getKind() == slang::TypeReflection::Kind::ConstantBuffer) {
            type_layout = type_layout->getElementTypeLayout();
        }

        if (type_layout->getKind() != slang::TypeReflection::Kind::Struct) {
            return std::unexpected("Target is not a struct.");
        }

        // 3. データの詰め込み (SoA構造への流し込み)
        uint32_t const field_count = type_layout->getFieldCount();

        ShaderStruct result_struct{
            .name = type_layout->getType()->getName(),
            .total_size = static_cast<uint32_t>(type_layout->getSize()),
            .members = {},
        };

        result_struct.members.names.reserve(field_count);
        result_struct.members.type_names.reserve(field_count);
        result_struct.members.offsets.reserve(field_count);
        result_struct.members.sizes.reserve(field_count);
        result_struct.members.categories.reserve(field_count);
        result_struct.members.array_sizes.reserve(field_count);

        for (uint32_t i = 0; i < field_count; ++i) {
            slang::VariableLayoutReflection* field_layout = type_layout->getFieldByIndex(i);
            slang::VariableReflection* field_var = field_layout->getVariable();
            slang::TypeReflection* field_type = field_layout->getTypeLayout()->getType();

            result_struct.members.names.emplace_back(field_var->getName());
            result_struct.members.type_names.emplace_back(field_type->getName());

            size_t const offset = field_layout->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM);
            result_struct.members.offsets.emplace_back(static_cast<uint32_t>(offset));

            size_t const size = field_layout->getTypeLayout()->getSize();
            result_struct.members.sizes.emplace_back(static_cast<uint32_t>(size));

            result_struct.members.categories.emplace_back(TypeCategory::BASIC);
            result_struct.members.array_sizes.emplace_back(1);
        }

        return result_struct;
    }

}  // namespace vanta::slang_generator
