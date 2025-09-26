#include <Renderer.hpp>
#include <ECS.hpp>
#include <Init.hpp>
#include <Material.hpp>
#include <Shader.hpp>
#include <moved_later/GUI.hpp>
#include <AssetManager.hpp>


Renderer::Renderer()
{

}

Renderer::~Renderer()
{
  
}

void Renderer::Render(ICamera& camera, Renderpass& renderPass, GUI gui, Shader& shader)
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
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpassDesc);
    pass.SetPipeline(shader.GetPipeline());
    auto& mesh = AssetManager::LoadedMeshes[0];
    pass.SetVertexBuffer(0, mesh.vertexBuffer, 0, mesh.vertexBuffer.GetSize());
    
    pass.SetIndexBuffer(mesh.indexBuffer, wgpu::IndexFormat::Uint32, 0, mesh.indexBuffer.GetSize());
    pass.SetBindGroup(0, shader.GetBindGroup(), 0, nullptr);
    uint32_t dynamicOffset = 0;
    
    pass.SetBindGroup(3, camera.GetBinding().GetBindGroup(), 0, nullptr);

    pass.SetBindGroup(1, shader.GetModelBindGroup(), 1, &dynamicOffset);
    for (Submesh& sm : mesh.submeshes)
    {
      pass.SetBindGroup(2, AssetManager::LoadedMaterials[sm.materialIndex].GetTextureBindGroup(), 0, nullptr);
      pass.DrawIndexed(sm.indexCount, 1, sm.startIndex, 0, 0);
    }
    gui.PostUpdateGUI(pass);
    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}