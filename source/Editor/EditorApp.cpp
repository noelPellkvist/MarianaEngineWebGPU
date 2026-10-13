#include <Editor/EditorApp.hpp>
#include <FileReader.hpp>
#include <Editor/EditorCameraController.hpp>
#include <BuiltinAssets.hpp>
#include <Buffer.hpp>
#include <ECS.hpp>
#include <Prefab.hpp>
#include <AnimationPlayer.hpp>
#include <Skin.hpp>
#include <SkyLight.hpp>

#include "ImGuizmo.h"

#include <Editor/Windows/Stats.hpp>
#include <Editor/Windows/AssetsExplorer.hpp>
#include <Editor/Windows/HierarchyWindow.hpp>
#include <Editor/Windows/InspectorWindow.hpp>
#include <Editor/Windows/LoadedAssetsWindow.hpp>
#include <Editor/Windows/PostProcessWindow.hpp>
#include <Editor/Windows/LightingWindow.hpp>

#include <sstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <algorithm>
#include <array>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
namespace fs = std::filesystem;

#pragma region Helpers

inline std::string ToString(const glm::vec3& v)
{
    std::ostringstream ss;
    ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return ss.str();
}

inline std::string ToString(const float& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

inline std::string ToString(const int& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

inline std::string ToString(const uint32_t& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

inline std::string ToString(const uint64_t& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

#pragma endregion

#pragma region ShaderUniforms

UBO ubo{};
TransformData transformBuffer{};
CameraInfo cameraInfo{};
BoneData boneData{};

GLTF::Vertex v{};
VertexBufferLayout vertexLayout{v, v.position, v.normal, v.tangent, v.texcoord0, v.texcoord1, v.color0, v.boneIndices, v.boneWeights};

#pragma endregion

Shader2 StandardPBRShader("StandardPBR");
Shader2 StandardSkyboxShader("StandardSkybox");
Shader2 ShadowMapShader("StandardShadowMap");
Shader2 ShadowMapMaskedShader("StandardShadowMapMasked");

struct Skybox {};

// Planes of a view-projection matrix with [0, 1] clip depth (Gribb/Hartmann), pointing inwards.
static Frustum ExtractFrustum(const glm::mat4& viewProj)
{
    const glm::vec4 row0(viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]);
    const glm::vec4 row1(viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]);
    const glm::vec4 row2(viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]);
    const glm::vec4 row3(viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]);

    Frustum frustum;
    frustum.planes[0] = row3 + row0; // left
    frustum.planes[1] = row3 - row0; // right
    frustum.planes[2] = row3 + row1; // bottom
    frustum.planes[3] = row3 - row1; // top
    frustum.planes[4] = row2;        // near
    frustum.planes[5] = row3 - row2; // far
    return frustum;
}


// Axis-aligned box around `box` moved by `m`.
static Bounds TransformBounds(const glm::mat4& m, const Bounds& box)
{
    const glm::vec3 localCenter = (box.min + box.max) * 0.5f;
    const glm::vec3 localExtent = (box.max - box.min) * 0.5f;

    const glm::vec3 center = glm::vec3(m * glm::vec4(localCenter, 1.0f));
    const glm::vec3 extent = glm::abs(glm::vec3(m[0])) * localExtent.x
                           + glm::abs(glm::vec3(m[1])) * localExtent.y
                           + glm::abs(glm::vec3(m[2])) * localExtent.z;
    return { center - extent, center + extent };
}

// Whether a world-space box is at least partly inside the frustum.
static bool IsVisible(const Frustum& frustum, const Bounds& box)
{
    const glm::vec3 center = (box.min + box.max) * 0.5f;
    const glm::vec3 extent = (box.max - box.min) * 0.5f;
    for (const glm::vec4& plane : frustum.planes)
    {
        const glm::vec3 normal(plane);
        if (glm::dot(normal, center) + plane.w < -glm::dot(glm::abs(normal), extent))
            return false;
    }
    return true;
}

// Whether the mesh, placed by `model`, is at least partly inside the frustum. Skinned meshes move
// beyond their bind-pose bounds, so they use the posed bounds the skinning pass computed instead.
static bool IsVisible(const Frustum& frustum, const IMesh& mesh, const float* model, const SkinComponent* skin)
{
    if (skin)
        return skin->bounds.IsEmpty() || IsVisible(frustum, skin->bounds);
    if (!mesh.hasBounds)
        return true;
    return IsVisible(frustum, TransformBounds(glm::make_mat4(model), Bounds{ mesh.boundsMin, mesh.boundsMax }));
}

// The pipeline a submesh draws with: lines and points have their own, double-sided materials skip
// culling, and a mirroring transform (negative determinant) culls the other side.
static ShaderVariant DrawVariant(PrimitiveTopology topology, bool doubleSided, bool mirrored)
{
    if (topology == PrimitiveTopology::Lines)
        return ShaderVariant::Lines;
    if (topology == PrimitiveTopology::Points)
        return ShaderVariant::Points;
    if (doubleSided)
        return ShaderVariant::DoubleSided;
    return mirrored ? ShaderVariant::Mirrored : ShaderVariant::Default;
}

// Finds the entity of each joint of `skin`. Joints live in the same model as the skinned mesh,
// so the search starts at the model's wrapper root: the first ancestor without an AnimationTargetEntity.
static void ResolveJoints(Scene& scene, Entity skinned, const Skin& skin, std::vector<Entity>& joints)
{
    Entity root = skinned;
    while (root.Has<AnimationTargetEntity>() && scene.Parent(root).IsValid())
        root = scene.Parent(root);

    std::unordered_map<uint32_t, Entity> byNode;
    auto visit = [&](Entity e) {
        if (const AnimationTargetEntity* target = e.TryGet<AnimationTargetEntity>())
            byNode.emplace(target->nodeIndex, e);
    };
    visit(root);
    scene.ForEachDescendant(root, visit);

    joints.clear();
    for (uint32_t node : skin.jointNodes)
    {
        auto it = byNode.find(node);
        joints.push_back(it != byNode.end() ? it->second : Entity{});
    }
}

