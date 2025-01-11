#pragma once

#include "GlobalVaribles.hpp"
#include "Renderpass.hpp"
#include "GameObject.hpp"
#include <glm.hpp>
#include <vector>
#include <GLFW/glfw3.h>

#include "Model.hpp"

class Application
{
    public:
    Application();
    ~Application();

    void Start();

    void WindowResized();

    wgpu::Buffer globalUBO;

    wgpu::Sampler sampler;

    private:
    Model* model;
    const char* name;
    int kWidth, kHeight;
    wgpu::Surface surface;
    wgpu::TextureFormat format;
    wgpu::RenderPipeline pipeline;  
    wgpu::TextureView depthTextureView;
    wgpu::TextureView tmpRender;

    std::vector<wgpu::TextureView> loadedTextures;

    wgpu::TextureView banana;

    
    wgpu::PipelineLayout layout;
    
    wgpu::BindGroup bindGroup;

    GLFWwindow* window;

    
    // GameObject gameObject;
    // GameObject kub;
    Renderpass* finalRenderPass;
    Renderpass* firstRenderpass;
    UBO ubo;
   

    
    

    void ConfigureSurface();
    void SetupWindow();
    void InitGraphics();
    void InitDepthTexture();
    void InitUniforms();
    void InitSampler();
    void CreateRenderPipeline();
    void Render();

    
};