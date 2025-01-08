#include "Application.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif
#include "Resources.h"
#include <iostream>

#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/euler_angles.hpp>
#include <chrono>



Application::Application() : name("Mariana Engine"), kWidth(1366), kHeight(768)
{
    std::cout << "Starting app" << std::endl;
    SetupWindow();

    InitGraphics();
    
    model = new Model("anim.glb");

  #if defined(__EMSCRIPTEN__)
  auto callback = [](void *arg) {
    Application* pApp = reinterpret_cast<Application*>(arg);
    pApp->Render();
  };
  emscripten_set_main_loop_arg(callback, this, 0, true);
#else
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    Render();
    surface.Present();
    instance.ProcessEvents();
  }
#endif
}

void Application::WindowResized()
{
  surface.Unconfigure();
  ConfigureSurface();
  float aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);
  ubo.projectionMatrix = glm::perspective(60.0f * 0.01745329251f, aspect, 0.01f, 100.0f);
  InitDepthTexture();
}

void Application::SetupWindow()
{
  if (!glfwInit()) {
    return;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  window = glfwCreateWindow(kWidth, kHeight, name, nullptr, nullptr);
  glfwSetWindowUserPointer(window, this);

  glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int, int){
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->WindowResized();
    });

    #if defined(__EMSCRIPTEN__)
  wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
  canvasDesc.selector = "#canvas";

  wgpu::SurfaceDescriptor surfaceDesc{.nextInChain = &canvasDesc};
  surface = instance.CreateSurface(&surfaceDesc);
#else
  surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
#endif
    
}

void Application::InitGUI()
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

Application::~Application()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
    std::cout << "Ending app" << std::endl;
}

void Application::ConfigureSurface()
{
  glfwGetFramebufferSize(window, &kWidth, &kHeight);
    wgpu::SurfaceCapabilities capabilities;
  surface.GetCapabilities(adapter, &capabilities);
  format = capabilities.formats[0];
  

  wgpu::SurfaceConfiguration config{
      .device = device,
      .format = format,
      .width = (uint32_t)kWidth,
      .height = (uint32_t)kHeight
    };
  surface.Configure(&config);
}

void Application::InitGraphics()
{
    ConfigureSurface();
    InitUniforms();
    InitSampler();
    CreateRenderPipeline();
    InitGUI();
}

void Application::InitUniforms()
{
    using namespace wgpu;
    BufferDescriptor bufferDesc;
    bufferDesc.size = sizeof(UBO);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    globalUBO = device.CreateBuffer(&bufferDesc);
    
    
    ubo.time = 1.0f;
    ubo.color[0] = -1;
    ubo.color[1] = -1;
    ubo.color[2] = 3;
    ubo.color[3] = 1;

    float aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);
    ubo.projectionMatrix = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 100.0f);
    float currentTime = 0;
    ubo.viewMatrix = glm::lookAt(glm::vec3(15 * glm::sin(currentTime), 0, 15 * glm::cos(currentTime)), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.modelMatrix = glm::mat4x4(12.0f);
    //ubo.modelMatrix = glm::rotate(ubo.modelMatrix, 3.14f, glm::vec3(0,1,0));
    ubo.modelMatrix = glm::rotate(ubo.modelMatrix, glm::radians(90.0f), glm::vec3(1,0,0));
    ubo.modelMatrix = glm::rotate(ubo.modelMatrix, glm::radians(-90.0f), glm::vec3(0,1,0));

    device.GetQueue().WriteBuffer(globalUBO, 0, &ubo, sizeof(UBO));
}