static ImGuizmo::OPERATION CurrentGizmoOperation = ImGuizmo::TRANSLATE;

EditorApp::EditorApp(const std::string& name) : Application(name), 
renderpass(false, true, { TextureFormat::RGBA16Float, TextureFormat::RGBA16Uint }, m_Window.GetWidth(), m_Window.GetHeight()),
standardPBRPipeline()
{
    ECS::RegisterTag<ShadowCasterTag>("ShadowCasterTag");
    ECS::RegisterComponent<NameComponent>("NameComponent");
    ECS::RegisterComponent<LocalTRS>("LocalTRS");
    ECS::RegisterComponent<WorldXform>("WorldXform");
    ECS::RegisterComponent<TransformClock>("TransformClock");
    ECS::RegisterComponent<XformCache>("XformCache");
    ECS::RegisterComponent<MeshComponent>("MeshComponent");
    ECS::RegisterComponent<AnimationTargetEntity>("AnimationTargetEntity");
    ECS::RegisterComponent<AnimationPlayer>("AnimationPlayer");
    ECS::RegisterComponent<SkinComponent>("SkinComponent");
    ECS::RegisterTag<Skybox>("Skybox");

    scene.UpdateComponentRegistry();

    cam = new EditorCameraController(input);
    cam->SetViewportSize(m_Window.GetWidth(), m_Window.GetHeight());

    renderpass.Init();
    // The scene renders linear HDR into renderpass target 0; post-processing tonemaps it to the screen.
    postProcess.Init(m_Window.GetWidth(), m_Window.GetHeight(), m_Window.GetWindowFormat(), renderpass.GetRenderTarget(0));
    for (uint32_t i = 0; i < kShadowCascades; ++i)
    {
        shadowpasses.push_back(std::make_unique<Renderpass>(false, true, std::vector<TextureFormat>{}, kShadowMapSize, kShadowMapSize));
        shadowpasses.back()->Init();
    }

    ShadowMapShader.Group(0)
    .AddBuffer("ShadowCascade", 0, standardPBRPipeline.shadowCascadeBuffer)
    .AddBuffer("ModelData", 1, standardPBRPipeline.transformBuffer)
    .AddBuffer("DrawInfo", 2, standardPBRPipeline.drawBuffer)
    .AddBuffer("BoneData", 3, standardPBRPipeline.boneBuffer);

    ShadowMapShader.SetVertexStructLayout(vertexLayout);
    ShadowMapShader.SetWGSL(FileReader::LoadRawString("/Shaders/shadow.wgsl"));
    ShadowMapShader.SetRenderpass(shadowpasses[0].get());
    ShadowMapShader.Build(true);

    // Alpha-tested casters: same shader plus a fragment stage that discards like the main pass.
    // Group 1 matches the PBR material group, so the materials' bind groups work here too.
    ShadowMapMaskedShader.Group(0)
    .AddBuffer("ShadowCascade", 0, standardPBRPipeline.shadowCascadeBuffer)
    .AddBuffer("ModelData", 1, standardPBRPipeline.transformBuffer)
    .AddBuffer("DrawInfo", 2, standardPBRPipeline.drawBuffer)
    .AddBuffer("BoneData", 3, standardPBRPipeline.boneBuffer)
    .AddBuffer("Material", 4, standardPBRPipeline.materialsBuffer);

    ShadowMapMaskedShader.Group(1)
    .AddTexture("albedoMap", 0, TextureType_2D)
    .AddTexture("normalMap", 1, TextureType_2D)
    .AddTexture("metallicRoughnessMap", 2, TextureType_2D)
    .AddTexture("occlusionMap", 3, TextureType_2D)
    .AddTexture("emissiveMap", 4, TextureType_2D)
    .AddSampler("albedoSampler", 5)
    .AddSampler("normalSampler", 6)
    .AddSampler("metallicRoughnessSampler", 7)
    .AddSampler("occlusionSampler", 8)
    .AddSampler("emissiveSampler", 9);

    ShadowMapMaskedShader.SetMaterialGroup(1);
    ShadowMapMaskedShader.SetVertexStructLayout(vertexLayout);
    ShadowMapMaskedShader.SetWGSL(FileReader::LoadRawString("/Shaders/shadow.wgsl"));
    ShadowMapMaskedShader.SetRenderpass(shadowpasses[0].get());
    ShadowMapMaskedShader.SetDepthOnlyFragment(true);
    ShadowMapMaskedShader.Build(true);

    // The sky is the background and a light: its cubemap (with mips) is reflected, and its
    // spherical harmonics light everything diffusely (SkyLight.hpp).
    std::vector<std::vector<uint8_t>> skyFaces;
    int skyFaceSize = 0;
    for (const char* face : { "right", "left", "top", "bottom", "back", "front" })
    {
        int width = 0, height = 0;
        skyFaces.push_back(FileReader::LoadPixelsFromImage(std::string("/Textures/skybox/") + face + ".jpg", width, height));
        skyFaceSize = width;
    }
    auto skyboxTex = std::make_shared<Texture>();
    skyboxTex->CreateCubeTexture(skyFaces, skyFaceSize, TextureFormat::RGBA8UnormSrgb);
    ubo.skySH = ComputeSkyIrradianceSH(skyFaces, skyFaceSize);
    AssetHandle<Texture> skyboxTexture = assets.RegisterAsset<Texture>(skyboxTex, AssetType::Texture, "Skybox Cubemap", "builtin://texture/skybox");
    assets.SetPersistent(skyboxTexture.GetID(), true);

    StandardPBRShader.Group(0)
    .AddBuffer("UBO", 0, standardPBRPipeline.uboBuffer)
    .AddBuffer("CameraInfo", 1, standardPBRPipeline.cameraBuffer)
    .AddBuffer("DrawInfo", 2, standardPBRPipeline.drawBuffer)
    .AddBuffer("ModelData", 3, standardPBRPipeline.transformBuffer)
    .AddBuffer("Material", 4, standardPBRPipeline.materialsBuffer)
    .AddBuffer("BoneData", 5, standardPBRPipeline.boneBuffer)
    .AddTexture("ShadowMap0", 6, TextureType_Depth)
    .AddTexture("ShadowMap1", 7, TextureType_Depth)
    .AddTexture("ShadowMap2", 8, TextureType_Depth)
    .AddTexture("ShadowMap3", 9, TextureType_Depth)
    .AddSampler("ShadowSampler", 10, SamplerType_Comparison)
    .AddTexture("SkyCube", 11, TextureType_Cube)
    .AddSampler("SkySampler", 12, SamplerType_LinearClamp);


    for (uint32_t i = 0; i < kShadowCascades; ++i)
        StandardPBRShader.SetTexture("ShadowMap" + std::to_string(i), shadowpasses[i]->GetDepthView());
    StandardPBRShader.SetTexture("SkyCube", *skyboxTex);

    StandardPBRShader.Group(1)
    .AddTexture("albedoMap", 0, TextureType_2D)
    .AddTexture("normalMap", 1, TextureType_2D)
    .AddTexture("metallicRoughnessMap", 2, TextureType_2D)
    .AddTexture("occlusionMap", 3, TextureType_2D)
    .AddTexture("emissiveMap", 4, TextureType_2D)
    .AddSampler("albedoSampler", 5)
    .AddSampler("normalSampler", 6)
    .AddSampler("metallicRoughnessSampler", 7)
    .AddSampler("occlusionSampler", 8)
    .AddSampler("emissiveSampler", 9);

    StandardPBRShader.SetMaterialGroup(1);
    StandardPBRShader.SetVertexStructLayout(vertexLayout);
    StandardPBRShader.SetWGSL(FileReader::LoadRawString("/Shaders/test.wgsl"));
    StandardPBRShader.SetRenderpass(&renderpass);
    StandardPBRShader.SetDrawVariants(true);

    StandardPBRShader.Build();

    standardPBRPipeline.boneBuffer.Write(boneData, 0);

    GLTF::GLTFLoader::RegisterLoader(assets, StandardPBRShader);
    defaultMaterial = BuiltinAssets::DefaultMaterial(assets, StandardPBRShader);

    StandardSkyboxShader.Group(0)
    .AddBuffer("CameraInfo", 0, standardPBRPipeline.cameraBuffer)
    .AddTexture("CubeMap", 1, TextureType_Cube)
    .AddSampler("Sampler", 2);

    StandardSkyboxShader.SetTexture("CubeMap", *skyboxTex);

    StandardSkyboxShader.SetVertexStructLayout(vertexLayout);
    StandardSkyboxShader.SetWGSL(FileReader::LoadRawString("/Shaders/skybox.wgsl"));
    StandardSkyboxShader.SetRenderpass(&renderpass);
    StandardSkyboxShader.Build();

    SpawnSkybox();

    writeTransformBufferSystem = scene.CreateSystem<WorldXform, MeshComponent>([&](Entity ent, WorldXform& form, MeshComponent& mesh, float dt){
        // Only transforms that changed since their last upload; the rest are already on the GPU.
        if (form.id >= uploadedTransformVersions.size())
            uploadedTransformVersions.resize(form.id + 1, ~0u);
        if (uploadedTransformVersions[form.id] == form.version)
            return;
        uploadedTransformVersions[form.id] = form.version;

        transformBuffer.modelMatrix = glm::make_mat4(form.model);
        transformBuffer.normalMatrix = glm::make_mat3(form.normal);
        transformBuffer.SetEntityID(ent.RawId());
        standardPBRPipeline.QueueTransform(form.id, transformBuffer);
    });

    animationSystem = scene.CreateSystem<AnimationPlayer>([](Entity e, AnimationPlayer& player, float dt) {
        player.Update(e, dt);
    });

    gatherDrawsSystem = scene.CreateSystem<MeshComponent, WorldXform>([&](Entity ent, MeshComponent& meshComp, WorldXform& form, float dt){
        IMesh* mesh = meshComp.mesh.Get().get();
        if (!mesh)
            return;

        if (ent.HasTag<Skybox>())
        {
            skyboxMesh = mesh;
            skyboxTransform = form.id;
            return;
        }

        const SkinComponent* skin = ent.TryGet<SkinComponent>();
        const uint32_t boneOffset = skin ? skin->boneOffset : ~0u;
        if (!IsVisible(cameraFrustum, *mesh, form.model, skin))
            return;

        const bool mirrored = glm::determinant(glm::mat3(glm::make_mat4(form.model))) < 0.0f;
        for (uint32_t i = 0; i < mesh->submeshes.size(); ++i)
        {
            const uint32_t materialIndex = mesh->submeshes[i].materialIndex;
            MaterialAsset* material = materialIndex < meshComp.materials.size()
                ? meshComp.materials[materialIndex].Get().get()
                : nullptr;
            if (!material || material->GetGpuSlot() == MaterialAsset::kInvalidSlot)
                material = defaultMaterial.Get().get();
            if (material)
                drawItems.push_back({ material, mesh, i, form.id, boneOffset, DrawVariant(mesh->submeshes[i].topology, material->doubleSided, mirrored) });
        }
    });

    gatherShadowCastersSystem = scene.CreateSystem<MeshComponent, WorldXform, ShadowCasterTag>([&](Entity ent, MeshComponent& meshComp, WorldXform& form, ShadowCasterTag& tag, float dt){
        IMesh* mesh = meshComp.mesh.Get().get();
        if (!mesh)
            return;

        // A skin's pose changes the shadow too, so its version is part of the caster's version.
        const SkinComponent* skin = ent.TryGet<SkinComponent>();
        const uint32_t version = skin ? form.version + skin->poseVersion : form.version;
        const uint32_t boneOffset = skin ? skin->boneOffset : ~0u;

        // Depth doesn't need materials, so a caster is drawn whole, except its alpha-tested submeshes:
        // those go on their own with their material, which cuts their holes out of the shadow, and
        // the opaque rest is drawn in as few index ranges as possible.
        auto alphaTestedMaterial = [&](const Submesh& sm) -> MaterialAsset* {
            MaterialAsset* material = sm.materialIndex < meshComp.materials.size() ? meshComp.materials[sm.materialIndex].Get().get() : nullptr;
            const bool alphaTested = material && material->GetGpuSlot() != MaterialAsset::kInvalidSlot && material->properties.alphaCutoff > 0.0f;
            return alphaTested ? material : nullptr;
        };

        // Points and lines cast no shadow.
        auto isTriangles = [](const Submesh& sm) { return sm.topology == PrimitiveTopology::Triangles; };

        casterParts.clear();
        if (std::all_of(mesh->submeshes.begin(), mesh->submeshes.end(), isTriangles) &&
            std::none_of(mesh->submeshes.begin(), mesh->submeshes.end(), alphaTestedMaterial))
        {
            casterParts.push_back({ nullptr, 0, static_cast<uint32_t>(mesh->IndexCount()) });
        }
        else
        {
            for (const Submesh& sm : mesh->submeshes)
            {
                if (!isTriangles(sm))
                    continue;
                if (MaterialAsset* material = alphaTestedMaterial(sm))
                    casterParts.push_back({ material, sm.startIndex, sm.indexCount });
                else if (!casterParts.empty() && !casterParts.back().material && casterParts.back().startIndex + casterParts.back().indexCount == sm.startIndex)
                    casterParts.back().indexCount += sm.indexCount;
                else
                    casterParts.push_back({ nullptr, sm.startIndex, sm.indexCount });
            }
        }

        // Each cascade only draws the casters inside its light volume.
        for (uint32_t c = 0; c < kShadowCascades; ++c)
        {
            if (!IsVisible(cascadeFrustums[c], *mesh, form.model, skin))
                continue;
            for (const CasterPart& part : casterParts)
                shadowItems[c].push_back({ mesh, part.material, part.startIndex, part.indexCount, form.id, version, boneOffset });
        }
    });

    skinningSystem = scene.CreateSystem<SkinComponent, MeshComponent, WorldXform>([&](Entity ent, SkinComponent& skinComp, MeshComponent& meshComp, WorldXform& form, float dt){
        const std::shared_ptr<Skin>& skin = skinComp.skin.Get();
        skinComp.boneOffset = ~0u;
        skinComp.bounds = {};
        if (!skin || frameBones.size() + skin->jointNodes.size() > StandardPBRPipeline::kMaxBones)
            return;

        if (skinComp.joints.size() != skin->jointNodes.size())
            ResolveJoints(scene, ent, *skin, skinComp.joints);

        // Relative to the mesh node, because the shader still applies the node's model matrix.
        const glm::mat4 worldToMesh = glm::inverse(glm::make_mat4(form.model));
        skinComp.boneOffset = static_cast<uint32_t>(frameBones.size());
        skinComp.poseVersion = 0;

        // Each joint's box, moved like the vertices it holds, grows the posed mesh's bounds.
        // Without joint bounds the bounds stay empty and the mesh is never culled.
        const IMesh* mesh = meshComp.mesh.Get().get();

        for (size_t i = 0; i < skin->jointNodes.size(); ++i)
        {
            const WorldXform* joint = skinComp.joints[i].IsValid() ? skinComp.joints[i].TryGet<WorldXform>() : nullptr;
            // Bind pose to world.
            const glm::mat4 jointSkin = (joint ? glm::make_mat4(joint->model) : glm::mat4(1.0f)) * skin->inverseBind[i];
            BoneData bone;
            bone.model = worldToMesh * jointSkin;
            frameBones.push_back(bone);
            skinComp.poseVersion += joint ? joint->version : 0;

            if (mesh && i < mesh->jointBounds.size() && !mesh->jointBounds[i].IsEmpty())
            {
                const Bounds box = TransformBounds(jointSkin, mesh->jointBounds[i]);
                skinComp.bounds.Add(box.min);
                skinComp.bounds.Add(box.max);
            }
        }
        skinComp.poseVersion += form.version;
    });

    scene.Update(0.f);
    m_Window.RegisterResizeCallback([this](int w, int h) {
        this->OnWindowResized(w, h);
    });
}

