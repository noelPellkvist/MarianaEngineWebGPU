#include <Mariana/Rendering/SceneView.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/ICamera.hpp>
#include <Mariana/Animation/Skin.hpp>
#include <Mariana/Foundation/FileReader.hpp>
#include "Rendering/SceneCulling.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <glm/gtc/type_ptr.hpp>

using SceneCulling::ExtractFrustum;
using SceneCulling::IsVisible;

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
}

SceneView::SceneView(SceneRenderer& renderer)
    : renderer(renderer)
{
}

SceneView::~SceneView()
{
    if (graph)
        graph->RemoveOwner(this);
}

void SceneView::Init(RenderGraph& renderGraph)
{
    graph = &renderGraph;
    settings = renderer.settings;
    shadowEpoch = renderer.shadowEpoch;

    AddShadowMaps();
    graph->AddTexture(kSceneDepth, GraphTextureDesc{ .format = TextureFormat::Depth24Plus }, this);
    graph->AddTexture(kSceneNormals, GraphTextureDesc{ .format = TextureFormat::RGB10A2Unorm }, this);
    graph->AddTexture(kEntityIds, GraphTextureDesc{ .format = TextureFormat::RGBA16Uint }, this);
    graph->AddTexture(kSceneColorMS, GraphTextureDesc{ .format = TextureFormat::RGBA16Float, .msaa = true }, this);
    graph->AddTexture(kSceneDepthMS, GraphTextureDesc{ .format = TextureFormat::Depth24Plus, .msaa = true }, this);
    graph->AddTexture(kSceneColor, GraphTextureDesc{ .format = TextureFormat::RGBA16Float }, this);
    graph->AddTexture(kAmbientOcclusion, GraphTextureDesc{ .format = TextureFormat::R8Unorm }, this);
    graph->SetFallback(kAmbientOcclusion, renderer.unoccluded.Get().get());

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

    BuildShaders(*shadowPasses[0], prepass, forward);
    CreateSystems();
}

void SceneView::AddShadowMaps()
{
    const uint32_t size = std::max(settings.shadowMapSize, 16u);
    for (uint32_t c = 0; c < kShadowCascades; ++c)
        graph->AddTexture(ShadowMapName(c), GraphTextureDesc{ .format = TextureFormat::Depth24Plus, .width = size, .height = size }, this);
}

void SceneView::BuildShaders(Renderpass& shadow, Renderpass& prepass, Renderpass& forward)
{
    StandardPBRPipeline& scene = renderer.pipeline;

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
    .AddBuffer("ShadowCascade", 0, buffers.shadowCascadeBuffer)
    .AddBuffer("ModelData", 1, scene.transformBuffer)
    .AddBuffer("DrawInfo", 2, buffers.drawBuffer)
    .AddBuffer("BoneData", 3, scene.boneBuffer);
    shadowShader.SetVertexStructLayout(vertexLayout);
    shadowShader.SetWGSL(FileReader::LoadRawString("/Shaders/shadow.wgsl"));
    shadowShader.SetRenderpass(&shadow);
    shadowShader.Build(true);

    // Alpha-tested casters: same shader plus a fragment stage that discards like the main pass.
    shadowMaskedShader.Group(0)
    .AddBuffer("ShadowCascade", 0, buffers.shadowCascadeBuffer)
    .AddBuffer("ModelData", 1, scene.transformBuffer)
    .AddBuffer("DrawInfo", 2, buffers.drawBuffer)
    .AddBuffer("BoneData", 3, scene.boneBuffer)
    .AddBuffer("Material", 4, scene.materialsBuffer);
    materialGroup(shadowMaskedShader);
    shadowMaskedShader.SetVertexStructLayout(vertexLayout);
    shadowMaskedShader.SetWGSL(FileReader::LoadRawString("/Shaders/shadow.wgsl"));
    shadowMaskedShader.SetRenderpass(&shadow);
    shadowMaskedShader.SetDepthOnlyFragment(true);
    shadowMaskedShader.Build(true);

    pbrShader.Group(0)
    .AddBuffer("UBO", 0, buffers.uboBuffer)
    .AddBuffer("CameraInfo", 1, buffers.cameraBuffer)
    .AddBuffer("DrawInfo", 2, buffers.drawBuffer)
    .AddBuffer("ModelData", 3, scene.transformBuffer)
    .AddBuffer("Material", 4, scene.materialsBuffer)
    .AddBuffer("BoneData", 5, scene.boneBuffer)
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
    pbrShader.SetTexture("SkyCube", *renderer.skyCubemap.Get());
    pbrShader.SetTexture("AmbientOcclusion", graph->GetTexture(kAmbientOcclusion));
    materialGroup(pbrShader);
    pbrShader.SetVertexStructLayout(vertexLayout);
    pbrShader.SetWGSL(FileReader::LoadRawString("/Shaders/test.wgsl"));
    pbrShader.SetRenderpass(&forward);
    pbrShader.SetDrawVariants(true);
    pbrShader.Build();

    prepassShader.Group(0)
    .AddBuffer("CameraInfo", 0, buffers.cameraBuffer)
    .AddBuffer("DrawInfo", 1, buffers.drawBuffer)
    .AddBuffer("ModelData", 2, scene.transformBuffer)
    .AddBuffer("Material", 3, scene.materialsBuffer)
    .AddBuffer("BoneData", 4, scene.boneBuffer);
    materialGroup(prepassShader);
    prepassShader.SetVertexStructLayout(vertexLayout);
    prepassShader.SetWGSL(FileReader::LoadRawString("/Shaders/prepass.wgsl"));
    prepassShader.SetRenderpass(&prepass);
    prepassShader.SetDrawVariants(true);
    prepassShader.Build();

    skyboxShader.Group(0)
    .AddBuffer("CameraInfo", 0, buffers.cameraBuffer)
    .AddTexture("CubeMap", 1, TextureType_Cube)
    .AddSampler("Sampler", 2)
    .AddBuffer("UBO", 3, buffers.uboBuffer);
    skyboxShader.SetTexture("CubeMap", *renderer.skyCubemap.Get());
    skyboxShader.SetVertexStructLayout(vertexLayout);
    skyboxShader.SetWGSL(FileReader::LoadRawString("/Shaders/skybox.wgsl"));
    skyboxShader.SetRenderpass(&forward);
    skyboxShader.Build();

    built = true;
}

