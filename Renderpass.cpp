#include "Renderpass.hpp"

Renderpass::Renderpass(wgpu::TextureView outputImage, wgpu::TextureView depthTextureView) : 
outputImage(outputImage), 
depthTextureView(depthTextureView)
{
}

Renderpass::~Renderpass()
{

}

void Renderpass::Draw(wgpu::CommandEncoder encoder, wgpu::RenderPipeline pipeline, GameObject* g)
{
    wgpu::RenderPassColorAttachment attachment{
      .view = outputImage,
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

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
    pass.SetPipeline(pipeline);
    g->Draw(pass);
    pass.End();
}

void Renderpass::Draw(wgpu::CommandEncoder encoder, wgpu::RenderPipeline pipeline, GameObject* g, wgpu::SurfaceTexture surface)
{
    wgpu::RenderPassColorAttachment attachment{
      .view = surface.texture.CreateView(),
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

    wgpu::RenderPassDescriptor renderpass{
                                        .label = wgpu::StringView("first"),
                                        .colorAttachmentCount = 1,
                                        .colorAttachments = &attachment,
                                        .depthStencilAttachment = &depthStencilAttachment};

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
    pass.SetPipeline(pipeline);
    g->Draw(pass);
    pass.End();
}