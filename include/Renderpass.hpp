#pragma once
#include <Texture.hpp>
#include <memory>

class Renderpass 
{
    public:
        Renderpass(bool MSSA, bool depthTexture, TextureFormat outputFormat, uint32_t width, uint32_t height);
        ~Renderpass();

        void Init();

        void Recreate(uint32_t width, uint32_t height);
        Texture& GetRenderTarget() { return m_RenderTarget; }
        Texture& GetDepthView() { return m_DepthTexture; }
        void* GetDepthStencilAttachment();// { return &m_DepthStencilAttachment; }

    private:
        bool m_MSSA = true;
        bool m_HasDepthTexture = true;
        TextureFormat m_OutputFormat;
        uint32_t m_Width;
        uint32_t m_Height;

        Texture m_RenderTarget;
        Texture m_DepthTexture;

        struct Impl;
        std::unique_ptr<Impl> _impl;

        void CreateMSSATexture();
        void CreateDepthTexture();
        void CreateDepthStencilAttachment();
};