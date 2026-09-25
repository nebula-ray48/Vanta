#!/bin/bash
set -e

echo "Setting up Vanta Engine development environment for Linux (Ubuntu/Debian)..."

# Update package list
sudo apt update

# Install build essentials and CMake
sudo apt install -y build-essential cmake git curl unzip zip tar

# Install Vulkan SDK components
sudo apt install -y libvulkan-dev vulkan-validationlayers-dev glslang-tools spirv-tools

# Install X11 and Wayland dependencies for GLFW
sudo apt install -y xorg-dev libwayland-dev libxkbcommon-dev wayland-protocols

echo "Linux setup complete! You can now configure CMake."
