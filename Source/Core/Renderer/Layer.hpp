#pragma once
#include "Shader.hpp"
#include <vector>

class RenderLayer
{
    public:
        RenderLayer(wgpu::TextureFormat targetFormat);
        ~RenderLayer();

        void Render(wgpu::SurfaceTexture& surfaceTexture);

    private:
        std::vector<Shader> shaders;
        wgpu::TextureFormat targetFormat;
};