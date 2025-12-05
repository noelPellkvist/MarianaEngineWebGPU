#pragma once
#include <Texture.hpp>
#include <ECS.hpp>
#include <memory>
#include <vector>

class Renderer;
class IShader;
class IMesh;
class IMaterial;

class Renderpass 
{
    public:
        Renderpass(bool MSSA, bool depthTexture, std::vector<TextureFormat> outputFormats, uint32_t width, uint32_t height);
        ~Renderpass();

        void Init(Scene& scene);

        void Recreate(uint32_t width, uint32_t height);
        uint8_t NumberOfOutputs() { return m_OutputFormats.size(); }
        Texture& GetRenderTarget(uint8_t index) { return m_RenderTargets[index]; }
        Texture& GetRenderResloveTarget(uint8_t index) { return m_RenderTargetsResolve[index]; }
        Texture& GetDepthView() { return m_DepthTexture; }
        const std::vector<TextureFormat>& GetOutputFormats() const { return m_OutputFormats; }
        void* GetDepthStencilAttachment();
        bool HasDepthTexture() const { return m_HasDepthTexture; }

        void SetShader(IShader* shader);
        void SetMesh(IMesh* shader);
        void SetMaterial(IMaterial* shader);
        void Draw(uint32_t indexCount, uint32_t startIndex);

        

    private:
        friend class Renderer;
        System renderSystem;
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

        void Start(void* encoder, bool surfaceTarget, void* resolveTarget);
};