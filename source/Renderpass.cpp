#include <Renderpass.hpp>
#include <Init.hpp>

Renderpass::Renderpass(bool MSSA, bool depthTexture, TextureFormat outputFormat, uint32_t width, uint32_t height) :
m_MSSA(MSSA),
m_HasDepthTexture(depthTexture),
m_OutputFormat(outputFormat),
m_Width(width),
m_Height(height)
{

}

void Renderpass::Init()
{
    if (m_MSSA) CreateMSSATexture();
    if (m_HasDepthTexture) CreateDepthTexture();
}

void Renderpass::Recreate(uint32_t width, uint32_t height)
{
    m_Width = width;
    m_Height = height;
    if (m_MSSA) CreateMSSATexture();
    if (m_HasDepthTexture) CreateDepthTexture();
}

Renderpass::~Renderpass()
{
    
}

void Renderpass::CreateMSSATexture()
{
    Texture renderTarget;
    renderTarget.CreateRenderTexture(m_OutputFormat, m_Width, m_Height, m_MSSA);
    m_RenderTarget = renderTarget;
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
    m_DepthStencilAttachment = {};
    m_DepthStencilAttachment.view = m_DepthTexture.GetTextureView();
    m_DepthStencilAttachment.depthClearValue = 1.0f;
    m_DepthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    m_DepthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
    m_DepthStencilAttachment.depthReadOnly = false;

    m_DepthStencilAttachment.stencilClearValue = 0;
    m_DepthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
    m_DepthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
    m_DepthStencilAttachment.stencilReadOnly = true;
}