#pragma once

#include "EditorGui.hpp"
#include "GlobalVaribles.hpp"
#include <iostream>



namespace MarianaEditor
{
void RenderGameObjectInInspector(Node* selectedNode)
{
  ImGui::Begin("Inspector");
  if (selectedNode == nullptr)
  { 
    ImGui::End();
    return;
  } //localPosition
  float position[3] = {selectedNode->localPosition.x, selectedNode->localPosition.y, selectedNode->localPosition.z};
  glm::vec3 newRot = glm::degrees(glm::eulerAngles(selectedNode->localRotation));
  float rotation[3] = {newRot.x, newRot.y, newRot.z};
  float scale[3] = {selectedNode->localScale.x, selectedNode->localScale.y, selectedNode->localScale.z};
  ImGui::Text("Position");
  ImGui::SameLine();
  ImGui::DragFloat3("##Position", position);

  ImGui::Text("Rotation");
  ImGui::SameLine();
  ImGui::DragFloat3("##Rotation", rotation);

  ImGui::Text("Scale");
  ImGui::SameLine();
  ImGui::DragFloat3("##Scale", scale);
  ImGui::End();

  selectedNode->localPosition = {position[0], position[1], position[2]};
  newRot = {rotation[0], rotation[1], rotation[2]};
  selectedNode->localRotation = glm::quat(glm::radians(newRot));
  selectedNode->localScale = {scale[0], scale[1], scale[2]};
}

void DrawGameObjectNode(Node* g, Node*& selectedNode) {
    // Set flags for the TreeNode
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick; // Expand only on arrow or double-click
    if (g == selectedNode) {
        flags |= ImGuiTreeNodeFlags_Selected; // Highlight if this node is selected
    }
    if (g->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf; // Mark as a leaf node if it has no children
    }

    // Create the TreeNode
    if (g->name.empty())
      return;

    bool nodeOpen = ImGui::TreeNodeEx(g->name.c_str(), flags);

    // Check if the node is clicked (but not toggled open/closed by the arrow)
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selectedNode = g; // Mark this node as selected
    }

    // If the node is open, draw its children
    if (nodeOpen) {
        for (auto* child : g->children) {
            DrawGameObjectNode(child, selectedNode);
        }
        ImGui::TreePop(); // Close the TreeNode
    }
}


void renderSceneHierarchy(Model* g, Node*& selectedNode)
{
  
  ImGui::Begin("Scene");
  for (Node* n : g->rootNodes)
    DrawGameObjectNode(n, selectedNode);
  ImGui::End();
}

void DrawEditor(wgpu::RenderPassEncoder renderPass, Model* model)
{
  static Node* selectedNode = nullptr;

  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::DockSpaceOverViewport(0, NULL, ImGuiDockNodeFlags_PassthruCentralNode);

  ImGui::Begin("Stats");
  ImGuiIO& io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
  ImGui::End();
  RenderGameObjectInInspector(selectedNode);

  renderSceneHierarchy(model, selectedNode);

  ImGui::EndFrame();
  ImGui::Render();
  ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass.Get());
}

void InitGUI(GLFWwindow* window, wgpu::TextureFormat format)
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

  ImGuiStyle* style = &ImGui::GetStyle();
    style->WindowPadding = ImVec2(15, 15);
    style->WindowRounding = 2.5f;
    style->FramePadding = ImVec2(5, 5);
    style->FrameRounding = 4.0f;
    style->ItemSpacing = ImVec2(12, 8);
    style->ItemInnerSpacing = ImVec2(8, 6);
    style->IndentSpacing = 25.0f;
    style->ScrollbarSize = 15.0f;
    style->ScrollbarRounding = 9.0f;
    style->GrabMinSize = 5.0f;
    style->GrabRounding = 3.0f;

    style->Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
    style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
    style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
    style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
    style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
    //style->Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);

  ImGui::LoadIniSettingsFromDisk((std::string(RESOURCE_DIR) + "/imgui.ini").c_str());
}

void ShutdownGUI()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
}

};