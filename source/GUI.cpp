#include <GUI.hpp>
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <webgpu/webgpu_cpp.h>
#include "Init.hpp"

static ImVec4 Lerp(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x)*t,
                  a.y + (b.y - a.y)*t,
                  a.z + (b.z - a.z)*t,
                  a.w + (b.w - a.w)*t);
}

inline void ApplyMarianaStyle(float uiScale = 1.0f,
                              ImVec4 accent = ImVec4(0.26f, 0.59f, 0.98f, 1.00f)) // default: blue
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // ----- Layout / metrics -----
    style.WindowPadding     = ImVec2(10, 10);
    style.FramePadding      = ImVec2(8, 6);
    style.CellPadding       = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 6);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.IndentSpacing     = 18.0f;
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 10.0f;
    style.TabRounding       = 8.0f;

    // ----- Rounding / borders -----
    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 6.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    // Optional: make whole tree rows clickable in your hierarchy
    // (use alongside ImGuiTreeNodeFlags_SpanAvailWidth in your TreeNodeEx)
    style.TouchExtraPadding = ImVec2(0, 2);

    // ----- Colors (dark base + accent) -----
    const ImVec4 bg0 = ImVec4(0.10f, 0.11f, 0.12f, 1.00f); // window bg
    const ImVec4 bg1 = ImVec4(0.13f, 0.14f, 0.16f, 1.00f); // child/menu
    const ImVec4 bg2 = ImVec4(0.18f, 0.19f, 0.22f, 1.00f); // headers/active
    const ImVec4 fg  = ImVec4(0.90f, 0.92f, 0.94f, 1.00f); // text
    const ImVec4 fg2 = ImVec4(0.66f, 0.70f, 0.74f, 1.00f); // text disabled
    const ImVec4 bd  = ImVec4(0.30f, 0.33f, 0.36f, 0.60f); // border
    const ImVec4 hl  = accent;                              // highlight (accent)
    const ImVec4 hlH = Lerp(accent, ImVec4(1,1,1,1), 0.20f); // hovered accent
    const ImVec4 hlA = Lerp(accent, ImVec4(0,0,0,1), 0.15f); // active accent

    colors[ImGuiCol_Text]                   = fg;
    colors[ImGuiCol_TextDisabled]           = fg2;
    colors[ImGuiCol_WindowBg]               = bg0;
    colors[ImGuiCol_ChildBg]                = bg1;
    colors[ImGuiCol_PopupBg]                = ImVec4(bg0.x, bg0.y, bg0.z, 0.98f);

    colors[ImGuiCol_Border]                 = bd;
    colors[ImGuiCol_BorderShadow]           = ImVec4(0,0,0,0);

    colors[ImGuiCol_FrameBg]                = bg1;
    colors[ImGuiCol_FrameBgHovered]         = Lerp(bg1, hl, 0.20f);
    colors[ImGuiCol_FrameBgActive]          = Lerp(bg1, hl, 0.35f);

    colors[ImGuiCol_TitleBg]                = bg1;
    colors[ImGuiCol_TitleBgActive]          = bg2;
    colors[ImGuiCol_TitleBgCollapsed]       = bg1;

    colors[ImGuiCol_MenuBarBg]              = bg1;

    colors[ImGuiCol_ScrollbarBg]            = bg1;
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.38f, 0.40f, 0.44f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.48f, 0.50f, 0.54f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.56f, 0.58f, 0.62f, 1.00f);

    colors[ImGuiCol_CheckMark]              = hlH;
    colors[ImGuiCol_SliderGrab]             = hl;
    colors[ImGuiCol_SliderGrabActive]       = hlA;

    colors[ImGuiCol_Button]                 = Lerp(bg1, hl, 0.12f);
    colors[ImGuiCol_ButtonHovered]          = Lerp(bg1, hl, 0.28f);
    colors[ImGuiCol_ButtonActive]           = Lerp(bg1, hl, 0.40f);

    colors[ImGuiCol_Header]                 = Lerp(bg1, hl, 0.16f); // tree/selected rows
    colors[ImGuiCol_HeaderHovered]          = Lerp(bg1, hl, 0.30f);
    colors[ImGuiCol_HeaderActive]           = Lerp(bg1, hl, 0.42f);

    colors[ImGuiCol_Separator]              = bd;
    colors[ImGuiCol_SeparatorHovered]       = hlH;
    colors[ImGuiCol_SeparatorActive]        = hlA;

    colors[ImGuiCol_ResizeGrip]             = ImVec4(0,0,0,0);
    colors[ImGuiCol_ResizeGripHovered]      = Lerp(bg1, hl, 0.40f);
    colors[ImGuiCol_ResizeGripActive]       = Lerp(bg1, hl, 0.60f);

    colors[ImGuiCol_Tab]                    = Lerp(bg2, hl, 0.18f);
    colors[ImGuiCol_TabHovered]             = Lerp(bg2, hl, 0.38f);
    colors[ImGuiCol_TabActive]              = Lerp(bg2, hl, 0.28f);
    colors[ImGuiCol_TabUnfocused]           = Lerp(bg2, hl, 0.08f);
    colors[ImGuiCol_TabUnfocusedActive]     = Lerp(bg2, hl, 0.20f);

    colors[ImGuiCol_DockingPreview]         = ImVec4(hl.x, hl.y, hl.z, 0.35f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(bg0.x, bg0.y, bg0.z, 1.00f);

    colors[ImGuiCol_TableHeaderBg]          = bg2;
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(bd.x, bd.y, bd.z, 0.8f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(bd.x, bd.y, bd.z, 0.4f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(1,1,1,0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1,1,1,0.03f);

    colors[ImGuiCol_TextSelectedBg]         = ImVec4(hl.x, hl.y, hl.z, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = Lerp(hl, ImVec4(1,1,1,1), 0.25f);

    colors[ImGuiCol_NavHighlight]           = Lerp(hl, ImVec4(1,1,1,1), 0.15f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1,1,1,0.30f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0,0,0,0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0,0,0,0.35f);

    // Scale all sizes for HiDPI
    if (uiScale != 1.0f) {
        style.ScaleAllSizes(uiScale);
        // If you manage fonts, scale those too:
        // for (auto* f : io.Fonts->Fonts) f->Scale = uiScale;
    }

    // If you use docking (you do), a little tweak helps readability on tabs
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextAlign      = ImVec2(0.0f, 0.5f);
}

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
    info.PipelineMultisampleState.count = 1;
    if(ImGui_ImplWGPU_Init(&info))
    {
    }
    ApplyMarianaStyle(1.0f, ImVec4(0.35f, 0.75f, 0.55f, 1.0f));
    ImFontConfig config;
    config.MergeMode = false;
    config.PixelSnapH = true;

    io.Fonts->AddFontFromFileTTF((std::string(RESOURCE_DIR) + "/Fonts/JetBrainsMonoNerdFont-Regular.ttf").c_str(), 16.0f, &config);
}

void GUI::PreUpdateGUI()
{
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, NULL, ImGuiDockNodeFlags_PassthruCentralNode);
}

void GUI::PostUpdateGUI(wgpu::RenderPassEncoder* renderPass)
{
    if(!renderPass) return;
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass->Get());
}

void GUI::DrawTexture(Texture texture, float width, float height)
{
    ImTextureID _tex = (ImTextureID)(((wgpu::TextureView*)texture.GetTextureView())->Get());
    ImVec2 size(width, height);

    ImGui::Image(_tex, size);
}

void GUI::KillGui()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
}
