#pragma once
#include <webgpu/webgpu_cpp.h>

// Opt-in access to the WebGPU objects behind the engine, for code that talks to WebGPU itself
// (e.g. a UI library's backend). The only engine header that includes webgpu_cpp.h.
namespace NativeGPU
{
    const wgpu::Device& Device();
    // The format of the window's swapchain (the backbuffer).
    wgpu::TextureFormat SurfaceFormat();
}
