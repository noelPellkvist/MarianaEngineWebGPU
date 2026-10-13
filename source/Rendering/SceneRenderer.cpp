#include <Rendering/SceneRenderer.hpp>
#include <Rendering/Components.hpp>
#include <Skin.hpp>
#include <AnimationPlayer.hpp>
#include <ICamera.hpp>
#include <FileReader.hpp>
#include <BuiltinAssets.hpp>
#include <Geometry/Primitives.hpp>
#include <SkyLight.hpp>
#include <Logger.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <glm/gtc/type_ptr.hpp>

namespace
{
    StandardVertex v{};
    VertexBufferLayout vertexLayout{v, v.position, v.normal, v.tangent, v.texcoord0, v.texcoord1, v.color0, v.boneIndices, v.boneWeights};

    const char* kSceneColorMS = "SceneColorMS";
    const char* kSceneDepthMS = "SceneDepthMS";

    std::string ShadowMapName(uint32_t cascade)
    {
        return "ShadowMap" + std::to_string(cascade);
    }

    // Planes of a view-projection matrix with [0, 1] clip depth (Gribb/Hartmann), pointing inwards.
    Frustum ExtractFrustum(const glm::mat4& viewProj)
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
    Bounds TransformBounds(const glm::mat4& m, const Bounds& box)
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
    bool IsVisible(const Frustum& frustum, const Bounds& box)
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
    bool IsVisible(const Frustum& frustum, const IMesh& mesh, const float* model, const SkinComponent* skin)
    {
        if (skin)
            return skin->bounds.IsEmpty() || IsVisible(frustum, skin->bounds);
        if (!mesh.hasBounds)
            return true;
        return IsVisible(frustum, TransformBounds(glm::make_mat4(model), Bounds{ mesh.boundsMin, mesh.boundsMax }));
    }

    // The pipeline a submesh draws with: lines and points have their own, double-sided materials skip
    // culling, a mirroring transform (negative determinant) culls the other side, and alpha-tested
    // materials use alpha to coverage.
    ShaderVariant DrawVariant(PrimitiveTopology topology, bool doubleSided, bool mirrored, bool alphaTested)
    {
        if (topology == PrimitiveTopology::Lines)
            return ShaderVariant::Lines;
        if (topology == PrimitiveTopology::Points)
            return ShaderVariant::Points;
        if (doubleSided)
            return alphaTested ? ShaderVariant::AlphaToCoverageDoubleSided : ShaderVariant::DoubleSided;
        if (mirrored)
            return alphaTested ? ShaderVariant::AlphaToCoverageMirrored : ShaderVariant::Mirrored;
        return alphaTested ? ShaderVariant::AlphaToCoverage : ShaderVariant::Default;
    }

    // Finds the entity of each joint of `skin`. Joints live in the same model as the skinned mesh,
    // so the search starts at the model's wrapper root: the first ancestor without an AnimationTargetEntity.
    void ResolveJoints(Scene& scene, Entity skinned, const Skin& skin, std::vector<Entity>& joints)
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

    // A cube seen from inside: the sky shader uses its positions as view directions.
    std::shared_ptr<IMesh> MakeSkyCube()
    {
        MeshSection cube = Primitives::Cube(2.0f);
        for (size_t i = 0; i + 2 < cube.indices.size(); i += 3)
            std::swap(cube.indices[i + 1], cube.indices[i + 2]);
        return BuildMesh({ std::move(cube) });
    }

    // Shader names of each MaterialTexture's texture and sampler.
    const char* const kMaterialTextureNames[] = { "albedoMap", "normalMap", "metallicRoughnessMap", "occlusionMap", "emissiveMap" };
    const char* const kMaterialSamplerNames[] = { "albedoSampler", "normalSampler", "metallicRoughnessSampler", "occlusionSampler", "emissiveSampler" };
}

SceneRenderer::SceneRenderer(Scene& scene, AssetManager2& assets)
    : scene(scene), assets(assets)
{
}

