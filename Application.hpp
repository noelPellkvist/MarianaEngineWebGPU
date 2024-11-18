#pragma once
#include "GlobalVaribles.hpp"
#include "Mesh.hpp"
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
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
    wgpu::TextureView depthTextureView;

    wgpu::Buffer globalUBO;
    wgpu::PipelineLayout layout;
    wgpu::BindGroupLayout bindGroupLayout;
    wgpu::BindGroup bindGroup;

    struct UBO {
        glm::mat4x4 projectionMatrix;
        glm::mat4x4 viewMatrix;
        glm::mat4x4 modelMatrix;
        float color[4];
        float time;
        float _pad[3];
    };

    UBO ubo;
    

    void ConfigureSurface();
    void SetupWindow();
    void InitGraphics();
    void InitUniforms();
    void CreateRenderPipeline();
    void Render();
};