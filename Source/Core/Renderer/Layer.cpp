#include "Layer.hpp"
#include "../GlobalVaribles.hpp"
#include "../Resources.hpp"
#include <iostream>
#include "../Logging.hpp"
#include "../Components/Mesh.hpp"
#include "../Components/Transform.hpp"

RenderLayer::RenderLayer(wgpu::TextureFormat targetFormat, std::string standardShader, bool useDepthStencil) : targetFormat(targetFormat)
{
    shaders.emplace_back(standardShader, targetFormat);
    if (useDepthStencil) CreateDepthStencil();
}

RenderLayer::~RenderLayer()
{
}

void RenderLayer::CreateDepthStencil()
{
  if (m_DepthTextureView)
  {
    m_DepthTextureView = nullptr;
  }

  wgpu::TextureFormat depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
  wgpu::TextureDescriptor depthTextureDesc;
  depthTextureDesc.dimension = wgpu::TextureDimension::e2D;
  depthTextureDesc.format = depthTextureFormat;
  depthTextureDesc.mipLevelCount = 1;
  depthTextureDesc.sampleCount = 1;
  depthTextureDesc.size = {(uint32_t)1336, (uint32_t)768, 1};
  depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
  depthTextureDesc.viewFormatCount = 1;
  depthTextureDesc.viewFormats = &depthTextureFormat;
  wgpu::Texture depthTexture = device.CreateTexture(&depthTextureDesc);

  wgpu::TextureViewDescriptor depthTextureViewDesc;
  depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
  depthTextureViewDesc.baseArrayLayer = 0;
  depthTextureViewDesc.arrayLayerCount = 1;
  depthTextureViewDesc.baseMipLevel = 0;
  depthTextureViewDesc.mipLevelCount = 1;
  depthTextureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
  depthTextureViewDesc.format = depthTextureFormat;
  m_DepthTextureView = depthTexture.CreateView(&depthTextureViewDesc);

  m_DepthStencilAttachment.view = m_DepthTextureView;
  m_DepthStencilAttachment.depthClearValue = 1;
  m_DepthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
  m_DepthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
  m_DepthStencilAttachment.depthReadOnly = false;
  m_DepthStencilAttachment.stencilClearValue = 0;
  m_DepthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
  m_DepthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
  m_DepthStencilAttachment.stencilReadOnly = true;
}

void RenderLayer::Render(std::vector<wgpu::TextureView>& targets, entt::registry& reg)
{
  std::vector<wgpu::RenderPassColorAttachment> attachments(targets.size());
  for (size_t i = 0; i < targets.size(); i++)
  {
    //surfaceTexture.texture.CreateView()
    attachments[i] = {
      .view = targets[i],
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store
    };
  }
  

    wgpu::RenderPassDescriptor renderpass{.colorAttachmentCount = (uint32_t)attachments.size(),
                                          .colorAttachments = attachments.data(),
                                          .depthStencilAttachment = &m_DepthStencilAttachment};

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
    gui->DrawGUI(pass);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}