SceneRenderer::~SceneRenderer()
{
    if (graph)
        graph->RemoveOwner(this);
}

void SceneRenderer::RegisterComponents()
{
    ECS::RegisterTag<ShadowCasterTag>("ShadowCasterTag");
    ECS::RegisterComponent<MeshComponent>("MeshComponent");
    ECS::RegisterComponent<SkinComponent>("SkinComponent");
}

void SceneRenderer::Init(RenderGraph& renderGraph)
{
    graph = &renderGraph;

    AddShadowMaps();
    graph->AddTexture(kSceneDepth, GraphTextureDesc{ .format = TextureFormat::Depth24Plus }, this);
    graph->AddTexture(kSceneNormals, GraphTextureDesc{ .format = TextureFormat::RGB10A2Unorm }, this);
    graph->AddTexture(kEntityIds, GraphTextureDesc{ .format = TextureFormat::RGBA16Uint }, this);
    graph->AddTexture(kSceneColorMS, GraphTextureDesc{ .format = TextureFormat::RGBA16Float, .msaa = true }, this);
    graph->AddTexture(kSceneDepthMS, GraphTextureDesc{ .format = TextureFormat::Depth24Plus, .msaa = true }, this);
    graph->AddTexture(kSceneColor, GraphTextureDesc{ .format = TextureFormat::RGBA16Float }, this);
    graph->AddTexture(kAmbientOcclusion, GraphTextureDesc{ .format = TextureFormat::R8Unorm }, this);
    unoccluded = BuiltinAssets::WhiteTexture(assets, false);
    graph->SetFallback(kAmbientOcclusion, unoccluded.Get().get());

    // What an empty material texture slot samples: neutral for its use.
    materialFallbacks = { BuiltinAssets::WhiteTexture(assets, true), BuiltinAssets::FlatNormalTexture(assets),
                          BuiltinAssets::WhiteTexture(assets, false), BuiltinAssets::WhiteTexture(assets, false),
                          BuiltinAssets::WhiteTexture(assets, true) };

    for (uint32_t c = 0; c < kShadowCascades; ++c)
    {
        shadowPasses[c] = &graph->AddPass(GraphPassDesc{
            .name = "Shadow cascade " + std::to_string(c),
            .depthTarget = ShadowMapName(c),
            .record = [this, c](Renderpass& pass) { RecordShadows(pass, c); } }, this);
    }
    // Depth, view-space normals (for screen-space effects) and entity ids (for picking), single sampled.
    Renderpass& prepass = graph->AddPass(GraphPassDesc{
        .name = "Prepass",
        .colorTargets = { kSceneNormals, kEntityIds },
        .depthTarget = kSceneDepth,
        .keep = writeEntityIds,
        .record = [this](Renderpass& pass) { RecordPrepass(pass); } }, this);
    // Linear HDR with 4x MSAA, resolved into SceneColor.
    std::vector<std::string> forwardReads = { kAmbientOcclusion };
    for (uint32_t c = 0; c < kShadowCascades; ++c)
        forwardReads.push_back(ShadowMapName(c));
    Renderpass& forward = graph->AddPass(GraphPassDesc{
        .name = "Forward",
        .colorTargets = { kSceneColorMS },
        .resolveTargets = { kSceneColor },
        .depthTarget = kSceneDepthMS,
        .reads = forwardReads,
        .record = [this](Renderpass& pass) { RecordForward(pass); } }, this);

    // Textures exist from here on, so the shaders can bind them.
    graph->OnTexturesChanged(this, [this]() { BindGraphTextures(); });
    graph->Compile();

    LoadSky();
    BuildShaders(*shadowPasses[0], prepass, forward);
    defaultMaterial = BuiltinAssets::DefaultMaterial(assets);
    CreateSystems();
}

