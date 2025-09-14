#include <moved_later/GUI.hpp>
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

GUI::GUI()
{}

GUI::~GUI()
{}

void GUI::InitGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    // ImGui_ImplGlfw_InitForOther(m_window, true);
    // ImGui_ImplWGPU_Init(m_device, 3, m_swapChainFormat, wgpu::TextureFormat::Depth24Plus);
}

void GUI::UpdateGUI(wgpu::RenderPassEncoder renderPass)
{

}

void GUI::KillGui()
{

}
