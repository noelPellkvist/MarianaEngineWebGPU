#pragma once
#include "GlobalVaribles.hpp"
#include <webgpu/webgpu_cpp.h>
#include <filesystem>
#include "Application.hpp"


class Resources 
{
    private:
    static std::string LoadRawString(const std::string& path);
    public:
    static wgpu::TextureView LoadTexture(const std::string& name);
    static wgpu::TextureView LoadCubemap(const std::string& name);
    static wgpu::TextureView GetEmptyTexture();
    static std::vector<wgpu::TextureView> LoadTextures(/*tinygltf::Image& img*/);
    static wgpu::TextureView CreateEmptyTexture(int width, int height, wgpu::TextureFormat format=wgpu::TextureFormat::BGRA8Unorm);
    static wgpu::ShaderModule LoadShader(const std::string& path);
    //static Mesh LoadOBJMesh(const std::string& path);
};