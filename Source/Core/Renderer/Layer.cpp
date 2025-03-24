#include "Layer.hpp"
#include "../GlobalVaribles.hpp"
#include "../Resources.hpp"
#include <iostream>
#include "../Logging.hpp"
#include "../Components/Mesh.hpp"
#include "../Components/Transform.hpp"

RenderLayer::RenderLayer(wgpu::TextureFormat targetFormat, std::string standardShader) : targetFormat(targetFormat)
{
    shaders.emplace_back(standardShader, targetFormat);
    
}

RenderLayer::~RenderLayer()
{
}

void RenderLayer::Render(wgpu::SurfaceTexture& surfaceTexture, entt::registry& reg)
{
    wgpu::RenderPassColorAttachment attachment{
      .view = surfaceTexture.texture.CreateView(),
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store};

    wgpu::RenderPassDescriptor renderpass{.colorAttachmentCount = 1,
                                          .colorAttachments = &attachment};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
    auto view = reg.view<const Transform, const Mesh>();
    
    for(auto [entity, transform, mesh]: view.each()) {
      pass.SetPipeline(shaders[mesh.shaderIndex].GetRenderPipeline());
      pass.SetBindGroup(0, shaders[mesh.shaderIndex].UBOData.bindGroup);
      pass.SetVertexBuffer(0, mesh.vertexBuffer, 0, mesh.vertexBuffer.GetSize());
      pass.SetIndexBuffer(mesh.indexBuffer, wgpu::IndexFormat::Uint16, 0, mesh.indexBuffer.GetSize());
      
      uint32_t transformOffset = shaders[mesh.shaderIndex].TransformData.uniformStride * transform.dataIndex;
      pass.SetBindGroup(1, shaders[mesh.shaderIndex].TransformData.bindGroup, 1, &transformOffset);
      pass.DrawIndexed(mesh.indexCount, 1, 0, 0);
    }
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);

    
}