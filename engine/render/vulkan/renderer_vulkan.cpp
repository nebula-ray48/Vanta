//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "rey_renderer.h"
#include "generated/render_command_generated.h"

#include <iostream>

void execute_render_command(const uint8_t* ptr, size_t len) {
    if (ptr == nullptr || len == 0) {
        std::cerr << "[C++ Error] Empty render command buffer." << '\n';
        return;
    }

    flatbuffers::Verifier verifier(ptr, len);
    if (!vanta::render::VerifyRenderCommandBuffer(verifier)) {
        std::cerr << "[C++ Error] Invalid FlatBuffer data!" << '\n';
        return;
    }

    const auto* command = vanta::render::GetRenderCommand(ptr);
    if (const auto* color = command->clear_color()) {
        std::cout << "[C++23] RenderCommand Received! ClearColor( R: "
                  << color->r() << ", G: " << color->g()
                  << ", B: " << color->b() << ", A: " << color->a()
                  << " )" << '\n';
    }
}