EditorApp::~EditorApp()
{

}

void EditorApp::OnWindowResized(int w, int h)
{
    // Minimized: nothing to render into.
    if (w <= 0 || h <= 0)
        return;

    m_Window.GetSurface();
    cam->SetViewportSize(w, h);
    renderpass.Recreate(w, h);
    postProcess.Resize(w, h);
}

void EditorApp::SpawnSkybox()
{
    AssetHandle<Prefab> skyboxModel = assets.Load<Prefab>(std::string(RESOURCE_DIR) + "/Models/SkyBox.glb");
    std::shared_ptr<Prefab> prefab = skyboxModel.Get();
    if (!prefab)
    {
        Logger::Error("Failed to load the skybox model");
        return;
    }

    // The skybox only needs the model's geometry; it is drawn with the skybox shader instead of its materials.
    AssetHandle<IMesh> mesh;
    auto findMesh = [&](auto&& self, Entity e) -> void {
        if (mesh.IsValid())
            return;
        if (const MeshComponent* meshComponent = e.TryGet<MeshComponent>())
            mesh = meshComponent->mesh;
        e.ForEachChild([&](Entity child) { self(self, child); });
    };
    findMesh(findMesh, prefab->Root());

    if (!mesh.IsValid())
    {
        Logger::Error("The skybox model has no mesh");
        return;
    }

    skyboxEntity = scene.Instantiate("SkyBox");
    skyboxEntity.Add<MeshComponent>(MeshComponent{ mesh, {} }).AddTag<Skybox>().SetScaleUniform(20).SetPosition(0, 2, 0);
}

