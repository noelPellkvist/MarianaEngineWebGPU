#include "Application.hpp"
#include <GLFW/glfw3.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

#include <iostream>

const char shaderCode[] = R"(

    struct VertexInput {
        @location(0) position: vec2f,
        @location(1) normal: vec3f,
    };

    struct VertexOutput {
        @builtin(position) position: vec4f,
        @location(0) normal: vec3f,
    };


    @vertex fn vertexMain(in: VertexInput) -> VertexOutput {
        var out: VertexOutput; // create the output struct
        out.position = vec4f(in.position, 0.0, 1.0); // same as what we used to directly return
        out.normal = in.normal; // forward the color attribute to the fragment shader
        return out;
    }

    @fragment fn fragmentMain(in: VertexOutput) -> @location(0) vec4f {
        return vec4f(in.normal, 1.0); // use the interpolated color coming from the vertex shader
}
)";

Mesh mesh;

Application::Application() : name("Mariana Engine"), kWidth(768), kHeight(480)
{
    std::cout << "Starting app" << std::endl;
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
    mesh.BuildMesh();
    ConfigureSurface();
    CreateRenderPipeline();
}

void Application::Render()
{
    wgpu::SurfaceTexture surfaceTexture;
  surface.GetCurrentTexture(&surfaceTexture);

  wgpu::RenderPassColorAttachment attachment{
      .view = surfaceTexture.texture.CreateView(),
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store};

  wgpu::RenderPassDescriptor renderpass{.colorAttachmentCount = 1,
                                        .colorAttachments = &attachment};

  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
  pass.SetPipeline(pipeline);
  pass.SetVertexBuffer(0, mesh.GetVertexBuffer(), 0, mesh.GetVertexBuffer().GetSize());
  pass.Draw(mesh.getVertexCount(), 1, 0, 0);
  pass.End();
  wgpu::CommandBuffer commands = encoder.Finish();
  device.GetQueue().Submit(1, &commands);
}

void Application::CreateRenderPipeline()
{
    using namespace wgpu;
    ShaderModuleWGSLDescriptor wgslDesc{};
  wgslDesc.code = shaderCode;

  ShaderModuleDescriptor shaderModuleDescriptor{
      .nextInChain = &wgslDesc};
  ShaderModule shaderModule =
      device.CreateShaderModule(&shaderModuleDescriptor);

  ColorTargetState colorTargetState{.format = format};

  FragmentState fragmentState{.module = shaderModule,
                                    .targetCount = 1,
                                    .targets = &colorTargetState};

  VertexBufferLayout vertexBufferLayout;

//   VertexAttribute positionAttrib[2]; = {
//     {.format = VertexFormat::Float32x2,
//      .offset = 0,
//      .shaderLocation = 0},
//     {.format = VertexFormat::Float32x2,
//      .offset = sizeof(glm::vec2)
//      .shaderLocation = 1}
//   };
//   positionAttrib.shaderLocation = 0;
//   positionAttrib.format = VertexFormat::Float32x2;
//   positionAttrib.offset = 0;

  std::vector<VertexAttribute> attributes(2);

  attributes[0].format = VertexFormat::Float32x2;
  attributes[0].offset = 0;
  attributes[0].shaderLocation = 0;

  attributes[1].format = VertexFormat::Float32x3;
  attributes[1].offset = sizeof(glm::vec2);
  attributes[1].shaderLocation = 1;

  vertexBufferLayout.attributeCount = attributes.size();
  vertexBufferLayout.attributes = attributes.data();
  vertexBufferLayout.arrayStride = sizeof(Mesh::Vertex);
  vertexBufferLayout.stepMode = VertexStepMode::Vertex;

  RenderPipelineDescriptor descriptor{
      .vertex = {.module = shaderModule,
                 .bufferCount = 1,
                 .buffers = &vertexBufferLayout},
      .fragment = &fragmentState};
  pipeline = device.CreateRenderPipeline(&descriptor);
}