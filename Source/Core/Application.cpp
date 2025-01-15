#include "Application.hpp"
#include "EditorGui.hpp"
#include "Resources.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

#include <iostream>

#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/euler_angles.hpp>
#include <chrono>




Application::Application() : name("Mariana Engine")
{
    kWidth = 1366;
    kHeight = 768;
    std::cout << "Starting app" << std::endl;
    SetupWindow();

    InitGraphics();
    
    model = new Model("InterpolationTest.glb");
}

void Application::Start()
{
  #if defined(__EMSCRIPTEN__)
  int width, height;
  if (emscripten_get_canvas_element_size("#canvas", &width, &height) == EMSCRIPTEN_RESULT_SUCCESS) {
        std::cout << "Size: Width was: " << kWidth << ", " << "  and is now: " << width << std::endl;
        kWidth = static_cast<uint32_t>(width);
        kHeight = static_cast<uint32_t>(height);
  }
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

void Application::WindowResized() {
    #if defined(__EMSCRIPTEN__)
    int width, height;
    if (emscripten_get_canvas_element_size("#canvas", &width, &height) == EMSCRIPTEN_RESULT_SUCCESS) {
        std::cout << "Size: Width was: " << kWidth << ", " << "  and is now: " << width << std::endl;
        kWidth = static_cast<uint32_t>(width);
        kHeight = static_cast<uint32_t>(height);
        float aspect = static_cast<float>(width) / static_cast<float>(height);
        
        ubo.projectionMatrix = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 100.0f);
        surface.Unconfigure();
        ConfigureSurface();
        pipeLine->InitDepthTexture();
    } else {
        std::cerr << "Failed to get canvas element size" << std::endl;
    }
    #else
    float aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);
    ubo.projectionMatrix = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 100.0f);
    surface.Unconfigure();
    ConfigureSurface();
    pipeLine->InitDepthTexture();
    #endif
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
    pipeLine = new Pipeline("standard.wgsl", format, &globalUBO, &sampler);
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

  depthStencilAttachment.view = pipeLine->depthTextureView;
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
  float distance = 15;
  ubo.viewMatrix = glm::lookAt(glm::vec3(distance * glm::sin(currentTime), 0, distance * glm::cos(currentTime)), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  // ubo.modelMatrix = glm::rotate(kub.modelMatrix, 0.01f, glm::vec3(0,0,1));
  device.GetQueue().WriteBuffer(globalUBO, 0, &ubo, sizeof(UBO));

  pass.SetPipeline(pipeLine->pipeline);
  pass.SetBindGroup(0, pipeLine->uboBindGroup, 0, nullptr);
  model->Draw(pass);
  
  MarianaEditor::DrawEditor(pass, model);
  pass.End();
  
  wgpu::CommandBuffer commands = encoder.Finish();
  
  device.GetQueue().Submit(1, &commands);
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