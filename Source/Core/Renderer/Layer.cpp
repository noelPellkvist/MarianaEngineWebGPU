#include "Layer.hpp"
#include "../GlobalVaribles.hpp"
#include <iostream>

RenderLayer::RenderLayer(wgpu::TextureFormat targetFormat) : targetFormat(targetFormat)
{
    Shader newShader("", targetFormat);
    shaders.push_back(newShader);
}

RenderLayer::~RenderLayer()
{
}

void RenderLayer::Render(wgpu::SurfaceTexture& surfaceTexture)
{
    wgpu::RenderPassColorAttachment attachment{
      .view = surfaceTexture.texture.CreateView(),
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store};

    wgpu::RenderPassDescriptor renderpass{.colorAttachmentCount = 1,
                                          .colorAttachments = &attachment};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
    pass.SetPipeline(shaders[0].GetRenderPipeline());
    pass.SetVertexBuffer(0, shaders[0].vertexBuffer);
    pass.Draw(3);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}