void EditorApp::SpawnPrefab(const fs::path& path)
{
    AssetHandle<Prefab> model = assets.Load<Prefab>(path);
    std::shared_ptr<Prefab> prefab = model.Get();
    if (!prefab)
    {
        Logger::Error("Failed to spawn prefab from: " + path.string());
        return;
    }

    Entity spawned = scene.Instantiate(*prefab, path.stem().string().c_str());
    scene.Update(0.0f);
    DeselectEntity();
    SelectEntity(spawned.RawId());
    Logger::Info("Spawned prefab from: " + path.string());
}

void EditorApp::OnStart()
{
    Logger::Info("Starting");

    EditorWindows["Stats"] = std::make_unique<StatsWindow>(gui);
    EditorWindows["Assets"] = std::make_unique<AssetsExplorer>(gui);
    auto inspector = std::make_unique<InspectorWindow>(gui);
    inspectorWindow = inspector.get();
    EditorWindows["Inspector"] = std::move(inspector);
    EditorWindows["Loaded Assets"] = std::make_unique<LoadedAssetsWindow>(gui, assets);
    EditorWindows["Post Processing"] = std::make_unique<PostProcessWindow>(gui, postProcess);
    EditorWindows["Lighting"] = std::make_unique<LightingWindow>(gui, lighting);
    EditorWindows["Hierarchy"] = std::make_unique<HierarchyWindow>(
        gui,
        scene,
        selectedEntityID,
        [this](uint64_t id) {
            DeselectEntity();
            SelectEntity(id);
        },
        [this](const fs::path& gltfPath) {
            SpawnPrefab(gltfPath);
        });

    if (auto* editorCam = dynamic_cast<EditorCameraController*>(cam)) {
        editorCam->SetPosition({-10.0f, 3.0f, -10.0f});
        editorCam->SetYawPitch(glm::half_pi<float>() / 2, -glm::half_pi<float>() / 5);
    } 

    for (auto& pass : shadowpasses)
        renderer.PushRenderpass(pass.get());
    renderer.PushRenderpass(&renderpass);
    postProcess.AddPasses(renderer);

    // The sun comes from `lighting` (ApplyLighting); cascades are fitted around the camera every
    // frame (UpdateShadowCascades).

    renderpass.recordCallback = [this]() {
        if (!mainBatches.empty())
        {
            for (const DrawBatch& batch : mainBatches)
            {
                renderpass.SetShader2(StandardPBRShader, batch.variant);
                renderpass.SetMesh(batch.mesh);
                renderpass.SetMaterial2(StandardPBRShader, batch.material->material);
                renderpass.Draw(batch.indexCount, batch.startIndex, batch.firstInstance, batch.instanceCount);
            }
        }

        // Drawn last: it sits at the far plane, so the depth test skips every pixel already covered.
        if (skyboxMesh)
        {
            renderpass.SetShader2(StandardSkyboxShader);
            renderpass.SetMesh(skyboxMesh);
            for (const Submesh& sm : skyboxMesh->submeshes)
            {
              renderpass.Draw(sm.indexCount, sm.startIndex, skyboxTransform);
            }
        }
    };

    for (uint32_t c = 0; c < kShadowCascades; ++c)
    {
        Renderpass& pass = *shadowpasses[c];
        pass.recordCallback = [this, &pass, c]() {
            for (const DrawBatch& batch : shadowBatches[c])
            {
                // Opaque casters sort first; alpha-tested ones need their material's texture.
                Shader2& shader = batch.material ? ShadowMapMaskedShader : ShadowMapShader;
                pass.SetShader2(shader);
                pass.SetBufferIndex("ShadowCascade", c);
                if (batch.material)
                    pass.SetMaterial2(shader, batch.material->material);
                pass.SetMesh(batch.mesh);
                pass.Draw(batch.indexCount, batch.startIndex, batch.firstInstance, batch.instanceCount);
            }
        };
    }

    Logger::Info("OnStart done");
}

