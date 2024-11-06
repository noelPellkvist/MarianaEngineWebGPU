#pragma once
#include "GlobalVaribles.hpp"
#include "Mesh.hpp"

#include <vector>

class Application
{
    public:
    Application();
    ~Application();

    private:
    const char* name;
    uint32_t kWidth, kHeight;
    wgpu::Surface surface;
    wgpu::TextureFormat format;
    wgpu::RenderPipeline pipeline;

    

    void ConfigureSurface();
    void InitGraphics();
    void CreateRenderPipeline();
    void Render();
};