#include <Editor/ViewRenderer.hpp>
#include <Mariana/Foundation/Profiler.hpp>
#include <Mariana/Rendering/ICamera.hpp>

ViewRenderer::ViewRenderer(SceneRenderer& sceneRenderer, bool entityIds)
    : m_View(sceneRenderer),
      m_Effects(m_Renderer.graph, m_View.CameraBuffer())
{
    // UNORM like the screen: the final effect writes sRGB-encoded colors, which ImGui shows as they are.
    // The scene view picks (entity ids); the game view doesn't.
    m_Renderer.graph.SetName(entityIds ? "Scene" : "Game");
    m_Renderer.graph.SetBackbufferFormat(TextureFormat::RGBA8Unorm);
    m_Renderer.graph.SetOutputSize(16, 16);
    m_View.writeEntityIds = entityIds;
    m_View.Init(m_Renderer.graph);
}

ViewRenderer::~ViewRenderer() = default;

bool ViewRenderer::Resize(glm::uvec2 size)
{
    if (size.x == 0 || size.y == 0 || size == m_Size)
        return false;
    // A new texture (not a reused one), so the image the UI showed stays valid until it is drawn.
    m_Target = Texture();
    m_Target.CreateRenderTexture(TextureFormat::RGBA8Unorm, static_cast<int>(size.x), static_cast<int>(size.y), false);
    m_Renderer.graph.SetOutputSize(size.x, size.y);
    m_Size = size;
    return true;
}

void ViewRenderer::Render(ICamera& camera)
{
    if (m_Size.x == 0)
        return;
    MARIANA_PROFILE(m_View.writeEntityIds ? "Scene view" : "Game view");
    m_View.Prepare(camera);
    m_Effects.Update();
    m_Renderer.RenderToTexture(m_Target);
}

void ViewRenderer::MirrorEffects(const EffectStack& source)
{
    const auto& from = source.Effects();
    const auto& to = m_Effects.Effects();
    bool same = from.size() == to.size();
    for (size_t i = 0; same && i < from.size(); ++i)
        same = from[i]->SharedDefinition() == to[i]->SharedDefinition();
    if (!same)
    {
        m_Effects.Clear();
        for (const auto& effect : from)
            m_Effects.Add(effect->SharedDefinition());
    }

    bool toggled = false;
    for (size_t i = 0; i < from.size() && i < to.size(); ++i)
    {
        if (to[i]->enabled != from[i]->enabled)
        {
            to[i]->enabled = from[i]->enabled;
            toggled = true;
        }
        for (size_t p = 0; p < from[i]->ParamCount(); ++p)
            to[i]->ParamValue(p) = from[i]->ParamValue(p);
    }
    if (toggled)
        m_Effects.MarkDirty();
}
