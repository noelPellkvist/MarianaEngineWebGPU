#pragma once
#include "Shader.hpp"
#include <vector>
#include "Mesh.hpp"

class RenderLayer
{
    public:
        RenderLayer(wgpu::TextureFormat targetFormat);
        ~RenderLayer();

        void Render(wgpu::SurfaceTexture& surfaceTexture);

    private:
        std::vector<Shader> shaders;
        wgpu::TextureFormat targetFormat;
        Mesh mesh;
};