void SceneRenderer::AddShadowMaps()
{
    const uint32_t size = std::max(settings.shadowMapSize, 16u);
    for (uint32_t c = 0; c < kShadowCascades; ++c)
        graph->AddTexture(ShadowMapName(c), GraphTextureDesc{ .format = TextureFormat::Depth24Plus, .width = size, .height = size }, this);
}

void SceneRenderer::SetSettings(const SceneRendererSettings& value)
{
    if (value == settings)
        return;
    const bool resize = value.shadowMapSize != settings.shadowMapSize;
    settings = value;
    // The graph recreates the maps on its next compile and the shaders rebind them (BindGraphTextures).
    if (resize && graph)
        AddShadowMaps();
    shadowsDirty = true;
}

void SceneRenderer::LoadSky()
{
    // The sky is the background and a light: its cubemap (with mips) is reflected, and its
    // spherical harmonics light everything diffusely (SkyLight.hpp).
    std::vector<std::vector<uint8_t>> faces;
    int faceSize = 0;
    for (const char* face : { "right", "left", "top", "bottom", "back", "front" })
    {
        int width = 0, height = 0;
        faces.push_back(FileReader::LoadPixelsFromImage(std::string("/Textures/skybox/") + face + ".jpg", width, height));
        faceSize = width;
    }
    auto cubemap = std::make_shared<Texture>();
    cubemap->CreateCubeTexture(faces, faceSize, TextureFormat::RGBA8UnormSrgb);
    ubo.skySH = ComputeSkyIrradianceSH(faces, faceSize);
    skyCubemap = assets.RegisterAsset<Texture>(cubemap, AssetType::Texture, "Skybox Cubemap", "builtin://texture/skybox");
    assets.SetPersistent(skyCubemap.GetID(), true);
    skyMesh = MakeSkyCube();
}

