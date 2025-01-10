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
#include "EditorGui.hpp"



Application::Application() : name("Mariana Engine"), kWidth(1366), kHeight(768)
{
    std::cout << "Starting app" << std::endl;
    SetupWindow();

    InitGraphics();
    
    model = new Model("InterpolationTest.glb");

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

Application::~Application()
{
    MarianaEditor::ShutdownGUI();
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
    MarianaEditor::InitGUI(window, format);
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
  MarianaEditor::DrawEditor(pass, model);
  pass.End();
  
  wgpu::CommandBuffer commands = encoder.Finish();
  
  device.GetQueue().Submit(1, &commands);
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


  std::vector<wgpu::BindGroupLayoutEntry> boneBindingLayouts(1);
  boneBindingLayouts[0] = {};
  boneBindingLayouts[0].binding = 0;
  boneBindingLayouts[0].visibility = wgpu::ShaderStage::Vertex;
  boneBindingLayouts[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
  boneBindingLayouts[0].buffer.hasDynamicOffset = false; 
  boneBindingLayouts[0].buffer.minBindingSize = 0; 
  wgpu::BindGroupLayoutDescriptor boneBindGroupLayoutDesc{};
  boneBindGroupLayoutDesc.entryCount = (uint32_t)boneBindingLayouts.size();
  boneBindGroupLayoutDesc.entries = boneBindingLayouts.data();
  wgpu::BindGroupLayout boneBindGroupLayout = device.CreateBindGroupLayout(&boneBindGroupLayoutDesc);


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

  std::vector<wgpu::BindGroupLayout> bindgroupLayouts = { bindGroupLayout1, modelBindGroupLayout, boneBindGroupLayout };

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