void EditorApp::OnUpdate(float deltaTime)
{
    if (!ImGui::GetIO().WantTextInput)
    {
        if (input.IsKeyPressed(Key::KEY_1))
            CurrentGizmoOperation = ImGuizmo::TRANSLATE;
        if (input.IsKeyPressed(Key::KEY_2))
            CurrentGizmoOperation = ImGuizmo::ROTATE;
        if (input.IsKeyPressed(Key::KEY_3))
            CurrentGizmoOperation = ImGuizmo::SCALE;
    }
    
    if(input.IsKeyPressed(Key::F11))
    {
        m_Window.ToggleFullscreen();
        m_Window.GetSurface();
        cam->SetViewportSize(m_Window.GetWidth(), m_Window.GetHeight());
        renderpass.Recreate(m_Window.GetWidth(), m_Window.GetHeight());
        postProcess.Resize(m_Window.GetWidth(), m_Window.GetHeight());
    }

    if (selectedEntityID != static_cast<uint64_t>(-1) &&
        input.IsKeyPressed(Key::DELETE) &&
        !ImGui::GetIO().WantTextInput &&
        !ImGuizmo::IsUsing())
    {
        scene.Destroy(selectedEntity);
        selectedEntity = Entity{};
        selectedEntityID = static_cast<uint64_t>(-1);
        if (inspectorWindow)
            inspectorWindow->ClearInspectedEntity();

        if (size_t unloaded = assets.UnloadUnused())
            Logger::Info("Unloaded " + std::to_string(unloaded) + " unused assets");
    }
    
    // Picking reads the id under the cursor back only when clicking; the answer arrives a frame
    // or two later. Clicks on UI or the gizmo don't pick.
    double x, y;
    input.GetMousePosition(x, y);
    Texture& pickTarget = renderpass.GetRenderTarget(1);
    if (input.IsMouseButtonPressed(MouseButton::Left) &&
        !ImGui::GetIO().WantCaptureMouse && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() &&
        x > 0 && y > 0 && x < m_Window.GetWidth() && y < m_Window.GetHeight())
    {
        pickTarget.RequestPixelReadback(static_cast<int>(x), static_cast<int>(y));
    }

    uint64_t pickedID;
    if (pickTarget.TryGetPixelReadback(pickedID))
    {
        DeselectEntity();
        if (pickedID != UINT64_MAX)
            SelectEntity(pickedID);
    }

    // Transforms changed here are propagated once, in OnRender.
    animationSystem.Run(deltaTime);
}

