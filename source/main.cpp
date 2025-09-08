#include <iostream>

#include <GLFW/glfw3.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif
#include <dawn/webgpu_cpp_print.h>
#include <webgpu/webgpu_cpp.h>

#include <Init.hpp>
#include <Window.hpp>
#include <Shader.hpp>
#include <Mesh.hpp>

wgpu::RenderPipeline pipeline;

Window m_Window(1366, 768, "MARIANA MANNEN");
Shader PBR_Shader;

std::vector<Vertex> verts = {
  {{0,1,0}, {1,0,0}},
  {{-1,-1,0}, {0,1,0}},
  {{1,-1,0}, {0,0,1}}
};

std::vector<uint16_t> indices = {
  0, 1, 2
};
Mesh<Vertex, uint16_t> mesh16(verts, indices);

const char shaderCode[] = R"(
    struct VertexInput {
        @location(0) position: vec3f,
        @location(1) normal: vec3f,
    };

    struct VertexOutput {
      @builtin(position) position: vec4f,
      @location(0) normal: vec3f
    };

    @vertex fn vertexMain(input: VertexInput) ->
      VertexOutput {
        var output: VertexOutput;
        output.position = vec4f(input.position.x, input.position.y, 0, 1);
        output.normal = input.normal;
        return output;
    }
    @fragment fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
        return vec4f(input.normal, 1);
    }
)";

void Render() {
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
  pass.SetPipeline(PBR_Shader.GetPipeline());
  pass.SetVertexBuffer(0, mesh16.vertexBuffer, 0, mesh16.vertexBuffer.GetSize());
  pass.Draw(mesh16.VertexCount(), 1, 0, 0);
  pass.End();
  wgpu::CommandBuffer commands = encoder.Finish();
  device.GetQueue().Submit(1, &commands);
}

void InitGraphics() {
  PBR_Shader.LoadShader(shaderCode, {windowFormat});
  mesh16.BuildMesh();
}

void Start() {
  m_Window.GetSurface();

  InitGraphics();

#if defined(__EMSCRIPTEN__)
  emscripten_set_main_loop(Render, 0, false);
#else
  while (!m_Window.ShouldClose()) {
    
    Render();
    surface.Present();
    instance.ProcessEvents();
  }
#endif
}

int main() {
  Init();
  Start();
}