void SceneRenderer::BuildShaders(Renderpass& shadow, Renderpass& prepass, Renderpass& forward)
{
    // The PBR material group; the masked shadow and prepass shaders declare the same one, so the
    // materials' bind groups work with them too.
    auto materialGroup = [](Shader2& shader) {
        shader.Group(1)
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
        shader.SetMaterialGroup(1);
    };

    shadowShader.Group(0)
    .AddBuffer("ShadowCascade", 0, pipeline.shadowCascadeBuffer)
    .AddBuffer("ModelData", 1, pipeline.transformBuffer)
    .AddBuffer("DrawInfo", 2, pipeline.drawBuffer)
    .AddBuffer("BoneData", 3, pipeline.boneBuffer);
    shadowShader.SetVertexStructLayout(vertexLayout);
    shadowShader.SetWGSL(FileReader::LoadRawString("/Shaders/shadow.wgsl"));
    shadowShader.SetRenderpass(&shadow);
    shadowShader.Build(true);

    // Alpha-tested casters: same shader plus a fragment stage that discards like the main pass.
    shadowMaskedShader.Group(0)
    .AddBuffer("ShadowCascade", 0, pipeline.shadowCascadeBuffer)
    .AddBuffer("ModelData", 1, pipeline.transformBuffer)
    .AddBuffer("DrawInfo", 2, pipeline.drawBuffer)
    .AddBuffer("BoneData", 3, pipeline.boneBuffer)
    .AddBuffer("Material", 4, pipeline.materialsBuffer);
    materialGroup(shadowMaskedShader);
    shadowMaskedShader.SetVertexStructLayout(vertexLayout);
    shadowMaskedShader.SetWGSL(FileReader::LoadRawString("/Shaders/shadow.wgsl"));
    shadowMaskedShader.SetRenderpass(&shadow);
    shadowMaskedShader.SetDepthOnlyFragment(true);
    shadowMaskedShader.Build(true);

    pbrShader.Group(0)
    .AddBuffer("UBO", 0, pipeline.uboBuffer)
    .AddBuffer("CameraInfo", 1, pipeline.cameraBuffer)
    .AddBuffer("DrawInfo", 2, pipeline.drawBuffer)
    .AddBuffer("ModelData", 3, pipeline.transformBuffer)
    .AddBuffer("Material", 4, pipeline.materialsBuffer)
    .AddBuffer("BoneData", 5, pipeline.boneBuffer)
    .AddTexture("ShadowMap0", 6, TextureType_Depth)
    .AddTexture("ShadowMap1", 7, TextureType_Depth)
    .AddTexture("ShadowMap2", 8, TextureType_Depth)
    .AddTexture("ShadowMap3", 9, TextureType_Depth)
    .AddSampler("ShadowSampler", 10, SamplerType_Comparison)
    .AddTexture("SkyCube", 11, TextureType_Cube)
    .AddSampler("SkySampler", 12, SamplerType_LinearClamp)
    .AddTexture("AmbientOcclusion", 13, TextureType_2D);
    for (uint32_t c = 0; c < kShadowCascades; ++c)
        pbrShader.SetTexture(ShadowMapName(c), graph->GetTexture(ShadowMapName(c)));
    pbrShader.SetTexture("SkyCube", *skyCubemap.Get());
    pbrShader.SetTexture("AmbientOcclusion", graph->GetTexture(kAmbientOcclusion));
    materialGroup(pbrShader);
    pbrShader.SetVertexStructLayout(vertexLayout);
    pbrShader.SetWGSL(FileReader::LoadRawString("/Shaders/test.wgsl"));
    pbrShader.SetRenderpass(&forward);
    pbrShader.SetDrawVariants(true);
    pbrShader.Build();

    prepassShader.Group(0)
    .AddBuffer("CameraInfo", 0, pipeline.cameraBuffer)
    .AddBuffer("DrawInfo", 1, pipeline.drawBuffer)
    .AddBuffer("ModelData", 2, pipeline.transformBuffer)
    .AddBuffer("Material", 3, pipeline.materialsBuffer)
    .AddBuffer("BoneData", 4, pipeline.boneBuffer);
    materialGroup(prepassShader);
    prepassShader.SetVertexStructLayout(vertexLayout);
    prepassShader.SetWGSL(FileReader::LoadRawString("/Shaders/prepass.wgsl"));
    prepassShader.SetRenderpass(&prepass);
    prepassShader.SetDrawVariants(true);
    prepassShader.Build();

    skyboxShader.Group(0)
    .AddBuffer("CameraInfo", 0, pipeline.cameraBuffer)
    .AddTexture("CubeMap", 1, TextureType_Cube)
    .AddSampler("Sampler", 2);
    skyboxShader.SetTexture("CubeMap", *skyCubemap.Get());
    skyboxShader.SetVertexStructLayout(vertexLayout);
    skyboxShader.SetWGSL(FileReader::LoadRawString("/Shaders/skybox.wgsl"));
    skyboxShader.SetRenderpass(&forward);
    skyboxShader.Build();

    pipeline.boneBuffer.Write(BoneData{}, 0);
    built = true;
}

void SceneRenderer::BindGraphTextures()
{
    if (!built)
        return;
    for (uint32_t c = 0; c < kShadowCascades; ++c)
        pbrShader.BindTexture(ShadowMapName(c), graph->GetTexture(ShadowMapName(c)));
    pbrShader.BindTexture("AmbientOcclusion", graph->GetTexture(kAmbientOcclusion));
    pbrShader.RebuildBindGroups();
}

void SceneRenderer::BuildMaterialBindGroups()
{
    // The masked shadow and prepass shaders declare the same material group as the PBR shader, so
    // one bind group serves all three.
    assets.ForEach<MaterialAsset>([this](AssetID, MaterialAsset& material) {
        if (!material.BindGroupDirty())
            return;

        material.material.InitFromShader(pbrShader);
        for (uint32_t t = 0; t < static_cast<uint32_t>(MaterialTexture::Count); ++t)
        {
            const MaterialAsset::TextureSlot& slot = material.GetTexture(static_cast<MaterialTexture>(t));
            Texture* texture = slot.texture.Get().get();
            if (!texture || !texture->GetTextureView())
                texture = materialFallbacks[t].Get().get();
            material.material.SetTexture(kMaterialTextureNames[t], *texture)
                             .SetSampler(kMaterialSamplerNames[t], slot.sampler);
        }
        material.material.Build();
        material.ClearBindGroupDirty();
    });
}

