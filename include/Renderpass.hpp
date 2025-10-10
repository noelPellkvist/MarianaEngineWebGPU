#pragma once
#include <Texture.hpp>
#include <memory>
#include <vector>

class Renderpass 
{
    public:
        Renderpass(bool MSSA, bool depthTexture, std::vector<TextureFormat> outputFormats, uint32_t width, uint32_t height);
        ~Renderpass();

        void Init();

        void Recreate(uint32_t width, uint32_t height);
        uint8_t NumberOfOutputs() { return m_OutputFormats.size(); }
        Texture& GetRenderTarget(uint8_t index) { return m_RenderTargets[index]; }
        Texture& GetRenderResloveTarget(uint8_t index) { return m_RenderTargetsResolve[index]; }
        Texture& GetDepthView() { return m_DepthTexture; }
        const std::vector<TextureFormat>& GetOutputFormats() const { return m_OutputFormats; }
        void* GetDepthStencilAttachment();

    private:
        bool m_MSSA = true;
        bool m_HasDepthTexture = true;
        std::vector<TextureFormat> m_OutputFormats;
        uint32_t m_Width;
        uint32_t m_Height;

        std::vector<Texture> m_RenderTargets;
        std::vector<Texture> m_RenderTargetsResolve;
        Texture m_DepthTexture;

        struct Impl;
        std::unique_ptr<Impl> _impl;

        void CreateMSSATexture();
        void CreateDepthTexture();
        void CreateDepthStencilAttachment();
};