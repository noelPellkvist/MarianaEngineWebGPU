#include "Application.hpp"
#include <GLFW/glfw3.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif
#include "Resources.h"
#include <iostream>

const char shaderCode[] = R"(

    struct VertexInput {
        @location(0) position: vec3f,
        @location(1) normal: vec3f,
    };

    struct VertexOutput {
        @builtin(position) position: vec4f,
        @location(0) normal: vec3f,
    };

    struct GB {
        projectionMatrix: mat4x4f,
        viewMatrix: mat4x4f,
        modelMatrix: mat4x4f,
        color: vec4f,
        time: f32,
    };

    @group(0) @binding(0) var<uniform> UBO: GB;


    @vertex fn vertexMain(in: VertexInput) -> VertexOutput {
        var out: VertexOutput;

        out.position = UBO.projectionMatrix * UBO.viewMatrix * UBO.modelMatrix * vec4f(in.position, 1.0);
        out.normal = (UBO.modelMatrix * vec4f(in.normal, 0.0)).xyz;
        return out;
    }

    @fragment fn fragmentMain(in: VertexOutput) -> @location(0) vec4f {
        let color = in.normal * UBO.color.rgb;
        let corrected_color = pow(color, vec3f(2.2));
        return vec4f(corrected_color, UBO.color.a);
}
)";

Mesh mesh;

Application::Application() : name("Mariana Engine"), kWidth(1280), kHeight(768)
{
    std::cout << "Starting app" << std::endl;
    SetupWindow();
}

void Application::SetupWindow()
{
  if (!glfwInit()) {
    return;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(kWidth, kHeight, name, nullptr, nullptr);

    #if defined(__EMSCRIPTEN__)
  wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
  canvasDesc.selector = "#canvas";

  wgpu::SurfaceDescriptor surfaceDesc{.nextInChain = &canvasDesc};
  surface = instance.CreateSurface(&surfaceDesc);
#else
  surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
#endif

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

Application::~Application()
{
    std::cout << "Ending app" << std::endl;
}

void Application::ConfigureSurface()
{
    wgpu::SurfaceCapabilities capabilities;
  surface.GetCapabilities(adapter, &capabilities);
  format = capabilities.formats[0];

  wgpu::SurfaceConfiguration config{
      .device = device,
      .format = format,
      .width = kWidth,
      .height = kHeight};
  surface.Configure(&config);
}

void Application::InitGraphics()
{
    mesh = Resources::LoadOBJMesh("/Cube.obj");
    mesh.BuildMesh();
    ConfigureSurface();
    InitUniforms();
    CreateRenderPipeline();
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
    ubo.color[0] = 1;
    ubo.color[1] = 1;
    ubo.color[2] = 1;
    ubo.color[3] = 1;

    float aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);
    ubo.projectionMatrix = glm::perspective(45.0f * 0.01745329251f, aspect, 0.01f, 100.0f);
    ubo.viewMatrix = glm::lookAt(glm::vec3(-10.0f, -10.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.modelMatrix = glm::mat4x4(1.0f);
    ubo.modelMatrix = glm::rotate(ubo.modelMatrix, 3.14f, glm::vec3(0,1,0));

    device.GetQueue().WriteBuffer(globalUBO, 0, &ubo, sizeof(UBO));
}

void Application::Render()
{
    wgpu::SurfaceTexture surfaceTexture;
  surface.GetCurrentTexture(&surfaceTexture);

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

  wgpu::RenderPassDescriptor renderpass{.colorAttachmentCount = 1,
                                        .colorAttachments = &attachment,
                                        .depthStencilAttachment = &depthStencilAttachment};

  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);

  ubo.time = static_cast<float>(glfwGetTime());
  ubo.modelMatrix = glm::rotate(ubo.modelMatrix, 0.01f, glm::vec3(0,0,1));
  device.GetQueue().WriteBuffer(globalUBO, 0, &ubo, sizeof(UBO));

  pass.SetPipeline(pipeline);
  pass.SetVertexBuffer(0, mesh.GetVertexBuffer(), 0, mesh.GetVertexBuffer().GetSize());
  pass.SetIndexBuffer(mesh.GetIndexBuffer(), wgpu::IndexFormat::Uint16, 0, mesh.GetIndexBuffer().GetSize());
  pass.SetBindGroup(0, bindGroup, 0, nullptr);
  pass.DrawIndexed(mesh.getIndexCount(), 1, 0, 0);
  pass.End();
  wgpu::CommandBuffer commands = encoder.Finish();
  device.GetQueue().Submit(1, &commands);
}

void Application::CreateRenderPipeline()
{
    using namespace wgpu;
  //   ShaderModuleWGSLDescriptor wgslDesc{};
  // wgslDesc.code = shaderCode;

  // ShaderModuleDescriptor shaderModuleDescriptor{
  //     .nextInChain = &wgslDesc};
  // ShaderModule shaderModule =
  //     device.CreateShaderModule(&shaderModuleDescriptor);
  //std::cout << "Creating shader module: " << (RESOURCE_DIR "/Shaders/standard.wgsl") << std::endl;
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

  BindGroupLayoutEntry bindingLayout = {};
  bindingLayout.binding = 0;
  bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
  bindingLayout.buffer.type = BufferBindingType::Uniform;
  bindingLayout.buffer.minBindingSize = sizeof(UBO);

  BindGroupLayoutDescriptor bindGroupLayoutDesc{};
  bindGroupLayoutDesc.entryCount = 1;
  bindGroupLayoutDesc.entries = &bindingLayout;
  bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc);

  BindGroupEntry binding{};

  binding.binding = 0;
  binding.buffer = globalUBO;
  binding.offset = 0;
  binding.size = sizeof(UBO);

  BindGroupDescriptor bindGroupDesc{};
  bindGroupDesc.layout = bindGroupLayout;
  bindGroupDesc.entryCount = 1;
  bindGroupDesc.entries = &binding;
  bindGroup = device.CreateBindGroup(&bindGroupDesc);

  PipelineLayoutDescriptor layoutDesc{};
  layoutDesc.bindGroupLayoutCount = 1;
  layoutDesc.bindGroupLayouts = &bindGroupLayout;
  layout = device.CreatePipelineLayout(&layoutDesc);

  TextureFormat depthTextureFormat = TextureFormat::Depth24Plus;
  TextureDescriptor depthTextureDesc;
  depthTextureDesc.dimension = TextureDimension::e2D;
  depthTextureDesc.format = depthTextureFormat;
  depthTextureDesc.mipLevelCount = 1;
  depthTextureDesc.sampleCount = 1;
  depthTextureDesc.size = {kWidth, kHeight, 1};
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

  DepthStencilState depthStencilState = {};
  depthStencilState.depthCompare = CompareFunction::LessEqual;
  depthStencilState.depthWriteEnabled = true;
  depthStencilState.format = depthTextureFormat;
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