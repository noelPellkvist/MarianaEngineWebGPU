#include <Mariana/Rendering/Renderer.hpp>
#include <Mariana/Scene/ECS.hpp>
#include "Graphics/Init.hpp"
#include "Graphics/GpuTimer.hpp"
#include <Mariana/Foundation/Profiler.hpp>
#include <Mariana/Graphics/Material.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Graphics/Shader.hpp>
#include <Mariana/Graphics/Texture.hpp>
#include <webgpu/webgpu_cpp.h>

Renderer::Renderer()
{

}

Renderer::~Renderer()
{
  
}

// The overlay draws on top of the frame (or a cleared screen), without depth.
static void RecordOverlay(wgpu::TextureView& view, wgpu::CommandEncoder& encoder, const Renderer::Overlay& overlay, bool clear)
{
  wgpu::RenderPassColorAttachment attachment{
    .view = view,
    .loadOp = clear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
    .storeOp = wgpu::StoreOp::Store,
    .clearValue = { 0.06, 0.06, 0.07, 1.0 }
  };

  wgpu::RenderPassDescriptor renderpassDesc{.colorAttachmentCount = 1,
                                        .colorAttachments = &attachment,
                                        .depthStencilAttachment = nullptr};
  static const std::string kLabel = "UI";
  wgpu::PassTimestampWrites timestamps;
  if (GpuTimer::Pass(kLabel, timestamps))
    renderpassDesc.timestampWrites = &timestamps;

  MARIANA_PROFILE("Encode UI");
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
      RecordOverlay(backbuffer, encoder, overlay, false);

    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}

void Renderer::RenderOverlay(const Overlay& overlay)
{
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    wgpu::TextureView backbuffer = surfaceTexture.texture.CreateView();
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    RecordOverlay(backbuffer, encoder, overlay, true);
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}

void Renderer::RenderToTexture(Texture& target)
{
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    graph.Execute(static_cast<void*>(&encoder), target.GetTextureView());
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}
