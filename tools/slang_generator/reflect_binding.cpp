//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include <slang.h>

#include <expected>
#include <string>

#include "generator_core.h"

namespace vanta::slang_generator {

    std::expected<ResourceBindingsSoA, std::string> extract_resource_bindings(slang::ShaderReflection* reflection) {
        if (reflection == nullptr) {
            return std::unexpected("Reflection data is null.");
        }

        uint32_t valid_resource_count = 0;
        uint32_t const param_count = reflection->getParameterCount();

        for (uint32_t i = 0; i < param_count; ++i) {
            slang::VariableLayoutReflection* param = reflection->getParameterByIndex(i);
            slang::TypeLayoutReflection* type_layout = param->getTypeLayout();
            slang::TypeReflection::Kind const kind = type_layout->getKind();

            if (kind == slang::TypeReflection::Kind::ConstantBuffer ||
                kind == slang::TypeReflection::Kind::Resource ||
                kind == slang::TypeReflection::Kind::SamplerState) {

                ++valid_resource_count;
            }
        }

        if (valid_resource_count == 0) {
            return ResourceBindingsSoA{};
        }

        ResourceBindingsSoA result_bindings;
        result_bindings.names.reserve(valid_resource_count);
        result_bindings.sets.reserve(valid_resource_count);
        result_bindings.bindings.reserve(valid_resource_count);

        for (uint32_t i = 0; i < param_count; ++i) {
            slang::VariableLayoutReflection* param = reflection->getParameterByIndex(i);
            slang::TypeReflection::Kind const kind = param->getTypeLayout()->getKind();

            if (kind == slang::TypeReflection::Kind::ConstantBuffer ||
                kind == slang::TypeReflection::Kind::Resource ||
                kind == slang::TypeReflection::Kind::SamplerState) {

                result_bindings.names.emplace_back(param->getName());
                result_bindings.sets.emplace_back(param->getBindingSpace());
                result_bindings.bindings.emplace_back(param->getBindingIndex());
            }
        }

        return result_bindings;
    }

}  // namespace vanta::slang_generator
