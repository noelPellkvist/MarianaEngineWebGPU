#pragma once
#include "GlobalVaribles.hpp"
#include <webgpu/webgpu_cpp.h>
#include <filesystem>

class Resources 
{
    private:
    static std::string LoadString(const std::string& path);
    public:
    static wgpu::ShaderModule LoadShader(const std::string& path);
};