void Application::Render()
{
 
    wgpu::SurfaceTexture surfaceTexture;
  surface.GetCurrentTexture(&surfaceTexture);
  
  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();  

  wgpu::RenderPassColorAttachment attachment{
      .view = surfaceTexture.texture.CreateView(),
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store};

  wgpu::RenderPassDepthStencilAttachment depthStencilAttachment;

  depthStencilAttachment.view = depthTextureView;
  depthStencilAttachment.depthClearValue = 1;
  depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
  depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
  depthStencilAttachment.depthReadOnly = false;
  depthStencilAttachment.stencilClearValue = 0;
  depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
  depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
  depthStencilAttachment.stencilReadOnly = true;

  wgpu::RenderPassDescriptor renderpass{
                                        .colorAttachmentCount = 1,
                                        .colorAttachments = &attachment,
                                        .depthStencilAttachment = &depthStencilAttachment};

  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
  static float currentTime = 0;
  currentTime += 1.0f / 144.0f;
  currentTime = 0;
  float distance = 15;
  ubo.viewMatrix = glm::lookAt(glm::vec3(distance * glm::sin(currentTime), 0, distance * glm::cos(currentTime)), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  // ubo.modelMatrix = glm::rotate(kub.modelMatrix, 0.01f, glm::vec3(0,0,1));
  device.GetQueue().WriteBuffer(globalUBO, 0, &ubo, sizeof(UBO));

  pass.SetPipeline(pipeline);
  pass.SetBindGroup(0, bindGroup, 0, nullptr);
  //model->gameObject.Draw(pass);
  model->Draw(pass);
  
  //kub.Draw(pass);
  UpdateGUI(pass);
  pass.End();
  
  wgpu::CommandBuffer commands = encoder.Finish();
  
  device.GetQueue().Submit(1, &commands);
}

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

void Application::UpdateGUI(wgpu::RenderPassEncoder renderPass)
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

void Application::InitDepthTexture()
{ 
  using namespace wgpu;
  if(depthTextureView) 
  {
    wgpuTextureViewRelease(depthTextureView.Get());
  }

  TextureFormat depthTextureFormat = TextureFormat::Depth24Plus;
  TextureDescriptor depthTextureDesc;
  depthTextureDesc.dimension = TextureDimension::e2D;
  depthTextureDesc.format = depthTextureFormat;
  depthTextureDesc.mipLevelCount = 1;
  depthTextureDesc.sampleCount = 1;
  depthTextureDesc.size = {(uint32_t)kWidth, (uint32_t)kHeight, 1};
  depthTextureDesc.usage = TextureUsage::RenderAttachment;
  depthTextureDesc.viewFormatCount = 1;
  depthTextureDesc.viewFormats = &depthTextureFormat;
  Texture depthTexture = device.CreateTexture(&depthTextureDesc);

  TextureViewDescriptor depthTextureViewDesc;
  depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
  depthTextureViewDesc.baseArrayLayer = 0;
  depthTextureViewDesc.arrayLayerCount = 1;
  depthTextureViewDesc.baseMipLevel = 0;
  depthTextureViewDesc.mipLevelCount = 1;
  depthTextureViewDesc.dimension = TextureViewDimension::e2D;
  depthTextureViewDesc.format = depthTextureFormat;
  depthTextureView = depthTexture.CreateView(&depthTextureViewDesc);
}

void Application::InitSampler()
{
  using namespace wgpu;
  SamplerDescriptor samplerDesc;
  samplerDesc.addressModeU = AddressMode::ClampToEdge;
  samplerDesc.addressModeV = AddressMode::ClampToEdge;
  samplerDesc.addressModeW = AddressMode::ClampToEdge;
  samplerDesc.magFilter = FilterMode::Linear;
  samplerDesc.minFilter = FilterMode::Linear;
  samplerDesc.mipmapFilter = MipmapFilterMode::Linear;
  samplerDesc.lodMinClamp = 0.0f;
  samplerDesc.lodMaxClamp = 1.0f;
  samplerDesc.compare = CompareFunction::Undefined;
  samplerDesc.maxAnisotropy = 1;

  sampler = device.CreateSampler(&samplerDesc);
}

void Application::CreateRenderPipeline()
{
    using namespace wgpu;
  ShaderModule shaderModule = Resources::LoadShader("/Shaders/standard.wgsl");
  if (shaderModule == nullptr) {
    std::cerr << "Could not load shader!" << std::endl;
    exit(1);
  } else std::cout << "Loaded shader succesfully" << std::endl;

  ColorTargetState colorTargetState{.format = format};

  FragmentState fragmentState{.module = shaderModule,
                                    .targetCount = 1,
                                    .targets = &colorTargetState};

  VertexBufferLayout vertexBufferLayout;
  std::vector<VertexAttribute> attributes(4);

  attributes[0].format = VertexFormat::Float32x3;
  attributes[0].offset = 0;
  attributes[0].shaderLocation = 0;

  attributes[1].format = VertexFormat::Float32x3;
  attributes[1].offset = sizeof(glm::vec3);
  attributes[1].shaderLocation = 1;

  attributes[2].format = VertexFormat::Float32x3;
  attributes[2].offset = 2 * sizeof(glm::vec3);
  attributes[2].shaderLocation = 2;

  attributes[3].format = VertexFormat::Float32x2;
  attributes[3].offset = 3 * sizeof(glm::vec3);
  attributes[3].shaderLocation = 3;

  vertexBufferLayout.attributeCount = attributes.size();
  vertexBufferLayout.attributes = attributes.data();
  vertexBufferLayout.arrayStride = sizeof(Mesh::Vertex);
  vertexBufferLayout.stepMode = VertexStepMode::Vertex;

  std::vector<BindGroupLayoutEntry> globalBindingLayouts(2);
  globalBindingLayouts[0] = {};
  globalBindingLayouts[0].binding = 0;
  globalBindingLayouts[0].visibility = ShaderStage::Vertex | ShaderStage::Fragment;
  globalBindingLayouts[0].buffer.type = BufferBindingType::Uniform;
  globalBindingLayouts[0].buffer.minBindingSize = sizeof(UBO);

  globalBindingLayouts[1] = {};
  globalBindingLayouts[1].binding = 1;
  globalBindingLayouts[1].visibility = ShaderStage::Fragment;
  globalBindingLayouts[1].sampler.type = SamplerBindingType::Filtering;

  BindGroupLayoutDescriptor bindGroupLayoutDesc1{};
  bindGroupLayoutDesc1.entryCount = (uint32_t)globalBindingLayouts.size();
  bindGroupLayoutDesc1.entries = globalBindingLayouts.data();
  wgpu::BindGroupLayout bindGroupLayout1 = device.CreateBindGroupLayout(&bindGroupLayoutDesc1);


  std::vector<BindGroupLayoutEntry> modelBindingLayouts(1);
  modelBindingLayouts[0] = {};
  modelBindingLayouts[0].binding = 0;
  modelBindingLayouts[0].visibility = ShaderStage::Vertex | ShaderStage::Fragment;
  modelBindingLayouts[0].buffer.type = BufferBindingType::Uniform;
  modelBindingLayouts[0].buffer.hasDynamicOffset = true;
  //modelBindingLayouts[0].buffer.minBindingSize = sizeof(ModelData);
  modelBindingLayouts[0].buffer.minBindingSize = sizeof(ModelData);

  BindGroupLayoutDescriptor modelBindGroupLayoutDesc{};
  modelBindGroupLayoutDesc.entryCount = (uint32_t)modelBindingLayouts.size();
  modelBindGroupLayoutDesc.entries = modelBindingLayouts.data();
  wgpu::BindGroupLayout modelBindGroupLayout = device.CreateBindGroupLayout(&modelBindGroupLayoutDesc);


  std::vector<BindGroupLayoutEntry> textureBindingLayouts(1);
  textureBindingLayouts[0] = {};
  textureBindingLayouts[0].binding = 0;
  textureBindingLayouts[0].visibility = ShaderStage::Fragment;
  textureBindingLayouts[0].texture.sampleType = TextureSampleType::Float;
  textureBindingLayouts[0].texture.viewDimension = TextureViewDimension::e2D;

  BindGroupLayoutDescriptor textureBindGroupLayoutDesc{};
  textureBindGroupLayoutDesc.entryCount = (uint32_t)textureBindingLayouts.size();
  textureBindGroupLayoutDesc.entries = textureBindingLayouts.data();
  wgpu::BindGroupLayout textureBindGroupLayout = device.CreateBindGroupLayout(&textureBindGroupLayoutDesc);
  

  std::vector<BindGroupEntry> bindings(2);

  bindings[0] = {};
  bindings[0].binding = 0;
  bindings[0].buffer = globalUBO;
  bindings[0].offset = 0;
  bindings[0].size = sizeof(UBO);

  bindings[1] = {};
  bindings[1].binding = 1;
  bindings[1].sampler = sampler;

  BindGroupDescriptor bindGroupDesc{};
  bindGroupDesc.layout = bindGroupLayout1;
  bindGroupDesc.entryCount = (uint32_t)bindings.size();
  bindGroupDesc.entries = bindings.data();
  bindGroup = device.CreateBindGroup(&bindGroupDesc);

  std::vector<wgpu::BindGroupLayout> bindgroupLayouts = { bindGroupLayout1, modelBindGroupLayout/*, textureBindGroupLayout*/ };

  PipelineLayoutDescriptor layoutDesc{};
  layoutDesc.bindGroupLayoutCount = bindgroupLayouts.size();
  layoutDesc.bindGroupLayouts = bindgroupLayouts.data();
  layout = device.CreatePipelineLayout(&layoutDesc);

  InitDepthTexture();

  DepthStencilState depthStencilState = {};
  depthStencilState.depthCompare = CompareFunction::Less;
  depthStencilState.depthWriteEnabled = true;
  depthStencilState.format = TextureFormat::Depth24Plus;
  depthStencilState.stencilReadMask = 0;
  depthStencilState.stencilWriteMask = 0;

  RenderPipelineDescriptor descriptor{
      .layout = layout,
      .vertex = {.module = shaderModule,
                 .bufferCount = 1,
                 .buffers = &vertexBufferLayout},
      .depthStencil = &depthStencilState,
      .fragment = &fragmentState};
  pipeline = device.CreateRenderPipeline(&descriptor);
}