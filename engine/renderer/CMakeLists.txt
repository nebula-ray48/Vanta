find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)

add_library(vanta_renderer STATIC
        vulkan/renderer_vulkan.cpp
        include/rey_renderer.h
        include/generated/render_command_generated.h
        vulkan/commands/command_recorder.h
        include/engine_error.h
        vulkan/pipeline/pipeline.h
        vulkan/core/vulkan_context.h
        vulkan/render/swapchain_target.h
        vulkan/render/swapchain_target.cpp
        vulkan/resources/buffers/buffer.h
        vulkan/resources/buffers/buffer.cpp
        vulkan/pipeline/pipeline.cpp
        scene/camera.h
        scene/mesh.h
        scene/transform.h
        include/render_types.h
        include/ext/vk_mem_alloc.h
        include/ext/vma_implementation.cpp
        vulkan/resources/descriptors/descriptor.h
        vulkan/resources/descriptors/descriptor.cpp
        vulkan/commands/command_utils.h
        vulkan/commands/command_utils.cpp
        vulkan/render/vulkan_renderer.h
        include/ext/glfw3.h
        include/ext/glfw_includeI_vulkan.cpp
        "${SHADER_DIR}/main.slang"
        vulkan/core/vulkan_context.cpp
        techniques/pbr/pbr_pipeline.hpp
        techniques/pbr/pbr_pipeline.cpp
        techniques/toon/toon_pipeline.hpp
        techniques/toon/toon_pipeline.cpp
        scene/mesh.cpp
        assets/image_loader.hpp
        assets/image_loader.cpp
        vulkan/resources/images/texture.h
        vulkan/resources/images/texture.cpp
        include/ext/stb_image.h
        vulkan/render/vulkan_renderer_core.cpp
        vulkan/render/vulkan_renderer_init.cpp
        vulkan/render/vulkan_renderer_draw.cpp
        scene/gltf_loader.cpp
        scene/gltf_loader.h
        vulkan/resources/buffers/vulkan_buffer_utils.h
        vulkan/resources/buffers/vulkan_buffer_utils.cpp
        scene/camera.cpp
        vulkan/frame_graph/render_graph_types.h
        vulkan/frame_graph/graph_compiler.h
        vulkan/frame_graph/graph_compiler.cpp
        vulkan/frame_graph/graph_executor.h
        vulkan/frame_graph/graph_executor.cpp
        vulkan/frame_graph/graph_builder.h
        vulkan/frame_graph/graph_builder.cpp
        vulkan/render/vulkan_device.h
        vulkan/render/swapchain_manager.h
        vulkan/frame/frame_context.h
        vulkan/frame/frame_context.cpp
)

target_include_directories(vanta_renderer PUBLIC
        .
        include
)

add_dependencies(vanta_renderer generate_shaders)

target_link_libraries(vanta_renderer PRIVATE
        Vulkan::Vulkan
)

target_link_libraries(vanta_renderer PUBLIC
        Vulkan::Vulkan
        glfw
        glm::glm
        flatbuffers::flatbuffers
        fastgltf::fastgltf
)

target_precompile_headers(vanta_renderer PRIVATE
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
    target_compile_options(vanta_renderer PRIVATE
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
