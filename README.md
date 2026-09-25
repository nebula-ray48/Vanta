# Vanta Engine

Vanta is a high-performance, modern Vulkan-based 3D rendering engine built with C++23, featuring physical-based rendering (PBR), Slang shader reflection, indirect draw driven rendering pipelines, and seamless cross-platform support (Windows, macOS, Linux).

---

## 🚀 Quick Start / Build Instructions

All third-party C++ dependencies (GLFW, GLM, fastgltf, FlatBuffers, ImGui, GoogleTest, Slang SDK) are automatically fetched via CMake FetchContent. You only need the **Vulkan SDK** and a C++23-compatible compiler.

### 1. Prerequisites & Environment Setup

Run the setup script for your OS to install Vulkan SDK and required tools:

#### **Windows (PowerShell as Administrator)**
```powershell
.\scripts\setup_windows.ps1
```
*(Or install the Vulkan SDK manually from [vulkan.lunarg.com](https://vulkan.lunarg.com/) and Visual Studio 2022 / C++ Build Tools)*

#### **macOS**
```bash
chmod +x ./scripts/setup_mac.sh
./scripts/setup_mac.sh
```

#### **Linux (Ubuntu / Debian)**
```bash
chmod +x ./scripts/setup_linux.sh
./scripts/setup_linux.sh
```

---

### 2. Configure & Build

Clone the repository and build using standard CMake commands:

```bash
# 1. Clone repository
git clone https://github.com/sakakibarayuto/Vanta.git
cd Vanta

# 2. Configure CMake (fetches dependencies automatically)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 3. Build all targets
cmake --build build --config Debug -j
```

---

### 3. Run

Run the engine test & viewer executable:

#### **macOS / Linux**
```bash
./build/tests/vanta_engine_tests
```

#### **Windows**
```powershell
.\build\tests\Debug\vanta_engine_tests.exe
```

---

## 🛠 Features & Tech Stack

- **C++23 Modern Architecture**
- **Vulkan API** with dynamic rendering & modern synchronization
- **Slang Shader System** with automatic C++ reflection code generation
- **glTF 2.0 Asset Loading** via fastgltf (PBR textures, materials, meshes)
- **Zero-Config Build**: CMake `FetchContent` managed dependencies
- **Filament-compatible IBL** (Image-Based Lighting) pipeline
- **Dear ImGui** integrated debug interface
