#include <Renderer.hpp>
#include <ECS.hpp>
#include <Init.hpp>
#include <Material.hpp>
#include <Shader.hpp>
#include <moved_later/GUI.hpp>

Renderer::Renderer()
{

}

Renderer::~Renderer()
{

}

void Renderer::Render(Renderpass& renderPass, GUI gui, Material& mat, Shader& shader, wgpu::Buffer vertexBuffer, wgpu::Buffer indexBuffer, uint32_t IndexCount)
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
    pass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
    pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32, 0, indexBuffer.GetSize());
    pass.SetBindGroup(0, shader.GetBindGroup(), 0, nullptr);
    uint32_t dynamicOffset = 0;
    pass.SetBindGroup(2, mat.GetTextureBindGroup(), 0, nullptr);

    for(int i = 0; i < 1; i++)
    {
      pass.SetBindGroup(1, shader.GetModelBindGroup(), 1, &dynamicOffset);
      pass.DrawIndexed(IndexCount, 1, 0, 0, 0);
      dynamicOffset += 256;
    }
    gui.PostUpdateGUI(pass);
    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}