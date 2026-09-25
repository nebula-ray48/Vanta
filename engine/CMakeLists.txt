find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_library(vanta_engine STATIC
        # Render
        render/vulkan/renderer_vulkan.cpp
        render/include/rey_renderer.h
        render/vulkan/commands/command_recorder.h
        render/vulkan/pipeline/pipeline.h
        render/vulkan/pipeline/pipeline.cpp
        render/vulkan/core/vulkan_context.h
        render/vulkan/core/vulkan_context.cpp
        render/vulkan/render/swapchain_target.h
        render/vulkan/render/swapchain_target.cpp
        render/vulkan/resources/buffers/buffer.h
        render/vulkan/resources/buffers/buffer.cpp
        render/vulkan/resources/descriptors/descriptor.h
        render/vulkan/resources/descriptors/descriptor.cpp
        render/vulkan/commands/command_utils.h
        render/vulkan/commands/command_utils.cpp
        render/vulkan/render/vulkan_renderer.h
        render/vulkan/render/vulkan_renderer_core.cpp
        render/vulkan/render/vulkan_renderer_init.cpp
        render/vulkan/render/vulkan_renderer_draw.cpp
        render/vulkan/resources/images/texture.h
        render/vulkan/resources/images/texture.cpp
        render/vulkan/resources/buffers/vulkan_buffer_utils.h
        render/vulkan/resources/buffers/vulkan_buffer_utils.cpp
        render/vulkan/frame_graph/render_graph_types.h
        render/vulkan/frame_graph/graph_compiler.h
        render/vulkan/frame_graph/graph_compiler.cpp
        render/vulkan/frame_graph/graph_executor.h
        render/vulkan/frame_graph/graph_executor.cpp
        render/vulkan/frame_graph/graph_builder.h
        render/vulkan/frame_graph/graph_builder.cpp
        render/vulkan/render/vulkan_device.h
        render/vulkan/render/swapchain_manager.h
        render/vulkan/frame/frame_context.h
        render/vulkan/frame/frame_context.cpp
        render/vulkan/resources/resource_registry.h
        render/vulkan/resources/resource_registry.cpp
        
        # Techniques
        render/techniques/pbr/pbr_pipeline.hpp
        render/techniques/pbr/pbr_pipeline.cpp
        render/techniques/toon/toon_pipeline.hpp
        render/techniques/toon/toon_pipeline.cpp
        
        # Core
        core/include/engine_error.h
        core/include/render_types.h
        
        # World/Scene
        world/scene/camera.h
        world/scene/camera.cpp
        world/scene/mesh.h
        world/scene/mesh.cpp
        world/scene/transform.h
        
        # Assets
        assets/image_loader.hpp
        assets/image_loader.cpp
        assets/gltf_loader.h
        assets/gltf_loader.cpp
        
        # Schemas / Generated (Included from outside)
        ${GENERATED_INCLUDE_DIR}/render_command_generated.h

        # Shaders
        "${SHADER_DIR}/materials/pbr_only/pbr_main.slang"

        # Ext (Still in render/include/ext)
        render/include/ext/vk_mem_alloc.h
        render/include/ext/vma_implementation.cpp
        render/include/ext/glfw3.h
        render/include/ext/glfw_includeI_vulkan.cpp
        render/include/ext/stb_image.h
        render/include/ext/imgui_impl_glfw.h
        render/include/ext/imgui_impl_glfw.cpp
        render/include/ext/imgui_impl_vulkan.h
        render/include/ext/imgui_impl_vulkan.cpp
)

target_include_directories(vanta_engine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/render
        ${CMAKE_CURRENT_SOURCE_DIR}/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/render/include
        ${GENERATED_INCLUDE_DIR}
)

add_dependencies(vanta_engine generate_shaders)

target_link_libraries(vanta_engine PRIVATE
        Vulkan::Vulkan
)

target_link_libraries(vanta_engine PUBLIC
        Vulkan::Vulkan
        glfw
        glm::glm
        flatbuffers::flatbuffers
        fastgltf::fastgltf
        imgui::imgui
)

target_precompile_headers(vanta_engine PRIVATE
        <vulkan/vulkan.h>
        <expected>
        <vector>
        <string>
        <optional>
        <span>
        <variant>
        <array>
        <cstdint>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(vanta_engine PRIVATE
            -Wall -Wextra -Wpedantic
            -Wconversion
            -Wshadow
            -Werror=return-type
            -Wno-missing-field-initializers
            -Wno-nullability-extension
            -Wno-nullability-completeness
            -Wno-unused-parameter
    )
endif()
