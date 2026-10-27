#include <Mariana/Rendering/SceneRenderer.hpp>
#include <Mariana/Foundation/Profiler.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Animation/Skin.hpp>
#include <Mariana/Animation/Skeleton.hpp>
#include <Mariana/Foundation/FileReader.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Geometry/Primitives.hpp>
#include <Mariana/Rendering/SkyLight.hpp>
#include "Rendering/SceneCulling.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <glm/gtc/type_ptr.hpp>

namespace
{
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

SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::RegisterComponents()
{
    ECS::RegisterTag<ShadowCasterTag>("ShadowCasterTag");
    ECS::RegisterComponent<MeshComponent>("MeshComponent");
    ECS::RegisterComponent<SkinComponent>("SkinComponent");
    ECS::RegisterRuntimeComponent<SkinPose>("SkinPose");
    ECS::RegisterRuntimeComponent<MeshOverride>("MeshOverride");
    ECS::RegisterComponent<CameraComponent>("CameraComponent");
}

void SceneRenderer::Init()
{
    unoccluded = BuiltinAssets::WhiteTexture(assets, false);
    // What an empty material texture slot samples: neutral for its use.
    materialFallbacks = { BuiltinAssets::WhiteTexture(assets, true), BuiltinAssets::FlatNormalTexture(assets),
                          BuiltinAssets::WhiteTexture(assets, false), BuiltinAssets::WhiteTexture(assets, false),
                          BuiltinAssets::WhiteTexture(assets, true) };
    defaultMaterial = BuiltinAssets::DefaultMaterial(assets);
    LoadSky();
    pipeline.boneBuffer.Write(BoneData{}, 0);
    CreateSystems();
}

void SceneRenderer::SetSettings(const SceneRendererSettings& value)
{
    // The views notice and apply it (new shadow maps, redrawn cascades) when they next prepare.
    settings = value;
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
    skySH = ComputeSkyIrradianceSH(faces, faceSize);
    skyCubemap = assets.RegisterAsset<Texture>(cubemap, AssetType::Texture, "Skybox Cubemap", "builtin://texture/skybox");
    assets.SetPersistent(skyCubemap.GetID(), true);
    skyMesh = MakeSkyCube();
}

void SceneRenderer::BuildMaterialBindGroups(Shader2& shader)
{
    // The masked shadow and prepass shaders declare the same material group as the PBR shader, so
    // one bind group serves all three, in every view.
    assets.ForEach<MaterialAsset>([&](AssetID, MaterialAsset& material) {
        if (!material.BindGroupDirty())
            return;

        material.material.InitFromShader(shader);
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

    skinningSystem = scene.CreateSystem<SkinComponent, SkinPose, MeshComponent, WorldXform>([this](Entity ent, SkinComponent& skinComp, SkinPose& pose, MeshComponent& meshComp, WorldXform& form, float dt){
        const std::shared_ptr<Skin>& skin = skinComp.skin.Get();
        pose.boneOffset = ~0u;
        pose.bounds = {};
        if (!skin || frameBones.size() + skin->jointNodes.size() > StandardPBRPipeline::kMaxBones)
            return;

        if (pose.joints.size() != skin->jointNodes.size())
            ResolveJoints(scene, ent, *skin, pose.joints);

        // Relative to the mesh node, because the shader still applies the node's model matrix.
        const glm::mat4 worldToMesh = glm::inverse(glm::make_mat4(form.model));
        pose.boneOffset = static_cast<uint32_t>(frameBones.size());
        pose.poseVersion = 0;

        // Each joint's box, moved like the vertices it holds, grows the posed mesh's bounds.
        // Without joint bounds the bounds stay empty and the mesh is never culled.
        const IMesh* mesh = meshComp.mesh.Get().get();

        for (size_t i = 0; i < skin->jointNodes.size(); ++i)
        {
            const WorldXform* joint = pose.joints[i].IsValid() ? pose.joints[i].TryGet<WorldXform>() : nullptr;
            // Bind pose to world.
            const glm::mat4 jointSkin = (joint ? glm::make_mat4(joint->model) : glm::mat4(1.0f)) * skin->inverseBind[i];
            BoneData bone;
            bone.model = worldToMesh * jointSkin;
            frameBones.push_back(bone);
            pose.poseVersion += joint ? joint->version : 0;

            if (mesh && i < mesh->jointBounds.size() && !mesh->jointBounds[i].IsEmpty())
            {
                const Bounds box = SceneCulling::TransformBounds(jointSkin, mesh->jointBounds[i]);
                pose.bounds.Add(box.min);
                pose.bounds.Add(box.max);
            }
        }
        pose.poseVersion += form.version;
    });
}

void SceneRenderer::Update()
{
    MARIANA_PROFILE("Scene data");
    writeTransformBufferSystem.Run();
    pipeline.FlushTransforms();
    pipeline.UploadDirtyMaterials(assets);

    // Every skin gets its SkinPose (added outside the queries; one without a skin any more loses it).
    std::vector<Entity> posed, unposed;
    scene.Each<SkinComponent>([&](Entity e, const SkinComponent&) { if (!e.TryGet<SkinPose>()) posed.push_back(e); });
    scene.Each<SkinPose>([&](Entity e, const SkinPose&) { if (!e.TryGet<SkinComponent>()) unposed.push_back(e); });
    for (Entity e : posed)
        e.Add<SkinPose>();
    for (Entity e : unposed)
        e.Remove<SkinPose>();

    // Joint matrices for every skin, in one write; draws find theirs through SkinPose::boneOffset.
    frameBones.clear();
    skinningSystem.Run();
    if (!frameBones.empty())
        pipeline.boneBuffer.Write(frameBones);
}
