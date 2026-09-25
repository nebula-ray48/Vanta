cmake_minimum_required(VERSION 3.20)
project(vanta LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/Dependencies.cmake)

add_subdirectory(tools/slang_generator)
add_subdirectory(schemas)

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
set(GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/schemas/generated")

file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR})

set(SLANG_SOURCES
        "${SHADER_DIR}/materials/pbr_only/pbr_main.slang"
        "${SHADER_DIR}/passes/environment/skybox.slang"
        "${SHADER_DIR}/passes/shadow/shadow.slang"
        "${SHADER_DIR}/passes/ssao/ssao.slang"
        "${SHADER_DIR}/passes/prepass/depth_normal.slang"
        "${SHADER_DIR}/postprocess/tonemap.slang"
        "${SHADER_DIR}/postprocess/bloom_extract.slang"
        "${SHADER_DIR}/postprocess/bloom_blur.slang"
        "${SHADER_DIR}/common/common_types.slang"
        "${SHADER_DIR}/common/common_bindings.slang"
        "${SHADER_DIR}/common/common_lighting.slang"
        "${SHADER_DIR}/common/common_utils.slang"
)

set(SPV_VERT "${SHADER_DIR}/main_vert.spv")
set(SPV_FRAG "${SHADER_DIR}/main_frag.spv")
set(SPV_SKYBOX_VERT "${SHADER_DIR}/skybox_vert.spv")
set(SPV_SKYBOX_FRAG "${SHADER_DIR}/skybox_frag.spv")
set(SPV_SHADOW_VERT "${SHADER_DIR}/shadow_vert.spv")
set(SPV_SSAO_COMP "${SHADER_DIR}/ssao_comp.spv")
set(SPV_DEPTH_NORMAL_VERT "${SHADER_DIR}/depth_normal_vert.spv")
set(SPV_DEPTH_NORMAL_FRAG "${SHADER_DIR}/depth_normal_frag.spv")
set(SPV_TONEMAP_VERT "${SHADER_DIR}/tonemap_vert.spv")
set(SPV_TONEMAP_FRAG "${SHADER_DIR}/tonemap_frag.spv")
set(SPV_BLOOM_EXTRACT_VERT "${SHADER_DIR}/bloom_extract_vert.spv")
set(SPV_BLOOM_EXTRACT_FRAG "${SHADER_DIR}/bloom_extract_frag.spv")
set(SPV_BLOOM_BLUR_VERT "${SHADER_DIR}/bloom_blur_vert.spv")
set(SPV_BLOOM_BLUR_FRAG "${SHADER_DIR}/bloom_blur_frag.spv")
set(CPP_REFLECTION_HEADER "${GENERATED_INCLUDE_DIR}/shader_reflection.h")

add_custom_command(
        OUTPUT ${SPV_VERT} ${SPV_FRAG} ${SPV_SKYBOX_VERT} ${SPV_SKYBOX_FRAG} ${SPV_SHADOW_VERT} ${SPV_SSAO_COMP} ${SPV_DEPTH_NORMAL_VERT} ${SPV_DEPTH_NORMAL_FRAG} ${SPV_TONEMAP_VERT} ${SPV_TONEMAP_FRAG} ${SPV_BLOOM_EXTRACT_VERT} ${SPV_BLOOM_EXTRACT_FRAG} ${SPV_BLOOM_BLUR_VERT} ${SPV_BLOOM_BLUR_FRAG} ${CPP_REFLECTION_HEADER}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/materials/pbr_only/pbr_main.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry vertexMain -stage vertex -o ${SPV_VERT}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/materials/pbr_only/pbr_main.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry fragmentMain -stage fragment -o ${SPV_FRAG}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/passes/environment/skybox.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry skyboxVertexMain -stage vertex -o ${SPV_SKYBOX_VERT}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/passes/environment/skybox.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry skyboxFragmentMain -stage fragment -o ${SPV_SKYBOX_FRAG}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/passes/shadow/shadow.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry mainVert -stage vertex -o ${SPV_SHADOW_VERT}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/passes/ssao/ssao.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry main -stage compute -o ${SPV_SSAO_COMP}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/passes/prepass/depth_normal.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry vertexMain -stage vertex -o ${SPV_DEPTH_NORMAL_VERT}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/passes/prepass/depth_normal.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry fragmentMain -stage fragment -o ${SPV_DEPTH_NORMAL_FRAG}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/postprocess/tonemap.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry vs_main -stage vertex -o ${SPV_TONEMAP_VERT}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/postprocess/tonemap.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry fs_main -stage fragment -o ${SPV_TONEMAP_FRAG}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/postprocess/bloom_extract.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry vs_main -stage vertex -o ${SPV_BLOOM_EXTRACT_VERT}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/postprocess/bloom_extract.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry fs_main -stage fragment -o ${SPV_BLOOM_EXTRACT_FRAG}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/postprocess/bloom_blur.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry vs_main -stage vertex -o ${SPV_BLOOM_BLUR_VERT}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/postprocess/bloom_blur.slang" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common" -target spirv -entry fs_main -stage fragment -o ${SPV_BLOOM_BLUR_FRAG}
        COMMAND rey_slang_generator "${SHADER_DIR}/materials/pbr_only/pbr_main.slang" "${CPP_REFLECTION_HEADER}" -I "${SHADER_DIR}" -I "${SHADER_DIR}/common"
        DEPENDS rey_slang_generator ${SLANG_SOURCES}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:vanta_engine_tests>/assets"
        COMMENT "Copying assets directory to test output directory..."
)

add_custom_target(generate_shaders ALL
        DEPENDS ${SPV_VERT} ${SPV_FRAG} ${SPV_SKYBOX_VERT} ${SPV_SKYBOX_FRAG} ${SPV_SHADOW_VERT} ${SPV_SSAO_COMP} ${SPV_DEPTH_NORMAL_VERT} ${SPV_DEPTH_NORMAL_FRAG} ${SPV_TONEMAP_VERT} ${SPV_TONEMAP_FRAG} ${SPV_BLOOM_EXTRACT_VERT} ${SPV_BLOOM_EXTRACT_FRAG} ${SPV_BLOOM_BLUR_VERT} ${SPV_BLOOM_BLUR_FRAG} ${CPP_REFLECTION_HEADER}
)


enable_testing()
add_subdirectory(engine)
add_subdirectory(tests)
