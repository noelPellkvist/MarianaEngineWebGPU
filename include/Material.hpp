#pragma once
#include <vector>
#include <webgpu/webgpu_cpp.h>
#include <string>

#include <Shader.hpp>

class Material
{
    public:
        Material();
        ~Material();

        void InitMaterial(Shader& shader, std::vector<std::string> textureNames);
        wgpu::BindGroup& GetTextureBindGroup() { return bindGroup; }

    private:
        std::vector<wgpu::Texture> textures;
        std::vector<wgpu::TextureView> textureViews;
        wgpu::BindGroup bindGroup;

        void LoadTexture(std::string texturePath);
};