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
#include <FileReader.hpp>
#include <moved_later/OBJLoader.hpp>

wgpu::RenderPipeline pipeline;

Window m_Window(1366, 768, "MARIANA MANNEN");
Shader PBR_Shader;

//Mesh<Vertex, uint16_t> mesh16 = LoadTestMesh();
Mesh<Vertex, uint32_t> mesh16 = LoadOBJMesh(std::string(RESOURCE_DIR) + "/Models/monkey.obj");

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
  pass.SetVertexBuffer(0, mesh16.vertexBuffer, 0, mesh16.vertexBuffer.GetSize()); //IsUINT16 
  pass.SetIndexBuffer(mesh16.indexBuffer, mesh16.IsUINT16() ? wgpu::IndexFormat::Uint16 : wgpu::IndexFormat::Uint32, 0, mesh16.indexBuffer.GetSize()); //IsUINT16
  pass.DrawIndexed(mesh16.IndexCount(), 1, 0, 0, 0);
  pass.End();
  wgpu::CommandBuffer commands = encoder.Finish();
  device.GetQueue().Submit(1, &commands);
}

void InitGraphics() {
  
  PBR_Shader.LoadShader(FileReader::LoadRawString("/Shaders/test.wgsl"), {windowFormat});
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
