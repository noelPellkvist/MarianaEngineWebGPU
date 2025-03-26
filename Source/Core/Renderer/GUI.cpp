#include "GUI.hpp"
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include "../GlobalVaribles.hpp"
#include <iostream>
#include "../ImGuizmo.h"
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <gtc/matrix_transform.hpp>


GUI::GUI(GLFWwindow* window, wgpu::TextureFormat format, UniformBuffer& TransfomBuffer) : m_TransfomBuffer(TransfomBuffer)
{
    IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui_ImplGlfw_InitForOther(window, true);
  
  ImGui_ImplWGPU_InitInfo info = {};
  info.Device = device.Get();
  info.NumFramesInFlight = 3;
  info.RenderTargetFormat = static_cast<WGPUTextureFormat>(format);
  info.DepthStencilFormat = WGPUTextureFormat_Depth24Plus;
  if(ImGui_ImplWGPU_Init(&info))
  {
    std::cout << "Inited imgui" << std::endl;
  }
  else
    std::cout << "Failed to initialize gui" << std::endl;

  ImGui::GetIO().FontGlobalScale = 1.2f;

//   ImGuiStyle* style = &ImGui::GetStyle();
//     style->WindowPadding = ImVec2(15, 15);
//     style->WindowRounding = 2.5f;
//     style->FramePadding = ImVec2(5, 5);
//     style->FrameRounding = 4.0f;
//     style->ItemSpacing = ImVec2(12, 8);
//     style->ItemInnerSpacing = ImVec2(8, 6);
//     style->IndentSpacing = 25.0f;
//     style->ScrollbarSize = 15.0f;
//     style->ScrollbarRounding = 9.0f;
//     style->GrabMinSize = 5.0f;
//     style->GrabRounding = 3.0f;

//     style->Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
//     style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
//     style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
//     style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
//     style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
//     style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
//     style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
//     style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
//     style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
//     style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
//     style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
//     style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
//     style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
//     style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
//     style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
//     style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
//     style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
//     style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
//     style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
//     style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
//     style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
//     style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
//     style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
//     style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
//     style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
//     style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
//     style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
//     style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
//     style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
//     style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
//     style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
//     style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
//     style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
//     style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
//     //style->Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);

  ImGui::LoadIniSettingsFromDisk((std::string(RESOURCE_DIR) + "/imgui.ini").c_str());
}

void DrawFps()
{
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImVec2 screenPos = ImVec2(ImGui::GetIO().DisplaySize.x - 400.0f, 10.0f);

    ImFont* font = ImGui::GetFont();

    char textBuffer[100];
    snprintf(textBuffer, sizeof(textBuffer), "Application average %.3f ms/frame (%.1f FPS)", 
             1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    // Display the formatted text in the top-right corner
    drawList->AddText(font, 16.0f, screenPos, IM_COL32(255, 255, 255, 255), textBuffer);
}

void GUI::DrawHierachry(Transform& transform, const Relationship& relationship, entt::registry& reg)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (relationship.children == 0) flags |= ImGuiTreeNodeFlags_Leaf;
    if(selectedTransform == &transform) flags |= ImGuiTreeNodeFlags_Selected;

    if (ImGui::TreeNodeEx(transform.name.c_str(), flags)) {
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            selectedTransform = &transform; // Mark this node as selected
        }
        entt::entity currentChild = relationship.first;
        while (currentChild != entt::null)
        {
            Transform& childTransform = reg.get<Transform>(currentChild);
            const Relationship& childRelation = reg.get<Relationship>(currentChild);
            DrawHierachry(childTransform, childRelation, reg);
            currentChild = reg.get<Relationship>(currentChild).next;
        }
        ImGui::TreePop();
    }
}

void GUI::DrawGizmo()
{
    static ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentMode = ImGuizmo::WORLD;
    {
        ImGui::Begin("Gizmo Controls");
        if (ImGui::RadioButton("Translate", currentOperation == ImGuizmo::TRANSLATE))
            currentOperation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", currentOperation == ImGuizmo::ROTATE))
            currentOperation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", currentOperation == ImGuizmo::SCALE))
            currentOperation = ImGuizmo::SCALE;
        
        if (ImGui::RadioButton("World", currentMode == ImGuizmo::WORLD))
            currentMode = ImGuizmo::WORLD;
        ImGui::SameLine();
        if (ImGui::RadioButton("Local", currentMode == ImGuizmo::LOCAL))
            currentMode = ImGuizmo::LOCAL;
        ImGui::End();
    }

    // 3. Render the gizmo over the entire GLFW window
    ImGuizmo::BeginFrame();
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x > 0.f && displaySize.y > 0.f)
    {
        // Set the area where the gizmo is drawn (the entire viewport)
        ImGuizmo::SetRect(0, 0, displaySize.x, displaySize.y);

        // Set up example matrices.
        // Replace these with your actual camera and model transforms as needed.
        float currentTime = 0.0f;
        float aspect = static_cast<float>(1336) / static_cast<float>(768);
        static glm::mat4 view = glm::lookAt(glm::vec3(15 * glm::sin(currentTime), 0.0f, 15 * glm::cos(currentTime)),
                                             glm::vec3(0.0f, 0.0f, 0.0f),
                                             glm::vec3(0.0f, 1.0f, 0.0f));
        static glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 100.0f);

        // Render and interact with the gizmo using the selector settings
        ImGuizmo::Manipulate(glm::value_ptr(view), 
                             glm::value_ptr(proj),
                             currentOperation,
                             currentMode,
                             glm::value_ptr(selectedTransform->data.modelMatrix));
    }

    glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(selectedTransform->data.modelMatrix)));
    glm::mat4 normalMatrix = glm::mat4(1.0f); // Start with an identity matrix
    normalMatrix[0] = glm::vec4(normalMat3[0], 0.0f); // First row of normal matrix
    normalMatrix[1] = glm::vec4(normalMat3[1], 0.0f); // Second row of normal matrix
    normalMatrix[2] = glm::vec4(normalMat3[2], 0.0f); // Third row of normal matrix
    selectedTransform->data.normalMatrix = normalMatrix;
    m_TransfomBuffer.UpdateValue(&selectedTransform->data, sizeof(TransformBufferData), selectedTransform->dataIndex);
}


void GUI::DrawGUI(wgpu::RenderPassEncoder renderPass, entt::registry& reg)
{
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(0, NULL, ImGuiDockNodeFlags_PassthruCentralNode);

    DrawFps();


    ImGui::Begin("Scene");
    auto view = reg.view<Transform, const Relationship>();
    for(auto [entity, transform, relation]: view.each()) {
        if(relation.parent == entt::null)
        {
            DrawHierachry(transform, relation, reg);
        }
    }
    ImGui::End();

    if (selectedTransform != nullptr) DrawGizmo();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass.Get());
}


GUI::~GUI()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
}