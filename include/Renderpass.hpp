#pragma once
#include <Texture.hpp>
#include <ECS.hpp>
#include <functional>
#include <memory>
#include <vector>

class Renderer;
class IShader;
class IMesh;
class IMaterial;
struct RendererComponent;

class Material2;
class Shader2;

// Pipelines a shader can draw with: Default (triangles, back faces culled) always, the others when
// built with SetDrawVariants(true). Mirrored culls front faces instead: a transform with a negative
// determinant flips the winding. Renderpass::SetShader2 picks one per draw.
// The AlphaToCoverage ones are the three triangle variants with alpha to coverage, for alpha-tested
// materials in MSAA passes (other passes fall back to the plain ones).
enum class ShaderVariant : uint8_t { Default, DoubleSided, Mirrored, Lines, Points,
                                     AlphaToCoverage, AlphaToCoverageDoubleSided, AlphaToCoverageMirrored, Count };

class Renderpass 
{
    public:
        Renderpass(bool MSSA, bool depthTexture, std::vector<TextureFormat> outputFormats, uint32_t width, uint32_t height);
        ~Renderpass();

        void Init();

        System renderSystem;
        // Called after renderSystem while the pass is open, for owners that record from their own draw lists.
        std::function<void()> recordCallback;

        // A disabled pass isn't recorded at all and keeps last frame's attachments as they were.
        void SetEnabled(bool enabled) { m_Enabled = enabled; }
        bool IsEnabled() const { return m_Enabled; }

        void Recreate(uint32_t width, uint32_t height);
        uint8_t NumberOfOutputs() { return m_OutputFormats.size(); }
        Texture& GetRenderTarget(uint8_t index) { return m_RenderTargets[index]; }
        Texture& GetRenderResloveTarget(uint8_t index) { return m_RenderTargetsResolve[index]; }
        Texture& GetDepthView() { return m_DepthTexture; }
        const std::vector<TextureFormat>& GetOutputFormats() const { return m_OutputFormats; }
        void* GetDepthStencilAttachment();
        bool HasDepthTexture() const { return m_HasDepthTexture; }
        // 4 samples per pixel, resolved into GetRenderResloveTarget at the end of the pass.
        bool IsMSAA() const { return m_MSSA; }

        // State setters skip the GPU call when the state is already bound.
        void SetMesh(IMesh* shader);
        // `id` is the first instance; consecutive instances get id + 1, id + 2, ...
        void Draw(uint32_t indexCount, uint32_t startIndex, uint32_t id, uint32_t instanceCount = 1);
        // One triangle covering the target, for post-processing (the vertex shader builds it from vertex_index).
        void DrawFullscreen();
        // `variant` picks one of the shader's pipelines (Shader2::SetDrawVariants).
        void SetShader2(Shader2& shader, ShaderVariant variant = ShaderVariant::Default);
        void SetMaterial2(Shader2& shader, Material2& material);
        void SetBufferIndex(std::string name, uint32_t index);



    private:
        friend class Renderer;
        std::vector<std::vector<uint32_t>> m_BindGroupOffsets;
        Shader2* m_CurrentShader = nullptr;
        ShaderVariant m_CurrentVariant = ShaderVariant::Default;
        IMesh* m_CurrentMesh = nullptr;
        void* m_CurrentMaterial = nullptr;
        bool m_BindGroupsDirty = true;
        bool m_Enabled = true;

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
        void BindShaderGroups();

        void Start(void* encoder, bool surfaceTarget, void* resolveTarget);
};