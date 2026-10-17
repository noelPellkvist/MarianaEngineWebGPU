#pragma once
#include <Mariana/Rendering/RenderGraph.hpp>
#include <functional>

class Texture;

// Draws a frame: executes the render graph (whatever writes "Backbuffer" ends up on screen), then
// an optional overlay (e.g. a UI) on top of it.
class Renderer
{
    public:
        // Records on top of the finished frame, in a pass that loads the backbuffer and has no
        // depth. It gets that pass's wgpu::RenderPassEncoder*.
        using Overlay = std::function<void(void* renderPassEncoder)>;

        Renderer();
        ~Renderer();

        // Set its backbuffer format before adding passes that render to the screen.
        RenderGraph graph;

        void Render(const Overlay& overlay = {});
        // Draws a frame into `target` instead of the screen (offscreen rendering, e.g. previews):
        // a render texture of the graph's backbuffer format and output size.
        void RenderToTexture(Texture& target);
        // Puts only the overlay on the screen, over a cleared background: for apps whose graphs
        // all render into textures (the editor shows its views as images in its UI).
        static void RenderOverlay(const Overlay& overlay);
};