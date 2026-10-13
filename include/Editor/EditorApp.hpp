#pragma once
#include <Application.hpp>
#include <Shader.hpp>
#include <Material.hpp>
#include <Renderer.hpp>
#include <Editor/GLTFLoader.hpp>
#include <Mesh.hpp>
#include <vector>
#include <UniformLayout.hpp>
#include <ECS.hpp>
#include <memory>
#include <Texture.hpp>
#include <Editor/Windows/EditorWindow.hpp>
#include <StandardPBR.hpp>
#include <PostProcess.hpp>
#include <AmbientOcclusion.hpp>
#include <MaterialAsset.hpp>

#include <array>
#include <filesystem>
#include <unordered_map>

class InspectorWindow;
class IMesh;

// Six inward-facing planes; a point p is inside when dot(plane.xyz, p) + plane.w >= 0 for all.
struct Frustum { glm::vec4 planes[6]; };

class EditorApp : public Application
{
    public:
        EditorApp(const std::string& name);
        ~EditorApp() override;

    protected:
        void OnStart() override;
        void OnUpdate(float deltaTime) override;
        void OnRender() override;
        void OnGUI() override;
        void OnShutdown() override;

    private:
        std::unordered_map<std::string, std::unique_ptr<EditorWindow>> EditorWindows;
        InspectorWindow* inspectorWindow = nullptr;
        Renderer renderer;
        Scene scene;
        // Systems query `scene`, so they are declared after it to be destroyed first.
        System writeTransformBufferSystem;
        System animationSystem;
        System gatherDrawsSystem;
        System gatherShadowCastersSystem;
        System skinningSystem;
        uint64_t selectedEntityID = -1;
        Entity selectedEntity;
        Entity skyboxEntity;
        Renderpass renderpass;
        Renderpass prepass; // depth, normals and entity ids, single sampled
        AmbientOcclusion ambientOcclusion;
        PostProcessPipeline postProcess;
        std::vector<std::unique_ptr<Renderpass>> shadowpasses; // one per shadow cascade

        StandardPBRPipeline standardPBRPipeline;
        AssetHandle<MaterialAsset> defaultMaterial;
        // Per-frame draw lists. Draws sharing mesh + submesh + material become one instanced
        // draw whose instances read consecutive DrawInfo entries (frameDraws).
        struct DrawItem { MaterialAsset* material; IMesh* mesh; uint32_t submesh; uint32_t transformIndex; uint32_t boneOffset; ShaderVariant variant; };
        // An index range of a caster; `material` is only set for alpha-tested parts (nullptr = opaque).
        struct ShadowItem { IMesh* mesh; MaterialAsset* material; uint32_t startIndex; uint32_t indexCount; uint32_t transformIndex; uint32_t transformVersion; uint32_t boneOffset; };
        struct CasterPart { MaterialAsset* material; uint32_t startIndex; uint32_t indexCount; };
        struct DrawBatch { IMesh* mesh; MaterialAsset* material; uint32_t submesh; uint32_t indexCount; uint32_t startIndex; uint32_t firstInstance; uint32_t instanceCount; ShaderVariant variant = ShaderVariant::Default; };

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
        IMesh* skyboxMesh = nullptr;
        uint32_t skyboxTransform = 0;
        // A cascade is only re-rendered when it moved or the casters inside it changed (or the light did: set shadowsDirty).
        std::array<uint64_t, kShadowCascades> shadowSignatures{};
        bool shadowsDirty = true;
        // WorldXform::version last uploaded per transform slot.
        std::vector<uint32_t> uploadedTransformVersions;

        LightingSettings lighting;
        bool uboDirty = true;

        void ApplyLighting();
        void UpdateShadowCascades();
        void BuildDrawLists();
        void OnWindowResized(int w, int h);
        void ResizeTargets(uint32_t w, uint32_t h);
        void SpawnSkybox();
        void SpawnPrefab(const std::filesystem::path& path);

        void LoadFileTexture(const std::string& path);
        void LoadFileTextures();
        void SelectEntity(uint64_t id);
        void DeselectEntity();

        void DrawTopMenu();
        void DrawAssetsWindow();
        void DrawMat4(const char* id, float m[16], bool editable = true, float speed = 0.05f, const char* fmt = "%.3f");
        bool DrawGizmo(glm::mat4& transform, const glm::mat4& view, const glm::mat4& proj);
};