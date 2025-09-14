#pragma once
#include <webgpu/webgpu_cpp.h>

class GUI
{
    public:
        GUI();
        ~GUI();

        void InitGui();
        void UpdateGUI(wgpu::RenderPassEncoder renderPass);
        void KillGui();
};