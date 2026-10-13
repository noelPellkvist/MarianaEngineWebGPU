#include <NativeGPU.hpp>
#include "Init.hpp"

namespace NativeGPU
{
    const wgpu::Device& Device()
    {
        return device;
    }

    wgpu::TextureFormat SurfaceFormat()
    {
        return windowFormat;
    }
}
