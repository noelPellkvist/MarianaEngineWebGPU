#pragma once
#include <webgpu/webgpu_cpp.h>
#include <vector>

#include "GameObject.hpp"

class Renderpass
{
    public:
    Renderpass(wgpu::TextureView outputImage,
    wgpu::TextureView depthTextureView);
    ~Renderpass();

    void Draw(wgpu::CommandEncoder encoder, wgpu::RenderPipeline pipeline, GameObject* g);
    void Draw(wgpu::CommandEncoder encoder, wgpu::RenderPipeline pipeline, GameObject* g, wgpu::SurfaceTexture surface);

    private:
    wgpu::TextureView outputImage;
    wgpu::TextureView depthTextureView;
};