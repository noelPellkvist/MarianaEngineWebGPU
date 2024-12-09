#include "Application.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif
#include "Resources.h"
#include <iostream>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <gtc/matrix_transform.hpp>



Application::Application() : name("Mariana Engine"), kWidth(1366), kHeight(768)
{
    std::cout << "Starting app" << std::endl;
    SetupWindow();

    InitGraphics();
    

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
  ubo.projectionMatrix = glm::perspective(45.0f * 0.01745329251f, aspect, 0.01f, 100.0f);
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
  
  //ImGui_ImplWGPU_Init()
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

  ImGui::LoadIniSettingsFromDisk((std::string(RESOURCE_DIR) + "/imgui.ini").c_str());
}

Application::~Application()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
    delete gameObject;
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
    kub = Resources::LoadGLTFMesh("DamagedHelmet.glb");
    kub.bindGroup = &bindGroup;
    finalRenderPass = new Renderpass(banana, depthTextureView);
    loadedTextures = Resources::LoadTextures();
    //tmpRender = Resources::CreateEmptyTexture(1366, 768);
    //firstRenderpass = new Renderpass(tmpRender, depthTextureView);
    InitGUI();
    banana = Resources::LoadTexture("Avocado_baseColor.png");
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
    ubo.projectionMatrix = glm::perspective(45.0f * 0.01745329251f, aspect, 0.01f, 100.0f);
    ubo.viewMatrix = glm::lookAt(glm::vec3(-10.0f, -10.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.modelMatrix = glm::mat4x4(4.0f);
    ubo.modelMatrix = glm::rotate(ubo.modelMatrix, 3.14f, glm::vec3(0,1,0));

    device.GetQueue().WriteBuffer(globalUBO, 0, &ubo, sizeof(UBO));
}

void Application::Render()
{
 
    wgpu::SurfaceTexture surfaceTexture;
  surface.GetCurrentTexture(&surfaceTexture);
  
  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();  

  //firstRenderpass->Draw(encoder, pipeline, gameObject);
  //finalRenderPass->Draw(encoder, pipeline, gameObject, surfaceTexture);

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
  ubo.modelMatrix = glm::rotate(kub.modelMatrix, 0.01f, glm::vec3(0,0,1));
  device.GetQueue().WriteBuffer(globalUBO, 0, &ubo, sizeof(UBO));

  pass.SetPipeline(pipeline);
  
  kub.Draw(pass);
  
  UpdateGUI(pass);
  pass.End();
  
  wgpu::CommandBuffer commands = encoder.Finish();
  
  device.GetQueue().Submit(1, &commands);
}

void RenderGameObjectInInspector(GameObject* gameObject, glm::vec4& light)
{
  float position[3] = {gameObject->position.x, gameObject->position.y, gameObject->position.z};
  float rotation[3] = {gameObject->rotation.x, gameObject->rotation.y, gameObject->rotation.z};
  float scale[3] = {gameObject->scale.x, gameObject->scale.y, gameObject->scale.z};
  float lightdirection[3] = {light.x, light.y, light.z};
  ImGui::Begin("Inspector");
  ImGui::Text("Position");
  ImGui::SameLine();
  ImGui::DragFloat3("##Position", position);

  ImGui::Text("Rotation");
  ImGui::SameLine();
  ImGui::DragFloat3("##Rotation", rotation);

  ImGui::Text("Scale");
  ImGui::SameLine();
  ImGui::DragFloat3("##Scale", scale);

  ImGui::Text("LightDirection");
  ImGui::SameLine();
  ImGui::DragFloat3("##LightDirection", lightdirection);
  ImGui::End();

  gameObject->position = {position[0], position[1], position[2]};
  gameObject->rotation = {rotation[0], rotation[1], rotation[2]};
  gameObject->scale = {scale[0], scale[1], scale[2]};
  light = {lightdirection[0], lightdirection[1], lightdirection[2], 0};
}

bool DrawGameObjectNode(GameObject* g)
{
  bool selected = false;

  if (ImGui::TreeNode(g->name.c_str())) {
        // Right-click context menu for the tree node
         ImGui::TreePop();
    }

    if (ImGui::BeginPopupContextItem("Gameobject Options")) {
            // Add items to the context menu
            if (ImGui::MenuItem("View in inspector")) {
                selected = true;
            }
            else selected = false;
            ImGui::EndPopup();
        }

  return selected = true;;
       
}

void renderSceneHierarchy(GameObject* g)
{
  ImGui::Begin("Scene");
  DrawGameObjectNode(g);
  ImGui::End();
}

void Application::UpdateGUI(wgpu::RenderPassEncoder renderPass)
{
  

  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::DockSpaceOverViewport(0, NULL, ImGuiDockNodeFlags_PassthruCentralNode);

  ImGui::Begin("Stats");
  ImGuiIO& io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
  for(int i = 0; i < loadedTextures.size(); i++)
  {
    ImTextureID texture_id = reinterpret_cast<ImTextureID>(loadedTextures[i].Get());
    ImVec2 window_size = ImGui::GetWindowSize();
    ImGui::Image(texture_id, ImVec2(window_size.x, window_size.x));
  }
  ImGui::End();

  glm::vec4 l = {ubo.color[0], ubo.color[1], ubo.color[2], 0};
  RenderGameObjectInInspector(&kub, l);
  ubo.color[0] = l.x;
  ubo.color[1] = l.y;
  ubo.color[2] = l.z;

  renderSceneHierarchy(&kub);

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

  std::vector<BindGroupLayoutEntry> bindingLayouts(5);
  bindingLayouts[0] = {};
  bindingLayouts[0].binding = 0;
  bindingLayouts[0].visibility = ShaderStage::Vertex | ShaderStage::Fragment;
  bindingLayouts[0].buffer.type = BufferBindingType::Uniform;
  bindingLayouts[0].buffer.minBindingSize = sizeof(UBO);

  bindingLayouts[1] = {};
  bindingLayouts[1].binding = 1;
  bindingLayouts[1].visibility = ShaderStage::Fragment;
  bindingLayouts[1].sampler.type = SamplerBindingType::Filtering;

  bindingLayouts[2] = {};
  bindingLayouts[2].binding = 2;
  bindingLayouts[2].visibility = ShaderStage::Fragment;
  bindingLayouts[2].texture.sampleType = TextureSampleType::Float;
  bindingLayouts[2].texture.viewDimension = TextureViewDimension::e2D;

  bindingLayouts[3] = {};
  bindingLayouts[3].binding = 3;
  bindingLayouts[3].visibility = ShaderStage::Fragment;
  bindingLayouts[3].texture.sampleType = TextureSampleType::Float;
  bindingLayouts[3].texture.viewDimension = TextureViewDimension::e2D;

  bindingLayouts[4] = {};
  bindingLayouts[4].binding = 4;
  bindingLayouts[4].visibility = ShaderStage::Fragment;
  bindingLayouts[4].texture.sampleType = TextureSampleType::Float;
  bindingLayouts[4].texture.viewDimension = TextureViewDimension::e2D;

  BindGroupLayoutDescriptor bindGroupLayoutDesc{};
  bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayouts.size();
  bindGroupLayoutDesc.entries = bindingLayouts.data();
  bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc);

  std::vector<BindGroupEntry> bindings(5);

  bindings[0] = {};
  bindings[0].binding = 0;
  bindings[0].buffer = globalUBO;
  bindings[0].offset = 0;
  bindings[0].size = sizeof(UBO);

  bindings[1] = {};
  bindings[1].binding = 1;
  bindings[1].sampler = sampler;


  bindings[2] = {};
  bindings[2].binding = 2;
  bindings[2].textureView = Resources::LoadTexture("helmetAlbedo.jpg");

  bindings[3] = {};
  bindings[3].binding = 3;
  bindings[3].textureView = Resources::LoadTexture("Default_metalRoughness.jpg");

  bindings[4] = {};
  bindings[4].binding = 4;
  bindings[4].textureView = Resources::LoadTexture("Default_AO.jpg");

  BindGroupDescriptor bindGroupDesc{};
  bindGroupDesc.layout = bindGroupLayout;
  bindGroupDesc.entryCount = (uint32_t)bindings.size();
  bindGroupDesc.entries = bindings.data();
  bindGroup = device.CreateBindGroup(&bindGroupDesc);

  PipelineLayoutDescriptor layoutDesc{};
  layoutDesc.bindGroupLayoutCount = 1;
  layoutDesc.bindGroupLayouts = &bindGroupLayout;
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