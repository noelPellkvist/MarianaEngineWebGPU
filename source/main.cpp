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

wgpu::RenderPipeline pipeline;

Window m_Window(512, 512, "Noels window");
Shader PBR_Shader;

const char shaderCode[] = R"(
    @vertex fn vertexMain(@builtin(vertex_index) i : u32) ->
      @builtin(position) vec4f {
        const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
        return vec4f(pos[i], 0, 1);
    }
    @fragment fn fragmentMain() -> @location(0) vec4f {
        return vec4f(0, 1, 1, 1);
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
  pass.Draw(3);
  pass.End();
  wgpu::CommandBuffer commands = encoder.Finish();
  device.GetQueue().Submit(1, &commands);
}

void InitGraphics() {
  PBR_Shader.LoadShader(shaderCode, {windowFormat});
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
