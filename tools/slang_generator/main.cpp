//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include <slang-com-ptr.h>
#include <slang.h>

#include <fstream>
#include <iostream>
#include <vector>
#include <string>

#include "generator_core.h"

int main(int argc, char** argv) {

    if (argc < 3) {
        std::cerr << "[Error] Usage: rey_slang_generator <input.slang> <output.h> [-I <include_path> ...]\n";
        return 1;
    }

    const char* inputPath = nullptr;
    const char* outputPath = nullptr;
    std::vector<const char*> searchPaths;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-I" && i + 1 < argc) {
            searchPaths.push_back(argv[++i]);
        } else if (!inputPath) {
            inputPath = argv[i];
        } else if (!outputPath) {
            outputPath = argv[i];
        }
    }

    if (!inputPath || !outputPath) {
        std::cerr << "[Error] Missing input or output paths.\n";
        return 1;
    }

    Slang::ComPtr<slang::IGlobalSession> globalSession;
    if (SLANG_FAILED(slang::createGlobalSession(globalSession.writeRef()))) {
        std::cerr << "[Error] Failed to create Slang global session.\n";
        return 1;
    }

    Slang::ComPtr<slang::ICompileRequest> request;
    globalSession->createCompileRequest(request.writeRef());

    int translationUnitIndex = request->addTranslationUnit(SLANG_SOURCE_LANGUAGE_SLANG, nullptr);
    request->addTranslationUnitSourceFile(translationUnitIndex, inputPath);

    for (const char* path : searchPaths) {
        request->addSearchPath(path);
    }

    int targetIndex = request->addCodeGenTarget(SLANG_SPIRV);
    request->setTargetProfile(targetIndex, globalSession->findProfile("sm_6_0"));

    const SlangResult compileRes = request->compile();
    if (SLANG_FAILED(compileRes)) {
        std::cerr << "[Slang Compile Error]\n" << request->getDiagnosticOutput() << "\n";
        return 1;
    }

    auto* reflection = reinterpret_cast<slang::ShaderReflection*>(request->getReflection());
    if (reflection == nullptr) {
        std::cerr << "[Error] Failed to get shader reflection data.\n";
        return 1;
    }

    auto extracted_result = vanta::slang_generator::extract_resource_bindings(reflection);
    if (!extracted_result.has_value()) {
        std::cerr << "[Error] Extraction Failed: " << extracted_result.error() << "\n";
        return 1;
    }

    auto generated_result = vanta::slang_generator::generate_cpp_bindings(extracted_result.value());
    if (!generated_result.has_value()) {
        std::cerr << "[Error] Generation Failed: " << generated_result.error() << "\n";
        return 1;
    }

    std::ofstream outFile(outputPath);
    if (!outFile.is_open()) {
        std::cerr << "[Error] Failed to open output file: " << outputPath << "\n";
        return 1;
    }

    outFile << generated_result.value();
    outFile.close();

    std::cout << "[Success] Generated reflection header: " << outputPath << "\n";
    return 0;
}

