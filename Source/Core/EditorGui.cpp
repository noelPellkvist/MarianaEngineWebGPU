#pragma once

#include "EditorGui.hpp"

void DrawEditor(wgpu::RenderPassEncoder renderPass)
{

}

void InitGUI()
{

}

void ShutdownGUI()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
}