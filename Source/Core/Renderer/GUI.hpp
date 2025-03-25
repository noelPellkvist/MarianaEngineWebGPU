#pragma once
#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>

class GUI
{
    public:
        GUI(GLFWwindow* window, wgpu::TextureFormat format);
        ~GUI();

        void DrawGUI(wgpu::RenderPassEncoder renderPass);
};