void EditorApp::OnGUI()
{
    if(selectedEntityID != static_cast<uint64_t>(-1) && selectedEntity.Has<WorldXform>())
    {
        WorldXform& form = *selectedEntity.Get<WorldXform>();
        glm::mat4 world = glm::make_mat4(form.model);

        if (DrawGizmo(world, cam->View(), cam->Projection()))
        {
            glm::mat4 local = world;
            Entity parent = scene.Parent(selectedEntity);
            if (parent.IsValid()) {
                glm::mat4 parentWorld = glm::make_mat4(parent.Get<WorldXform>()->model);
                local = glm::inverse(parentWorld) * world;
            }

            glm::vec3 translation = glm::vec3(local[3]);
            glm::vec3 scale(
                glm::length(glm::vec3(local[0])),
                glm::length(glm::vec3(local[1])),
                glm::length(glm::vec3(local[2])));

            glm::mat3 rotationMatrix(1.0f);
            if (scale.x > 0.000001f) rotationMatrix[0] = glm::vec3(local[0]) / scale.x;
            if (scale.y > 0.000001f) rotationMatrix[1] = glm::vec3(local[1]) / scale.y;
            if (scale.z > 0.000001f) rotationMatrix[2] = glm::vec3(local[2]) / scale.z;

            glm::quat rotation = glm::normalize(glm::quat_cast(rotationMatrix));

            selectedEntity.SetPosition(translation.x, translation.y, translation.z);
            selectedEntity.SetRotationQuat(rotation.x, rotation.y, rotation.z, rotation.w);
            selectedEntity.SetScale(scale.x, scale.y, scale.z);
        }
    }

    DrawTopMenu();
    for (auto& [name, window] : EditorWindows)
    {
        window->Draw();
    }
}

void EditorApp::SelectEntity(uint64_t id)
{
    if(ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;
    Entity entity = scene.FromId(id);
    if (!entity.IsValid())
        return;

    selectedEntityID = entity.RawId();
    selectedEntity = entity;
    if (inspectorWindow)
        inspectorWindow->SetInspectedEntity(scene, selectedEntity);
}

void EditorApp::DeselectEntity()
{
    if(ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;
    selectedEntityID = -1;
    if (inspectorWindow)
        inspectorWindow->ClearInspectedEntity();
}

#pragma region GUI


void EditorApp::DrawMat4(const char* id, float m[16], bool editable, float speed, const char* fmt)
{
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit
                          | ImGuiTableFlags_Borders
                          | ImGuiTableFlags_RowBg
                          | ImGuiTableFlags_NoSavedSettings;

    if (ImGui::BeginTable(id, 5, flags))
    {
        ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, 22.0f);
        ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("W", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6, 4));

        for (int r = 0; r < 4; ++r)
        {
            ImGui::TableNextRow();

            // Row label
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("r%d", r);

            for (int c = 0; c < 4; ++c)
            {
                ImGui::TableSetColumnIndex(c + 1);

                // Highlight the diagonal
                bool is_diag = (r == c);
                if (is_diag)
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 120, 255)); // warm tint

                ImGui::PushID(r * 4 + c);
                float& v = m[r * 4 + c]; // <-- change to m[c*4 + r] if your data is column-major

                if (editable)
                {
                    // Right-aligned cells
                    float w = ImGui::GetContentRegionAvail().x;
                    ImGui::SetNextItemWidth(w);
                    ImGui::DragFloat("##cell", &v, speed, 0, 0, fmt);
                }
                else
                {
                    ImGui::Text(fmt, v);
                }

                ImGui::PopID();
                if (is_diag)
                    ImGui::PopStyleColor();
            }
        }

        ImGui::PopStyleVar();
        ImGui::EndTable();

        // Small legend
        ImGui::TextDisabled("Diagonal highlighted • %s", editable ? "editable" : "read-only");
    }
}

