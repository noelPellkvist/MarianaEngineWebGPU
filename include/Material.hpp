#pragma once
#include <vector>
#include <webgpu/webgpu_cpp.h>
#include <string>

#include <Shader.hpp>
#include <Sampler.hpp>
#include <Texture.hpp>

class IMaterial
{
    public:
        IMaterial();
        ~IMaterial();

        void InitMaterial(Shader& shader, std::vector<Texture> textures);
        wgpu::BindGroup& GetTextureBindGroup() { return bindGroup; }
    
    private:
        std::vector<Texture> m_Textures;
        std::vector<wgpu::Sampler> samplers;
        wgpu::BindGroup bindGroup;

        void LoadSampler(int minFilter, int magFilter, WrapMode wrapS, WrapMode wrapT);
};

template<typename MaterialData>
class Material : public IMaterial
{
    private:
        
};