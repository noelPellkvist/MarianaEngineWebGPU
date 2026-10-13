#pragma once
#include <Rendering/RenderGraph.hpp>
#include <functional>

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
};