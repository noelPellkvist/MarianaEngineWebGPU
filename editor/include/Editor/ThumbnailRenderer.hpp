#pragma once
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Foundation/Image.hpp>
#include <Mariana/Graphics/Texture.hpp>
#include <Mariana/Rendering/Effect.hpp>
#include <Mariana/Rendering/ICamera.hpp>
#include <Mariana/Rendering/Renderer.hpp>
#include <Mariana/Rendering/SceneRenderer.hpp>
#include <Mariana/Rendering/SceneView.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace GLTF { struct ParsedGLTF; }
class Prefab;

// What a thumbnail is rendered from, read and decoded ahead (Prepare, any thread) so rendering
// only uploads: a parsed model or a material file, and every image their materials use.
struct ThumbnailSource
{
    enum class Kind { Model, Material };

    struct DecodedImage
    {
        std::filesystem::path file;
        bool srgb = false;
        Image::Pixels pixels;
    };

    Kind kind = Kind::Model;
    std::filesystem::path file;
    std::shared_ptr<GLTF::ParsedGLTF> model; // Kind::Model
    std::vector<DecodedImage> images;
    // Every file besides `file` that the thumbnail shows (buffers, images, material files), whether
    // it exists or not.
    std::vector<std::filesystem::path> dependencies;
    std::string error; // set when it can't be rendered

    // Images are halved until they fit `maxImageSize`.
    static ThumbnailSource Prepare(Kind kind, const std::filesystem::path& file, uint32_t maxImageSize);
};

// Renders models and materials (on a sphere) into small images with the engine's renderer: a
// Renderer, Scene, SceneRenderer, SceneView and EffectStack of its own (and an AssetManager2, so
// nothing is shared with the editor's scene) drawing offscreen with /Thumbnails/Thumbnail.pipeline. The
// subject is scaled into a unit sphere and seen from its front (+z), lit from the front by the
// sun and the sky's brightest side (the sky is turned to put it there), on a transparent
// background. GPU work: main thread only, one thumbnail at a time.
class ThumbnailRenderer
{
public:
    explicit ThumbnailRenderer(IInput& input, uint32_t size = 256);
    ~ThumbnailRenderer();

    ThumbnailRenderer(const ThumbnailRenderer&) = delete;
    ThumbnailRenderer& operator=(const ThumbnailRenderer&) = delete;

    uint32_t Size() const { return m_Size; }
    // Rendering, waiting for the image to come back.
    bool Busy() const { return m_Busy; }

    // Renders `source` (its meshes are used up); the image arrives through TryGetResult a frame
    // or so later. False, with `error`, when there is nothing to draw.
    bool Render(ThumbnailSource& source, std::string& error);
    // The finished image (Size() square, RGBA8, sRGB-encoded, alpha 0 around the subject).
    bool TryGetResult(Image::Pixels& image);

private:
    class Camera;

    uint32_t m_Size;
    AssetManager2 m_Assets;
    // Declared before the view and effects, which remove their passes from its graph.
    Renderer m_Renderer;
    Scene m_Scene;
    SceneRenderer m_SceneRenderer;
    SceneView m_View;
    EffectStack m_Effects;
    std::unique_ptr<Camera> m_Camera;
    Texture m_Target;
    // Systems query m_Scene, so they are destroyed before it.
    System m_AnimationSystem;
    System m_BoundsSystem;
    Bounds m_Bounds;

    std::shared_ptr<Prefab> m_Prefab;
    bool m_Busy = false;

    // Drops everything the last thumbnail loaded.
    void Clear();
};
