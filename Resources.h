#pragma once
#include "GlobalVaribles.hpp"
#include <webgpu/webgpu_cpp.h>
#include <filesystem>

class Resources 
{
    public:
    static wgpu::ShaderModule LoadShader(const std::filesystem::path& path);
};