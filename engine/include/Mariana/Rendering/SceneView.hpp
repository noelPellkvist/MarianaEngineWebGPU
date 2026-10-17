#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Graphics/Shader.hpp>
#include <Mariana/Rendering/SceneRenderer.hpp>
#include <Mariana/Rendering/StandardPBR.hpp>
#include <Mariana/Rendering/RenderGraph.hpp>

class ICamera;

// Six inward-facing planes; a point p is inside when dot(plane.xyz, p) + plane.w >= 0 for all.
struct Frustum { glm::vec4 planes[6]; };

// One camera's view of a SceneRenderer, drawn by a render graph: frustum culling, sorted instanced
// draw lists, cascaded shadow maps, a depth/normal/entity-id prepass, the forward PBR pass (4x MSAA)
// and the sky. Its results are graph textures that other passes and effects read; it reads
// AmbientOcclusion, which an effect may write (unoccluded otherwise). The scene's data comes from the
// renderer, so another view only adds its own camera, draws, shadow maps and graph textures.
class SceneView
{
public:
    static constexpr const char* kSceneColor = "SceneColor";             // linear HDR, resolved
    static constexpr const char* kSceneDepth = "SceneDepth";             // prepass depth, single sample
    static constexpr const char* kSceneNormals = "SceneNormals";         // view-space normals * 0.5 + 0.5
    static constexpr const char* kEntityIds = "EntityIds";               // 64-bit entity id as 4 x u16, 0 = none
    static constexpr const char* kAmbientOcclusion = "AmbientOcclusion"; // read: 1 = unoccluded

    explicit SceneView(SceneRenderer& renderer);
    ~SceneView();

    SceneView(const SceneView&) = delete;
    SceneView& operator=(const SceneView&) = delete;

    // Adds the passes and textures to `graph`, whose backbuffer format and output size should
    // already be set, and builds the shaders. The renderer must be initialised.
    void Init(RenderGraph& graph);
    // Uploads this view's camera, lighting and shadow cascades and builds its draw lists. Once per
    // frame per view, after SceneRenderer::Update and before the graph executes.
    void Prepare(ICamera& camera);

    // Keeps the prepass (and its entity ids) even when nothing in the graph reads it: editor
    // picking. Set before Init.
    bool writeEntityIds = true;

    Buffer& CameraBuffer() { return buffers.cameraBuffer; }

private:
    struct DrawItem { MaterialAsset* material; IMesh* mesh; uint32_t submesh; uint32_t transformIndex; uint32_t boneOffset; ShaderVariant variant; };
    // An index range of a caster; `material` is only set for alpha-tested parts (nullptr = opaque).
    struct ShadowItem { IMesh* mesh; MaterialAsset* material; uint32_t startIndex; uint32_t indexCount; uint32_t transformIndex; uint32_t transformVersion; uint32_t boneOffset; };
    struct CasterPart { MaterialAsset* material; uint32_t startIndex; uint32_t indexCount; };
    // Draws sharing mesh + submesh + material become one instanced draw whose instances read
    // consecutive DrawInfo entries (frameDraws).
    struct DrawBatch { IMesh* mesh; MaterialAsset* material; uint32_t submesh; uint32_t indexCount; uint32_t startIndex; uint32_t firstInstance; uint32_t instanceCount; ShaderVariant variant = ShaderVariant::Default; };

    SceneRenderer& renderer;
    RenderGraph* graph = nullptr;
    StandardPBRView buffers;

    Shader2 pbrShader{"StandardPBR"};
    Shader2 skyboxShader{"StandardSkybox"};
    Shader2 shadowShader{"StandardShadowMap"};
    Shader2 shadowMaskedShader{"StandardShadowMapMasked"};
    Shader2 prepassShader{"StandardPrepass"};

    // Systems query the renderer's scene, which outlives this view.
    System gatherDrawsSystem;
    System gatherShadowCastersSystem;

    std::array<Renderpass*, kShadowCascades> shadowPasses{};
    bool built = false;
    // The renderer's settings as this view last applied them.
    SceneRendererSettings settings;

    UBO ubo{};
    bool uboDirty = true;
    CameraInfo cameraInfo{};
    Frustum cameraFrustum{};
    std::array<Frustum, kShadowCascades> cascadeFrustums{};
    std::vector<DrawItem> drawItems;
    std::array<std::vector<ShadowItem>, kShadowCascades> shadowItems;
    std::vector<CasterPart> casterParts; // scratch for gatherShadowCastersSystem
    std::vector<DrawBatch> mainBatches;
    std::array<std::vector<DrawBatch>, kShadowCascades> shadowBatches;
    std::vector<DrawData> frameDraws;
    // A cascade is only re-rendered when it moved or the casters inside it changed (or the light did: shadowsDirty).
    std::array<uint64_t, kShadowCascades> shadowSignatures{};
    bool shadowsDirty = true;
    uint32_t shadowEpoch = 0; // the renderer's, when the shadows were last invalidated

    void AddShadowMaps();
    void BuildShaders(Renderpass& shadow, Renderpass& prepass, Renderpass& forward);
    void CreateSystems();
    void BindGraphTextures();
    void ApplySettings();
    void ApplyLighting();
    void UpdateShadowCascades(const ICamera& camera);
    void BuildDrawLists();
    void RecordForward(Renderpass& pass);
    void RecordPrepass(Renderpass& pass);
    void RecordShadows(Renderpass& pass, uint32_t cascade);
};
