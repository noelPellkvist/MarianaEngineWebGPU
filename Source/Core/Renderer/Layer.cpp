#include "Layer.hpp"
#include "../GlobalVaribles.hpp"
#include "../Resources.hpp"
#include <iostream>
#include "../Logging.hpp"

RenderLayer::RenderLayer(wgpu::TextureFormat targetFormat) : targetFormat(targetFormat)
{
    Shader newShader("", targetFormat);
    shaders.push_back(newShader);
    mesh = Resources::LoadObjMesh("cube.obj", shaders[0]);
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
    pass.SetVertexBuffer(0, mesh.vertexBuffer, 0, mesh.vertexBuffer.GetSize());
    pass.SetIndexBuffer(mesh.indexBuffer, wgpu::IndexFormat::Uint16, 0, mesh.indexBuffer.GetSize());
    pass.SetBindGroup(0, shaders[0].UBOData.bindGroup);
    pass.DrawIndexed(mesh.indexCount, 1, 0, 0);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}