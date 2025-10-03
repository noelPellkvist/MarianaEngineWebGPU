#include <Renderer.hpp>
#include <ECS.hpp>
#include <Init.hpp>
#include <Material.hpp>
#include <Mesh.hpp>
#include <Shader.hpp>
#include <GUI.hpp>
#include <AssetManager.hpp>


Renderer::Renderer()
{

}

Renderer::~Renderer()
{
  
}

void Renderer::Init(Scene& scene)
{
    renderSystem = scene.CreateSystem<RendererComponent>([&](Entity ent, RendererComponent& rendererComp, float dt){
        auto shader = AssetManager::LoadedShaders[rendererComp.shaderIndex];
        pass.SetPipeline(shader->GetPipeline());
        auto& mesh = AssetManager::LoadedMeshes[rendererComp.meshIndex];
        pass.SetVertexBuffer(0, mesh->vertexBuffer, 0, mesh->vertexBuffer.GetSize());

        pass.SetIndexBuffer(mesh->indexBuffer, wgpu::IndexFormat::Uint32, 0, mesh->indexBuffer.GetSize());

        uint32_t transformOffset = 0;
        uint32_t materialOffset = 0;

        for (Submesh& sm : mesh->submeshes)
        {
          IMaterial& material = *(AssetManager::LoadedMaterials[sm.materialIndex]);
          transformOffset = shader->GetTransformDynamicOffset(rendererComp.transformIndex);
          materialOffset = shader->GetMaterialDynamicOffset(sm.materialIndex);
          pass.SetBindGroup(0, material.GetBindGroup(0), 0, nullptr);
          pass.SetBindGroup(1, material.GetBindGroup(1), 1, &transformOffset);
          pass.SetBindGroup(2, material.GetBindGroup(2), 1, &materialOffset);
          pass.SetBindGroup(3, material.GetBindGroup(3), 0, nullptr);
          pass.DrawIndexed(sm.indexCount, 1, sm.startIndex, 0, 0);
        }
    });
}

void Renderer::Render(Renderpass& renderPass, GUI& gui)
{
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    wgpu::RenderPassColorAttachment attachment{
      .view = renderPass.GetMSSATextureView(),
      .resolveTarget = surfaceTexture.texture.CreateView(),
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store};

    wgpu::RenderPassDescriptor renderpassDesc{.colorAttachmentCount = 1,
                                        .colorAttachments = &attachment,
                                        .depthStencilAttachment = renderPass.GetDepthStencilAttachment()};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    pass = encoder.BeginRenderPass(&renderpassDesc);
    renderSystem.Run();
    gui.PostUpdateGUI(pass);
    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}