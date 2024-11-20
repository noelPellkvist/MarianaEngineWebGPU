#pragma once
#include <webgpu/webgpu_cpp.h>
#include <vector>

class Renderpass
{
    public:
    Renderpass();
    ~Renderpass();

    private:
    bool ToRender = true;
    wgpu::TextureView outputImage;
    wgpu::TextureView depthTextureView;
    std::vector<wgpu::TextureView> inputImages;
}