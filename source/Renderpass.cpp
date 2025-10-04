#include <Renderpass.hpp>
#include <Init.hpp>

Renderpass::Renderpass(bool MSSA, bool depthTexture, wgpu::TextureFormat outputFormat, uint32_t width, uint32_t height) :
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
    m_MssaTexture = nullptr;
    m_MssaTextureView = nullptr;

    wgpu::TextureDescriptor mssaDesc;
    mssaDesc.dimension = wgpu::TextureDimension::e2D;
    mssaDesc.format = windowFormat;
    mssaDesc.mipLevelCount = 1;
    mssaDesc.sampleCount = m_MSSA ? 4 : 1;
    mssaDesc.size = {m_Width, m_Height, 1};
    mssaDesc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    m_MssaTexture = device.CreateTexture(&mssaDesc);

    m_MssaTextureView = m_MssaTexture.CreateView();
}

void Renderpass::CreateDepthTexture()
{
    m_DepthTexture = nullptr;
    m_DepthTextureView = nullptr;

    wgpu::TextureFormat depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
    wgpu::TextureDescriptor depthTextureDesc;
    depthTextureDesc.dimension = wgpu::TextureDimension::e2D;
    depthTextureDesc.format = depthTextureFormat;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = m_MSSA ? 4 : 1;
    depthTextureDesc.size = {m_Width, m_Height, 1};
    depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
    m_DepthTexture = device.CreateTexture(&depthTextureDesc);

    wgpu::TextureViewDescriptor depthTextureViewDesc;
    depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.baseMipLevel = 0;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    depthTextureViewDesc.format = depthTextureFormat;
    m_DepthTextureView = m_DepthTexture.CreateView(&depthTextureViewDesc);

    CreateDepthStencilAttachment();
}

void Renderpass::CreateDepthStencilAttachment()
{
    m_DepthStencilAttachment = {};
    m_DepthStencilAttachment.view = m_DepthTextureView;
    m_DepthStencilAttachment.depthClearValue = 1.0f;
    m_DepthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    m_DepthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
    m_DepthStencilAttachment.depthReadOnly = false;

    m_DepthStencilAttachment.stencilClearValue = 0;
    m_DepthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
    m_DepthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
    m_DepthStencilAttachment.stencilReadOnly = true;
}