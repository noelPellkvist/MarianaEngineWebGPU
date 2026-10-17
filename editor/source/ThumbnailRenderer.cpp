#include <Editor/ThumbnailRenderer.hpp>

#include <Mariana/Animation/AnimationPlayer.hpp>
#include <Mariana/Animation/Skin.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <Mariana/Importers/ImageImporter.hpp>
#include <Mariana/Importers/MaterialFile.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Rendering/RenderPipeline.hpp>
#include <Mariana/Scene/Prefab.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace fs = std::filesystem;

namespace
{
    const char* const kPipeline = "/Thumbnails/Thumbnail.pipeline";
    // Where the key light stands (degrees around +y, 0 = +z): in front of the subject, on the
    // other side from the camera (about +27), so lit and shaded sides both show.
    constexpr float kKeyAzimuth = -30.0f;
}

// Looks at the unit sphere around the origin the subject is scaled into: from the front (+z,
// where glTF models face), a little from the side and above.
class ThumbnailRenderer::Camera final : public ICamera
{
public:
    explicit Camera(IInput& input) : ICamera(input)
    {
        fov = kFov;
        aspect = 1.0f;
        const float distance = 1.05f / std::sin(glm::radians(kFov * 0.5f));
        _pos = glm::normalize(glm::vec3(0.5f, 0.4f, 1.0f)) * distance;
        nearClip = distance - 1.1f;
        farClip = distance + 1.1f;
        _view = glm::lookAtLH(_pos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        UpdateBuffer();
    }

    // Narrows the view until `box` just fits (the sphere around it always does).
    void Fit(const Bounds& box)
    {
        fov = kFov;
        UpdateBuffer();
        const glm::mat4 viewProj = Projection() * View();
        float extent = 0.0f;
        for (int i = 0; i < 8; ++i)
        {
            const glm::vec3 corner((i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y, (i & 4) ? box.max.z : box.min.z);
            const glm::vec4 clip = viewProj * glm::vec4(corner, 1.0f);
            if (clip.w > 1e-4f)
                extent = std::max({ extent, std::abs(clip.x / clip.w), std::abs(clip.y / clip.w) });
        }
        if (extent > 0.0f)
            fov = glm::degrees(2.0f * std::atan(std::tan(glm::radians(kFov * 0.5f)) * std::min(extent * 1.06f, 1.0f)));
        UpdateBuffer();
    }

private:
    static constexpr float kFov = 30.0f;
};

ThumbnailSource ThumbnailSource::Prepare(Kind kind, const fs::path& file, uint32_t maxImageSize)
{
    ThumbnailSource source;
    source.kind = kind;
    source.file = file;

    std::vector<fs::path> materialFiles;
    if (kind == Kind::Model)
    {
        GLTF::ParseOptions options;
        options.maxImageSize = maxImageSize;
        source.model = GLTF::GLTFLoader::Parse(file, source.error, options);
        if (!source.model)
            return source;
        source.dependencies = GLTF::GLTFLoader::Dependencies(*source.model);
        // Extracted materials replace the glTF's own; their images are decoded here too.
        std::error_code ec;
        for (const fs::path& dependency : source.dependencies)
            if (dependency.extension() == ".material" && fs::exists(dependency, ec))
                materialFiles.push_back(dependency);
    }
    else
    {
        materialFiles.push_back(file);
    }

    std::map<fs::path, Image::Pixels> decoded;
    std::set<std::pair<fs::path, bool>> added;
    for (const fs::path& materialFile : materialFiles)
    {
        std::string error;
        for (const MaterialFile::ImageRef& image : MaterialFile::Images(materialFile, error))
        {
            source.dependencies.push_back(image.file);
            auto [it, isNew] = decoded.try_emplace(image.file);
            if (isNew && Image::Load(image.file, it->second, error))
                it->second = Image::Downscale(std::move(it->second), maxImageSize);
            if (!it->second.Empty() && added.insert({ image.file, image.srgb }).second)
                source.images.push_back({ image.file, image.srgb, it->second });
        }
    }

    std::sort(source.dependencies.begin(), source.dependencies.end());
    source.dependencies.erase(std::unique(source.dependencies.begin(), source.dependencies.end()), source.dependencies.end());
    return source;
}

ThumbnailRenderer::ThumbnailRenderer(IInput& input, uint32_t size)
    : m_Size(size),
      m_SceneRenderer(m_Scene, m_Assets),
      m_Effects(m_Renderer.graph, m_SceneRenderer.CameraBuffer()),
      m_Camera(std::make_unique<Camera>(input))
{
    AnimationPlayer::RegisterComponents();
    SceneRenderer::RegisterComponents();
    m_Scene.UpdateComponentRegistry();
    MaterialFile::RegisterLoader(m_Assets);

    m_SceneRenderer.writeEntityIds = false;

    m_Renderer.graph.SetBackbufferFormat(TextureFormat::RGBA8Unorm);
    m_Renderer.graph.SetOutputSize(size, size);
    m_SceneRenderer.Init(m_Renderer.graph);

    // The sky is turned so its brightest side is the key light's (the default sky is brightest
    // toward -z, which would light models from behind), and the sun stands there too.
    const glm::vec3 sky = m_SceneRenderer.SkyDirection();
    m_SceneRenderer.lighting.skyRotation = kKeyAzimuth - glm::degrees(std::atan2(sky.x, sky.z));
    m_SceneRenderer.lighting.sunAzimuth = kKeyAzimuth;
    m_SceneRenderer.lighting.sunElevation = 45.0f;

    RenderPipelineDesc pipeline;
    std::string error;
    if (RenderPipelineDesc::Load(kPipeline, pipeline, error))
        pipeline.Apply(m_Effects, m_SceneRenderer, m_Camera.get());
    else
        Logger::Error("Could not load the thumbnail pipeline: " + error);

    m_Target.CreateRenderTexture(TextureFormat::RGBA8Unorm, static_cast<int>(size), static_cast<int>(size), false);

    m_AnimationSystem = m_Scene.CreateSystem<AnimationPlayer>([](Entity e, AnimationPlayer& player, float dt) {
        player.Update(e, dt);
    });
    // World bounds of everything drawn: posed skins by their joints, the rest by their mesh.
    m_BoundsSystem = m_Scene.CreateSystem<MeshComponent, WorldXform>([this](Entity entity, MeshComponent& mesh, WorldXform& world, float) {
        if (const SkinComponent* skin = entity.TryGet<SkinComponent>(); skin && !skin->bounds.IsEmpty())
        {
            m_Bounds.Add(skin->bounds.min);
            m_Bounds.Add(skin->bounds.max);
            return;
        }
        const IMesh* data = mesh.mesh.Get().get();
        if (!data || !data->hasBounds)
            return;
        const glm::mat4 model = glm::make_mat4(world.model);
        for (int i = 0; i < 8; ++i)
        {
            const glm::vec3 corner((i & 1) ? data->boundsMax.x : data->boundsMin.x,
                                   (i & 2) ? data->boundsMax.y : data->boundsMin.y,
                                   (i & 4) ? data->boundsMax.z : data->boundsMin.z);
            m_Bounds.Add(glm::vec3(model * glm::vec4(corner, 1.0f)));
        }
    });
}

ThumbnailRenderer::~ThumbnailRenderer()
{
    Clear();
}

bool ThumbnailRenderer::Render(ThumbnailSource& source, std::string& error)
{
    if (m_Busy)
    {
        error = "Already rendering";
        return false;
    }
    if (!source.error.empty())
    {
        error = source.error;
        return false;
    }

    // Loading the materials finds these instead of reading the files.
    for (const ThumbnailSource::DecodedImage& image : source.images)
        ImageImporter::RegisterTexture(m_Assets, image.file, image.srgb, image.pixels);

    Entity root;
    if (source.kind == ThumbnailSource::Kind::Model)
    {
        if (source.model)
            m_Prefab = GLTF::GLTFLoader::LoadGLTF(m_Assets, *source.model);
        if (m_Prefab)
            root = m_Scene.Instantiate(*m_Prefab);
    }
    else if (AssetHandle<MaterialAsset> material = m_Assets.Load<MaterialAsset>(source.file); material.IsValid())
    {
        root = m_Scene.Instantiate("Material");
        root.Add<MeshComponent>(MeshComponent{ BuiltinAssets::PrimitiveMesh(m_Assets, BuiltinMesh::Sphere), { material } }).AddTag<ShadowCasterTag>();
    }
    if (!root.IsValid())
    {
        error = "Could not load " + source.file.string();
        Clear();
        return false;
    }

    // Animated models show the first frame of their first clip. Prepare poses the skins, whose
    // bounds the framing needs.
    m_AnimationSystem.Run(0.0f);
    m_Scene.Update(0.0f);
    m_SceneRenderer.Prepare(*m_Camera);
    m_Bounds = {};
    m_BoundsSystem.Run();
    if (m_Bounds.IsEmpty())
    {
        error = "Nothing to draw in " + source.file.string();
        Clear();
        return false;
    }

    const glm::vec3 center = (m_Bounds.min + m_Bounds.max) * 0.5f;
    const float scale = 1.0f / std::max(glm::length(m_Bounds.max - m_Bounds.min) * 0.5f, 1e-6f);
    root.SetScale(scale, scale, scale);
    root.SetPosition(-center.x * scale, -center.y * scale, -center.z * scale);
    m_Scene.Update(0.0f);
    Bounds fitted;
    fitted.Add((m_Bounds.min - center) * scale);
    fitted.Add((m_Bounds.max - center) * scale);
    m_Camera->Fit(fitted);

    m_SceneRenderer.InvalidateShadows();
    m_SceneRenderer.Prepare(*m_Camera);
    m_Effects.Update();
    m_Renderer.RenderToTexture(m_Target);
    m_Target.RequestImageReadback();
    m_Busy = true;
    return true;
}

bool ThumbnailRenderer::TryGetResult(Image::Pixels& image)
{
    if (!m_Busy || !m_Target.TryGetImageReadback(image.rgba))
        return false;
    image.width = image.height = static_cast<int>(m_Size);
    Clear();
    return true;
}

void ThumbnailRenderer::Clear()
{
    m_Scene.Clear();
    m_Prefab.reset();
    m_SceneRenderer.InvalidateTransforms();
    m_Assets.UnloadUnused();
    m_Busy = false;
}