void EditorApp::DrawTopMenu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New", "Ctrl+N")) {/* TODO */}
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {/* TODO */}
            if (ImGui::MenuItem("Save", "Ctrl+S")) {/* TODO */}
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {/* TODO */}
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences...", "Ctrl+,")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {/* set a flag to quit */}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {/* TODO */}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {/* TODO */}
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {/* TODO */}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {/* TODO */}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {/* TODO */}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            if (ImGui::MenuItem("Assets")) { EditorWindows["Assets"]->SetOpen(!EditorWindows["Assets"]->IsOpen()); }
            if (ImGui::MenuItem("Hierarchy")) { EditorWindows["Hierarchy"]->SetOpen(!EditorWindows["Hierarchy"]->IsOpen()); }
            if (ImGui::MenuItem("Inspector")) { EditorWindows["Inspector"]->SetOpen(!EditorWindows["Inspector"]->IsOpen()); }
            if (ImGui::MenuItem("Stats")) { EditorWindows["Stats"]->SetOpen(!EditorWindows["Stats"]->IsOpen()); }
            if (ImGui::MenuItem("Loaded Assets")) { EditorWindows["Loaded Assets"]->SetOpen(!EditorWindows["Loaded Assets"]->IsOpen()); }
            if (ImGui::MenuItem("Post Processing")) { EditorWindows["Post Processing"]->SetOpen(!EditorWindows["Post Processing"]->IsOpen()); }
            if (ImGui::MenuItem("Lighting")) { EditorWindows["Lighting"]->SetOpen(!EditorWindows["Lighting"]->IsOpen()); }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Documentation")) {/* open URL */}
            if (ImGui::MenuItem("Report Issue"))   {/* open URL */}
            ImGui::Separator();
            if (ImGui::MenuItem("About")) {}
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

bool EditorApp::DrawGizmo(glm::mat4& transform, const glm::mat4& view, const glm::mat4& proj)
{
    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);

    // 1. draw OUTSIDE any imgui window
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

    // 2. cover the entire app window
    ImGuizmo::SetRect(
        0,
        0,
        ImGui::GetIO().DisplaySize.x,
        ImGui::GetIO().DisplaySize.y
    );

    float matrix[16];
    memcpy(matrix, glm::value_ptr(transform), sizeof(matrix));

    bool manipulated = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                                            CurrentGizmoOperation, ImGuizmo::LOCAL, matrix);
    if (manipulated)
    {
        transform = glm::make_mat4(matrix);
    }

    return manipulated;
}


#pragma endregion

void EditorApp::ApplyLighting()
{
    const glm::vec3 lightDir = lighting.LightDirection();
    const glm::vec4 sunColor(lighting.sunColor * lighting.sunIntensity, 1.0f);
    const glm::vec4 skyParams(lighting.skyIntensity, lighting.reflectionIntensity, 0.0f, 0.0f);
    if (lightDir == ubo.lightDir && sunColor == ubo.sunColor && skyParams == ubo.skyParams)
        return;

    // A new sun direction moves every cascade, so they all redraw.
    if (lightDir != ubo.lightDir)
        shadowsDirty = true;
    ubo.lightDir = lightDir;
    ubo.sunColor = sunColor;
    ubo.skyParams = skyParams;
    uboDirty = true;
}