void SceneView::BindGraphTextures()
{
    if (!built)
        return;
    for (uint32_t c = 0; c < kShadowCascades; ++c)
        pbrShader.BindTexture(ShadowMapName(c), graph->GetTexture(ShadowMapName(c)));
    pbrShader.BindTexture("AmbientOcclusion", graph->GetTexture(kAmbientOcclusion));
    pbrShader.RebuildBindGroups();
}

void SceneView::CreateSystems()
{
    Scene& scene = renderer.scene;

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
                material = renderer.defaultMaterial.Get().get();
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
}

void SceneView::Prepare(ICamera& camera)
{
    ApplySettings();
    // Uploaded here rather than earlier in the frame: the camera may move after the game updates.
    cameraInfo = camera.GetCameraInfo();
    buffers.cameraBuffer.Write(cameraInfo, 0);
    ApplyLighting();
    UpdateShadowCascades(camera);
    if (uboDirty)
    {
        buffers.uboBuffer.Write(ubo, 0);
        uboDirty = false;
    }

    // Built once, whichever view sees a material first; every view's pipelines take them.
    renderer.BuildMaterialBindGroups(pbrShader);
    BuildDrawLists();
}

void SceneView::ApplySettings()
{
    if (renderer.shadowEpoch != shadowEpoch)
    {
        shadowEpoch = renderer.shadowEpoch;
        shadowsDirty = true;
    }
    if (renderer.settings == settings)
        return;
    const bool resize = renderer.settings.shadowMapSize != settings.shadowMapSize;
    settings = renderer.settings;
    // The graph recreates the maps on its next compile and the shaders rebind them (BindGraphTextures).
    if (resize && graph)
        AddShadowMaps();
    shadowsDirty = true;
}

void SceneView::ApplyLighting()
{
    const LightingSettings& lighting = renderer.lighting;
    const glm::vec3 lightDir = lighting.LightDirection();
    const glm::vec4 sunColor(lighting.sunColor * lighting.sunIntensity, 1.0f);
    const glm::vec4 skyParams(lighting.skyIntensity, lighting.reflectionIntensity, glm::radians(lighting.skyRotation), 0.0f);
    if (lightDir == ubo.lightDir && sunColor == ubo.sunColor && skyParams == ubo.skyParams && ubo.skySH == renderer.skySH)
        return;

    // A new sun direction moves every cascade, so they all redraw.
    if (lightDir != ubo.lightDir)
        shadowsDirty = true;
    ubo.lightDir = lightDir;
    ubo.sunColor = sunColor;
    ubo.skyParams = skyParams;
    ubo.skySH = renderer.skySH;
    uboDirty = true;
}

void SceneView::UpdateShadowCascades(const ICamera& camera)
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
            buffers.shadowCascadeBuffer.Write(ShadowCascadeData{ viewProj }, c);
            changed = true;
        }
        cascadeFrustums[c] = ExtractFrustum(viewProj);
        sliceNear = sliceFar;
    }

    if (changed)
        uboDirty = true;
}

void SceneView::BuildDrawLists()
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
        if (frameDraws.size() >= StandardPBRView::kMaxDraws)
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
            if (frameDraws.size() >= StandardPBRView::kMaxDraws)
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
        buffers.drawBuffer.Write(frameDraws);
}

void SceneView::RecordShadows(Renderpass& pass, uint32_t cascade)
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

void SceneView::RecordPrepass(Renderpass& pass)
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

void SceneView::RecordForward(Renderpass& pass)
{
    for (const DrawBatch& batch : mainBatches)
    {
        pass.SetShader2(pbrShader, batch.variant);
        pass.SetMesh(batch.mesh);
        pass.SetMaterial2(pbrShader, batch.material->material);
        pass.Draw(batch.indexCount, batch.startIndex, batch.firstInstance, batch.instanceCount);
    }

    // Drawn last: it sits at the far plane, so the depth test skips every pixel already covered.
    // Without it the background stays clear (0, alpha 0).
    if (renderer.skyMesh && settings.sky)
    {
        pass.SetShader2(skyboxShader);
        pass.SetMesh(renderer.skyMesh.get());
        pass.Draw(static_cast<uint32_t>(renderer.skyMesh->IndexCount()), 0, 0);
    }
}