void SceneRenderer::CreateSystems()
{
    writeTransformBufferSystem = scene.CreateSystem<WorldXform, MeshComponent>([this](Entity ent, WorldXform& form, MeshComponent& mesh, float dt){
        // Only transforms that changed since their last upload; the rest are already on the GPU.
        if (form.id >= uploadedTransformVersions.size())
            uploadedTransformVersions.resize(form.id + 1, ~0u);
        if (uploadedTransformVersions[form.id] == form.version)
            return;
        uploadedTransformVersions[form.id] = form.version;

        transformData.modelMatrix = glm::make_mat4(form.model);
        transformData.normalMatrix = glm::make_mat3(form.normal);
        transformData.SetEntityID(ent.RawId());
        pipeline.QueueTransform(form.id, transformData);
    });

    gatherDrawsSystem = scene.CreateSystem<MeshComponent, WorldXform>([this](Entity ent, MeshComponent& meshComp, WorldXform& form, float dt){
        IMesh* mesh = meshComp.mesh.Get().get();
        if (!mesh)
            return;

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
                drawItems.push_back({ material, mesh, i, form.id, boneOffset, DrawVariant(mesh->submeshes[i].topology, material->doubleSided, mirrored, material->properties.alphaCutoff > 0.0f) });
        }
    });

    gatherShadowCastersSystem = scene.CreateSystem<MeshComponent, WorldXform, ShadowCasterTag>([this](Entity ent, MeshComponent& meshComp, WorldXform& form, ShadowCasterTag& tag, float dt){
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

    skinningSystem = scene.CreateSystem<SkinComponent, MeshComponent, WorldXform>([this](Entity ent, SkinComponent& skinComp, MeshComponent& meshComp, WorldXform& form, float dt){
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
}

void SceneRenderer::Prepare(ICamera& camera)
{
    // Uploaded here rather than earlier in the frame: the camera may move after the game updates.
    cameraInfo = camera.GetCameraInfo();
    pipeline.cameraBuffer.Write(cameraInfo, 0);
    ApplyLighting();
    UpdateShadowCascades(camera);
    if (uboDirty)
    {
        pipeline.uboBuffer.Write(ubo, 0);
        uboDirty = false;
    }

    writeTransformBufferSystem.Run();
    pipeline.FlushTransforms();
    BuildMaterialBindGroups();
    pipeline.UploadDirtyMaterials(assets);

    // Joint matrices for every skin, in one write; draws find theirs through SkinComponent::boneOffset.
    frameBones.clear();
    skinningSystem.Run();
    if (!frameBones.empty())
        pipeline.boneBuffer.Write(frameBones);

    BuildDrawLists();
}

void SceneRenderer::ApplyLighting()
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

void SceneRenderer::UpdateShadowCascades(const ICamera& camera)
{
    // How far toward the light a caster can be and still shadow a cascade.
    constexpr float kShadowCasterReach = 100.0f;
    // Split blend between logarithmic (even texel density on screen) and uniform slices.
    constexpr float kLogSplitWeight = 0.75f;

    const float nearClip = camera.NearClip();
    const float farClip = std::max(std::min(settings.shadowDistance, camera.FarClip()), nearClip * 2.0f);
    const float tanHalfFov = std::tan(glm::radians(camera.FieldOfView()) * 0.5f);
    // Distance of a slice corner from the view axis, per unit of view depth.
    const float cornerSlope = tanHalfFov * std::sqrt(camera.Aspect() * camera.Aspect() + 1.0f);

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
        const float texel = 2.0f * radius / std::max(settings.shadowMapSize, 16u);
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
            pipeline.shadowCascadeBuffer.Write(ShadowCascadeData{ viewProj }, c);
            changed = true;
        }
        cascadeFrustums[c] = ExtractFrustum(viewProj);
        sliceNear = sliceFar;
    }

    if (changed)
        uboDirty = true;
}

void SceneRenderer::BuildDrawLists()
{
    drawItems.clear();
    for (auto& items : shadowItems)
        items.clear();
    mainBatches.clear();
    for (auto& batches : shadowBatches)
        batches.clear();
    frameDraws.clear();

    cameraFrustum = ExtractFrustum(cameraInfo.viewProj);
    gatherDrawsSystem.Run();
    // Without shadows the maps are cleared once (no casters) and then left alone.
    if (settings.shadows)
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
        // An empty map is the same wherever the cascade is.
        const float* cascadeMatrix = glm::value_ptr(ubo.cascadeVP[c]);
        for (int i = 0; i < 16 && !shadowItems[c].empty(); ++i)
        {
            uint32_t bits;
            std::memcpy(&bits, &cascadeMatrix[i], sizeof(bits));
            signature = (signature ^ bits) * 0x100000001B3ull;
        }
        for (const ShadowItem& item : shadowItems[c])
        {
            uint64_t h = reinterpret_cast<uintptr_t>(item.mesh) ^ (uint64_t(item.transformIndex) << 32) ^ item.transformVersion;
            h ^= (uint64_t(item.startIndex) << 20) ^ reinterpret_cast<uintptr_t>(item.material);
            // An edited alpha-tested material may cut different holes.
            if (item.material)
                h ^= uint64_t(item.material->Version()) * 0xC2B2AE3D27D4EB4Full;
            h *= 0x9E3779B97F4A7C15ull;
            signature += h ^ (h >> 29);
        }

        const bool redraw = shadowsDirty || signature != shadowSignatures[c];
        shadowSignatures[c] = signature;
        shadowPasses[c]->SetEnabled(redraw);
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
        pipeline.drawBuffer.Write(frameDraws);
}

void SceneRenderer::RecordShadows(Renderpass& pass, uint32_t cascade)
{
    for (const DrawBatch& batch : shadowBatches[cascade])
    {
        // Opaque casters sort first; alpha-tested ones need their material's texture.
        Shader2& shader = batch.material ? shadowMaskedShader : shadowShader;
        pass.SetShader2(shader);
        pass.SetBufferIndex("ShadowCascade", cascade);
        if (batch.material)
            pass.SetMaterial2(shader, batch.material->material);
        pass.SetMesh(batch.mesh);
        pass.Draw(batch.indexCount, batch.startIndex, batch.firstInstance, batch.instanceCount);
    }
}

void SceneRenderer::RecordPrepass(Renderpass& pass)
{
    // The same draws as the forward pass.
    for (const DrawBatch& batch : mainBatches)
    {
        pass.SetShader2(prepassShader, batch.variant);
        pass.SetMesh(batch.mesh);
        pass.SetMaterial2(prepassShader, batch.material->material);
        pass.Draw(batch.indexCount, batch.startIndex, batch.firstInstance, batch.instanceCount);
    }
}

void SceneRenderer::RecordForward(Renderpass& pass)
{
    for (const DrawBatch& batch : mainBatches)
    {
        pass.SetShader2(pbrShader, batch.variant);
        pass.SetMesh(batch.mesh);
        pass.SetMaterial2(pbrShader, batch.material->material);
        pass.Draw(batch.indexCount, batch.startIndex, batch.firstInstance, batch.instanceCount);
    }

    // Drawn last: it sits at the far plane, so the depth test skips every pixel already covered.
    if (skyMesh)
    {
        pass.SetShader2(skyboxShader);
        pass.SetMesh(skyMesh.get());
        pass.Draw(static_cast<uint32_t>(skyMesh->IndexCount()), 0, 0);
    }
}