void EditorApp::UpdateShadowCascades()
{
    // How far toward the light a caster can be and still shadow a cascade.
    constexpr float kShadowCasterReach = 100.0f;
    // Split blend between logarithmic (even texel density on screen) and uniform slices.
    constexpr float kLogSplitWeight = 0.75f;

    const float nearClip = cam->NearClip();
    const float farClip = std::min(kShadowDistance, cam->FarClip());
    const float tanHalfFov = std::tan(glm::radians(cam->FieldOfView()) * 0.5f);
    // Distance of a slice corner from the view axis, per unit of view depth.
    const float cornerSlope = tanHalfFov * std::sqrt(cam->Aspect() * cam->Aspect() + 1.0f);

    const glm::vec3 lightDir = glm::normalize(ubo.lightDir);
    const glm::vec3 up = std::abs(lightDir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    // Rotation-only light view, used to snap cascade centres to whole shadow texels.
    const glm::mat4 lightRotation = glm::lookAtLH(glm::vec3(0.0f), lightDir, up);
    const glm::mat4 inverseLightRotation = glm::inverse(lightRotation);

    bool changed = false;
    float sliceNear = nearClip;
    for (uint32_t c = 0; c < kShadowCascades; ++c)
    {
        const float t = static_cast<float>(c + 1) / kShadowCascades;
        const float uniformSplit = nearClip + (farClip - nearClip) * t;
        const float logSplit = nearClip * std::pow(farClip / nearClip, t);
        const float sliceFar = glm::mix(uniformSplit, logSplit, kLogSplitWeight);

        // Smallest sphere around the slice with its centre on the view axis. It only depends on the
        // slice, not on where the camera looks, so the cascade keeps its size while turning.
        const float centerDepth = std::min(sliceFar, (sliceFar + sliceNear) * (1.0f + cornerSlope * cornerSlope) * 0.5f);
        float radius = std::sqrt(sliceFar * sliceFar * cornerSlope * cornerSlope + (sliceFar - centerDepth) * (sliceFar - centerDepth));
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // Snap the centre to whole texels in light space, so shadow edges don't crawl as the camera moves.
        const float texel = 2.0f * radius / kShadowMapSize;
        glm::vec3 center = glm::vec3(cameraInfo.invView * glm::vec4(0.0f, 0.0f, centerDepth, 1.0f));
        glm::vec3 centerLight = glm::vec3(lightRotation * glm::vec4(center, 1.0f));
        centerLight.x = std::floor(centerLight.x / texel) * texel;
        centerLight.y = std::floor(centerLight.y / texel) * texel;
        center = glm::vec3(inverseLightRotation * glm::vec4(centerLight, 1.0f));

        const float back = radius + kShadowCasterReach;
        const glm::mat4 viewProj = glm::orthoLH_ZO(-radius, radius, -radius, radius, 0.0f, back + radius) *
                                   glm::lookAtLH(center - lightDir * back, center, up);

        if (viewProj != ubo.cascadeVP[c] || ubo.cascadeSplits[c] != sliceFar)
        {
            ubo.cascadeVP[c] = viewProj;
            ubo.cascadeSplits[c] = sliceFar;
            ubo.cascadeTexel[c] = texel;
            standardPBRPipeline.shadowCascadeBuffer.Write(ShadowCascadeData{ viewProj }, c);
            changed = true;
        }
        cascadeFrustums[c] = ExtractFrustum(viewProj);
        sliceNear = sliceFar;
    }

    if (changed)
        uboDirty = true;
}

void EditorApp::BuildDrawLists()
{
    drawItems.clear();
    for (auto& items : shadowItems)
        items.clear();
    mainBatches.clear();
    for (auto& batches : shadowBatches)
        batches.clear();
    frameDraws.clear();
    skyboxMesh = nullptr;

    cameraFrustum = ExtractFrustum(cameraInfo.viewProj);
    gatherDrawsSystem.Run();
    gatherShadowCastersSystem.Run();

    // Identical (material, mesh, submesh) draws end up adjacent; each run becomes one instanced draw.
    std::sort(drawItems.begin(), drawItems.end(), [](const DrawItem& a, const DrawItem& b) {
        if (a.variant != b.variant) return a.variant < b.variant;
        if (a.material != b.material) return a.material < b.material;
        if (a.mesh != b.mesh) return a.mesh < b.mesh;
        return a.submesh < b.submesh;
    });

    for (const DrawItem& item : drawItems)
    {
        if (frameDraws.size() >= StandardPBRPipeline::kMaxDraws)
            break;

        DrawBatch* batch = mainBatches.empty() ? nullptr : &mainBatches.back();
        if (!batch || batch->material != item.material || batch->mesh != item.mesh || batch->submesh != item.submesh || batch->variant != item.variant)
        {
            const Submesh& sm = item.mesh->submeshes[item.submesh];
            mainBatches.push_back({ item.mesh, item.material, item.submesh, sm.indexCount, sm.startIndex,
                                    static_cast<uint32_t>(frameDraws.size()), 0, item.variant });
            batch = &mainBatches.back();
        }
        frameDraws.push_back(DrawData{ item.transformIndex, item.material->GetGpuSlot(), item.boneOffset });
        ++batch->instanceCount;
    }

    // The light is static, so a cascade only needs redrawing when it moved (camera) or the set of
    // casters inside it, or one of their transforms, changed. The caster part of the signature is a
    // sum so caster order doesn't matter.
    for (uint32_t c = 0; c < kShadowCascades; ++c)
    {
        uint64_t signature = shadowItems[c].size();
        const float* cascadeMatrix = glm::value_ptr(ubo.cascadeVP[c]);
        for (int i = 0; i < 16; ++i)
        {
            uint32_t bits;
            std::memcpy(&bits, &cascadeMatrix[i], sizeof(bits));
            signature = (signature ^ bits) * 0x100000001B3ull;
        }
        for (const ShadowItem& item : shadowItems[c])
        {
            uint64_t h = reinterpret_cast<uintptr_t>(item.mesh) ^ (uint64_t(item.transformIndex) << 32) ^ item.transformVersion;
            h ^= (uint64_t(item.startIndex) << 20) ^ reinterpret_cast<uintptr_t>(item.material);
            h *= 0x9E3779B97F4A7C15ull;
            signature += h ^ (h >> 29);
        }

        const bool redraw = shadowsDirty || signature != shadowSignatures[c];
        shadowSignatures[c] = signature;
        shadowpasses[c]->SetEnabled(redraw);
        if (!redraw)
            continue;

        // Identical index ranges are drawn instanced per caster; opaque ranges (no material) come first.
        std::vector<ShadowItem>& items = shadowItems[c];
        std::sort(items.begin(), items.end(), [](const ShadowItem& a, const ShadowItem& b) {
            if (a.material != b.material) return a.material < b.material;
            if (a.mesh != b.mesh) return a.mesh < b.mesh;
            return a.startIndex < b.startIndex;
        });

        for (const ShadowItem& item : items)
        {
            if (frameDraws.size() >= StandardPBRPipeline::kMaxDraws)
                break;

            DrawBatch* batch = shadowBatches[c].empty() ? nullptr : &shadowBatches[c].back();
            if (!batch || batch->mesh != item.mesh || batch->material != item.material ||
                batch->startIndex != item.startIndex || batch->indexCount != item.indexCount)
            {
                shadowBatches[c].push_back({ item.mesh, item.material, 0, item.indexCount, item.startIndex,
                                             static_cast<uint32_t>(frameDraws.size()), 0 });
                batch = &shadowBatches[c].back();
            }
            frameDraws.push_back(DrawData{ item.transformIndex, item.material ? item.material->GetGpuSlot() : 0, item.boneOffset });
            ++batch->instanceCount;
        }
    }
    shadowsDirty = false;

    if (!frameDraws.empty())
        standardPBRPipeline.drawBuffer.Write(frameDraws);
}

void EditorApp::OnRender()
{
    scene.Update(0.0f);

    // Uploaded here rather than in OnUpdate: the camera moves after OnUpdate each frame.
    cameraInfo = cam->GetCameraInfo();
    cameraInfo.exposure = postProcess.exposure; // the skybox undoes it, see skybox.wgsl
    standardPBRPipeline.cameraBuffer.Write(cameraInfo, 0);
    ApplyLighting();
    UpdateShadowCascades();
    if (uboDirty)
    {
        standardPBRPipeline.uboBuffer.Write(ubo, 0);
        uboDirty = false;
    }

    writeTransformBufferSystem.Run();
    standardPBRPipeline.FlushTransforms();
    standardPBRPipeline.UploadDirtyMaterials(assets);

    // Joint matrices for every skin, in one write; draws find theirs through SkinComponent::boneOffset.
    frameBones.clear();
    skinningSystem.Run();
    if (!frameBones.empty())
        standardPBRPipeline.boneBuffer.Write(frameBones);

    BuildDrawLists();
    postProcess.Update();
    renderer.Render(&gui);
}

void EditorApp::OnShutdown()
{
    Logger::Info("Shutdown now");
}




