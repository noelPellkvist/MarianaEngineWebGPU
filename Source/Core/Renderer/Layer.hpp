#pragma once
#include "Shader.hpp"
#include <vector>
#include "../Components/Mesh.hpp"
#include <entt/entt.hpp>

class RenderLayer
{
    public:
        RenderLayer(wgpu::TextureFormat targetFormat, std::string standardShader = "standard.wgsl", bool useDepthStencil = true);
        ~RenderLayer();

        void Render(std::vector<wgpu::TextureView>& targets, entt::registry& reg);
        std::vector<Shader>& GetShaders() { return shaders; };

    private:
        std::vector<Shader> shaders;
        wgpu::TextureFormat targetFormat;

        wgpu::RenderPassDepthStencilAttachment m_DepthStencilAttachment;
        wgpu::TextureView m_DepthTextureView;

        void CreateDepthStencil();
};