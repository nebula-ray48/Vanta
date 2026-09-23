//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "descriptor.h"

#include "vulkan/core/vulkan_context.h"

namespace vanta::render {

    std::expected<VkDescriptorSetLayout, EngineError> create_global_ubo_layout(VkDevice device) {
        constexpr VkDescriptorSetLayoutBinding ubo_layout_binding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        const VkDescriptorSetLayoutCreateInfo layout_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &ubo_layout_binding,
        };

        VkDescriptorSetLayout layout{};
        if (vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &layout) != VK_SUCCESS) {
            return std::unexpected(EngineError{LegacyError{"Global UBO Descriptor Set Layout の作成失敗"}});
        }

        return layout;
    }

    std::expected<VkDescriptorPool, EngineError> create_descriptor_pool(const VulkanContext& context) {
        constexpr VkDescriptorPoolSize pool_size{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        };

        const VkDescriptorPoolCreateInfo pool_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &pool_size,
        };

        VkDescriptorPool pool{};
        if (vkCreateDescriptorPool(context.device, &pool_info, nullptr, &pool) != VK_SUCCESS) {
            return std::unexpected(EngineError{LegacyError{"Descriptor Pool の作成失敗"}});
        }

        return pool;
    }

    std::expected<VkDescriptorSet, EngineError> create_descriptor_set(
        const VulkanContext& context,
        VkDescriptorPool pool,
        VkDescriptorSetLayout layout,
        VkBuffer ubo_buffer)
    {
        const VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout,
        };

        VkDescriptorSet descriptor_set{};
        if (vkAllocateDescriptorSets(context.device, &alloc_info, &descriptor_set) != VK_SUCCESS) {
            return std::unexpected(EngineError{LegacyError{"Descriptor Set の確保失敗"}});
        }

        const VkDescriptorBufferInfo buffer_info{
            .buffer = ubo_buffer,
            .offset = 0,
            .range = sizeof(GlobalUbo),
        };

        const VkWriteDescriptorSet descriptor_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info,
        };

        vkUpdateDescriptorSets(context.device, 1, &descriptor_write, 0, nullptr);

        return descriptor_set;
    }

    std::expected<VkDescriptorSetLayout, EngineError> BindlessDescriptorLayout::create(VkDevice device) noexcept {

    constexpr uint32_t MAX_BINDLESS_RESOURCES = 100000;

    std::array bindings = {
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = MAX_BINDLESS_RESOURCES,
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .pImmutableSamplers = nullptr
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        }
    };

    // 各BindingにBindless用のフラグを付与する
        std::array<VkDescriptorBindingFlags, 2> binding_flags = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<uint32_t>(binding_flags.size()),
            .pBindingFlags = binding_flags.data(),
        };

        VkDescriptorSetLayoutCreateInfo layout_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &flags_info,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        };

        VkDescriptorSetLayout layout;
        if (vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &layout) != VK_SUCCESS) {
            return std::unexpected(EngineError{LegacyError{"Bindless DescriptorLayoutの作成に失敗しました"}});
        }

        return layout;
    }

    std::expected<VkDescriptorPool, EngineError> BindlessDescriptorManager::create_pool(VkDevice device) noexcept {
    // Layoutで定義したのと同じ最大数を指定する
    constexpr uint32_t MAX_BINDLESS_RESOURCES = 100000;

        std::array pool_sizes = {
            VkDescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // 画像用
                .descriptorCount = MAX_BINDLESS_RESOURCES
            },
            VkDescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,       // サンプラー用 (これが欠けていました)
                .descriptorCount = 1
            }
        };

        VkDescriptorPoolCreateInfo const pool_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets = 1,
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes = pool_sizes.data()
        };

    VkDescriptorPool pool{};
    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &pool) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"Bindless DescriptorPoolの作成に失敗しました"}});
    }

    return pool;
}

void BindlessDescriptorManager::destroy_pool(VkDevice device, VkDescriptorPool pool) noexcept {
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
}

std::expected<VkDescriptorSet, EngineError> BindlessDescriptorManager::allocate_set(
    VkDevice device,
    VkDescriptorPool pool,
    VkDescriptorSetLayout layout) noexcept
{
    VkDescriptorSetAllocateInfo const alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };

    VkDescriptorSet set{};
    if (vkAllocateDescriptorSets(device, &alloc_info, &set) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"Bindless DescriptorSetの確保に失敗しました"}});
    }

    return set;
}

void BindlessDescriptorLayout::destroy(VkDevice device, VkDescriptorSetLayout layout) noexcept {
    if (layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, layout, nullptr);
    }
}

}  // namespace vanta::render

