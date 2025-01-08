#pragma once

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <webgpu/webgpu_cpp.h>

namespace MarianaEditor
{
    void DrawEditor(wgpu::RenderPassEncoder renderPass);
    void InitGUI();
    void ShutdownGUI();
};