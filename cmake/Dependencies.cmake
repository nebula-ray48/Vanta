include(FetchContent)

find_package(Vulkan REQUIRED)

# ------------------------------------------------------------------------------
# 1. Slang SDK (Auto-download based on OS)
# ------------------------------------------------------------------------------
set(SLANG_VERSION "2024.1.7")
if(WIN32)
    set(SLANG_OS "windows")
    set(SLANG_ARCH "x86_64")
    set(SLANG_BIN_DIR "windows-x64")
elseif(APPLE)
    set(SLANG_OS "macos")
    # Determine arch
    execute_process(COMMAND uname -m OUTPUT_VARIABLE MAC_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(MAC_ARCH STREQUAL "arm64")
        set(SLANG_ARCH "aarch64")
        set(SLANG_BIN_DIR "macosx-aarch64")
    else()
        set(SLANG_ARCH "x86_64")
        set(SLANG_BIN_DIR "macosx-x86_64")
    endif()
else()
    set(SLANG_OS "linux")
    set(SLANG_ARCH "x86_64")
    set(SLANG_BIN_DIR "linux-x86_64")
endif()

set(SLANG_ZIP_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_OS}-${SLANG_ARCH}.zip")
set(SLANG_SDK_DIR "${CMAKE_BINARY_DIR}/slang_sdk")

if(NOT EXISTS "${SLANG_SDK_DIR}/slang.h")
    message(STATUS "Downloading Slang SDK ${SLANG_VERSION} for ${SLANG_OS}-${SLANG_ARCH}...")
    file(DOWNLOAD ${SLANG_ZIP_URL} "${CMAKE_BINARY_DIR}/slang.zip" SHOW_PROGRESS)
    file(ARCHIVE_EXTRACT INPUT "${CMAKE_BINARY_DIR}/slang.zip" DESTINATION "${SLANG_SDK_DIR}")
    file(REMOVE "${CMAKE_BINARY_DIR}/slang.zip")
endif()

if(WIN32)
    set(SLANGC_EXECUTABLE "${SLANG_SDK_DIR}/bin/${SLANG_BIN_DIR}/release/slangc.exe" CACHE PATH "Path to slangc")
else()
    set(SLANGC_EXECUTABLE "${SLANG_SDK_DIR}/bin/${SLANG_BIN_DIR}/release/slangc" CACHE PATH "Path to slangc")
    execute_process(COMMAND chmod +x ${SLANGC_EXECUTABLE})
endif()

# Create an imported target for slang to link against
if(NOT TARGET slang::slang)
    add_library(slang::slang SHARED IMPORTED GLOBAL)
    if(WIN32)
        set_target_properties(slang::slang PROPERTIES
            IMPORTED_LOCATION "${SLANG_SDK_DIR}/bin/${SLANG_BIN_DIR}/release/slang.dll"
            IMPORTED_IMPLIB "${SLANG_SDK_DIR}/lib/slang.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${SLANG_SDK_DIR}"
        )
    elseif(APPLE)
        set_target_properties(slang::slang PROPERTIES
            IMPORTED_LOCATION "${SLANG_SDK_DIR}/bin/${SLANG_BIN_DIR}/release/libslang.dylib"
            INTERFACE_INCLUDE_DIRECTORIES "${SLANG_SDK_DIR}"
        )
    else()
        set_target_properties(slang::slang PROPERTIES
            IMPORTED_LOCATION "${SLANG_SDK_DIR}/bin/${SLANG_BIN_DIR}/release/libslang.so"
            INTERFACE_INCLUDE_DIRECTORIES "${SLANG_SDK_DIR}"
        )
    endif()
endif()


# ------------------------------------------------------------------------------
# 2. Third-party Libraries (FetchContent)
# ------------------------------------------------------------------------------

# GLFW
find_package(glfw3 CONFIG QUIET)
if (NOT glfw3_FOUND AND NOT TARGET glfw)
    message(STATUS "Fetching GLFW...")
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# GLM
find_package(glm CONFIG QUIET)
if (NOT glm_FOUND AND NOT TARGET glm::glm)
    message(STATUS "Fetching GLM...")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )
    set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glm)
endif()

# FlatBuffers
find_package(flatbuffers CONFIG QUIET)
if (NOT flatbuffers_FOUND AND NOT TARGET flatbuffers::flatbuffers)
    message(STATUS "Fetching FlatBuffers...")
    FetchContent_Declare(
        flatbuffers
        GIT_REPOSITORY https://github.com/google/flatbuffers.git
        GIT_TAG v24.3.25
    )
    set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(FLATBUFFERS_BUILD_FLATC ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(flatbuffers)
endif()

# fastgltf
message(STATUS "Fetching fastgltf...")
FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG v0.7.2
)
set(FASTGLTF_ENABLE_DEPRECATED_CODE OFF CACHE BOOL "" FORCE)
set(FASTGLTF_USE_SYSTEM_SIMDJSON OFF CACHE BOOL "" FORCE)
set(FASTGLTF_DOWNLOAD_SIMDJSON ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fastgltf)

# ImGui
find_package(imgui CONFIG QUIET)
if (NOT imgui_FOUND AND NOT TARGET imgui::imgui)
    message(STATUS "Fetching ImGui...")
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG master
    )
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
        add_library(imgui STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        )
        target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
        target_link_libraries(imgui PRIVATE glfw Vulkan::Vulkan)
        add_library(imgui::imgui ALIAS imgui)
    endif()
endif()

# GTest
find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND AND NOT TARGET GTest::gtest)
    message(STATUS "Fetching GTest...")
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()
