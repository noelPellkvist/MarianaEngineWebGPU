#pragma once
#include "Shader.hpp"
#include <vector>
#include "../Components/Mesh.hpp"
#include <entt/entt.hpp>

class RenderLayer
{
    public:
        RenderLayer(wgpu::TextureFormat targetFormat, std::string standardShader = "standard.wgsl");
        ~RenderLayer();

        void Render(wgpu::SurfaceTexture& surfaceTexture, entt::registry& reg);
        std::vector<Shader>& GetShaders() { return shaders; };

    private:
        std::vector<Shader> shaders;
        wgpu::TextureFormat targetFormat;
};