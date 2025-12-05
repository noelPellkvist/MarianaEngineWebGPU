#include <Renderpass.hpp>
#include "Init.hpp"
#include <webgpu/webgpu_cpp.h>
#include <Renderer.hpp>
#include <AssetManager.hpp>

struct Renderpass::Impl
{
    wgpu::RenderPassDepthStencilAttachment m_DepthStencilAttachment;
    wgpu::RenderPassEncoder pass;
};

Renderpass::Renderpass(bool MSSA, bool depthTexture, std::vector<TextureFormat> outputFormats, uint32_t width, uint32_t height) :
m_MSSA(MSSA),
m_HasDepthTexture(depthTexture),
m_OutputFormats(outputFormats),
m_Width(width),
m_Height(height),
_impl(std::make_unique<Impl>())
{

}

void Renderpass::Init(Scene& scene)
{
    CreateMSSATexture();
    if (m_HasDepthTexture) CreateDepthTexture();
}

void Renderpass::Recreate(uint32_t width, uint32_t height)
{
    m_Width = width;
    m_Height = height;
    CreateMSSATexture();
    if (m_HasDepthTexture) CreateDepthTexture();
}

Renderpass::~Renderpass() = default;

void* Renderpass::GetDepthStencilAttachment()
{
    return &_impl->m_DepthStencilAttachment;
}

void Renderpass::CreateMSSATexture()
{
    m_RenderTargets.resize(m_OutputFormats.size());
    for(int i = 0; i < m_OutputFormats.size(); i++)
    {
        Texture renderTarget;
        renderTarget.CreateRenderTexture(m_OutputFormats[i], m_Width, m_Height, m_MSSA);
        m_RenderTargets[i] = renderTarget;
    }
    if (m_MSSA)
    {
        m_RenderTargetsResolve.resize(m_OutputFormats.size());
        for(int i = 0; i < m_OutputFormats.size(); i++)
        {
            Texture renderTarget;
            renderTarget.CreateRenderTexture(m_OutputFormats[i], m_Width, m_Height, false);
            m_RenderTargetsResolve[i] = renderTarget;
        }
    }
}

void Renderpass::CreateDepthTexture()
{
    Texture depthTarget;
    depthTarget.CreateDepthTexture(TextureFormat::Depth24Plus, m_Width, m_Height, m_MSSA);
    m_DepthTexture = depthTarget;

    CreateDepthStencilAttachment();
}

void Renderpass::CreateDepthStencilAttachment()
{
    _impl->m_DepthStencilAttachment = {};
    _impl->m_DepthStencilAttachment.view = *static_cast<wgpu::TextureView*>(m_DepthTexture.GetTextureView());
    _impl->m_DepthStencilAttachment.depthClearValue = 1.0f;
    _impl->m_DepthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    _impl->m_DepthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
    _impl->m_DepthStencilAttachment.depthReadOnly = false;

    _impl->m_DepthStencilAttachment.stencilClearValue = 0;
    _impl->m_DepthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
    _impl->m_DepthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
    _impl->m_DepthStencilAttachment.stencilReadOnly = true;
}

#pragma region Drawing

void Renderpass::SetShader(IShader* shader)
{
    _impl->pass.SetPipeline(*static_cast<wgpu::RenderPipeline*>(shader->GetPipeline()));
}

void Renderpass::SetMesh(IMesh* mesh)
{
    _impl->pass.SetVertexBuffer(0, *static_cast<wgpu::Buffer*>(mesh->GetVertexBuffer()), 0, (*static_cast<wgpu::Buffer*>(mesh->GetVertexBuffer())).GetSize());
    _impl->pass.SetIndexBuffer(*static_cast<wgpu::Buffer*>(mesh->GetIndexBuffer()), mesh->IsUINT16() ? wgpu::IndexFormat::Uint16 :  wgpu::IndexFormat::Uint32, 0, (*static_cast<wgpu::Buffer*>(mesh->GetIndexBuffer())).GetSize());
}

void Renderpass::SetMaterial(IShader* shader, IMaterial* material, RendererComponent* rendererComp, uint32_t materialIndex)
{
    uint32_t materialBinding = shader->GetBindingsCount() - 1;
    for(uint32_t i = 0; i < materialBinding; i++)
    {
      size_t dynamicOffsetCount = shader->GetBufferDynamicOffsets(static_cast<uint32_t>(i));
      uint32_t offset = dynamicOffsetCount == 0 ? 0 : shader->GetBufferDynamicOffset(static_cast<uint32_t>(i), rendererComp->transformIndex);
      _impl->pass.SetBindGroup(i, *static_cast<wgpu::BindGroup*>(material->GetBindGroup(i)), dynamicOffsetCount, dynamicOffsetCount == 0 ? nullptr : &offset);
    }
    uint32_t materialOffset = shader->GetMaterialDynamicOffset(materialIndex);
    _impl->pass.SetBindGroup(materialBinding, *static_cast<wgpu::BindGroup*>(material->GetBindGroup(materialBinding)), 1, &materialOffset);
}

void Renderpass::Draw(uint32_t indexCount, uint32_t startIndex)
{
    _impl->pass.DrawIndexed(indexCount, 1, startIndex, 0, 0);
}


void Renderpass::Start(void* encoder, bool surfaceTarget, void* surfaceView)
{
    std::vector<wgpu::RenderPassColorAttachment> attachments(NumberOfOutputs());
    for (uint8_t i = 0; i < NumberOfOutputs(); i++)
    {
      attachments[i] = {
      .view = (i == 0 && surfaceTarget) ? *static_cast<wgpu::TextureView*>(surfaceView) : *static_cast<wgpu::TextureView*>(GetRenderTarget(i).GetTextureView()),
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store};
    }

    wgpu::RenderPassDescriptor renderpassDesc{.colorAttachmentCount = attachments.size(),
                                          .colorAttachments = attachments.size() == 0 ? nullptr : attachments.data(),
                                          .depthStencilAttachment = HasDepthTexture() ? static_cast<wgpu::RenderPassDepthStencilAttachment*>(GetDepthStencilAttachment()) : nullptr};
    wgpu::CommandEncoder R_encoder = *(static_cast<wgpu::CommandEncoder*>(encoder));
    _impl->pass = R_encoder.BeginRenderPass(&renderpassDesc);
    renderSystem.Run();
    _impl->pass.End();
}

#pragma endregion