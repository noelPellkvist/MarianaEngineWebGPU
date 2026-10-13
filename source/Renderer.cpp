#include <Renderer.hpp>
#include <ECS.hpp>
#include "Init.hpp"
#include <Material.hpp>
#include <Mesh.hpp>
#include <Shader.hpp>
#include <webgpu/webgpu_cpp.h>

Renderer::Renderer()
{

}

Renderer::~Renderer()
{
  
}

// The overlay draws on top of the frame, without depth.
void RenderOverlay(wgpu::TextureView& view, wgpu::CommandEncoder& encoder, const Renderer::Overlay& overlay)
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
  overlay(static_cast<void*>(&pass));
  pass.End();
}

void Renderer::Render(const Overlay& overlay)
{
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    wgpu::TextureView backbuffer = surfaceTexture.texture.CreateView();
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    graph.Execute(static_cast<void*>(&encoder), static_cast<void*>(&backbuffer));
    if (overlay)
      RenderOverlay(backbuffer, encoder, overlay);

    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}