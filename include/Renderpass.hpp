#pragma once
#include <Texture.hpp>
#include <ECS.hpp>
#include <memory>
#include <vector>

class Renderer;
class IShader;
class IMesh;
class IMaterial;
struct RendererComponent;

class Material2;
class Shader2;

class Renderpass 
{
    public:
        Renderpass(bool MSSA, bool depthTexture, std::vector<TextureFormat> outputFormats, uint32_t width, uint32_t height);
        ~Renderpass();

        void Init();

        System renderSystem;

        void Recreate(uint32_t width, uint32_t height);
        uint8_t NumberOfOutputs() { return m_OutputFormats.size(); }
        Texture& GetRenderTarget(uint8_t index) { return m_RenderTargets[index]; }
        Texture& GetRenderResloveTarget(uint8_t index) { return m_RenderTargetsResolve[index]; }
        Texture& GetDepthView() { return m_DepthTexture; }
        const std::vector<TextureFormat>& GetOutputFormats() const { return m_OutputFormats; }
        void* GetDepthStencilAttachment();
        bool HasDepthTexture() const { return m_HasDepthTexture; }

        void SetShader(IShader* shader, uint32_t transformIndex);
        void SetMesh(IMesh* shader);
        void SetMaterial(IShader* shader, IMaterial* material, RendererComponent* rendererComp, uint32_t materialIndex);
        void Draw(uint32_t indexCount, uint32_t startIndex);

        void SetShader2(Shader2& shader, uint32_t transformIndex);
        void SetMaterial2(Shader2& shader, Material2& material, uint32_t materialIndex);

        

    private:
        friend class Renderer;
        
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