#include <moved_later/GUI.hpp>
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include <Init.hpp>


GUI::GUI()
{}

GUI::~GUI()
{}

void GUI::InitGui(Window& window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOther(window.GetWindow(), true);

    ImGui_ImplWGPU_InitInfo info = {};
    info.Device = device.Get();
    info.NumFramesInFlight = 3;
    info.RenderTargetFormat = static_cast<WGPUTextureFormat>(windowFormat);
    info.DepthStencilFormat = WGPUTextureFormat_Depth24Plus;
    info.PipelineMultisampleState.count = 4;
    if(ImGui_ImplWGPU_Init(&info))
    {
    }
}

void GUI::UpdateGUI(wgpu::RenderPassEncoder renderPass)
{
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, NULL, ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::Begin("Hello, world!"); 
    ImGui::Text("This is some useful text.");         
    ImGui::End();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass.Get());
}

void GUI::KillGui()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
}
