#include <Renderer.hpp>
#include <ECS.hpp>
#include "Init.hpp"
#include <Material.hpp>
#include <Mesh.hpp>
#include <Shader.hpp>
#include <GUI.hpp>
#include <AssetManager.hpp>
#include <webgpu/webgpu_cpp.h>

struct Renderer::Impl
{
  wgpu::RenderPassEncoder pass;
};

Renderer::Renderer() : _impl(std::make_unique<Impl>())
{

}

Renderer::~Renderer()
{
  
}

void Renderer::Init(Scene& scene)
{
    renderSystem = scene.CreateSystem<RendererComponent>([&](Entity ent, RendererComponent& rendererComp, float dt){
        wgpu::RenderPassEncoder& pass = _impl->pass;
        auto shader = AssetManager::LoadedShaders[rendererComp.shaderIndex];
        pass.SetPipeline(*static_cast<wgpu::RenderPipeline*>(shader->GetPipeline()));
        auto& mesh = AssetManager::LoadedMeshes[rendererComp.meshIndex];
        pass.SetVertexBuffer(0, *static_cast<wgpu::Buffer*>(mesh->GetVertexBuffer()), 0, (*static_cast<wgpu::Buffer*>(mesh->GetVertexBuffer())).GetSize());

        pass.SetIndexBuffer(*static_cast<wgpu::Buffer*>(mesh->GetIndexBuffer()), wgpu::IndexFormat::Uint32, 0, (*static_cast<wgpu::Buffer*>(mesh->GetIndexBuffer())).GetSize());

        for (Submesh& sm : mesh->submeshes)
        {
          IMaterial& material = *(AssetManager::LoadedMaterials[sm.materialIndex]);
          uint32_t materialBinding = shader->GetBindingsCount() - 1;
          for(uint32_t i = 0; i < materialBinding; i++)
          {
            size_t dynamicOffsetCount = shader->GetBufferDynamicOffsets(static_cast<uint32_t>(i));
            uint32_t offset = dynamicOffsetCount == 0 ? 0 : shader->GetBufferDynamicOffset(static_cast<uint32_t>(i), rendererComp.transformIndex);
            pass.SetBindGroup(i, *static_cast<wgpu::BindGroup*>(material.GetBindGroup(i)), dynamicOffsetCount, dynamicOffsetCount == 0 ? nullptr : &offset);
          }
          uint32_t materialOffset = shader->GetMaterialDynamicOffset(sm.materialIndex);
          pass.SetBindGroup(materialBinding, *static_cast<wgpu::BindGroup*>(material.GetBindGroup(materialBinding)), 1, &materialOffset);
          pass.DrawIndexed(sm.indexCount, 1, sm.startIndex, 0, 0);
        }
    });
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

void Renderer::Render(Renderpass& renderPass, GUI& gui)
{
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    wgpu::TextureView resolveTarget = surfaceTexture.texture.CreateView();

    std::vector<wgpu::RenderPassColorAttachment> attachments(renderPass.NumberOfOutputs());
    for (uint8_t i = 0; i < renderPass.NumberOfOutputs(); i++)
    {
      attachments[i] = {
      .view = (i == 0) ? resolveTarget : *static_cast<wgpu::TextureView*>(renderPass.GetRenderTarget(i).GetTextureView()),
      //.resolveTarget = (i == 0) ? resolveTarget : *static_cast<wgpu::TextureView*>(renderPass.GetRenderResloveTarget(i).GetTextureView()),
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store};
    }

    wgpu::RenderPassDescriptor renderpassDesc{.colorAttachmentCount = attachments.size(),
                                        .colorAttachments = attachments.data(),
                                        .depthStencilAttachment = static_cast<wgpu::RenderPassDepthStencilAttachment*>(renderPass.GetDepthStencilAttachment())};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    _impl->pass = encoder.BeginRenderPass(&renderpassDesc);
    renderSystem.Run();
    
    _impl->pass.End();

    RenderGUI(resolveTarget, //*static_cast<wgpu::TextureView*>(renderPass.GetRenderTarget(0).GetTextureView()),
              resolveTarget,
              static_cast<wgpu::RenderPassDepthStencilAttachment*>(renderPass.GetDepthStencilAttachment()),
              encoder,
              gui);

    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}