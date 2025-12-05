#include <Renderer.hpp>
#include <ECS.hpp>
#include "Init.hpp"
#include <Material.hpp>
#include <Mesh.hpp>
#include <Shader.hpp>
#include <GUI.hpp>
#include <AssetManager.hpp>
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

      std::vector<wgpu::RenderPassColorAttachment> attachments(renderPass->NumberOfOutputs());
      for (uint8_t i = 0; i < renderPass->NumberOfOutputs(); i++)
      {
        attachments[i] = {
        .view = (i == 0 && surfaceTarget) ? resolveTarget : *static_cast<wgpu::TextureView*>(renderPass->GetRenderTarget(i).GetTextureView()),
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store};
      }
      wgpu::RenderPassDescriptor renderpassDesc{.colorAttachmentCount = attachments.size(),
                                          .colorAttachments = attachments.size() == 0 ? nullptr : attachments.data(),
                                          .depthStencilAttachment = renderPass->HasDepthTexture() ? static_cast<wgpu::RenderPassDepthStencilAttachment*>(renderPass->GetDepthStencilAttachment()) : nullptr};

      
      renderPass->Start(static_cast<void*>(&encoder), surfaceTarget, static_cast<void*>(&resolveTarget));
      

      if(gui != nullptr && surfaceTarget && renderPass->NumberOfOutputs() > 0)
      {
        RenderGUI(resolveTarget, 
                  resolveTarget,
                  renderPass->HasDepthTexture() ? static_cast<wgpu::RenderPassDepthStencilAttachment*>(renderPass->GetDepthStencilAttachment()) : nullptr,
                  encoder,
                  *gui);
      }
    }
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}