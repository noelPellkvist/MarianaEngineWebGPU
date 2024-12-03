#pragma once

#include "GlobalVaribles.hpp"
#include "Renderpass.hpp"
#include "GameObject.hpp"
#include <glm.hpp>
#include <vector>
#include <GLFW/glfw3.h>

class Application
{
    public:
    Application();
    ~Application();

    private:
    const char* name;
    int kWidth, kHeight;
    wgpu::Surface surface;
    wgpu::TextureFormat format;
    wgpu::RenderPipeline pipeline;  
    wgpu::TextureView depthTextureView;
    wgpu::TextureView tmpRender;

    std::vector<wgpu::TextureView> loadedTextures;

    wgpu::TextureView banana;

    wgpu::Buffer globalUBO;
    wgpu::PipelineLayout layout;
    wgpu::BindGroupLayout bindGroupLayout;
    wgpu::BindGroup bindGroup;

    GLFWwindow* window;

    struct UBO {
        glm::mat4x4 projectionMatrix;
        glm::mat4x4 viewMatrix;
        glm::mat4x4 modelMatrix;
        float color[4];
        float time;
        float _pad[3];
    };
    GameObject* gameObject;
    GameObject kub;
    Renderpass* finalRenderPass;
    Renderpass* firstRenderpass;
    UBO ubo;
    wgpu::Sampler sampler;

    
    

    void ConfigureSurface();
    void SetupWindow();
    void InitGraphics();
    void InitDepthTexture();
    void InitUniforms();
    void InitSampler();
    void CreateRenderPipeline();
    void InitGUI();
    void UpdateGUI(wgpu::RenderPassEncoder renderPass);
    void Render();

    void WindowResized();
};