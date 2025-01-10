#pragma once
#include "Model.hpp"
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

namespace MarianaEditor
{
    void DrawEditor(wgpu::RenderPassEncoder renderPass, Model* model);
    void InitGUI(GLFWwindow* window, wgpu::TextureFormat format);
    void ShutdownGUI();
};