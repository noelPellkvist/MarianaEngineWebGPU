#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include <ECS.hpp>
#include <Shader.hpp>
#include <Mesh.hpp>
#include <Texture.hpp>
#include <StandardPBR.hpp>
#include <MaterialAsset.hpp>
#include <Rendering/Components.hpp>
#include <Rendering/RenderGraph.hpp>
#include <AssetsSystem/AssetManager.hpp>

class ICamera;
struct SkinComponent;

// Six inward-facing planes; a point p is inside when dot(plane.xyz, p) + plane.w >= 0 for all.
struct Frustum { glm::vec4 planes[6]; };

// The scene renderer's options that can change while it runs (see RenderPipelineDesc).
struct SceneRendererSettings
{
    bool shadows = true;
    float shadowDistance = 80.0f;  // shadows fade out this far from the camera
    uint32_t shadowMapSize = 2048; // texels per side of each cascade's depth map

    bool operator==(const SceneRendererSettings&) const = default;
};

// Renders an ECS scene from a camera into a render graph: frustum culling, sorted instanced draw
// lists, GPU skinning, cascaded shadow maps, a depth/normal/entity-id prepass, the forward PBR pass
// (4x MSAA) and the sky. It draws entities with a MeshComponent, whatever made their meshes and
// materials (an importer, Primitives, code), and builds the materials' GPU bind groups itself.
// Its results are graph textures that other passes and effects read; it reads AmbientOcclusion,
// which an effect may write (unoccluded otherwise).
class SceneRenderer
{
public:
    static constexpr const char* kSceneColor = "SceneColor";             // linear HDR, resolved
    static constexpr const char* kSceneDepth = "SceneDepth";             // prepass depth, single sample
    static constexpr const char* kSceneNormals = "SceneNormals";         // view-space normals * 0.5 + 0.5
    static constexpr const char* kEntityIds = "EntityIds";               // 64-bit entity id as 4 x u16, 0 = none
    static constexpr const char* kAmbientOcclusion = "AmbientOcclusion"; // read: 1 = unoccluded

    SceneRenderer(Scene& scene, AssetManager2& assets);
    ~SceneRenderer();

    // The ECS components the renderer reads; call before Scene::UpdateComponentRegistry.
    static void RegisterComponents();

    // Loads the sky, builds the shaders and adds the passes and textures to `graph`, whose
    // backbuffer format and output size should already be set.
    void Init(RenderGraph& graph);
    // Uploads the camera, lighting, transforms, materials and bones and builds this frame's draw
    // lists. Call once per frame after the scene updated and before the graph executes.
    void Prepare(ICamera& camera);

    LightingSettings lighting;

    const SceneRendererSettings& Settings() const { return settings; }
    // Takes effect from the next Prepare; a new shadow map size recreates the maps.
    void SetSettings(const SceneRendererSettings& value);
    // Keeps the prepass (and its entity ids) even when nothing in the graph reads it: editor picking.
    bool writeEntityIds = true;
    // Redraws every shadow cascade (the cache can't see e.g. edited geometry).
    void InvalidateShadows() { shadowsDirty = true; }

    Buffer& CameraBuffer() { return pipeline.cameraBuffer; }
    // What submeshes without a (valid) material draw with.
    const AssetHandle<MaterialAsset>& DefaultMaterial() const { return defaultMaterial; }

private:
    struct DrawItem { MaterialAsset* material; IMesh* mesh; uint32_t submesh; uint32_t transformIndex; uint32_t boneOffset; ShaderVariant variant; };
    // An index range of a caster; `material` is only set for alpha-tested parts (nullptr = opaque).
    struct ShadowItem { IMesh* mesh; MaterialAsset* material; uint32_t startIndex; uint32_t indexCount; uint32_t transformIndex; uint32_t transformVersion; uint32_t boneOffset; };
    struct CasterPart { MaterialAsset* material; uint32_t startIndex; uint32_t indexCount; };
    // Draws sharing mesh + submesh + material become one instanced draw whose instances read
    // consecutive DrawInfo entries (frameDraws).
    struct DrawBatch { IMesh* mesh; MaterialAsset* material; uint32_t submesh; uint32_t indexCount; uint32_t startIndex; uint32_t firstInstance; uint32_t instanceCount; ShaderVariant variant = ShaderVariant::Default; };

    Scene& scene;
    AssetManager2& assets;
    RenderGraph* graph = nullptr;

    StandardPBRPipeline pipeline;
    Shader2 pbrShader{"StandardPBR"};
    Shader2 skyboxShader{"StandardSkybox"};
    Shader2 shadowShader{"StandardShadowMap"};
    Shader2 shadowMaskedShader{"StandardShadowMapMasked"};
    Shader2 prepassShader{"StandardPrepass"};

    // Systems query `scene`, which outlives this renderer.
    System writeTransformBufferSystem;
    System gatherDrawsSystem;
    System gatherShadowCastersSystem;
    System skinningSystem;

    AssetHandle<MaterialAsset> defaultMaterial;
    std::array<AssetHandle<Texture>, static_cast<size_t>(MaterialTexture::Count)> materialFallbacks;
    AssetHandle<Texture> skyCubemap;
    std::shared_ptr<IMesh> skyMesh;
    AssetHandle<Texture> unoccluded; // what AmbientOcclusion reads when nothing writes it

    SceneRendererSettings settings;
    std::array<Renderpass*, kShadowCascades> shadowPasses{};
    bool built = false;

    UBO ubo{};
    bool uboDirty = true;
    CameraInfo cameraInfo{};
    TransformData transformData{};
    Frustum cameraFrustum{};
    std::array<Frustum, kShadowCascades> cascadeFrustums{};
    std::vector<DrawItem> drawItems;
    std::array<std::vector<ShadowItem>, kShadowCascades> shadowItems;
    std::vector<CasterPart> casterParts; // scratch for gatherShadowCastersSystem
    std::vector<DrawBatch> mainBatches;
    std::array<std::vector<DrawBatch>, kShadowCascades> shadowBatches;
    std::vector<DrawData> frameDraws;
    // Joint matrices of every skin this frame (SkinComponent::boneOffset points into it).
    std::vector<BoneData> frameBones;
    // A cascade is only re-rendered when it moved or the casters inside it changed (or the light did: shadowsDirty).
    std::array<uint64_t, kShadowCascades> shadowSignatures{};
    bool shadowsDirty = true;
    // WorldXform::version last uploaded per transform slot.
    std::vector<uint32_t> uploadedTransformVersions;

    void AddShadowMaps();
    void LoadSky();
    void BuildShaders(Renderpass& shadow, Renderpass& prepass, Renderpass& forward);
    void CreateSystems();
    void BindGraphTextures();
    // (Re)builds the texture bind group of every material that is new or had a texture changed.
    void BuildMaterialBindGroups();
    void ApplyLighting();
    void UpdateShadowCascades(const ICamera& camera);
    void BuildDrawLists();
    void RecordForward(Renderpass& pass);
    void RecordPrepass(Renderpass& pass);
    void RecordShadows(Renderpass& pass, uint32_t cascade);
};
