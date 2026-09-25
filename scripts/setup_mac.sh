#!/bin/bash
set -e

echo "Setting up Vanta Engine development environment for macOS..."

# Install Homebrew if not installed
if ! command -v brew &> /dev/null; then
    echo "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# Install required dependencies
echo "Installing Vulkan SDK and tools..."
brew install vulkan-headers vulkan-loader moltenvk glslang spirv-tools

# Filament cmgen for IBL generation
if ! command -v cmgen &> /dev/null; then
    echo "cmgen (Filament) is required for IBL generation but not found."
    echo "You may need to download Filament releases from: https://github.com/google/filament/releases"
    echo "Or run: brew install filament (if available)"
fi

echo "macOS setup complete! You can now configure CMake."
