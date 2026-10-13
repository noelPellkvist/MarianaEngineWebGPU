#include <Mariana/Graphics/Renderpass.hpp>
#include "Graphics/Init.hpp"
#include <webgpu/webgpu_cpp.h>
#include <Mariana/Rendering/Renderer.hpp>
#include <Mariana/Graphics/Material.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Graphics/Shader.hpp>

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

void Renderpass::Init()
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

void Renderpass::SetMesh(IMesh* mesh)
{
    if (mesh == m_CurrentMesh) return;
    m_CurrentMesh = mesh;

    const wgpu::Buffer& vertexBuffer = *static_cast<wgpu::Buffer*>(mesh->GetVertexBuffer());
    const wgpu::Buffer& indexBuffer = *static_cast<wgpu::Buffer*>(mesh->GetIndexBuffer());
    _impl->pass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
    _impl->pass.SetIndexBuffer(indexBuffer, mesh->IsUINT16() ? wgpu::IndexFormat::Uint16 :  wgpu::IndexFormat::Uint32, 0, indexBuffer.GetSize());
}

void Renderpass::BindShaderGroups()
{
    assert(m_CurrentShader != nullptr);
    if (m_BindGroupsDirty)
    {
        // The shader's own groups only change with the shader or its dynamic offsets.
        Shader2& shader = *m_CurrentShader;
        uint32_t shaderBindingCount = shader.GetGroupCount();
        for(uint32_t i = 0; i < shaderBindingCount; i++)
        {
            if (i == shader.m_MaterialIndex) continue;
            _impl->pass.SetBindGroup(i, *static_cast<wgpu::BindGroup*>(shader.GetBindGroup(i)), m_BindGroupOffsets[i].size(), m_BindGroupOffsets[i].data());
        }
        m_BindGroupsDirty = false;
    }
}

void Renderpass::Draw(uint32_t indexCount, uint32_t startIndex, uint32_t id, uint32_t instanceCount)
{
    BindShaderGroups();
    _impl->pass.DrawIndexed(indexCount, instanceCount, startIndex, 0, id);
}

void Renderpass::DrawFullscreen()
{
    BindShaderGroups();
    _impl->pass.Draw(3, 1, 0, 0);
}

void Renderpass::SetBufferIndex(std::string name, uint32_t index)
{
    assert(m_CurrentShader != nullptr);
    assert(std::holds_alternative<BufferResource>(*m_CurrentShader->m_BindGroupLayoutMap[name]));
    const BufferResource& br = std::get<BufferResource>(*m_CurrentShader->m_BindGroupLayoutMap[name]);
    assert(br.buffer->IsDynamic());
    uint32_t& offset = m_BindGroupOffsets[br.group][br.dynamicBufferOffsetIndex];
    const uint32_t newOffset = br.buffer->GetStride() * index;
    if (offset == newOffset) return;
    offset = newOffset;
    m_BindGroupsDirty = true;
}

void Renderpass::SetShader2(Shader2& shader, ShaderVariant variant)
{
    if (&shader == m_CurrentShader)
    {
        // Same shader, other variant: same layout, so the bound groups and material stay.
        if (variant != m_CurrentVariant)
        {
            m_CurrentVariant = variant;
            _impl->pass.SetPipeline(*static_cast<wgpu::RenderPipeline*>(shader.GetPipeline(variant)));
        }
        return;
    }

    _impl->pass.SetPipeline(*static_cast<wgpu::RenderPipeline*>(shader.GetPipeline(variant)));
    m_CurrentVariant = variant;
    m_BindGroupOffsets.resize(shader.GetGroupCount());
    for (size_t i = 0; i < m_BindGroupOffsets.size(); i++)
    {
        m_BindGroupOffsets[i].assign(shader.Group(i).GetDynamicBufferCount(), 0);
    }
    m_CurrentShader = &shader;
    m_CurrentMaterial = nullptr;
    m_BindGroupsDirty = true;
}

void Renderpass::SetMaterial2(Shader2& shader, Material2& material)
{
    void* bindGroup = material.GetBindGroup();
    if (bindGroup == m_CurrentMaterial) return;
    m_CurrentMaterial = bindGroup;
    _impl->pass.SetBindGroup(shader.m_MaterialIndex, *static_cast<wgpu::BindGroup*>(bindGroup), 0, nullptr);
}

void Renderpass::SetTargets(std::vector<Texture*> colors, std::vector<Texture*> resolves, Texture* depth, uint32_t width, uint32_t height)
{
    m_External = true;
    m_ColorTargets = std::move(colors);
    m_ResolveTargets = std::move(resolves);
    m_DepthTarget = depth;
    m_Width = width;
    m_Height = height;
}

void Renderpass::SetLoadOps(std::vector<bool> loadColor, bool loadDepth)
{
    m_LoadColor = std::move(loadColor);
    m_LoadDepth = loadDepth;
}

void Renderpass::Start(void* encoder, void* backbuffer)
{
    auto viewOf = [](Texture* texture) { return *static_cast<wgpu::TextureView*>(texture->GetTextureView()); };

    std::vector<wgpu::RenderPassColorAttachment> attachments(NumberOfOutputs());
    for (uint8_t i = 0; i < NumberOfOutputs(); i++)
    {
      Texture* target = m_External ? m_ColorTargets[i] : &m_RenderTargets[i];
      Texture* resolve = m_External ? (i < m_ResolveTargets.size() ? m_ResolveTargets[i] : nullptr)
                                    : (m_MSSA ? &m_RenderTargetsResolve[i] : nullptr);
      const bool load = i < m_LoadColor.size() && m_LoadColor[i];
      // MSAA targets resolve into the single-sample ones (which later passes read); the samples
      // themselves aren't kept.
      attachments[i] = {
      .view = target ? viewOf(target) : *static_cast<wgpu::TextureView*>(backbuffer),
      .resolveTarget = resolve ? viewOf(resolve) : wgpu::TextureView(),
      .loadOp = load ? wgpu::LoadOp::Load : wgpu::LoadOp::Clear,
      .storeOp = resolve ? wgpu::StoreOp::Discard : wgpu::StoreOp::Store};
    }

    wgpu::RenderPassDepthStencilAttachment depthAttachment{};
    if (HasDepthTexture())
    {
        depthAttachment.view = viewOf(m_External ? m_DepthTarget : &m_DepthTexture);
        depthAttachment.depthClearValue = 1.0f;
        depthAttachment.depthLoadOp = m_LoadDepth ? wgpu::LoadOp::Load : wgpu::LoadOp::Clear;
        depthAttachment.depthStoreOp = wgpu::StoreOp::Store;
        depthAttachment.depthReadOnly = false;
        depthAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
        depthAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
        depthAttachment.stencilReadOnly = true;
    }

    wgpu::RenderPassDescriptor renderpassDesc{.colorAttachmentCount = attachments.size(),
                                          .colorAttachments = attachments.size() == 0 ? nullptr : attachments.data(),
                                          .depthStencilAttachment = HasDepthTexture() ? &depthAttachment : nullptr};
    wgpu::CommandEncoder R_encoder = *(static_cast<wgpu::CommandEncoder*>(encoder));
    _impl->pass = R_encoder.BeginRenderPass(&renderpassDesc);

    // Nothing is bound in a fresh pass.
    m_CurrentShader = nullptr;
    m_CurrentMesh = nullptr;
    m_CurrentMaterial = nullptr;
    m_BindGroupsDirty = true;

    renderSystem.Run();
    if (recordCallback) recordCallback();
    _impl->pass.End();
}

#pragma endregion

