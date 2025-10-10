#include <Renderpass.hpp>
#include "Init.hpp"
#include <webgpu/webgpu_cpp.h>

struct Renderpass::Impl
{
    wgpu::RenderPassDepthStencilAttachment m_DepthStencilAttachment;
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
    if (m_MSSA) CreateMSSATexture();
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