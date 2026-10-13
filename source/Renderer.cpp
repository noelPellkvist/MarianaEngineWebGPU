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

void RenderGUI(wgpu::TextureView& view, wgpu::TextureView& resolve, wgpu::RenderPassDepthStencilAttachment* depth, wgpu::CommandEncoder& encoder, GUI& gui)
{
  wgpu::RenderPassColorAttachment attachment{
    .view = view,
    //.resolveTarget = resolve,
    .loadOp = wgpu::LoadOp::Load,
    .storeOp = wgpu::StoreOp::Store
  };

  wgpu::RenderPassDescriptor renderpassDesc{.colorAttachmentCount = 1,
                                        .colorAttachments = &attachment,
                                        .depthStencilAttachment = depth};

  auto pass = encoder.BeginRenderPass(&renderpassDesc);
  gui.PostUpdateGUI(&pass);
  pass.End();
}

void Renderer::Render(GUI* gui)
{
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    wgpu::TextureView resolveTarget = surfaceTexture.texture.CreateView();
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    for(auto& renderPass : m_Renderpasses)
    {
      bool surfaceTarget = &renderPass == &m_Renderpasses.back();
      if (!surfaceTarget && !renderPass->IsEnabled())
        continue;

      renderPass->Start(static_cast<void*>(&encoder), surfaceTarget, static_cast<void*>(&resolveTarget));

      // ImGui draws without depth, so its pass doesn't touch (or clear) a depth buffer.
      if(gui != nullptr && surfaceTarget && renderPass->NumberOfOutputs() > 0)
      {
        RenderGUI(resolveTarget,
                  resolveTarget,
                  nullptr,
                  encoder,
                  *gui);
      }
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}