#pragma once
#include <webgpu/webgpu_cpp.h>

class Renderpass 
{
    public:
        Renderpass(bool MSSA, bool depthTexture, wgpu::TextureFormat outputFormat, uint32_t width, uint32_t height);
        ~Renderpass();

        void Init();

        void Recreate(uint32_t width, uint32_t height);

        wgpu::TextureView& GetDepthTextureView() { return m_DepthTextureView; }
        wgpu::TextureView& GetMSSATextureView() { return m_MssaTextureView; }
        wgpu::RenderPassDepthStencilAttachment* GetDepthStencilAttachment() { return &m_DepthStencilAttachment; }

    private:
        bool m_MSSA = true;
        bool m_HasDepthTexture = true;
        wgpu::TextureFormat m_OutputFormat;
        uint32_t m_Width;
        uint32_t m_Height;

        wgpu::Texture m_DepthTexture;
        wgpu::TextureView m_DepthTextureView;

        wgpu::Texture m_MssaTexture;
        wgpu::TextureView m_MssaTextureView;

        wgpu::RenderPassDepthStencilAttachment m_DepthStencilAttachment;

        void CreateMSSATexture();
        void CreateDepthTexture();
        void CreateDepthStencilAttachment();
};