Write-Host "Setting up Vanta Engine development environment for Windows..."

# Check if winget is available
if (Get-Command winget -ErrorAction SilentlyContinue) {
    Write-Host "Installing Vulkan SDK..."
    winget install KhronosGroup.VulkanSDK
    
    Write-Host "Installing CMake (if not present)..."
    winget install Kitware.CMake
} else {
    Write-Host "winget is not available. Please install the Vulkan SDK manually from https://vulkan.lunarg.com/"
    Write-Host "And install CMake from https://cmake.org/download/"
}

Write-Host "Windows setup complete! Please restart your terminal if Vulkan SDK was just installed."
