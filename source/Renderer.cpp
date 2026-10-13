#include <Renderer.hpp>
#include <ECS.hpp>
#include "Init.hpp"
#include <Material.hpp>
#include <Mesh.hpp>
#include <Shader.hpp>
#include <GUI.hpp>
#include <webgpu/webgpu_cpp.h>

Renderer::Renderer()
{

}

Renderer::~Renderer()
{
  
}

// ImGui draws on top of the frame, without depth.
void RenderGUI(wgpu::TextureView& view, wgpu::CommandEncoder& encoder, GUI& gui)
{
  wgpu::RenderPassColorAttachment attachment{
    .view = view,
    .loadOp = wgpu::LoadOp::Load,
    .storeOp = wgpu::StoreOp::Store
  };

  wgpu::RenderPassDescriptor renderpassDesc{.colorAttachmentCount = 1,
                                        .colorAttachments = &attachment,
                                        .depthStencilAttachment = nullptr};

  auto pass = encoder.BeginRenderPass(&renderpassDesc);
  gui.PostUpdateGUI(&pass);
  pass.End();
}

void Renderer::Render(GUI* gui)
{
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    wgpu::TextureView backbuffer = surfaceTexture.texture.CreateView();
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    graph.Execute(static_cast<void*>(&encoder), static_cast<void*>(&backbuffer));
    if (gui != nullptr)
      RenderGUI(backbuffer, encoder, *gui);

    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}