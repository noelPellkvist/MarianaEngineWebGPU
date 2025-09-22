#pragma once
#include <webgpu/webgpu_cpp.h>
#include <Window.hpp>

class GUI
{
    public:
        GUI();
        ~GUI();

        void InitGui(Window& window);
        void PreUpdateGUI();
        void PostUpdateGUI(wgpu::RenderPassEncoder renderPass);
        void KillGui();
};