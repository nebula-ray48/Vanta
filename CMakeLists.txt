cmake_minimum_required(VERSION 3.20)
project(vanta LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)


add_subdirectory(tools/slang_generator)
add_subdirectory(schemas)

find_program(SLANGC_EXECUTABLE slangc REQUIRED)

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
set(GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/schemas/generated")

file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR})

set(SLANG_SOURCES
        "${SHADER_DIR}/main.slang"
        "${SHADER_DIR}/skybox.slang"
        "${SHADER_DIR}/shadow.slang"
        "${SHADER_DIR}/ssao.slang"
        "${SHADER_DIR}/aces_tonemap.slang"
)

set(SPV_VERT "${SHADER_DIR}/main_vert.spv")
set(SPV_FRAG "${SHADER_DIR}/main_frag.spv")
set(SPV_SKYBOX_VERT "${SHADER_DIR}/skybox_vert.spv")
set(SPV_SKYBOX_FRAG "${SHADER_DIR}/skybox_frag.spv")
set(SPV_SHADOW_VERT "${SHADER_DIR}/shadow_vert.spv")
set(SPV_SSAO_COMP "${SHADER_DIR}/ssao_comp.spv")
set(SPV_ACES_TONEMAP_FRAG "${SHADER_DIR}/aces_tonemap_frag.spv")
set(CPP_REFLECTION_HEADER "${GENERATED_INCLUDE_DIR}/shader_reflection.h")

add_custom_command(
        OUTPUT ${SPV_VERT} ${SPV_FRAG} ${SPV_SKYBOX_VERT} ${SPV_SKYBOX_FRAG} ${SPV_SHADOW_VERT} ${SPV_SSAO_COMP} ${SPV_ACES_TONEMAP_FRAG} ${CPP_REFLECTION_HEADER}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/main.slang" -target spirv -entry vertexMain -stage vertex -o ${SPV_VERT}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/main.slang" -target spirv -entry fragmentMain -stage fragment -o ${SPV_FRAG}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/skybox.slang" -target spirv -entry skyboxVertexMain -stage vertex -o ${SPV_SKYBOX_VERT}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/skybox.slang" -target spirv -entry skyboxFragmentMain -stage fragment -o ${SPV_SKYBOX_FRAG}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/shadow.slang" -target spirv -entry mainVert -stage vertex -o ${SPV_SHADOW_VERT}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/ssao.slang" -target spirv -entry main -stage compute -o ${SPV_SSAO_COMP}
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_DIR}/aces_tonemap.slang" -target spirv -entry main -stage fragment -o ${SPV_ACES_TONEMAP_FRAG}
        COMMAND rey_slang_generator "${SHADER_DIR}/main.slang" "${CPP_REFLECTION_HEADER}"
        DEPENDS rey_slang_generator ${SLANG_SOURCES}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:vanta_engine_tests>/assets"
        COMMENT "Copying assets directory to test output directory..."
)

add_custom_target(generate_shaders ALL
        DEPENDS ${SPV_VERT} ${SPV_FRAG} ${SPV_SKYBOX_VERT} ${SPV_SKYBOX_FRAG} ${SPV_SHADOW_VERT} ${SPV_SSAO_COMP} ${SPV_ACES_TONEMAP_FRAG} ${CPP_REFLECTION_HEADER}
)


enable_testing()
add_subdirectory(engine)
add_subdirectory(tests)
