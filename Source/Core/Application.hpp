#pragma once

#include "GlobalVaribles.hpp"
#include "Pipeline.hpp"
#include "Model.hpp"

#include <glm.hpp>
#include <vector>
#include <GLFW/glfw3.h>



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
    Model* skybox;
    Pipeline* pipeLine;
    Pipeline* skyBoxPipeline;
    const char* name;
    wgpu::Surface surface;
    wgpu::TextureFormat format;

    GLFWwindow* window;
    UBO ubo;
   

    
    

    void ConfigureSurface();
    void SetupWindow();
    void InitGraphics();
    void InitUniforms();
    void InitSampler();
    void Render();

    
};