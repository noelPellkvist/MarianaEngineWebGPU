#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Graphics/Shader.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Graphics/Texture.hpp>
#include <Mariana/Rendering/StandardPBR.hpp>
#include <Mariana/Rendering/SkyLight.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Assets/AssetManager.hpp>

class SceneView;

// The scene renderer's options that can change while it runs (see RenderPipelineDesc).
struct SceneRendererSettings
{
    bool shadows = true;
    float shadowDistance = 80.0f;  // shadows fade out this far from the camera
    uint32_t shadowMapSize = 2048; // texels per side of each cascade's depth map
    bool sky = true;               // the sky as the background (it lights the scene either way)

    bool operator==(const SceneRendererSettings&) const = default;
};

// The GPU side of a scene, shared by every view of it (SceneView): transforms, material constants
// and bind groups, the skins' joint matrices, the sky and the lighting. Update() uploads what changed
// once per frame, however many views then draw it from their cameras. It draws entities with a
// MeshComponent, whatever made their meshes and materials (an importer, Primitives, code).
class SceneRenderer
{
public:
    SceneRenderer(Scene& scene, AssetManager2& assets);
    ~SceneRenderer();

    // The ECS components the renderer reads; call before Scene::UpdateComponentRegistry.
    static void RegisterComponents();

    // Loads the sky and sets up what every view uses.
    void Init();
    // Uploads changed transforms and material constants and poses the skins. Once per frame after
    // the scene updated, before the views prepare (no need when no view draws this frame).
    void Update();

    LightingSettings lighting;

    const SceneRendererSettings& Settings() const { return settings; }
    // Takes effect when the views next prepare; a new shadow map size recreates their maps.
    void SetSettings(const SceneRendererSettings& value);
    // Redraws every view's shadow cascades (the cache can't see e.g. edited geometry).
    void InvalidateShadows() { ++shadowEpoch; }
    // Uploads every transform again and redraws the shadows: after Scene::Clear, when slots are
    // reused by new entities.
    void InvalidateTransforms() { uploadedTransformVersions.clear(); ++shadowEpoch; }

    // Where the sky's light comes from most (its brightest side), before `lighting.skyRotation`.
    glm::vec3 SkyDirection() const { return SkyBrightestDirection(skySH); }
    // What submeshes without a (valid) material draw with.
    const AssetHandle<MaterialAsset>& DefaultMaterial() const { return defaultMaterial; }

private:
    friend class SceneView;

    Scene& scene;
    AssetManager2& assets;
    StandardPBRPipeline pipeline;

    // Systems query `scene`, which outlives this renderer.
    System writeTransformBufferSystem;
    System skinningSystem;

    AssetHandle<MaterialAsset> defaultMaterial;
    std::array<AssetHandle<Texture>, static_cast<size_t>(MaterialTexture::Count)> materialFallbacks;
    AssetHandle<Texture> skyCubemap;
    std::array<glm::vec4, 9> skySH{};
    std::shared_ptr<IMesh> skyMesh;
    AssetHandle<Texture> unoccluded; // what AmbientOcclusion reads when nothing writes it

    SceneRendererSettings settings;
    // Bumped whenever every shadow map must be redrawn; each view compares it with the last it drew.
    uint32_t shadowEpoch = 0;

    TransformData transformData{};
    // WorldXform::version last uploaded per transform slot.
    std::vector<uint32_t> uploadedTransformVersions;
    // Joint matrices of every skin this frame (SkinComponent::boneOffset points into it).
    std::vector<BoneData> frameBones;

    void LoadSky();
    void CreateSystems();
    // (Re)builds the texture bind group of every material that is new or had a texture changed,
    // against `shader`'s material group (any view's PBR shader: they all declare the same one).
    void BuildMaterialBindGroups(Shader2& shader);
};
