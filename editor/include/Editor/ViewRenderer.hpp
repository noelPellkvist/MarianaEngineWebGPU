#pragma once

#include <Mariana/Graphics/Texture.hpp>
#include <Mariana/Rendering/Effect.hpp>
#include <Mariana/Rendering/Renderer.hpp>
#include <Mariana/Rendering/SceneRenderer.hpp>
#include <Mariana/Rendering/SceneView.hpp>

#include <glm/glm.hpp>

class ICamera;

// One view of the editor's scene renderer, drawn into a texture an editor window shows: a render
// graph, SceneView and EffectStack of its own on the scene renderer's shared data, at the window's
// size. Views that aren't shown aren't drawn.
class ViewRenderer
{
public:
    // `entityIds`: keep the entity id prepass (picking).
    ViewRenderer(SceneRenderer& sceneRenderer, bool entityIds);
    ~ViewRenderer();

    ViewRenderer(const ViewRenderer&) = delete;
    ViewRenderer& operator=(const ViewRenderer&) = delete;

    // Recreates the image and the graph's textures for a new size (zero is ignored). Call before the
    // UI shows the image this frame, so it never shows one that is gone. True when it changed.
    bool Resize(glm::uvec2 size);
    glm::uvec2 Size() const { return m_Size; }

    // Draws the scene from `camera` into Image(); SceneRenderer::Update must have run this frame.
    void Render(ICamera& camera);
    // nullptr until it has a size.
    Texture* Image() { return m_Size.x > 0 ? &m_Target : nullptr; }

    RenderGraph& Graph() { return m_Renderer.graph; }
    SceneView& View() { return m_View; }
    EffectStack& Effects() { return m_Effects; }

    // Makes this view's effects match `source` (another view's): the same effect files in the same
    // order, parameters and toggles. Effects are only reloaded when the list itself changed.
    void MirrorEffects(const EffectStack& source);

private:
    // Declared first: the view and the effects remove their passes from its graph.
    Renderer m_Renderer;
    SceneView m_View;
    EffectStack m_Effects;
    Texture m_Target;
    glm::uvec2 m_Size{ 0 };
};
