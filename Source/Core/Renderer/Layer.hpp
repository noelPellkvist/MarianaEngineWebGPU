#pragma once
#include "Shader.hpp"
#include <vector>
#include "Mesh.hpp"

class RenderLayer
{
    public:
        RenderLayer(wgpu::TextureFormat targetFormat, std::string standardShader = "standard.wgsl");
        ~RenderLayer();

        void Render(wgpu::SurfaceTexture& surfaceTexture, const Mesh& mesh);
        std::vector<Shader>& GetShaders() { return shaders; };

    private:
        std::vector<Shader> shaders;
        wgpu::TextureFormat targetFormat;
};