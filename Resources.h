#pragma once
#include "GlobalVaribles.hpp"
#include "Mesh.hpp"
#include <webgpu/webgpu_cpp.h>
#include <filesystem>

class Resources 
{
    private:
    static std::string LoadString(const std::string& path);
    public:
    static wgpu::TextureView LoadTexture(const std::string& name);
    static wgpu::TextureView CreateEmptyTexture(int width, int height, wgpu::TextureFormat format=wgpu::TextureFormat::BGRA8Unorm);
    static wgpu::ShaderModule LoadShader(const std::string& path);
    static Mesh LoadOBJMesh(const std::string& path);
};